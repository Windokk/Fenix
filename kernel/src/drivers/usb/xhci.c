#include "drivers/usb/xhci.h"

#include "memory/paging_pages.h"

#include "interrupts/idt.h"

#include "interrupts/irq.h"

#include "input/input_subsys.h"

#include "interrupts/lapic_timer.h"

static uint8_t* xhci_bar0;
static uint8_t* xhci_op;
static uint8_t* xhci_ir0;

static trb_t cmd_ring[XHCI_CMD_RING_SIZE] __attribute__((aligned(64)));
static uint8_t cmd_pcs = 1;
static uint32_t cmd_enqueue = 0;

static trb_t evt_ring[XHCI_EVT_RING_SIZE] __attribute__((aligned(64)));
static uint8_t evt_ccs = 1;
static uint32_t evt_dequeue = 0;

static erst_entry_t erst[1] __attribute__((aligned(64)));

static uint64_t dcbaap[XHCI_MAX_SLOTS_PLUS_ONE] __attribute__((aligned(64))) = {0} ;

static uint32_t xhci_dboff;

typedef struct port_desc_t{
    uint8_t port;
    uint8_t parent_hub_slot_id;
    uint8_t speed;
} port_desc_t;

// Slots logic
static uint8_t slot_port_map[XHCI_MAX_SLOTS_PLUS_ONE] = {0};
static port_desc_t pending_ports[XHCI_MAX_SLOTS_PLUS_ONE] = {0};
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

#define XHCI_CFG_BUF_LEN 512
#define XHCI_HUB_DESC_BUF_LEN 512
#define XHCI_HUB_STATUS_BUF_LEN 512

static uint8_t cfg_buf[XHCI_MAX_SLOTS_PLUS_ONE][XHCI_CFG_BUF_LEN] __attribute__((aligned(16)));
static bool slot_is_hub[XHCI_MAX_SLOTS_PLUS_ONE];
static uint16_t slot_cfg_total_len[XHCI_MAX_SLOTS_PLUS_ONE];
static uint16_t slot_maxpkt[XHCI_MAX_SLOTS_PLUS_ONE];
static uint8_t slot_protocol[XHCI_MAX_SLOTS_PLUS_ONE]; // 0 = none/unsupported, 1 = keyboard, 2 = mouse
static uint8_t slot_iface_num[XHCI_MAX_SLOTS_PLUS_ONE]; // bInterfaceNumber of the HID interface we bound to
static uint8_t slot_ep_addr[XHCI_MAX_SLOTS_PLUS_ONE];   // raw bEndpointAddress of the interrupt IN endpoint
static uint8_t slot_ep_dci[XHCI_MAX_SLOTS_PLUS_ONE];    // Device Context Index derived from slot_ep_addr
static uint8_t slot_ep_interval[XHCI_MAX_SLOTS_PLUS_ONE]; // bInterval of that endpoint
static uint8_t str_buf[XHCI_MAX_SLOTS_PLUS_ONE][64];
static char manufacturer_buf[XHCI_MAX_SLOTS_PLUS_ONE][64];
static char product_buf[XHCI_MAX_SLOTS_PLUS_ONE][64];
static char serial_buf[XHCI_MAX_SLOTS_PLUS_ONE][32];
static usb_device_desc_t device_desc[XHCI_MAX_SLOTS_PLUS_ONE];
static uint8_t hub_desc_buf[XHCI_MAX_SLOTS_PLUS_ONE][XHCI_HUB_DESC_BUF_LEN];
static uint8_t hub_status_buf[XHCI_MAX_SLOTS_PLUS_ONE][XHCI_HUB_STATUS_BUF_LEN];
static uint8_t hub_port_status_buf[XHCI_MAX_SLOTS_PLUS_ONE][4];
static uint8_t slot_hub_query_port[XHCI_MAX_SLOTS_PLUS_ONE];

// Sequential CLEAR_PORT_FEATURE queue for the port currently being serviced on this hub slot.
static uint8_t hub_change_queue[XHCI_MAX_SLOTS_PLUS_ONE][XHCI_HUB_MAX_PENDING_CHANGES]; // feature selectors (16-20)
static uint8_t hub_change_count[XHCI_MAX_SLOTS_PLUS_ONE];
static uint8_t hub_change_idx[XHCI_MAX_SLOTS_PLUS_ONE];
static bool hub_port_reset_pending[XHCI_MAX_SLOTS_PLUS_ONE];
static bool hub_port_reset_done[XHCI_MAX_SLOTS_PLUS_ONE];

//Device data
static uint8_t hid_buf[XHCI_MAX_SLOTS_PLUS_ONE][8] __attribute__((aligned(16)));
static bool caps_lock_state[XHCI_MAX_SLOTS_PLUS_ONE];
uint8_t prev_hid_buf[XHCI_MAX_SLOTS_PLUS_ONE][8];
static const unsigned char hid_to_ascii[256] = {
    0, 0, 0, 0,                                                                 // 0x00-0x03
    'a','b','c','d','e','f','g','h','i','j','k','l','m',                        // 0x04-0x10
    'n','o','p','q','r','s','t','u','v','w','x','y','z',                        // 0x11-0x1D
    '1','2','3','4','5','6','7','8','9','0',                                    // 0x1E-0x27
    '\n', 0, '\b', '\t', ' ',                                                   // 0x28-0x2C
    '-','=','[',']','\\',                                                       // 0x2D-0x31
    0,                                                                          // 0x32 non-US hash
    ';','\'','`',',','.','/',                                                   // 0x33-0x38
};

mouse_pending_t mouse_state;
static uint8_t prev_mouse_buttons;
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

    *(volatile uint32_t*)(xhci_bar0 + xhci_dboff) = 0;
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

static void xhci_fetch_config_header(uint32_t slot_id, xhci_slot_state_t next_state){

    if(device_desc[slot_id].product_name && device_desc[slot_id].serial_number)
    {
        KLOG_INFO("XHCI", "Device infos : %s (Serial number : %s)", device_desc[slot_id].product_name, device_desc[slot_id].serial_number);
    }

    trb_t setup_trb = {0};
    setup_trb.parameter = 0x0009000002000680; // GET_DESCRIPTOR(CONFIGURATION, 0), wLength=9
    setup_trb.status = 8;
    setup_trb.control = (2 << 10) | (3 << 16) | (1 << 6) | ep0_pcs[slot_id];
    xhci_enqueue_ep0(setup_trb, slot_id);

    trb_t data_trb = {0};
    data_trb.parameter = TO_PHYS_U64(&cfg_buf[slot_id][0]);
    data_trb.status    = 9; // TRB Transfer Length = wLength
    data_trb.control   = (3 << 10) | (1 << 16) | (1 << 5) | ep0_pcs[slot_id];
    xhci_enqueue_ep0(data_trb, slot_id);

    trb_t status_trb = {0};
    status_trb.parameter = 0;
    status_trb.status    = 0;
    status_trb.control   = (4 << 10) | ep0_pcs[slot_id];
    xhci_enqueue_ep0(status_trb, slot_id);

    *(volatile uint32_t*)(xhci_bar0 + xhci_dboff + slot_id * 4) = 1; // Doorbell

    slot_state[slot_id] = next_state;
}

static void xhci_fetch_config_full(uint32_t slot_id, uint16_t length, xhci_slot_state_t next_state){

    uint64_t param = 0x0000000002000680ULL | ((uint64_t)length << 48); // GET_DESCRIPTOR(CONFIGURATION, 0), wLength=length

    trb_t setup_trb = {0};
    setup_trb.parameter = param;
    setup_trb.status = 8;
    setup_trb.control = (2 << 10) | (3 << 16) | (1 << 6) | ep0_pcs[slot_id];
    xhci_enqueue_ep0(setup_trb, slot_id);

    trb_t data_trb = {0};
    data_trb.parameter = TO_PHYS_U64(&cfg_buf[slot_id][0]);
    data_trb.status    = length; // TRB Transfer Length = wLength
    data_trb.control   = (3 << 10) | (1 << 16) | (1 << 5) | ep0_pcs[slot_id];
    xhci_enqueue_ep0(data_trb, slot_id);

    trb_t status_trb = {0};
    status_trb.parameter = 0;
    status_trb.status    = 0;
    status_trb.control   = (4 << 10) | ep0_pcs[slot_id];
    xhci_enqueue_ep0(status_trb, slot_id);

    *(volatile uint32_t*)(xhci_bar0 + xhci_dboff + slot_id * 4) = 1; // Doorbell

    slot_state[slot_id] = next_state;
}

static void xhci_fetch_config_hub(uint32_t slot_id, uint16_t descriptor_type, uint16_t length){
    uint16_t wValue  = (descriptor_type << 8) | 0x00;
    uint16_t wIndex  = 0x0000;

    uint32_t dword0 = 0xA0 | (0x06 << 8) | ((uint32_t)wValue << 16);
    uint32_t dword1 = wIndex | ((uint32_t)(length == 0 ? 8 : length) << 16);
    uint64_t param  = ((uint64_t)dword1 << 32) | dword0;

    trb_t setup_trb = {0};
    setup_trb.parameter = param;
    setup_trb.status    = 8;
    setup_trb.control    = (2 << 10) | (3 << 16) | (1 << 6) | ep0_pcs[slot_id];
    xhci_enqueue_ep0(setup_trb, slot_id);

    trb_t data_trb = {0};
    data_trb.parameter = TO_PHYS_U64(&hub_desc_buf[slot_id]);
    data_trb.status     = (length == 0 ? 8 : length);
    data_trb.control    = (3 << 10) | (1 << 16)| (1 << 5) | ep0_pcs[slot_id];
    xhci_enqueue_ep0(data_trb, slot_id);

    trb_t status_trb = {0};
    status_trb.parameter = 0;
    status_trb.status    = 0;
    status_trb.control    = (4 << 10) | ep0_pcs[slot_id];
    xhci_enqueue_ep0(status_trb, slot_id);

    *(volatile uint32_t*)(xhci_bar0 + xhci_dboff + slot_id * 4) = 1; // DCI EP0 = 1

    slot_state[slot_id] = (length == 0 ? XHCI_SLOT_WAIT_HUB_DESC : XHCI_SLOT_HUB_DESC_DONE);
}

static void xhci_hub_port_feature(uint32_t slot_id, uint8_t bRequest, uint16_t feature_selector, uint16_t port, xhci_slot_state_t next_state){
    uint32_t dword0 = 0x23 | ((uint32_t)bRequest << 8) | ((uint32_t)feature_selector << 16);
    uint32_t dword1 = port; // wIndex = port, wLength = 0
    uint64_t param  = ((uint64_t)dword1 << 32) | dword0;

    trb_t setup_trb = {0};
    setup_trb.parameter = param;
    setup_trb.status    = 8;
    setup_trb.control    = (2 << 10) | (0 << 16) | (1 << 6) | ep0_pcs[slot_id]; // TRT=0, no data stage
    xhci_enqueue_ep0(setup_trb, slot_id);

    trb_t status_trb = {0};
    status_trb.parameter = 0;
    status_trb.status    = 0;
    status_trb.control    = (4 << 10) | (1 << 16) | (1 << 5) | ep0_pcs[slot_id]; // Status stage is always IN when there's no data stage
    xhci_enqueue_ep0(status_trb, slot_id);

    *(volatile uint32_t*)(xhci_bar0 + xhci_dboff + slot_id * 4) = 1; // DCI EP0 = 1

    slot_state[slot_id] = next_state;
}

static void xhci_hub_ack_next_change(uint32_t slot_id){
    uint8_t port = slot_hub_query_port[slot_id];

    if(hub_change_idx[slot_id] < hub_change_count[slot_id]){
        uint16_t feature = hub_change_queue[slot_id][hub_change_idx[slot_id]];
        xhci_hub_port_feature(slot_id, 0x01, feature, port, XHCI_SLOT_HUB_PORT_CLEAR_SENT);
        return;
    }

    if(hub_port_reset_pending[slot_id]){
        hub_port_reset_pending[slot_id] = false;
        xhci_hub_port_feature(slot_id, 0x03, XHCI_HUB_PORT_FEATURE_RESET, port, XHCI_SLOT_HUB_PORT_RESET_SENT);
        return;
    }

    if(hub_port_reset_done[slot_id]){
        uint16_t wPortStatus = hub_port_status_buf[slot_id][0] | ((uint16_t)hub_port_status_buf[slot_id][1] << 8);

        uint8_t speed;
        if(wPortStatus & (1 << 9)){
            speed = 2;
        }
        else if(wPortStatus & (1 << 10)){
            speed = 3;
        }
        else{
            speed = 1;
        }
        pending_ports[pending_tail].speed = speed;
        pending_ports[pending_tail].port = port;
        pending_ports[pending_tail].parent_hub_slot_id = slot_id;
        pending_tail++;

        trb_t cmd = {0};
        cmd.control = (9 << 10) | cmd_pcs; // type=9, cycle bit
        xhci_enqueue_cmd(cmd);
    }

    slot_state[slot_id] = XHCI_SLOT_HUB_POLLING;
}

static bool xhci_parse_config(uint32_t slot_id, uint16_t total_len, uint8_t iface_class_target, uint8_t iface_protocol_targets[4], bool accept_any_protocol){
    uint16_t offset = 0;

    bool in_target_iface = false;
    bool found_iface = false;
    bool found_ep = false;

    uint8_t iface_num = 0;
    uint8_t iface_protocol = 0;

    while(offset + 2 <= total_len){
        uint8_t b_length = cfg_buf[slot_id][offset];
        uint8_t b_type   = cfg_buf[slot_id][offset + 1];

        if(b_length < 2 || offset + b_length > total_len){
            break; // malformed descriptor, stop parsing defensively
        }

        if(b_type == 0x04 && b_length >= 9){ // Interface descriptor
            iface_num      = cfg_buf[slot_id][offset + 2];
            uint8_t iface_class = cfg_buf[slot_id][offset + 5];
            iface_protocol = cfg_buf[slot_id][offset + 7];

            in_target_iface = false;

            if(!found_iface && iface_class == iface_class_target && (accept_any_protocol ? 1 : (iface_protocol == iface_protocol_targets[0] || iface_protocol == iface_protocol_targets[1]  || iface_protocol == iface_protocol_targets[2]  || iface_protocol == iface_protocol_targets[3]))){
                found_iface = true;
                in_target_iface = true;
            }
        }
        else if(b_type == 0x05 && b_length >= 7 && in_target_iface && !found_ep){ // Endpoint descriptor
            uint8_t ep_addr = cfg_buf[slot_id][offset + 2];
            uint8_t ep_attr = cfg_buf[slot_id][offset + 3];
            bool is_in        = (ep_addr & 0x80) != 0;
            bool is_interrupt = (ep_attr & 0x03) == 3;

            if(is_in && is_interrupt){
                uint16_t maxpkt = cfg_buf[slot_id][offset + 4] | ((uint16_t)cfg_buf[slot_id][offset + 5] << 8);
                uint8_t epnum = ep_addr & 0x0F;

                slot_iface_num[slot_id]   = iface_num;
                slot_protocol[slot_id]    = iface_protocol;
                slot_ep_addr[slot_id]     = ep_addr;
                slot_ep_interval[slot_id] = cfg_buf[slot_id][offset + 6];
                slot_ep_dci[slot_id]      = (uint8_t)(2 * epnum + (is_in ? 1 : 0));

                if(maxpkt > sizeof(hid_buf[slot_id])){
                    KLOG_WARN("XHCI", "HID report size %u exceeds buffer, clamping to %u",
                              (unsigned)maxpkt, (unsigned)sizeof(hid_buf[slot_id]));
                    maxpkt = sizeof(hid_buf[slot_id]);
                }
                slot_maxpkt[slot_id] = maxpkt;

                found_ep = true;
            }
        }

        offset += b_length;
    }

    return found_ep;
}

static void xhci_send_set_protocol(uint32_t slot_id, uint8_t protocol_value){
    uint64_t param = 0x0000000000000B21ULL
                    | ((uint64_t)protocol_value << 16)
                    | ((uint64_t)slot_iface_num[slot_id] << 32);

    trb_t setup_trb = {0};
    setup_trb.parameter = param;
    setup_trb.status    = 8;
    setup_trb.control   = (2 << 10) | (0 << 16) | (1 << 6) | ep0_pcs[slot_id];
    xhci_enqueue_ep0(setup_trb, slot_id);

    trb_t status_trb = {0};
    status_trb.parameter = 0;
    status_trb.status    = 0;
    status_trb.control   = (4 << 10) | (1 << 16) | (1 << 5) | ep0_pcs[slot_id];
    xhci_enqueue_ep0(status_trb, slot_id);

    *(volatile uint32_t*)(xhci_bar0 + xhci_dboff + slot_id * 4) = 1; // Doorbell
}

static void xhci_send_set_idle(uint32_t slot_id){
    uint64_t param = 0x0000000000000A21ULL
                    | ((uint64_t)slot_iface_num[slot_id] << 32);

    trb_t setup_trb = {0};
    setup_trb.parameter = param;
    setup_trb.status    = 8;
    setup_trb.control   = (2 << 10) | (0 << 16) | (1 << 6) | ep0_pcs[slot_id];
    xhci_enqueue_ep0(setup_trb, slot_id);

    trb_t status_trb = {0};
    status_trb.parameter = 0;
    status_trb.status    = 0;
    status_trb.control   = (4 << 10) | (1 << 16) | (1 << 5) | ep0_pcs[slot_id];
    xhci_enqueue_ep0(status_trb, slot_id);

    *(volatile uint32_t*)(xhci_bar0 + xhci_dboff + slot_id * 4) = 1; // Doorbell
}

static void xhci_legacy_handoff(uint8_t* bar0, uint64_t mapped_size){
    uint32_t hccparams1 = *(volatile uint32_t*)(bar0 + XHCI_CAP_HCCPARAMS1);
    uint32_t xecp = (hccparams1 >> 16) & 0xFFFF;

    if(xecp == 0){
        return; // No Extended Capabilities list at all
    }

    uint32_t ec_offset = xecp * 4;
    uint32_t usblegsup_offset = 0;

    // Walk the linked list of Extended Capabilities looking for ID 0x01
    while(1){
        // Never read outside of what is mapped. We need 8 bytes because
        // USBLEGCTLSTS sits right after USBLEGSUP.
        if((uint64_t)ec_offset + 8 > mapped_size){
            KLOG_WARN("XHCI", "Extended Capabilities list exceeds mapped BAR, skipping handoff");
            return;
        }

        uint32_t ec_dword = *(volatile uint32_t*)(bar0 + ec_offset);
        uint8_t cap_id = ec_dword & 0xFF;
        uint8_t next_ptr = (ec_dword >> 8) & 0xFF;

        if(cap_id == 0x01){
            usblegsup_offset = ec_offset;
            break;
        }

        if(next_ptr == 0){
            break; // End of list, capability not found
        }

        ec_offset += next_ptr * 4;
    }

    if(usblegsup_offset == 0){
        return; // Firmware does not implement legacy support, nothing to do
    }

    KLOG_INFO("XHCI", "USB Legacy Support Capability found, requesting BIOS handoff");

    // USBLEGSUP: request OS ownership without touching the rest of the register
    uint32_t usblegsup = *(volatile uint32_t*)(bar0 + usblegsup_offset);
    usblegsup |= (1 << 24); // HC OS Owned Semaphore
    *(volatile uint32_t*)(bar0 + usblegsup_offset) = usblegsup;

    // Poll for the BIOS to release ownership, with a bounded timeout
    bool handed_off = false;
    for(uint32_t i = 0; i < XHCI_LEGACY_HANDOFF_TIMEOUT_ITER; i++){
        usblegsup = *(volatile uint32_t*)(bar0 + usblegsup_offset);
        if((usblegsup & (1 << 16)) == 0){
            handed_off = true;
            break;
        }
        wait_ms(100); // ~1ms per iteration
    }

    if(!handed_off){
        KLOG_WARN("XHCI", "BIOS did not release USB ownership in time, forcing handoff");
        usblegsup &= ~(1u << 16);
        usblegsup |= (1 << 24);
        *(volatile uint32_t*)(bar0 + usblegsup_offset) = usblegsup;
    }
    else{
        KLOG_INFO("XHCI", "BIOS to OS handoff complete");
    }

    // USBLEGCTLSTS sits right after USBLEGSUP. Disable all SMI generation
    // sources and acknowledge (RW1C) any pending SMI status bits so the
    // firmware stops interfering with the controller.
    uint32_t usblegctlsts_offset = usblegsup_offset + 4;
    uint32_t usblegctlsts = *(volatile uint32_t*)(bar0 + usblegctlsts_offset);

    usblegctlsts &= ~XHCI_LEGCTLSTS_SMI_ENABLE_MASK; // disable SMI enables
    usblegctlsts |= XHCI_LEGCTLSTS_SMI_STATUS_MASK;  // ack pending SMI status (RW1C)

    *(volatile uint32_t*)(bar0 + usblegctlsts_offset) = usblegctlsts;
}

static uint32_t xhci_compute_ep_interval(uint8_t speed, uint8_t interval, uint32_t slot_id){
    uint32_t ret;
    if(speed == 1){
        ret = (uint32_t)(max(min((31 - __builtin_clz(interval * 8)), 10), 3) << 16);
    }
    else if(speed == 2){
        ret = (uint32_t)(max(min((31 - __builtin_clz(interval * 256)), 10), 3) << 16);
    }
    else if(speed == 3 || speed == 4){
        ret = (uint32_t)((interval - 1) << 16);
    }
    else{
        // 0 OR >5
        ret = (uint32_t)(100 << 16);
        KLOG_WARN("XHCI", "Interval was 0 or more than 4 when computing ep interval for slot id : %llu", slot_id);
    }
    return ret;
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
                    
                    if(desc_buf[slot_id][4] == 0x09){
                        slot_is_hub[slot_id] = true;
                        KLOG_INFO("XHCI", "Found USB Hub at slot : %u", slot_id);
                        xhci_fetch_config_hub(slot_id, ((uint16_t)desc_buf[slot_id][2] | (desc_buf[slot_id][3] << 8)) >= 0x0300 ? 0x2A : 0x29, 0); // 0 means first pass
                    }
                    else{
                        if(!xhci_fetch_str(slot_id, desc_buf[slot_id][14], XHCI_SLOT_STR_MANUFACTURER)){ 
                            device_desc[slot_id].manufacturer_name = "";
                            if(!xhci_fetch_str(slot_id, desc_buf[slot_id][15], XHCI_SLOT_STR_PRODUCT)){
                                device_desc[slot_id].product_name = "";
                                if(!xhci_fetch_str(slot_id, desc_buf[slot_id][16], XHCI_SLOT_STR_SERIAL)){
                                    device_desc[slot_id].serial_number = "";
                                    xhci_fetch_config_header(slot_id, XHCI_SLOT_CFG_HDR_DONE);
                                }
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
                            xhci_fetch_config_header(slot_id, XHCI_SLOT_CFG_HDR_DONE);
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
                        xhci_fetch_config_header(slot_id, XHCI_SLOT_CFG_HDR_DONE);
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

                    xhci_fetch_config_header(slot_id, XHCI_SLOT_CFG_HDR_DONE);
                
                }
                else if(slot_state[slot_id] == XHCI_SLOT_CFG_HDR_DONE){
                    // Bytes 2-3 of the Configuration descriptor = wTotalLength (LE)
                    uint16_t total_len = cfg_buf[slot_id][2] | ((uint16_t)cfg_buf[slot_id][3] << 8);

                    if(total_len > XHCI_CFG_BUF_LEN){
                        KLOG_WARN("XHCI", "Config descriptor is %u bytes, clamping to %u",
                                  (unsigned)total_len, (unsigned)XHCI_CFG_BUF_LEN);
                        total_len = XHCI_CFG_BUF_LEN;
                    }

                    slot_cfg_total_len[slot_id] = total_len;
                    xhci_fetch_config_full(slot_id, total_len, XHCI_SLOT_CFG_DONE);
                }
                else if(slot_state[slot_id] == XHCI_SLOT_CFG_DONE){
                    uint8_t target_protocols[4] = {1, 2};
                    bool hid_found = xhci_parse_config(slot_id, slot_cfg_total_len[slot_id], 0x03, target_protocols, false);

                    if(!hid_found){
                        KLOG_WARN("XHCI", "No boot-compatible HID interrupt IN endpoint found for this device, skipping HID setup");
                        slot_protocol[slot_id] = 0;
                    }

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
                    if(slot_protocol[slot_id] == 0){
                        // No usable HID interface/endpoint: leave the device
                        // configured but idle instead of programming garbage
                        // into an endpoint context.
                        slot_state[slot_id] = XHCI_SLOT_IDLE_UNSUPPORTED;
                    }
                    else{
                        xhci_send_set_protocol(slot_id, 0); // 0 = Boot Protocol
                        slot_state[slot_id] = XHCI_SLOT_SETPROTO_DONE;
                    }
                }
                else if(slot_state[slot_id] == XHCI_SLOT_SETPROTO_DONE){
                    xhci_send_set_idle(slot_id);
                    slot_state[slot_id] = XHCI_SLOT_SETIDLE_DONE;
                }
                else if(slot_state[slot_id] == XHCI_SLOT_SETIDLE_DONE){
                    uint8_t dci = slot_ep_dci[slot_id];
                    uint32_t ep_base = 8 + dci * 8; // Input Control(8) + Slot Context(8) + dci * EndpointContext(8)

                    ep1in_ring[slot_id][XHCI_EP1IN_RING_SIZE - 1].parameter = TO_PHYS_U64(&ep1in_ring[slot_id][0]);
                    ep1in_ring[slot_id][XHCI_EP1IN_RING_SIZE - 1].control   = (6 << 10) | (1 << 1) | 1;
                    ep1in_pcs[slot_id] = 1;

                    input_ctx[slot_id][1] = (1u << 0) | (1u << dci); // A0 (slot) + Adci (this endpoint)

                    uint8_t speed = (input_ctx[slot_id][8] >> 20) & 0xF;

                    input_ctx[slot_id][8] = (3 << 27) | (speed << 20);

                    input_ctx[slot_id][ep_base + 0] = xhci_compute_ep_interval(speed, slot_ep_interval[slot_id], slot_id);
                    input_ctx[slot_id][ep_base + 1] = ((uint32_t)slot_maxpkt[slot_id] << 16) | (7 << 3) | (3 << 1);
                    input_ctx[slot_id][ep_base + 2] = (TO_PHYS_U64(&ep1in_ring[slot_id][0]) & 0xFFFFFFFF) | 1;
                    input_ctx[slot_id][ep_base + 3] = TO_PHYS_U64(&ep1in_ring[slot_id][0]) >> 32;
                    input_ctx[slot_id][ep_base + 4] = slot_maxpkt[slot_id];

                    trb_t cmd = {0};
                    cmd.parameter = TO_PHYS_U64(input_ctx[slot_id]);
                    cmd.status    = 0;
                    cmd.control   = (12 << 10) | (slot_id << 24) | cmd_pcs;
                    xhci_enqueue_cmd(cmd);

                    slot_state[slot_id] = XHCI_SLOT_EP_CONFIGURED;
                }
                else if(slot_state[slot_id] == XHCI_SLOT_WAIT_HUB_DESC){
                    xhci_fetch_config_hub(slot_id, ((uint16_t)desc_buf[slot_id][2] | (desc_buf[slot_id][3] << 8)) >= 0x0300 ? 0x2A : 0x29, hub_desc_buf[slot_id][0]); // now we pass the real desc length
                }
                else if(slot_state[slot_id] == XHCI_SLOT_HUB_DESC_DONE){
                    input_ctx[slot_id][1] = 1;
                    bool mtt = (desc_buf[slot_id][7] == 0x02);
                    input_ctx[slot_id][8] |= (1 << 26) | (mtt << 25);
                    input_ctx[slot_id][9] |= ((uint32_t)hub_desc_buf[slot_id][2] << 24);

                    trb_t cmd = {0};
                    cmd.parameter = TO_PHYS_U64(input_ctx[slot_id]);
                    cmd.status    = 0;
                    cmd.control   = (13 << 10) | (slot_id << 24) | cmd_pcs;
                    xhci_enqueue_cmd(cmd);

                    slot_state[slot_id] = XHCI_SLOT_HUB_EVALCTX_SENT;
                }
                else if(slot_state[slot_id] == XHCI_SLOT_HUB_CFG_HDR_DONE){
                    // Bytes 2-3 of the Configuration descriptor = wTotalLength (LE)
                    uint16_t total_len = cfg_buf[slot_id][2] | ((uint16_t)cfg_buf[slot_id][3] << 8);

                    if(total_len > XHCI_CFG_BUF_LEN){
                        KLOG_WARN("XHCI", "Config descriptor is %u bytes, clamping to %u",
                                  (unsigned)total_len, (unsigned)XHCI_CFG_BUF_LEN);
                        total_len = XHCI_CFG_BUF_LEN;
                    }

                    slot_cfg_total_len[slot_id] = total_len;
                    xhci_fetch_config_full(slot_id, total_len, XHCI_SLOT_HUB_CFG_DONE);
                }
                else if(slot_state[slot_id] == XHCI_SLOT_HUB_CFG_DONE){
                    uint8_t target_protocols[4] = {0, 0, 0, 0};
                    bool hub_found = xhci_parse_config(slot_id, slot_cfg_total_len[slot_id], 0x09, target_protocols, true);

                    if(hub_found){
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

                        slot_state[slot_id] = XHCI_SLOT_HUB_SET_CFG_DONE;
                    }
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
                        uint8_t mouse_buttons = hid_buf[slot_id][0] & 0x07;
                        int8_t mouse_delta_x = (int8_t)hid_buf[slot_id][1];
                        int8_t mouse_delta_y = (int8_t)hid_buf[slot_id][2];
                        int8_t mouse_delta_wheel = (int8_t)hid_buf[slot_id][3];

                        __atomic_fetch_add(&mouse_state.pending_dx, mouse_delta_x, __ATOMIC_SEQ_CST);
                        __atomic_fetch_add(&mouse_state.pending_dy, mouse_delta_y, __ATOMIC_SEQ_CST);
                        __atomic_fetch_add(&mouse_state.pending_wheel, mouse_delta_wheel, __ATOMIC_SEQ_CST);
                        
                        uint8_t changed = mouse_buttons ^ prev_mouse_buttons;
                        uint8_t pressed = mouse_buttons & changed;
                        uint8_t released = ~mouse_buttons & changed;

                        prev_mouse_buttons = mouse_buttons;

                        __atomic_fetch_or(&mouse_state.buttons_pressed, pressed, __ATOMIC_SEQ_CST);
                        __atomic_fetch_or(&mouse_state.buttons_released, released, __ATOMIC_SEQ_CST);

                        sem_signal(&mouse_sem);
                    }

                    trb_t normal_trb = {0};
                    normal_trb.parameter = TO_PHYS_U64(hid_buf[slot_id]);
                    normal_trb.status    = slot_maxpkt[slot_id];
                    normal_trb.control   = (1 << 10) | (1 << 5) | ep1in_pcs[slot_id];
                    xhci_enqueue_ep1in(normal_trb, slot_id);
                    *(volatile uint32_t*)(xhci_bar0 + xhci_dboff + slot_id * 4) = slot_ep_dci[slot_id];
                }
                else if(slot_state[slot_id] == XHCI_SLOT_HUB_SET_CFG_DONE){
                    uint8_t dci = slot_ep_dci[slot_id];
                    uint32_t ep_base = 8 + dci * 8;

                    ep1in_ring[slot_id][XHCI_EP1IN_RING_SIZE - 1].parameter = TO_PHYS_U64(&ep1in_ring[slot_id][0]);
                    ep1in_ring[slot_id][XHCI_EP1IN_RING_SIZE - 1].control   = (6 << 10) | (1 << 1) | 1;
                    ep1in_pcs[slot_id] = 1;

                    input_ctx[slot_id][1] = (1u << 0) | (1u << dci); // A0 (slot) + Adci (this endpoint)

                    uint32_t psc_off  = 0x400 + (slot_port_map[slot_id] - 1) * 0x10;
                    uint32_t portsc   = *(volatile uint32_t*)(xhci_op + psc_off);
                    uint8_t speed = (portsc >> 10) & 0xF;

                    bool mtt = (desc_buf[slot_id][7] == 0x02);

                    input_ctx[slot_id][8] = (3 << 27) | (speed << 20) | (1 << 26) | (mtt << 25);

                    input_ctx[slot_id][ep_base + 0] = xhci_compute_ep_interval(speed, slot_ep_interval[slot_id], slot_id);
                    input_ctx[slot_id][ep_base + 1] = ((uint32_t)slot_maxpkt[slot_id] << 16) | (7 << 3) | (3 << 1);
                    input_ctx[slot_id][ep_base + 2] = (TO_PHYS_U64(&ep1in_ring[slot_id][0]) & 0xFFFFFFFF) | 1;
                    input_ctx[slot_id][ep_base + 3] = TO_PHYS_U64(&ep1in_ring[slot_id][0]) >> 32;
                    input_ctx[slot_id][ep_base + 4] = slot_maxpkt[slot_id];

                    trb_t cmd = {0};
                    cmd.parameter = TO_PHYS_U64(input_ctx[slot_id]);
                    cmd.status    = 0;
                    cmd.control   = (12 << 10) | (slot_id << 24) | cmd_pcs;
                    xhci_enqueue_cmd(cmd);

                    slot_state[slot_id] = XHCI_SLOT_HUB_EP_CONFIGURED;
                }
                else if(slot_state[slot_id] == XHCI_SLOT_HUB_POLLING){
                    uint8_t nbr_ports = hub_desc_buf[slot_id][2];

                    for(uint8_t bit = 1; bit <= nbr_ports; bit++){
                        bool bit_up = hub_status_buf[slot_id][bit/8] & (1 << (bit%8));

                        if(bit_up){  
                            uint16_t wIndex  = bit;

                            uint32_t dword0 = 0xA3 | (0x00 << 8) | ((uint32_t)0x0000 << 16);
                            uint32_t dword1 = wIndex | (4 << 16); // index | length (length = 4)
                            uint64_t param  = ((uint64_t)dword1 << 32) | dword0;

                            trb_t setup_trb = {0};
                            setup_trb.parameter = param;
                            setup_trb.status    = 8;
                            setup_trb.control    = (2 << 10) | (3 << 16) | (1 << 6) | ep0_pcs[slot_id];
                            xhci_enqueue_ep0(setup_trb, slot_id);

                            trb_t data_trb = {0};
                            data_trb.parameter = TO_PHYS_U64(&hub_port_status_buf[slot_id]);
                            data_trb.status     = 4;
                            data_trb.control    = (3 << 10) | (1 << 16)| (1 << 5) | ep0_pcs[slot_id];
                            xhci_enqueue_ep0(data_trb, slot_id);

                            trb_t status_trb = {0};
                            status_trb.parameter = 0;
                            status_trb.status    = 0;
                            status_trb.control    = (4 << 10) | ep0_pcs[slot_id];
                            xhci_enqueue_ep0(status_trb, slot_id);

                            *(volatile uint32_t*)(xhci_bar0 + xhci_dboff + slot_id * 4) = 1; // DCI EP0 = 1

                            slot_hub_query_port[slot_id] = bit;

                            slot_state[slot_id] = XHCI_SLOT_HUB_PORT_STATUS_WAIT;

                            break;
                        }
                        
                    }

                    trb_t normal_trb;
                    normal_trb.parameter = TO_PHYS_U64(hub_status_buf[slot_id]);
                    normal_trb.status    = slot_maxpkt[slot_id];
                    normal_trb.control    = (1 << 10) | (1 << 5) | ep1in_pcs[slot_id];
                    xhci_enqueue_ep1in(normal_trb, slot_id);
                    *(volatile uint32_t*)(xhci_bar0 + xhci_dboff + slot_id * 4) = slot_ep_dci[slot_id];
                }
                else if(slot_state[slot_id] == XHCI_SLOT_HUB_PORT_STATUS_WAIT){
                    uint16_t wPortStatus = hub_port_status_buf[slot_id][0] | ((uint16_t)hub_port_status_buf[slot_id][1] << 8);
                    uint16_t wPortChange = hub_port_status_buf[slot_id][2] | ((uint16_t)hub_port_status_buf[slot_id][3] << 8);
                    uint8_t port_id = slot_hub_query_port[slot_id];

                    bool connected = (wPortStatus & (1 << XHCI_HUB_C_PORT_CONNECTION)) != 0;

                    static const struct { uint8_t change_bit; uint16_t feature; } change_map[XHCI_HUB_MAX_PENDING_CHANGES] = {
                        { XHCI_HUB_C_PORT_CONNECTION,   XHCI_HUB_PORT_FEATURE_C_CONNECTION },
                        { XHCI_HUB_C_PORT_ENABLE,       XHCI_HUB_PORT_FEATURE_C_ENABLE },
                        { XHCI_HUB_C_PORT_SUSPEND,      XHCI_HUB_PORT_FEATURE_C_SUSPEND },
                        { XHCI_HUB_C_PORT_OVER_CURRENT, XHCI_HUB_PORT_FEATURE_C_OVER_CURRENT },
                        { XHCI_HUB_C_PORT_RESET,        XHCI_HUB_PORT_FEATURE_C_RESET },
                    };

                    hub_change_count[slot_id] = 0;
                    hub_change_idx[slot_id]   = 0;
                    hub_port_reset_pending[slot_id] = false;
                    hub_port_reset_done[slot_id]    = false;

                    for(uint8_t i = 0; i < XHCI_HUB_MAX_PENDING_CHANGES; i++){
                        if(wPortChange & (1 << change_map[i].change_bit)){
                            hub_change_queue[slot_id][hub_change_count[slot_id]++] = change_map[i].feature;
                        }
                    }

                    if(wPortChange & (1 << XHCI_HUB_C_PORT_CONNECTION)){
                        if(connected){
                            hub_port_reset_pending[slot_id] = true;
                        }
                        else{
                            // Device went away.
                            KLOG_INFO("XHCI", "Hub slot %u port %u: device disconnected", slot_id, port_id);
                        }
                    }

                    if(wPortChange & (1 << XHCI_HUB_C_PORT_RESET)){
                        hub_port_reset_done[slot_id] = true;
                    }

                    xhci_hub_ack_next_change(slot_id);
                }
                else if(slot_state[slot_id] == XHCI_SLOT_HUB_PORT_CLEAR_SENT){
                    hub_change_idx[slot_id]++;
                    xhci_hub_ack_next_change(slot_id);
                }
                else if(slot_state[slot_id] == XHCI_SLOT_HUB_PORT_RESET_SENT){
                    xhci_hub_ack_next_change(slot_id);
                }
            }
            else{
                KLOG_ERROR("XHCI", "Transfer event : completion code : %llu", completion_code);
            }
        }
        else if(type == 33){
            uint32_t slot_id = (evt_ring[evt_dequeue].control >> 24) & 0xFF;
            uint32_t completion_code = (evt_ring[evt_dequeue].status >> 24) & 0xFF;
            if(completion_code == 1 || completion_code == 13) 
            {
                port_desc_t pending_port = pending_ports[pending_head];

                if(slot_port_map[slot_id] == 0){
                    slot_port_map[slot_id] = pending_port.port;
                    pending_head++;
                }
                
                if(slot_state[slot_id] == XHCI_SLOT_ENABLE_SENT){
                    uint8_t parent_hub_slot = pending_port.parent_hub_slot_id;

                    input_ctx[slot_id][0] = 0;
                    input_ctx[slot_id][1] = 0b11;

                    input_ctx[slot_id][2] = 0;
                    input_ctx[slot_id][3] = 0;
                    input_ctx[slot_id][4] = 0;
                    input_ctx[slot_id][5] = 0;
                    input_ctx[slot_id][6] = 0;
                    input_ctx[slot_id][7] = 0;

                    if(parent_hub_slot != 0){
                        input_ctx[slot_id][8] |= (1 << 27) | (slot_port_map[slot_id] << 0x0) | (pending_port.speed << 0x14);
                        input_ctx[slot_id][9] |= (slot_port_map[parent_hub_slot] << 0x10);
                        input_ctx[slot_id][10] |= (parent_hub_slot << 0x0) | (slot_port_map[slot_id] << 0x08);
                    }
                    else{
                        uint32_t psc_off  = 0x400 + (slot_port_map[slot_id] - 1) * 0x10;
                        uint32_t portsc   = *(volatile uint32_t*)(xhci_op + psc_off);

                        uint8_t speed = (portsc >> 10) & 0xF;

                        input_ctx[slot_id][8] = (1 << 27) | (speed << 20);
                        input_ctx[slot_id][9] = (slot_port_map[slot_id] << 16);

                        input_ctx[slot_id][10] = 0;
                    }

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
                    cmd.parameter = TO_PHYS_U64(input_ctx[slot_id]);
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

                    *(volatile uint32_t*)(xhci_bar0 + xhci_dboff + slot_id * 4) = slot_ep_dci[slot_id];

                    slot_state[slot_id] = XHCI_SLOT_POLLING;
                }
                else if(slot_state[slot_id] == XHCI_SLOT_HUB_EP_CONFIGURED){
                    
                    trb_t normal_trb = {0};
                    normal_trb.parameter = TO_PHYS_U64(hub_status_buf[slot_id]);
                    normal_trb.status    = slot_maxpkt[slot_id];
                    normal_trb.control   = (1 << 10) | (1 << 5) | ep1in_pcs[slot_id];
                    xhci_enqueue_ep1in(normal_trb, slot_id);

                    *(volatile uint32_t*)(xhci_bar0 + xhci_dboff + slot_id * 4) = slot_ep_dci[slot_id];

                    slot_state[slot_id] = XHCI_SLOT_HUB_POLLING;
                }
                else if(slot_state[slot_id] == XHCI_SLOT_HUB_EVALCTX_SENT){
                    xhci_fetch_config_header(slot_id, XHCI_SLOT_HUB_CFG_HDR_DONE);
                }
            }
            else{
                KLOG_ERROR("XHCI", "Command completion event : completion code : %llu", completion_code);
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
                    pending_ports[pending_tail].parent_hub_slot_id = 0;
                    pending_ports[pending_tail].port = port;
                    pending_tail++;
                }
                else if(ccs && ped){
                    trb_t cmd = {0};
                    cmd.control = (9 << 10) | cmd_pcs; // type=9, cycle bit
                    xhci_enqueue_cmd(cmd);
                    pending_ports[pending_tail].parent_hub_slot_id = 0;
                    pending_ports[pending_tail].port = port;
                    pending_tail++;
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

    if(!cfg){
        KLOG_ERROR("XHCI", "Failed to map configuration space");
        return -1;
    }

    uint16_t cmd = pci_read16(device->bus, device->device, device->function, 0x04, cfg);
    cmd |= (1 << 2);   // Bus Master Enable
    pci_write16(device->bus, device->device, device->function, 0x04, cmd, cfg);
    paging_unmap_mmio(cfg, 4096);

    uint64_t bar0_addr;
    
    if((device->bars[0] & 0x6) == 0x4){
        bar0_addr = (device->bars[0] | ((uint64_t)device->bars[1] << 32)) & ~0xFULL;
    }
    else{
        bar0_addr = device->bars[0] & ~0xFULL;
    }

    xhci_bar0 = paging_map_mmio(bar0_addr, 0x1000);

    if(xhci_bar0 == NULL)
    {
        KLOG_ERROR("XHCI", "Failed to map BAR 0 with size 4Ko");
        return -1;
    }

    // The first 4 KiB mapping is only used to read the Capability Registers,
    // which are all located in the first few bytes of the BAR.
    uint8_t  caplength  = *(volatile uint8_t*)(xhci_bar0 + 0x00);
    uint32_t hccparams1 = *(volatile uint32_t*)(xhci_bar0 + XHCI_CAP_HCCPARAMS1);
    uint32_t rtsoff     = *(volatile uint32_t*)(xhci_bar0 + 0x18) & ~(uint32_t)0x1F;
    uint32_t dboff      = *(volatile uint32_t*)(xhci_bar0 + 0x14) & ~(uint32_t)0x3;
    uint8_t  max_slots  = *(volatile uint8_t*)(xhci_bar0 + XHCI_CAP_HCSPARAMS1);
    uint8_t  max_ports  = (*(volatile uint32_t*)(xhci_bar0 + XHCI_CAP_HCSPARAMS1) >> 24) & 0xFF;
    
    uint64_t xecp_offset = (uint64_t)((hccparams1 >> 16) & 0xFFFF) * 4;

    paging_unmap_mmio(xhci_bar0, 0x1000);

    uint64_t op_size = caplength + 0x400 + max_ports * 0x10;
    uint64_t rt_size = rtsoff + 0x20;
    uint64_t db_size = dboff + 0x4;
    uint64_t map_size = op_size;
    if (rt_size > map_size) map_size = rt_size;
    if (db_size > map_size) map_size = db_size;

    // The Extended Capabilities list may live beyond the operational/runtime/
    // doorbell registers. Make sure its start (plus a generous window) is mapped.
    if (xecp_offset != 0 && xecp_offset + 0x1000 > map_size){
        map_size = xecp_offset + 0x1000;
    }

    xhci_bar0 = paging_map_mmio(bar0_addr, map_size);

    if(!xhci_bar0){
        KLOG_ERROR("XHCI", "Failed to map BAR 0 with correct size");
        return -1;
    }

    // Must happen before HCRST and before we touch any operational register:
    // ask the firmware to give up ownership of the controller if it holds it.
    xhci_legacy_handoff(xhci_bar0, map_size);

    xhci_op   = xhci_bar0 + caplength;
    xhci_ir0  = xhci_bar0 + rtsoff + 0x20;

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
            pending_ports[pending_tail++].port = i + 1;
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