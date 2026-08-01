#pragma once

#include "common.h"

#include "tasks/task.h"

#include "drivers/pci.h"

extern pci_driver_t xhci_driver;

int xhci_probe(pci_device_t* device);

void xhci_remove(pci_device_t* device);

#define XHCI_CAP_CAPLENGTH  0x00
#define XHCI_CAP_HCSPARAMS1 0x04
#define XHCI_CAP_HCCPARAMS1 0x10
#define XHCI_CAP_DBOFF      0x14
#define XHCI_CAP_RTSOFF     0x18

#define XHCI_OP_USBCMD	0x00
#define XHCI_OP_USBSTS	0x04
#define XHCI_OP_CRCR	0x18
#define XHCI_OP_DCBAAP	0x30
#define XHCI_OP_CONFIG	0x38

#define XHCI_IR_IMAN	0x00
#define XHCI_IR_ERSTSZ	0x08
#define XHCI_IR_ERSTBA	0x10
#define XHCI_IR_ERDP	0x18

#define XHCI_CMD_RING_SIZE      256
#define XHCI_EVT_RING_SIZE      256
#define XHCI_MAX_SLOTS          255
#define XHCI_MAX_SLOTS_PLUS_ONE 256

#define XHCI_EP0_RING_SIZE 16
#define XHCI_EP1IN_RING_SIZE 16

#define XHCI_LEGACY_HANDOFF_TIMEOUT_ITER 500   // 500ms
#define XHCI_LEGCTLSTS_SMI_ENABLE_MASK  0x000E1FEE
#define XHCI_LEGCTLSTS_SMI_STATUS_MASK  0x7000

#define XHCI_HUB_C_PORT_CONNECTION 0x0
#define XHCI_HUB_C_PORT_ENABLE 0x1
#define XHCI_HUB_C_PORT_SUSPEND 0x2
#define XHCI_HUB_C_PORT_OVER_CURRENT 0x3
#define XHCI_HUB_C_PORT_RESET 0x4

// USB Hub Class port feature selectors (USB 2.0 spec, table 11-17)
// "state" selector, used with SET_PORT_FEATURE to kick off a reset
#define XHCI_HUB_PORT_FEATURE_RESET          4
// "change" selectors, used with CLEAR_PORT_FEATURE to acknowledge a wPortChange bit
#define XHCI_HUB_PORT_FEATURE_C_CONNECTION   16
#define XHCI_HUB_PORT_FEATURE_C_ENABLE       17
#define XHCI_HUB_PORT_FEATURE_C_SUSPEND      18
#define XHCI_HUB_PORT_FEATURE_C_OVER_CURRENT 19
#define XHCI_HUB_PORT_FEATURE_C_RESET        20

// Max number of wPortChange bits we can queue up for sequential acking on one port
#define XHCI_HUB_MAX_PENDING_CHANGES 5

typedef struct trb_t{
    uint64_t parameter;
    uint32_t status;
    uint32_t control;
} trb_t;

typedef struct erst_entry_t{
    uint64_t base_address;
    uint32_t segment_size;
    uint32_t reserved;
} erst_entry_t;

typedef enum {
    XHCI_SLOT_ENABLE_SENT,  // Enable Slot done, Address Device in progress
    XHCI_SLOT_ADDRESS_SENT,  // Address Device done, Get Device Descriptor in progress
    XHCI_SLOT_DESC_DONE,  // Device Descriptor received, Get Config Descriptor in progress
    XHCI_SLOT_STR_MANUFACTURER,  // GET_DESCRIPTOR(String, iManufacturer) in progress
    XHCI_SLOT_STR_PRODUCT,  // GET_DESCRIPTOR(String, iProduct) in progress
    XHCI_SLOT_STR_SERIAL,  // GET_DESCRIPTOR(String, iSerialNumber) in progress
    XHCI_SLOT_CFG_HDR_DONE,
    XHCI_SLOT_CFG_DONE,  // Config Descriptor received, Set Configuration in progress
    XHCI_SLOT_SETCFG_DONE,  // Set Configuration done, Configure Endpoint in progress
    XHCI_SLOT_EP_CONFIGURED,  // Configure Endpoint done, first Normal TRB posted
    XHCI_SLOT_SETPROTO_DONE,
    XHCI_SLOT_SETIDLE_DONE,
    XHCI_SLOT_POLLING,  // Polling active, HID reports arriving
    XHCI_SLOT_HUB_DESC_DONE,
    XHCI_SLOT_WAIT_HUB_DESC,
    XHCI_SLOT_HUB_EVALCTX_SENT,
    XHCI_SLOT_HUB_PORT_STATUS_WAIT,
    XHCI_SLOT_HUB_PORT_CLEAR_SENT,  // CLEAR_PORT_FEATURE in flight, waiting for ack
    XHCI_SLOT_HUB_PORT_RESET_SENT,  // SET_PORT_FEATURE(PORT_RESET) in flight, waiting for ack
    XHCI_SLOT_HUB_CFG_HDR_DONE,
    XHCI_SLOT_HUB_CFG_DONE,
    XHCI_SLOT_HUB_SET_CFG_DONE,
    XHCI_SLOT_HUB_EP_CONFIGURED,
    XHCI_SLOT_HUB_POLLING,
    XHCI_SLOT_IDLE_UNSUPPORTED
} xhci_slot_state_t;

typedef struct usb_device_desc_t{
    char* manufacturer_name;
    char* product_name;
    char* serial_number;
} usb_device_desc_t;

typedef struct {
    int32_t pending_dx;
    int32_t pending_dy;
    int8_t  pending_wheel;
    uint8_t buttons;
    bool    dirty;
} mouse_pending_t;

extern mouse_pending_t mouse_state;
extern semaphore_t mouse_sem;