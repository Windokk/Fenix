#include "drivers/pci.h"

#include "acpi/acpi.h"

#include "stdio.h"

#include "memory/paging_pages.h"

static bool g_pci_use_ecam = false;
static uint64_t g_ecam_base;
static uint16_t g_ecam_seg;
static uint8_t g_ecam_bus_start;
static uint8_t g_ecam_bus_end;

static void pci_scan_bus(uint8_t bus); // Forward decl

pci_device_t pci_devices[8192];
uint32_t     pci_device_count;

pci_vendor_pair_t pci_vendors_names[PCI_VENDOR_COUNT] = {
{0x8086, "Intel Corporation"},
{0x1022, "AMD"},
{0x10DE, "NVIDIA Corporation"},
{0x14E4, "Broadcom Inc."},
{0x10EC, "Realtek Semiconductor"},
{0x1106, "VIA Technologies"},
{0x104C, "Texas Instruments"},
{0x1234, "QEMU"},
{0x1AF4, "Red Hat VirtIO"},
{0x15AD, "VMware"},
{0x80EE, "VirtualBox"},
{0x1013, "Cirrus Logic"},
{0x5333, "S3 Graphics"},
{0x102B, "Matrox Electronics"},
{0x1002, "ATI Technologies"},
{0x106B, "Apple Inc."},
{0x1033, "NEC Corporation"},
{0x103C, "HP Inc."},
{0x1028, "Dell Inc."},
{0x17AA, "Lenovo"},
{0x1043, "ASUSTeK Computer"},
{0x1458, "Gigabyte Technology"},
{0x1462, "Micro-Star International"},
{0x1849, "ASRock"},
{0x104D, "Sony Corporation"},
{0x107B, "Gateway"},
{0x1179, "Toshiba"},
{0x144D, "Samsung Electronics"},
{0x10A9, "Silicon Integrated Systems"},
{0x1217, "O2 Micro"},
{0x1180, "Ricoh Co Ltd"},
{0x197B, "JMicron Technology"},
{0x1B21, "ASMedia Technology"},
{0x1095, "Silicon Image"},
{0x11AB, "Marvell Technology"},
{0x14F1, "Conexant Systems"},
{0x168C, "Qualcomm Atheros"},
{0x1969, "Atheros Communications"},
{0x1814, "Ralink Technology"},
{0x14C3, "MediaTek"},
{0x1260, "Intersil"},
{0x11C1, "Agere Systems"},
{0x14DB, "Avago Technologies"},
{0x1131, "Philips Semiconductors"},
{0x13FE, "Kingston Technology"},
{0x13B1, "Linksys"},
{0x17CB, "Qualcomm Technologies"},
{0x105A, "Promise Technology"},
{0x1191, "Artop Electronic"},
{0x125D, "ESS Technology"},
{0x1274, "Ensoniq"},
{0x1102, "Creative Technology"},
{0x1412, "IC Ensemble"},
{0x13F6, "C-Media Electronics"},
{0x12EB, "Aureal Semiconductor"},
{0x10B9, "ULi Electronics"},
{0x1039, "Silicon Integrated Systems"},
{0x10B7, "3Com Corporation"},
{0x11AD, "Lite-On Communications"},
{0x1186, "D-Link System"},
{0x126C, "Nortel Networks"},
{0x12AE, "Alteon Networks"},
{0x1113, "Accton Technology"},
{0x10CF, "Fujitsu Siemens"},
{0x108E, "Sun Microsystems"},
{0x1166, "ServerWorks"},
{0x1168, "Broadcom ServerWorks"},
{0x1077, "QLogic Corporation"},
{0x107E, "Gateway 2000"},
{0x111D, "IDT"},
{0x117C, "Taiyo Yuden"},
{0x11F6, "Compaq"},
{0x0E11, "Compaq Computer"},
{0x9004, "Adaptec"},
{0x9005, "Adaptec Inc"},
{0x9006, "Adaptec"},
{0x9008, "Adaptec"},
{0x1000, "LSI Logic"},
{0x1001, "Kolter Electronic"},
{0x1004, "VLSI Technology"},
{0x100C, "Tseng Labs"},
{0x1011, "Digital Equipment Corporation"},
{0x1014, "IBM"},
{0x101A, "NCR"},
{0x1023, "Trident Microsystems"},
{0x1025, "Acer Incorporated"},
{0x102F, "CTC Research"},
{0x1031, "Fujitsu"},
{0x103E, "Solliday Engineering"},
{0x1042, "PC Technology"},
{0x1044, "Adaptec"},
{0x1045, "OPTi Inc"},
{0x1048, "Elsa AG"},
{0x104A, "STMicroelectronics"},
{0x1050, "Winbond Electronics"},
{0x1054, "Hitachi"},
{0x1057, "Motorola"},
{0x105D, "Number Nine Visual"},
{0x1060, "United Microelectronics"},
{0x1061, "I-O Data Device"},
{0x106C, "Tseng Labs"},
{0x1073, "Yamaha Corporation"},
{0x1078, "Cyrix Corporation"},
{0x1080, "Contaq Microsystems"},
{0x1083, "Forex Computer"},
{0x108A, "Apple Computer"},
{0x1092, "Diamond Multimedia"},
{0x109E, "Brooktree"},
{0x10A0, "NeoMagic"},
{0x10A8, "SGS Thomson"},
{0x10AA, "Fujitsu Limited"},
{0x10B5, "PLX Technology"},
{0x10C8, "Neomagic"},
{0x10D9, "Macronix"},
{0x10F0, "IMS"},
{0x10F1, "Digicom"},
{0x10FB, "Spectrum Signal"},
{0x1101, "Initio Corporation"},
{0x110A, "Siemens"},
{0x1114, "Atmel Corporation"},
{0x1119, "Elitegroup Computer Systems"},
{0x1127, "FORE Systems"},
{0x1137, "Cisco Systems"},
{0x113C, "C-Cube Microsystems"},
{0x1148, "SysKonnect"},
{0x114D, "Lexmark International"},
{0x1154, "Digitools"},
{0x115D, "Xircom"},
{0x1163, "Rendition"},
{0x116F, "Broadcom"},
{0x1172, "Altera Corporation"},
{0x117A, "Tandberg"},
{0x1183, "Excel Switching"},
{0x118A, "Mitsubishi Electronics"},
{0x1198, "NetVin"},
{0x11A9, "Siemens Nixdorf"},
{0x11BD, "Galil Motion Control"},
{0x11C6, "Performance Technologies"},
{0x11D1, "Auravision"},
{0x11D4, "Analog Devices"},
{0x11DE, "Zoran Corporation"},
{0x11E8, "Crystal Semiconductor"},
{0x11F8, "PMC-Sierra"},
{0x120E, "Cyclades"},
{0x121A, "3dfx Interactive"},
{0x122D, "Aztech Systems"},
{0x123D, "Sigma Designs"},
{0x1241, "Aureal"},
{0x124B, "National Semiconductor"},
{0x1259, "Allied Telesis"},
{0x126F, "Silicon Motion"},
{0x1279, "Transmeta"},
{0x1283, "ICS Advent"},
{0x1287, "Cambridge Silicon Radio"},
{0x128A, "Asante Technologies"},
{0x1292, "Dolphin Interconnect"},
{0x12D2, "NVIDIA"},
{0x12D8, "Pericom Semiconductor"},
{0x12E2, "ALi Corporation"},
{0x12EB, "Aureal Semiconductor"},
{0x12F8, "Electronic Design"},
{0x1317, "ADMtek"},
{0x131F, "Siig"},
{0x1332, "SVEC"},
{0x1344, "Micron Technology"},
{0x134D, "Sharp Corporation"},
{0x135A, "Phoenix Technologies"},
{0x1365, "Hypercope"},
{0x1371, "Ensoniq"},
{0x1373, "Yamaha Corporation"},
{0x1385, "Netgear"},
{0x1397, "COLogne Chip "},
{0x13A3, "Huawei Technologies"},
{0x13B9, "NXP Semiconductors"},
{0x13C1, "LSI Corporation"},
{0x13D1, "ABB"},
{0x13D8, "Exar Corporation"},
{0x13F0, "Sanei Electric"},
{0x1407, "Lava Computer"},
{0x1414, "Microsoft Corporation"},
{0x1415, "Oxford Semiconductor"},
{0x1425, "Chelsio Communications"},
{0x1432, "Edimax Technology"},
{0x1435, "SCM Microsystems"},
{0x144A, "ASUS"},
{0x1468, "AboCom Systems"},
{0x147B, "ABIT Computer"},
{0x148C, "Atheros"},
{0x1498, "KYE Systems"},
{0x14A4, "StarTech.com"},
{0x14A9, "HERCULES"},
{0x14B7, "PNY Technologies"},
{0x14CD, "Super Micro Computer"},
{0x14E6, "C-Media Electronics"},
{0x1507, "Cedar Technologies"},
{0x1516, "Myson Technology"},
{0x1524, "ENE Technology"},
{0x153B, "TerraTec Electronic"},
{0x1545, "VisionTek"},
{0x1565, "Biostar Microtech"},
{0x1571, "ZOOM Telephonics"},
{0x1590, "Faraday Technology"},
{0x15B7, "Sandisk Corporation"},
{0x15D9, "Super Talent"},
{0x15E9, "USI"},
{0x161F, "Synopsys"},
{0x163C, "Intel Server Systems"},
{0x1657, "Broadcom NetXtreme"},
{0x166D, "Actiontec"},
{0x1682, "XFX Pine Group"},
{0x16AE, "Global Sun Technology"},
{0x16CA, "CACE Technologies"},
{0x16EC, "Ubiquiti Networks"},
{0x1734, "Fujitsu Technology Solutions"},
{0x177D, "Cavium Networks"},
{0x17FE, "Linksys Group"},
{0x18CA, "XGI Technology"},
{0x18EC, "ARK Logic"},
{0x196D, "Club 3D"},
{0x1A03, "ASPEED Technology"},
{0x1B4B, "Marvell Technology Group"},
{0x1C5C, "SK hynix"},
{0x1CC1, "Accusys"},
{0x1D17, "Fresco Logic "},
{0x1DE1, "Tekram Technology"}
};

pci_device_pair_t pci_devices_names[149] = {
    /* =========================
     * Unclassified
     * ========================= */
    {0x00, 0x00, 0x00, "Non-VGA unclassified device"},
    {0x00, 0x01, 0x00, "VGA compatible unclassified device"},

    /* =========================
     * Mass Storage Controllers
     * ========================= */
    {0x01, 0x00, 0x00, "SCSI storage controller"},

    {0x01, 0x01, 0x00, "ISA Compatibility IDE controller"},
    {0x01, 0x01, 0x05, "PCI Native IDE controller"},
    {0x01, 0x01, 0x0A, "ISA Compatibility IDE controller with Bus Mastering"},
    {0x01, 0x01, 0x0F, "PCI Native IDE controller with Bus Mastering"},
    {0x01, 0x01, 0x80, "ISA Compatibility IDE controller with switching"},
    {0x01, 0x01, 0x85, "PCI Native IDE controller with switching"},
    {0x01, 0x01, 0x8A, "ISA Compatibility IDE controller with switching and Bus Mastering"},
    {0x01, 0x01, 0x8F, "PCI Native IDE controller with switching and Bus Mastering"},

    {0x01, 0x02, 0x00, "Floppy disk controller"},
    {0x01, 0x03, 0x00, "IPI bus controller"},
    {0x01, 0x04, 0x00, "RAID controller"},

    {0x01, 0x05, 0x20, "ATA controller (Single DMA)"},
    {0x01, 0x05, 0x30, "ATA controller (Chained DMA)"},

    {0x01, 0x06, 0x00, "Vendor-specific SATA controller"},
    {0x01, 0x06, 0x01, "AHCI SATA controller"},
    {0x01, 0x06, 0x02, "Serial Storage Bus SATA controller"},

    {0x01, 0x07, 0x00, "SAS controller"},

    {0x01, 0x08, 0x01, "NVMHCI controller"},
    {0x01, 0x08, 0x02, "NVMe controller"},

    {0x01, 0x80, 0x00, "Other mass storage controller"},

    /* =========================
     * Network Controllers
     * ========================= */
    {0x02, 0x00, 0x00, "Ethernet controller"},
    {0x02, 0x01, 0x00, "Token Ring controller"},
    {0x02, 0x02, 0x00, "FDDI controller"},
    {0x02, 0x03, 0x00, "ATM controller"},
    {0x02, 0x04, 0x00, "ISDN controller"},
    {0x02, 0x05, 0x00, "WorldFip controller"},
    {0x02, 0x06, 0x00, "PICMG controller"},
    {0x02, 0x07, 0x00, "InfiniBand controller"},
    {0x02, 0x08, 0x00, "Fabric controller"},
    {0x02, 0x80, 0x00, "Other network controller"},

    /* =========================
     * Display Controllers
     * ========================= */
    {0x03, 0x00, 0x00, "VGA compatible controller"},
    {0x03, 0x00, 0x01, "8514-compatible controller"},
    {0x03, 0x01, 0x00, "XGA controller"},
    {0x03, 0x02, 0x00, "3D controller"},
    {0x03, 0x80, 0x00, "Other display controller"},

    /* =========================
     * Multimedia Controllers
     * ========================= */
    {0x04, 0x00, 0x00, "Video device"},
    {0x04, 0x01, 0x00, "Audio device"},
    {0x04, 0x02, 0x00, "Computer telephony device"},
    {0x04, 0x03, 0x00, "Mixed mode multimedia device"},
    {0x04, 0x80, 0x00, "Other multimedia controller"},

    /* =========================
     * Memory Controllers
     * ========================= */
    {0x05, 0x00, 0x00, "RAM controller"},
    {0x05, 0x01, 0x00, "Flash controller"},
    {0x05, 0x80, 0x00, "Other memory controller"},

    /* =========================
     * Bridges
     * ========================= */
    {0x06, 0x00, 0x00, "Host bridge"},
    {0x06, 0x01, 0x00, "ISA bridge"},
    {0x06, 0x02, 0x00, "EISA bridge"},
    {0x06, 0x03, 0x00, "MCA bridge"},
    {0x06, 0x04, 0x00, "PCI-to-PCI bridge"},
    {0x06, 0x04, 0x01, "Subtractive PCI-to-PCI bridge"},
    {0x06, 0x05, 0x00, "PCMCIA bridge"},
    {0x06, 0x06, 0x00, "NuBus bridge"},
    {0x06, 0x07, 0x00, "CardBus bridge"},
    {0x06, 0x08, 0x00, "RACEway bridge"},
    {0x06, 0x09, 0x40, "Semi-transparent PCI primary bridge"},
    {0x06, 0x09, 0x80, "Semi-transparent PCI secondary bridge"},
    {0x06, 0x0A, 0x00, "InfiniBand-to-PCI host bridge"},
    {0x06, 0x80, 0x00, "Other bridge device"},

    /* =========================
     * Communication Controllers
     * ========================= */
    {0x07, 0x00, 0x00, "8250 serial controller"},
    {0x07, 0x00, 0x01, "16450 serial controller"},
    {0x07, 0x00, 0x02, "16550 serial controller"},
    {0x07, 0x00, 0x03, "16650 serial controller"},
    {0x07, 0x00, 0x04, "16750 serial controller"},
    {0x07, 0x00, 0x05, "16850 serial controller"},
    {0x07, 0x00, 0x06, "16950 serial controller"},

    {0x07, 0x01, 0x00, "Parallel port"},
    {0x07, 0x01, 0x01, "Bi-directional parallel port"},
    {0x07, 0x01, 0x02, "ECP parallel port"},
    {0x07, 0x01, 0x03, "IEEE1284 controller"},
    {0x07, 0x01, 0xFE, "IEEE1284 target device"},

    {0x07, 0x02, 0x00, "Multiport serial controller"},
    {0x07, 0x03, 0x00, "Generic modem"},
    {0x07, 0x03, 0x01, "Hayes modem (16450)"},
    {0x07, 0x03, 0x02, "Hayes modem (16550)"},
    {0x07, 0x03, 0x03, "Hayes modem (16650)"},
    {0x07, 0x03, 0x04, "Hayes modem (16750)"},
    {0x07, 0x05, 0x00, "Smart card controller"},
    {0x07, 0x80, 0x00, "Other communication controller"},

    /* =========================
     * Generic System Peripherals
     * ========================= */
    {0x08, 0x00, 0x00, "8259 PIC"},
    {0x08, 0x00, 0x01, "ISA PIC"},
    {0x08, 0x00, 0x02, "EISA PIC"},
    {0x08, 0x00, 0x10, "I/O APIC"},
    {0x08, 0x00, 0x20, "I/O(x) APIC"},

    {0x08, 0x01, 0x00, "8237 DMA controller"},
    {0x08, 0x01, 0x01, "ISA DMA controller"},
    {0x08, 0x01, 0x02, "EISA DMA controller"},

    {0x08, 0x02, 0x00, "8254 timer"},
    {0x08, 0x02, 0x01, "ISA timer"},
    {0x08, 0x02, 0x02, "EISA timer"},
    {0x08, 0x02, 0x03, "HPET"},

    {0x08, 0x03, 0x00, "Generic RTC"},
    {0x08, 0x03, 0x01, "ISA RTC"},

    {0x08, 0x04, 0x00, "PCI Hot-Plug controller"},
    {0x08, 0x05, 0x00, "SD Host controller"},
    {0x08, 0x06, 0x00, "IOMMU"},
    {0x08, 0x80, 0x00, "Other system peripheral"},

    /* =========================
     * Input Devices
     * ========================= */
    {0x09, 0x00, 0x00, "Keyboard controller"},
    {0x09, 0x01, 0x00, "Digitizer pen"},
    {0x09, 0x02, 0x00, "Mouse controller"},
    {0x09, 0x03, 0x00, "Scanner controller"},
    {0x09, 0x04, 0x00, "Gameport controller"},
    {0x09, 0x80, 0x00, "Other input controller"},

    /* =========================
     * Serial Bus Controllers
     * ========================= */
    {0x0C, 0x00, 0x00, "FireWire controller"},
    {0x0C, 0x00, 0x10, "OHCI FireWire controller"},

    {0x0C, 0x01, 0x00, "ACCESS.bus controller"},
    {0x0C, 0x02, 0x00, "SSA controller"},

    {0x0C, 0x03, 0x00, "UHCI USB controller"},
    {0x0C, 0x03, 0x10, "OHCI USB controller"},
    {0x0C, 0x03, 0x20, "EHCI USB2 controller"},
    {0x0C, 0x03, 0x30, "XHCI USB3 controller"},
    {0x0C, 0x03, 0x80, "Unspecified USB controller"},
    {0x0C, 0x03, 0xFE, "USB Device controller"},

    {0x0C, 0x04, 0x00, "Fibre Channel controller"},
    {0x0C, 0x05, 0x00, "SMBus controller"},
    {0x0C, 0x06, 0x00, "InfiniBand controller"},

    {0x0C, 0x07, 0x00, "IPMI SMIC interface"},
    {0x0C, 0x07, 0x01, "IPMI Keyboard controller interface"},
    {0x0C, 0x07, 0x02, "IPMI Block transfer interface"},

    {0x0C, 0x08, 0x00, "SERCOS controller"},
    {0x0C, 0x09, 0x00, "CANbus controller"},

    /* =========================
     * Wireless Controllers
     * ========================= */
    {0x0D, 0x00, 0x00, "iRDA controller"},
    {0x0D, 0x01, 0x00, "Consumer IR controller"},
    {0x0D, 0x10, 0x00, "RF controller"},
    {0x0D, 0x11, 0x00, "Bluetooth controller"},
    {0x0D, 0x12, 0x00, "Broadband controller"},
    {0x0D, 0x20, 0x00, "802.11a controller"},
    {0x0D, 0x21, 0x00, "802.11b controller"},
    {0x0D, 0x80, 0x00, "Other wireless controller"},

    /* =========================
     * Intelligent I/O
     * ========================= */
    {0x0E, 0x00, 0x00, "I2O controller"},

    /* =========================
     * Satellite
     * ========================= */
    {0x0F, 0x01, 0x00, "Satellite TV controller"},
    {0x0F, 0x02, 0x00, "Satellite audio controller"},
    {0x0F, 0x03, 0x00, "Satellite voice controller"},
    {0x0F, 0x04, 0x00, "Satellite data controller"},

    /* =========================
     * Encryption
     * ========================= */
    {0x10, 0x00, 0x00, "Network encryption controller"},
    {0x10, 0x10, 0x00, "Entertainment encryption controller"},
    {0x10, 0x80, 0x00, "Other encryption controller"},

    /* =========================
     * Signal Processing
     * ========================= */
    {0x11, 0x00, 0x00, "DPIO module"},
    {0x11, 0x01, 0x00, "Performance counter"},
    {0x11, 0x10, 0x00, "Communication synchronizer"},
    {0x11, 0x20, 0x00, "Signal processing management"},
    {0x11, 0x80, 0x00, "Other signal processing controller"},

    /* =========================
     * Modern PCIe classes
     * ========================= */
    {0x12, 0x00, 0x00, "Processing accelerator"},
    {0x13, 0x00, 0x00, "Non-Essential Instrumentation"},
    {0x14, 0x00, 0x00, "Reserved"},
    {0x40, 0x00, 0x00, "Co-processor"},
    {0xFF, 0x00, 0x00, "Unassigned device"},

    {0x00, 0x00, 0x00, NULL}
};

void get_pci_device_full_name(uint16_t vendor_id, uint8_t class_code, uint8_t subclass, uint8_t prog_if, char *buffer, size_t buffer_size)
{
    const char *vendor_str = "Unknown vendor";
    const char *device_str = "Unknown device";

    for (uint32_t i = 0; i < PCI_VENDOR_COUNT; i++) {
        if (pci_vendors_names[i].vendor_id == vendor_id) {
            vendor_str = pci_vendors_names[i].readable_name;
            break;
        }
    }

    for (uint32_t i = 0; pci_devices_names[i].readable_name != NULL; i++) {
        if (pci_devices_names[i].class_code == class_code &&
            pci_devices_names[i].subclass_code == subclass &&
            pci_devices_names[i].prog_if == prog_if)
        {
            device_str = pci_devices_names[i].readable_name;
            break;
        }
    }

    snprintf(buffer, buffer_size,
             "%s - %s",
             vendor_str,
             device_str);
}

static uint8_t pci_read8(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, void* virt) {
    if(g_pci_use_ecam){
        uint32_t value = *(volatile uint32_t*)((uint8_t*)virt + (offset & ~3));
        return (uint8_t)(value >> ((offset & 3) * 8)) & 0xFF;
    }
    else{
        uint32_t address;
        uint32_t lbus  = (uint32_t)bus;
        uint32_t lslot = (uint32_t)device;
        uint32_t lfunc = (uint32_t)func;
        uint8_t tmp = 0;
    
        // Create configuration address as per Figure 1
        address = (uint32_t)((lbus << 16)  | (lslot << 11) | 
                (lfunc << 8) | ((offset & ~3) & 0xFC) | ((uint32_t)0x80000000));
    
        // Write out the address
        outl(0xCF8, address);
        // Read in the data
        tmp = (uint8_t)((inl(0xCFC) >> ((offset & 3) * 8)) & 0xFF);
        return tmp;
    }
    
}

static uint16_t pci_read16(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, void* virt) {
    if(g_pci_use_ecam){
        uint32_t value = *(volatile uint32_t*)((uint8_t*)virt + (offset & ~3));
        return (uint16_t)(value >> ((offset & 2) * 8)) & 0xFFFF;
    }
    else{
        uint32_t address;
        uint32_t lbus  = (uint32_t)bus;
        uint32_t lslot = (uint32_t)device;
        uint32_t lfunc = (uint32_t)func;
        uint16_t tmp = 0;
    
        // Create configuration address as per Figure 1
        address = (uint32_t)((lbus << 16) | (lslot << 11) | 
                (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    
        // Write out the address
        outl(0xCF8, address);
        // Read in the data
        // (offset & 2) * 8) = 0 will choose the first word of the 32-bit register
        tmp = (uint16_t)((inl(0xCFC) >> ((offset & 2) * 8)) & 0xFFFF);
        return tmp;
    }
}

static uint32_t pci_read32(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, void* virt){
    if(g_pci_use_ecam){
        uint32_t value = *(volatile uint32_t*)((uint8_t*)virt + (offset & ~3));
        return value;
    }
    else{
        uint32_t address;
        uint32_t lbus  = (uint32_t)bus;
        uint32_t lslot = (uint32_t)device;
        uint32_t lfunc = (uint32_t)func;
    
        // Create configuration address as per Figure 1
        address = (uint32_t)((lbus << 16) | (lslot << 11) | 
                (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    
        // Write out the address
        outl(PCI_CONFIG_ADDRESS, address);
        // Read in the data
        return inl(PCI_CONFIG_DATA);
    }
}

static pci_device_t pci_scan_function(uint8_t bus, uint8_t device, uint8_t func){

    uint64_t phys = (g_ecam_base + ((uint64_t)(bus - g_ecam_bus_start) << 20)) + ((uint64_t)device << 15) + ((uint64_t)func << 12);
    void* virt = NULL;
    if(g_pci_use_ecam)
        virt = paging_map_mmio(phys, 4096);

    pci_device_t pci_device;
    memset(&pci_device, 0, sizeof(pci_device));
    uint16_t vendor_id = pci_read16(bus, device, func, 0x00, virt);
    if(vendor_id == 0xFFFF){
        pci_device.vendor_id = 0xFFFF;
        if(g_pci_use_ecam)
            paging_unmap_mmio(virt, 4096);
        return pci_device;
    }

    pci_device.vendor_id = vendor_id;
    pci_device.bus = bus;
    pci_device.device = device;
    pci_device.function = func;

    pci_device.device_id  = pci_read16(bus, device, func, 0x02, virt);
    pci_device.revision    = pci_read8 (bus, device, func, 0x08, virt);
    pci_device.prog_if     = pci_read8 (bus, device, func, 0x09, virt);
    pci_device.subclass    = pci_read8 (bus, device, func, 0x0A, virt);
    pci_device.class_code  = pci_read8 (bus, device, func, 0x0B, virt);
    pci_device.header_type = pci_read8 (bus, device, func, 0x0E, virt);

    if((pci_device.header_type & 0x7F) == 0x01){
        uint8_t sec_bus = pci_read8(bus, device, func, 0x19, virt);
        KLOG_INFO("PCI", "Found bridge : secondary bus %X", sec_bus);
        pci_scan_bus(sec_bus);
        if(pci_device_count < PCI_MAX_DEVICES){
            pci_devices[pci_device_count] = pci_device;
            pci_device_count++;
        }
        if(g_pci_use_ecam)
            paging_unmap_mmio(virt, 4096);
        return pci_device;
    }

    for(uint8_t i = 0; i < 6; i++){
        pci_device.bars[i] = pci_read32(bus, device, func, 0x10 + i*4, virt);
    }

    pci_device.irq_line = pci_read8(bus, device, func, 0x3C, virt);
    pci_device.irq_pin  = pci_read8(bus, device, func, 0x3D, virt);

    if(pci_device_count < PCI_MAX_DEVICES){
        pci_devices[pci_device_count] = pci_device;
        pci_device_count++;
    }

    char name[128];

    get_pci_device_full_name(vendor_id, pci_device.class_code, pci_device.subclass, pci_device.prog_if, name, sizeof(name));

    KLOG_INFO("PCI", "%X:%X.%X : %s",
           bus, device, func, name);

    if(g_pci_use_ecam)
        paging_unmap_mmio(virt, 4096);
    return pci_device;
}

static void pci_scan_device(uint8_t bus, uint8_t device){
    pci_device_t function0 = pci_scan_function(bus, device, 0);
    if(function0.vendor_id == 0xFFFF){
        return;
    }

    if(function0.header_type & 0x80){
        for(int i = 1; i < 8; i++){
            pci_scan_function(bus, device, i);
        }
    }
}

static void pci_scan_bus(uint8_t bus){
    for(int i = 0; i < 32; i++){
        pci_scan_device(bus, i);
    }
}

void pci_init(void)
{
    pci_device_count = 0;

    if(g_acpi.mcfg_count > 0){
        // MCFG parsed : using modern ECAM method
        g_pci_use_ecam = true;
        g_ecam_base    = g_acpi.mcfg[0].address;
        g_ecam_seg     = g_acpi.mcfg[0].segment;
        g_ecam_bus_start = g_acpi.mcfg[0].start_bus;
        g_ecam_bus_end   = g_acpi.mcfg[0].end_bus;
        KLOG_INFO("PCI", "ECAM @ 0x%llX  bus %u-%u", g_ecam_base, g_ecam_bus_start, g_ecam_bus_end);
    }
    else{
        // MCFG not parsed : using legacy IO ports method
        g_pci_use_ecam = false;
        KLOG_INFO("PCI", "MCFG not parsed, using legacy 0xCF8/0xCFC mode");
    }

    pci_scan_bus(0);

    KLOG_INFO("PCI", "%u devices found", pci_device_count);
}

pci_device_t* get_pci_device(uint8_t class_code, uint8_t subclass, uint8_t prog_if)
{
    if(pci_device_count <= 0){
        return NULL;
    }
    
    for(int i = 0; i < pci_device_count; i++){
        if(pci_devices[i].class_code == class_code && pci_devices[i].subclass == subclass && pci_devices[i].prog_if == prog_if){
            return &pci_devices[i];
        }
    }

    return NULL;
}
