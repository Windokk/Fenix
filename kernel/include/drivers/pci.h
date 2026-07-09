#pragma once

#include "common.h"

// IO Ports method 

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

// Common
#define PCI_MAX_DEVICES        8192
#define PCI_OFFSET_VENDOR      0x00
#define PCI_OFFSET_DEVICE      0x02
#define PCI_OFFSET_PROG_IF     0x09
#define PCI_OFFSET_SUBCLASS    0x0A
#define PCI_OFFSET_CLASS       0x0B
#define PCI_OFFSET_HEADER_TYPE 0x0E
#define PCI_OFFSET_BAR0        0x10
#define PCI_OFFSET_SEC_BUS     0x19
#define PCI_OFFSET_IRQ_LINE    0x3C
#define PCI_OFFSET_IRQ_PIN     0x3D

#define PCI_VENDOR_COUNT       200

#define MAX_DRIVERS            10000
#define MAX_DEVICES            8192

typedef struct {
    uint16_t vendor_id;
    char* readable_name;
} pci_vendor_name_t;

typedef struct {
    uint8_t class_code;
    uint8_t subclass_code;
    uint8_t prog_if;
    const char *readable_name;
} pci_device_name_t;

extern pci_vendor_name_t pci_vendors_names[PCI_VENDOR_COUNT];
extern pci_device_name_t pci_devices_names[149];

typedef struct pci_device_t {
    uint8_t  bus, device, function;
    uint16_t vendor_id, device_id;
    uint8_t  class_code, subclass, prog_if, revision;
    uint8_t  header_type;
    uint32_t bars[6];
    uint8_t  irq_line;
    uint8_t  irq_pin;
} pci_device_t;

typedef struct pci_device_id_t{
    uint16_t vendor_id, device_id;
} pci_device_id_t;

typedef struct pci_driver_t{
    char* name;
    pci_device_id_t* id_table; // Sentinel-ended ({0,0})
    int  (*probe)(pci_device_t*);
    void (*remove)(pci_device_t*);
} pci_driver_t;

extern pci_device_t pci_devices[MAX_DEVICES];
extern uint32_t     pci_devices_count;

extern pci_driver_t* pci_drivers[MAX_DRIVERS];
extern uint32_t      pci_drivers_count;

extern void pci_init(void);

extern void pci_register_driver(pci_driver_t* driver);

extern void pci_match_driver(pci_device_t* device);