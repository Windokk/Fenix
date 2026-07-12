#pragma once

#include "common.h"

#include "tasks/task.h"

#include "drivers/pci.h"

extern pci_driver_t xhci_driver;

int xhci_probe(pci_device_t* device);

void xhci_remove(pci_device_t* device);

#define XHCI_CAP_CAPLENGTH  0x00
#define XHCI_CAP_HCSPARAMS1 0x04
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
    XHCI_SLOT_ENABLE_SENT       = 0,  // Enable Slot done, Address Device in progress
    XHCI_SLOT_ADDRESS_SENT      = 1,  // Address Device done, Get Device Descriptor in progress
    XHCI_SLOT_DESC_DONE         = 2,  // Device Descriptor received, Get Config Descriptor in progress
    XHCI_SLOT_STR_MANUFACTURER  = 3,  // GET_DESCRIPTOR(String, iManufacturer) in progress
    XHCI_SLOT_STR_PRODUCT       = 4,  // GET_DESCRIPTOR(String, iProduct) in progress
    XHCI_SLOT_STR_SERIAL        = 5,  // GET_DESCRIPTOR(String, iSerialNumber) in progress
    XHCI_SLOT_CFG_DONE          = 6,  // Config Descriptor received, Set Configuration in progress
    XHCI_SLOT_SETCFG_DONE       = 7,  // Set Configuration done, Configure Endpoint in progress
    XHCI_SLOT_EP_CONFIGURED     = 8,  // Configure Endpoint done, first Normal TRB posted
    XHCI_SLOT_POLLING           = 9,  // Polling active, HID reports arriving
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