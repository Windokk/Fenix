#include "drivers/usb/xhci.h"

#include "memory/paging_pages.h"

#include "interrupts/idt.h"

#include "interrupts/irq.h"

#include "input/input_subsys.h"

static volatile uint8_t* xhci_bar0;
static volatile uint8_t* xhci_op;
static volatile uint8_t* xhci_ir0;

static trb_t cmd_ring[XHCI_CMD_RING_SIZE] __attribute__((aligned(64)));
static uint8_t cmd_pcs = 1;
static uint32_t cmd_enqueue = 0;

static trb_t evt_ring[XHCI_EVT_RING_SIZE] __attribute__((aligned(64)));
static uint8_t evt_ccs = 1;
static uint32_t evt_dequeue = 0;

static erst_entry_t erst[1] __attribute__((aligned(64)));

static uint64_t dcbaap[XHCI_MAX_SLOTS_PLUS_ONE] __attribute__((aligned(64))) = {0} ;

static uint32_t xhci_dboff;

// Slots logic
static uint8_t slot_port_map[XHCI_MAX_SLOTS_PLUS_ONE] = {0};
static uint8_t pending_ports[XHCI_MAX_SLOTS_PLUS_ONE] = {0};
static uint8_t pending_head = 0; // consumed by handler type 33
static uint8_t pending_tail = 0;
static xhci_slot_state_t slot_state[XHCI_MAX_SLOTS_PLUS_ONE];

//Device logic
static uint32_t device_ctx[XHCI_MAX_SLOTS_PLUS_ONE][16] __attribute__((aligned(64))); // 16 dwords = 64 bytes per slot
static uint32_t input_ctx[XHCI_MAX_SLOTS_PLUS_ONE][40] __attribute__((aligned(64))); // 17 dwords = 68 bytes : 1 Input Control Context + Slot Context + EP0 Context
static trb_t ep0_ring[XHCI_MAX_SLOTS_PLUS_ONE][XHCI_EP0_RING_SIZE] __attribute__((aligned(16))); // Transfer Ring for EP0, 16 TRBs per slot
static uint8_t ep0_pcs[XHCI_MAX_SLOTS_PLUS_ONE];
static uint8_t ep0_enqueue[XHCI_MAX_SLOTS_PLUS_ONE];  // index for ep0_ring

static trb_t ep1in_ring[XHCI_MAX_SLOTS_PLUS_ONE][XHCI_EP1IN_RING_SIZE] __attribute__((aligned(16)));
static uint8_t ep1in_pcs[XHCI_MAX_SLOTS_PLUS_ONE];
static uint8_t ep1in_enqueue[XHCI_MAX_SLOTS_PLUS_ONE];

//Devices Descriptors
static uint8_t desc_buf[XHCI_MAX_SLOTS_PLUS_ONE][18] __attribute__((aligned(16)));
static uint8_t cfg_buf[XHCI_MAX_SLOTS_PLUS_ONE][255] __attribute__((aligned(16)));
static uint8_t slot_maxpkt[XHCI_MAX_SLOTS_PLUS_ONE];
static uint8_t slot_protocol[XHCI_MAX_SLOTS_PLUS_ONE];
static uint8_t str_buf[XHCI_MAX_SLOTS_PLUS_ONE][64];
static char manufacturer_buf[XHCI_MAX_SLOTS_PLUS_ONE][64];
static char product_buf[XHCI_MAX_SLOTS_PLUS_ONE][64];
static char serial_buf[XHCI_MAX_SLOTS_PLUS_ONE][32];
static usb_device_desc_t device_desc[XHCI_MAX_SLOTS_PLUS_ONE];

//Device data
static uint8_t hid_buf[XHCI_MAX_SLOTS_PLUS_ONE][8] __attribute__((aligned(16)));
static bool caps_lock_state[XHCI_MAX_SLOTS_PLUS_ONE];
uint8_t prev_hid_buf[XHCI_MAX_SLOTS_PLUS_ONE][8];
static const unsigned char hid_to_ascii[256] = {
    0, 0, 0, 0,                                                                 // 0x00-0x03
    'a','b','c','d','e','f','g','h','i','j','k','l','m',                         // 0x04-0x10
    'n','o','p','q','r','s','t','u','v','w','x','y','z',                         // 0x11-0x1D
    '1','2','3','4','5','6','7','8','9','0',                                     // 0x1E-0x27
    '\n', 0, '\b', '\t', ' ',                                                     // 0x28-0x2C
    '-','=','[',']','\\',                                                          // 0x2D-0x31
    0,                                                                             // 0x32 non-US hash
    ';','\'','`',',','.','/',                                                      // 0x33-0x38
};

mouse_pending_t mouse_state;
semaphore_t mouse_sem;

pci_device_id_t id_compat_table[2] = {
    {0x1B36, 0x000D}, // QEMU XHCI (only for testing)
    {0x0,    0x0}
};

pci_device_code_t code_compat_table[2] = {
    {0x0C,   0x03,    0x30},
    {0x0,    0x0,     0x0}
};

pci_driver_t xhci_driver = {
    "xHCI driver",
    id_compat_table,
    code_compat_table,
    xhci_probe,
    xhci_remove
};

static void xhci_enqueue_cmd(trb_t cmd){
    cmd_ring[cmd_enqueue] = cmd;
    cmd_enqueue++;

    if(cmd_enqueue >= XHCI_CMD_RING_SIZE - 1){
        cmd_ring[XHCI_CMD_RING_SIZE - 1].control =
            (cmd_ring[XHCI_CMD_RING_SIZE - 1].control & ~1u) | cmd_pcs;
        cmd_enqueue = 0;
        cmd_pcs ^= 1;
    }

    *(volatile uint32_t*)(xhci_bar0 +xhci_dboff) = 0;
}

static void xhci_enqueue_ep0(trb_t cmd, uint32_t slot_id){
    ep0_ring[slot_id][ep0_enqueue[slot_id]] = cmd;
    ep0_enqueue[slot_id]++;

    if(ep0_enqueue[slot_id] >= XHCI_EP0_RING_SIZE - 1){
        ep0_ring[slot_id][XHCI_EP0_RING_SIZE - 1].control =
            (ep0_ring[slot_id][XHCI_EP0_RING_SIZE - 1].control & ~1u) | ep0_pcs[slot_id];
        ep0_enqueue[slot_id] = 0;
        ep0_pcs[slot_id] ^= 1;
    }
}

static void xhci_enqueue_ep1in(trb_t cmd, uint32_t slot_id){
    ep1in_ring[slot_id][ep1in_enqueue[slot_id]] = cmd;
    ep1in_enqueue[slot_id]++;

    if(ep1in_enqueue[slot_id] >= XHCI_EP1IN_RING_SIZE - 1){
        ep1in_ring[slot_id][XHCI_EP1IN_RING_SIZE - 1].control =
            (ep1in_ring[slot_id][XHCI_EP1IN_RING_SIZE - 1].control & ~1u) | ep1in_pcs[slot_id];
        ep1in_enqueue[slot_id] = 0;
        ep1in_pcs[slot_id] ^= 1;
    }
}

static bool xhci_fetch_str(uint32_t slot_id, uint8_t index, xhci_slot_state_t next_state){
    
    if(index != 0){
        uint64_t param = (0x0040ULL << 48) | (0x0409ULL << 32) | ((0x0300ULL | index) << 16) | 0x0680;

        trb_t setup_trb = {0};
        setup_trb.parameter = param;
        setup_trb.status = 8;
        setup_trb.control = (2 << 10) | (3 << 16) | (1 << 6) | ep0_pcs[slot_id];
        xhci_enqueue_ep0(setup_trb, slot_id);

        trb_t data_trb = {0};
        data_trb.parameter = TO_PHYS_U64(&str_buf[slot_id][0]);
        data_trb.status    = 64; // TRB Transfer Length = wLength
        data_trb.control   = (3 << 10) | (1 << 16) | (1 << 5) | ep0_pcs[slot_id];
        xhci_enqueue_ep0(data_trb, slot_id);

        trb_t status_trb = {0};
        status_trb.parameter = 0;
        status_trb.status    = 0;
        status_trb.control   = (4 << 10) | ep0_pcs[slot_id];
        xhci_enqueue_ep0(status_trb, slot_id);

        *(volatile uint32_t*)(xhci_bar0 + xhci_dboff + slot_id * 4) = 1; // Doorbell

        slot_state[slot_id] = next_state;
        return true;
    }

    return false;
}

static void xhci_fetch_config(uint32_t slot_id){

    if(device_desc[slot_id].product_name && device_desc[slot_id].serial_number)
    {
        KLOG_INFO("XHCI", "Device infos : %s (Serial number : %s)", device_desc[slot_id].product_name, device_desc[slot_id].serial_number);
    }

    trb_t setup_trb = {0};
    setup_trb.parameter = 0x00FF000002000680;
    setup_trb.status = 8;
    setup_trb.control = (2 << 10) | (3 << 16) | (1 << 6) | ep0_pcs[slot_id];
    xhci_enqueue_ep0(setup_trb, slot_id);

    trb_t data_trb = {0};
    data_trb.parameter = TO_PHYS_U64(&cfg_buf[slot_id][0]);
    data_trb.status    = 255; // TRB Transfer Length = wLength
    data_trb.control   = (3 << 10) | (1 << 16) | (1 << 5) | ep0_pcs[slot_id];
    xhci_enqueue_ep0(data_trb, slot_id);

    trb_t status_trb = {0};
    status_trb.parameter = 0;
    status_trb.status    = 0;
    status_trb.control   = (4 << 10) | ep0_pcs[slot_id];
    xhci_enqueue_ep0(status_trb, slot_id);

    *(volatile uint32_t*)(xhci_bar0 + xhci_dboff + slot_id * 4) = 1; // Doorbell

    slot_state[slot_id] = XHCI_SLOT_CFG_DONE;
}

void xhci_irq_handler(isr_ctx_t* ctx){
    (void)ctx;
    while((evt_ring[evt_dequeue].control & 1) == evt_ccs){
        uint32_t type = (evt_ring[evt_dequeue].control >> 10) & 0x3F;
        if(type == 32){
            uint32_t slot_id = (evt_ring[evt_dequeue].control >> 24) & 0xFF;
            uint32_t completion_code = (evt_ring[evt_dequeue].status >> 24) & 0xFF;
            if(completion_code == 1 || completion_code == 13)
            {
                if(slot_state[slot_id] == XHCI_SLOT_DESC_DONE){
                    
                    if(!xhci_fetch_str(slot_id, desc_buf[slot_id][14], XHCI_SLOT_STR_MANUFACTURER)){ 
                        device_desc[slot_id].manufacturer_name = "";
                        if(!xhci_fetch_str(slot_id, desc_buf[slot_id][15], XHCI_SLOT_STR_PRODUCT)){
                            device_desc[slot_id].product_name = "";
                            if(!xhci_fetch_str(slot_id, desc_buf[slot_id][16], XHCI_SLOT_STR_SERIAL)){
                                device_desc[slot_id].serial_number = "";
                                xhci_fetch_config(slot_id);
                            }
                        }
                    }
                }
                else if(slot_state[slot_id] == XHCI_SLOT_STR_MANUFACTURER){

                    // Read string : Vendor name

                    uint8_t nb_chars = (str_buf[slot_id][0] - 2) / 2;
                    char* ascii_buff = manufacturer_buf[slot_id];
                    device_desc[slot_id].manufacturer_name = ascii_buff;
                    for(uint32_t i = 0; i < nb_chars; i++){
                        *ascii_buff++ = str_buf[slot_id][2 + i * 2];
                    }
                    *ascii_buff = '\0';

                    if(!xhci_fetch_str(slot_id, desc_buf[slot_id][15], XHCI_SLOT_STR_PRODUCT)){
                        device_desc[slot_id].product_name = "";
                        if(!xhci_fetch_str(slot_id, desc_buf[slot_id][16], XHCI_SLOT_STR_SERIAL)){
                            device_desc[slot_id].serial_number = "";
                            xhci_fetch_config(slot_id);
                        }
                    }
                }
                else if(slot_state[slot_id] == XHCI_SLOT_STR_PRODUCT){

                    // Read string : Product name

                    uint8_t nb_chars = (str_buf[slot_id][0] - 2) / 2;
                    char* ascii_buff = product_buf[slot_id];
                    device_desc[slot_id].product_name = ascii_buff;
                    for(uint32_t i = 0; i < nb_chars; i++){
                        *ascii_buff++ = str_buf[slot_id][2 + i * 2];
                    }
                    *ascii_buff = '\0';

                    if(!xhci_fetch_str(slot_id, desc_buf[slot_id][16], XHCI_SLOT_STR_SERIAL)){
                        device_desc[slot_id].serial_number = "";
                        xhci_fetch_config(slot_id);
                    }
                }
                else if(slot_state[slot_id] == XHCI_SLOT_STR_SERIAL){

                    // Read string : Serial number

                    uint8_t nb_chars = (str_buf[slot_id][0] - 2) / 2;
                    char* ascii_buff = serial_buf[slot_id];
                    device_desc[slot_id].serial_number = ascii_buff;
                    for(uint32_t i = 0; i < nb_chars; i++){
                        *ascii_buff++ = str_buf[slot_id][2 + i * 2];
                    }
                    *ascii_buff = '\0';

                    xhci_fetch_config(slot_id);
                
                }
                else if(slot_state[slot_id] == XHCI_SLOT_CFG_DONE){
                    slot_maxpkt[slot_id] = (cfg_buf[slot_id][32] << 8) | cfg_buf[slot_id][31];
                    slot_protocol[slot_id] = cfg_buf[slot_id][16];

                    trb_t setup_trb = {0};
                    setup_trb.parameter = 0x0000000000010900;
                    setup_trb.status    = 8;
                    setup_trb.control   = (2 << 10) | (0 << 16) | (1 << 6) | ep0_pcs[slot_id];
                    xhci_enqueue_ep0(setup_trb, slot_id);

                    trb_t status_trb = {0};
                    status_trb.parameter = 0;
                    status_trb.status    = 0;
                    status_trb.control = (4 << 10) | (1 << 16) | (1 << 5) | ep0_pcs[slot_id];
                    xhci_enqueue_ep0(status_trb, slot_id);

                    *(volatile uint32_t*)(xhci_bar0 + xhci_dboff + slot_id * 4) = 1;

                    slot_state[slot_id] = XHCI_SLOT_SETCFG_DONE;
                }
                else if(slot_state[slot_id] == XHCI_SLOT_SETCFG_DONE){
                    ep1in_ring[slot_id][XHCI_EP1IN_RING_SIZE - 1].parameter = TO_PHYS_U64(&ep1in_ring[slot_id][0]);
                    ep1in_ring[slot_id][XHCI_EP1IN_RING_SIZE - 1].control   = (6 << 10) | (1 << 1) | 1;
                    ep1in_pcs[slot_id] = 1;

                    input_ctx[slot_id][1] = 0b1001;

                    uint32_t psc_off  = 0x400 + (slot_port_map[slot_id] - 1) * 0x10;
                    uint32_t portsc   = *(volatile uint32_t*)(xhci_op + psc_off);
                    uint8_t speed = (portsc >> 10) & 0xF;

                    input_ctx[slot_id][8] = (3 << 27) | (speed << 20);

                    input_ctx[slot_id][32] = ((cfg_buf[slot_id][33] - 1) << 16);
                    input_ctx[slot_id][33] = (slot_maxpkt[slot_id] << 16) | (7 << 3) | (3 << 1);
                    input_ctx[slot_id][34] = (TO_PHYS_U64(&ep1in_ring[slot_id][0]) & 0xFFFFFFFF) | 1;
                    input_ctx[slot_id][35] = TO_PHYS_U64(&ep1in_ring[slot_id][0]) >> 32;
                    input_ctx[slot_id][36] = slot_maxpkt[slot_id];

                    trb_t cmd = {0};
                    cmd.parameter = TO_PHYS_U64(input_ctx[slot_id]);  // no BSR, bit 0 = 0
                    cmd.status    = 0;
                    cmd.control   = (12 << 10) | (slot_id << 24) | cmd_pcs;
                    xhci_enqueue_cmd(cmd);

                    slot_state[slot_id] = XHCI_SLOT_EP_CONFIGURED;
                }
                else if(slot_state[slot_id] == XHCI_SLOT_POLLING){
                    // TODO : Move this branch into a separate HID only file
                    
                    // HID Devices Interrupts Handler
                    if(slot_protocol[slot_id] == 1){
                        //keyboard
                        bool lctrl = hid_buf[slot_id][0] & (1 << 0);
                        bool lshift = hid_buf[slot_id][0] & (1 << 1);
                        bool lalt = hid_buf[slot_id][0] & (1 << 2);
                        //bool meta = hid_buf[slot_id][0] & (1 << 3);
                        bool rctrl = hid_buf[slot_id][0] & (1 << 4);
                        bool rshift = hid_buf[slot_id][0] & (1 << 5);
                        bool ralt = hid_buf[slot_id][0] & (1 << 6);
                        //meta = meta || hid_buf[slot_id][0] & (1 << 7);
                        
                        for(uint32_t i = 2; i < 8; i++)
                        {
                            bool create_evt = true;
                            
                            if(hid_buf[slot_id][i] == 0x00 || hid_buf[slot_id][i] == 0x01){
                                create_evt = false;
                            }
                            bool caps_lock_new = true;
                            for(uint32_t k = 2; k < 8; k++){
                                if(prev_hid_buf[slot_id][k] == 0x39){
                                    caps_lock_new = false;
                                }
                            }
                            if(hid_buf[slot_id][i] == 0x39 && caps_lock_new){
                                caps_lock_state[slot_id] = !caps_lock_state[slot_id];
                            }
                            for(uint32_t k = 2; k < 8; k++){
                                if(prev_hid_buf[slot_id][k] == hid_buf[slot_id][i]){
                                    create_evt = false;
                                }
                            }

                            if(create_evt){
                                key_event_t ev = {
                                    .scancode   = hid_buf[slot_id][i],
                                    .ascii      = hid_to_ascii[hid_buf[slot_id][i]],
                                    .released   = false,
                                    .lshift     = lshift,
                                    .rshift     = rshift,
                                    .lctrl      = lctrl,
                                    .rctrl      = rctrl,
                                    .lalt       = lalt,
                                    .ralt       = ralt,
                                    .caps_lock = caps_lock_state[slot_id],
                                };

                                input_key_event(ev);
                            }            
                        }

                        for(uint32_t i = 2; i < 8; i++)
                        {
                            bool create_evt = true;

                            if(prev_hid_buf[slot_id][i] == 0x00 || prev_hid_buf[slot_id][i] == 0x01){
                                create_evt = false;
                            }
                            for(uint32_t k = 2; k < 8; k++){
                                if(prev_hid_buf[slot_id][i] == hid_buf[slot_id][k]){
                                    create_evt = false;
                                }
                            }

                            if(create_evt){
                                key_event_t ev = {
                                    .scancode   = prev_hid_buf[slot_id][i],
                                    .ascii      = hid_to_ascii[prev_hid_buf[slot_id][i]],
                                    .released   = true,
                                    .lshift     = lshift,
                                    .rshift     = rshift,
                                    .lctrl      = lctrl,
                                    .rctrl      = rctrl,
                                    .lalt       = lalt,
                                    .ralt       = ralt,
                                    .caps_lock = caps_lock_state[slot_id],
                                };

                                input_key_event(ev);
                            }
                        }

                        for(uint32_t i = 0; i < 8; i++){
                            prev_hid_buf[slot_id][i] = hid_buf[slot_id][i];
                        }
                    }
                    else if(slot_protocol[slot_id] == 2){
                        //mouse
                        uint8_t mouse_buttons = hid_buf[slot_id][0];
                        int8_t mouse_delta_x = (int8_t)hid_buf[slot_id][1];
                        int8_t mouse_delta_y = (int8_t)hid_buf[slot_id][2];
                        int8_t mouse_delta_wheel = (int8_t)hid_buf[slot_id][3];

                        uintptr_t flags = disable_interrupts();

                        mouse_state.pending_dx = mouse_delta_x;
                        mouse_state.pending_dy = mouse_delta_y;
                        mouse_state.buttons = mouse_buttons;
                        mouse_state.pending_wheel = mouse_delta_wheel;
                        mouse_state.dirty = 1;

                        sem_signal(&mouse_sem);

                        restore_interrupts(flags);
                    }

                    trb_t normal_trb = {0};
                    normal_trb.parameter = TO_PHYS_U64(hid_buf[slot_id]);
                    normal_trb.status    = slot_maxpkt[slot_id];
                    normal_trb.control   = (1 << 10) | (1 << 5) | ep1in_pcs[slot_id];
                    xhci_enqueue_ep1in(normal_trb, slot_id);
                    *(volatile uint32_t*)(xhci_bar0 + xhci_dboff + slot_id * 4) = 3;
                }
            }
        }
        else if(type == 33){
            uint32_t slot_id = (evt_ring[evt_dequeue].control >> 24) & 0xFF;
            uint32_t completion_code = (evt_ring[evt_dequeue].status >> 24) & 0xFF;
            if(completion_code == 1 || completion_code == 13) 
            {
                if(slot_port_map[slot_id] == 0){
                    slot_port_map[slot_id] = pending_ports[pending_head++];
                }   
                
                if(slot_state[slot_id] == XHCI_SLOT_ENABLE_SENT){
                    uint32_t psc_off  = 0x400 + (slot_port_map[slot_id] - 1) * 0x10;
                    uint32_t portsc   = *(volatile uint32_t*)(xhci_op + psc_off);

                    uint8_t speed = (portsc >> 10) & 0xF;

                    input_ctx[slot_id][0] = 0;
                    input_ctx[slot_id][1] = 0b11;

                    input_ctx[slot_id][2] = 0;
                    input_ctx[slot_id][3] = 0;
                    input_ctx[slot_id][4] = 0;
                    input_ctx[slot_id][5] = 0;
                    input_ctx[slot_id][6] = 0;
                    input_ctx[slot_id][7] = 0;

                    input_ctx[slot_id][8] = (1 << 27) | (speed << 20);
                    input_ctx[slot_id][9] = (slot_port_map[slot_id] << 16);

                    input_ctx[slot_id][10] = 0;
                    input_ctx[slot_id][11] = 0;
                    input_ctx[slot_id][12] = 0;
                    input_ctx[slot_id][13] = 0;
                    input_ctx[slot_id][14] = 0;
                    input_ctx[slot_id][15] = 0;

                    input_ctx[slot_id][16] = 0;
                    input_ctx[slot_id][17] = (64 << 16) | (4 << 3) | (3 << 1);
                    input_ctx[slot_id][18] = (TO_PHYS_U64(&ep0_ring[slot_id][0]) & 0xFFFFFFFF) | 1;
                    input_ctx[slot_id][19] = TO_PHYS_U64(&ep0_ring[slot_id][0]) >> 32;
                    input_ctx[slot_id][20] = 8;

                    ep0_ring[slot_id][XHCI_EP0_RING_SIZE - 1].parameter = TO_PHYS_U64(&ep0_ring[slot_id][0]);
                    ep0_ring[slot_id][XHCI_EP0_RING_SIZE - 1].control   = (6 << 10) | (1 << 1) | 1;
                    
                    ep0_pcs[slot_id] = 1;

                    dcbaap[slot_id] = TO_PHYS_U64(device_ctx[slot_id]);

                    trb_t cmd = {0};
                    cmd.parameter = TO_PHYS_U64(input_ctx[slot_id]);  // no BSR, bit 0 = 0
                    cmd.status    = 0;
                    cmd.control   = (11 << 10) | (slot_id << 24) | cmd_pcs;
                    xhci_enqueue_cmd(cmd);

                    slot_state[slot_id] = XHCI_SLOT_ADDRESS_SENT;
                }
                else if(slot_state[slot_id] == XHCI_SLOT_ADDRESS_SENT){

                    trb_t setup_trb = {0};
                    setup_trb.parameter = 0x0012000001000680;
                    setup_trb.status = 8;
                    setup_trb.control = (2 << 10) | (3 << 16) | (1 << 6) | ep0_pcs[slot_id];
                    xhci_enqueue_ep0(setup_trb, slot_id);

                    trb_t data_trb = {0};
                    data_trb.parameter = TO_PHYS_U64(&desc_buf[slot_id][0]);
                    data_trb.status    = 18; // TRB Transfer Length = wLength
                    data_trb.control   = (3 << 10) | (1 << 16) | (1 << 5) | ep0_pcs[slot_id];
                    xhci_enqueue_ep0(data_trb, slot_id);

                    trb_t status_trb = {0};
                    status_trb.parameter = 0;
                    status_trb.status    = 0;
                    status_trb.control   = (4 << 10) | ep0_pcs[slot_id];
                    xhci_enqueue_ep0(status_trb, slot_id);

                    *(volatile uint32_t*)(xhci_bar0 + xhci_dboff + slot_id * 4) = 1; // Doorbell

                    slot_state[slot_id] = XHCI_SLOT_DESC_DONE;
                }
                else if(slot_state[slot_id] == XHCI_SLOT_EP_CONFIGURED){

                    trb_t normal_trb = {0};
                    normal_trb.parameter = TO_PHYS_U64(hid_buf[slot_id]);
                    normal_trb.status    = slot_maxpkt[slot_id];
                    normal_trb.control   = (1 << 10) | (1 << 5) | ep1in_pcs[slot_id];
                    xhci_enqueue_ep1in(normal_trb, slot_id);

                    *(volatile uint32_t*)(xhci_bar0 + xhci_dboff + slot_id * 4) = 3;

                    slot_state[slot_id] = XHCI_SLOT_POLLING;
                }
            }
        }
        else if(type == 34){
            uint32_t completion_code = (evt_ring[evt_dequeue].status >> 24) & 0xFF;

            if(completion_code == 1 || completion_code == 13)
            { 
                uint8_t port     = (evt_ring[evt_dequeue].parameter >> 24) & 0xFF;
                uint32_t psc_off  = 0x400 + (port - 1) * 0x10;
                uint32_t portsc   = *(volatile uint32_t*)(xhci_op + psc_off);

                bool ccs = portsc & (1 << 0);   // device enabled
                bool csc = portsc & (1 << 16);  // connect status changed
                bool prc = portsc & (1 << 20);  // port reset done
                bool ped = portsc & (1 << 1);   // port enabled

                if(csc && ccs){
                    KLOG_INFO("XHCI", "Port %u: device connected, starting reset", port);
                    *(volatile uint32_t*)(xhci_op + psc_off) = (portsc & ~0x00EF0000) | (1<<16) | (1<<4);
                }
                else if(prc && ped){
                    KLOG_INFO("XHCI", "Port %u: reset complete, sending Enable Slot", port);
                    *(volatile uint32_t*)(xhci_op + psc_off) = (portsc & ~0x00EF0000) | (1<<20);

                    trb_t cmd = {0};
                    cmd.control = (9 << 10) | cmd_pcs; // type=9, cycle bit
                    xhci_enqueue_cmd(cmd);
                    pending_ports[pending_tail++] = port;
                }
                else if(ccs && ped){
                    trb_t cmd = {0};
                    cmd.control = (9 << 10) | cmd_pcs; // type=9, cycle bit
                    xhci_enqueue_cmd(cmd);
                    pending_ports[pending_tail++] = port;
                }
            }
        }
        
        evt_dequeue++;
        if(evt_dequeue >= XHCI_EVT_RING_SIZE){
            evt_dequeue = 0;
            evt_ccs ^= 1;
        }
    }

    *(volatile uint64_t*)(xhci_ir0 + 0x18) = TO_PHYS_U64(&evt_ring[evt_dequeue]) | (1 << 3);
    *(volatile uint32_t*)(xhci_ir0 + 0x00) |= 1;
}

int xhci_probe(pci_device_t* device){
    void* cfg = paging_map_mmio(device->phys, 4096);
    uint16_t cmd = pci_read16(device->bus, device->device, device->function, 0x04, cfg);
    cmd |= (1 << 2);   // Bus Master Enable
    pci_write16(device->bus, device->device, device->function, 0x04, cmd, cfg);
    paging_unmap_mmio(cfg, 4096);

    xhci_bar0 = paging_map_mmio((device->bars[0] | ((uint64_t)device->bars[1] << 32)) & ~0xFULL, 0x10000);

    if(xhci_bar0 == NULL)
    {
        KLOG_ERROR("XHCI", "Failed to map BAR 0");
        return -1;
    }

    uint8_t caplength = *(volatile uint8_t*)(xhci_bar0 + 0x00);
    xhci_op   = xhci_bar0 + caplength;

    uint32_t rtsoff    = *(volatile uint32_t*)(xhci_bar0 + 0x18) & ~(uint32_t)0x1F;
    xhci_ir0  = xhci_bar0 + rtsoff + 0x20;

    uint8_t max_slots = *(volatile uint8_t*)(xhci_bar0 + XHCI_CAP_HCSPARAMS1);

    uint8_t max_ports = (*(volatile uint32_t*)(xhci_bar0 + XHCI_CAP_HCSPARAMS1) >> 24) & 0xFF;

    *(volatile uint32_t*)(xhci_op + 0x00) |= (1 << 1);

    while(*(volatile uint32_t*)(xhci_op + 0x04) & (1 << 11)){}

    *(volatile uint32_t*)(xhci_op + 0x38) = max_slots;

    *(volatile uint64_t*)(xhci_op + 0x30) = TO_PHYS_U64(dcbaap);

    cmd_ring[XHCI_CMD_RING_SIZE - 1].parameter = TO_PHYS_U64(&cmd_ring[0]);
    cmd_ring[XHCI_CMD_RING_SIZE - 1].control = (6 << 10) | (1 << 1) | cmd_pcs;

    *(volatile uint64_t*)(xhci_op + 0x18) = TO_PHYS_U64(&cmd_ring[0]) | cmd_pcs;

    erst[0].base_address  = TO_PHYS_U64(evt_ring);
    erst[0].segment_size  = XHCI_EVT_RING_SIZE;

    *(volatile uint32_t*)(xhci_ir0 + 0x08) = 1;
    *(volatile uint64_t*)(xhci_ir0 + 0x10) = TO_PHYS_U64(erst);
    *(volatile uint64_t*)(xhci_ir0 + 0x18) = TO_PHYS_U64(evt_ring);

    uint8_t vectors[1];
    int msix_ret = pci_enable_msix(device, vectors, 1);
    if(msix_ret == -1){
        vectors[0] = alloc_irq_vector();
        pci_enable_msi(device, vectors[0]);
    }

    idt_register_irq_handler(vectors[0], xhci_irq_handler);

    xhci_dboff = *(volatile uint32_t*)(xhci_bar0 + 0x14);

    *(volatile uint32_t*)(xhci_ir0 + 0x00) |= (1 << 1);

    *(volatile uint32_t*)(xhci_op + 0x00) |= (1 << 0) | (1 << 2); 

    for(uint32_t i = 0; i < max_ports; i++){
        uint32_t portsc = *(volatile uint32_t*)(xhci_op + 0x400 + i * 0x10);
        bool ccs = portsc & (1 << 0);
        bool ped = portsc & (1 << 1);
        bool pp = portsc & (1 << 9);
        if(ccs && ped == 0 && pp){
            *(volatile uint32_t*)(xhci_op + 0x400 + i * 0x10) = (portsc & ~0x00EF0000) | (1 << 4);
        }
        else if(ccs && ped && pp){
            trb_t cmd = {0};
            cmd.control = (9 << 10) | cmd_pcs; // type=9, cycle bit
            xhci_enqueue_cmd(cmd);
            pending_ports[pending_tail++] = i + 1;
        }

        portsc = *(volatile uint32_t*)(xhci_op + 0x400 + i * 0x10);
        bool csc = portsc & (1 << 17);
        if(csc){
            *(volatile uint32_t*)(xhci_op + 0x400 + i * 0x10) = (portsc & ~0x00EF0000) | (1 << 17);
        }
    }

    sem_init(&mouse_sem, 0);

    KLOG_INFO("XHCI", "XHCI probe successful");

    return 0;
}

void xhci_remove(pci_device_t* device){
    (void)device;
}