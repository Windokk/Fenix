#pragma once

#ifndef __MULTIBOOT_H
#define __MULTIBOOT_H

#include <stdint.h>

typedef struct multiboot_tag {
    uint32_t type;
    uint32_t size;
} multiboot_tag_t;

struct multiboot_tag_framebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint16_t reserved;
};

typedef struct multiboot_tag_module {
    uint32_t type;
    uint32_t size;
    uint32_t mod_start;
    uint32_t mod_end;
    char cmdline[];
} multiboot_tag_module_t;

// Multiboot2 tags
#define MULTIBOOT2_TAG_TYPE_END        0
#define MULTIBOOT2_TAG_TYPE_CMDLINE    1
#define MULTIBOOT2_TAG_TYPE_BOOT_LOADER_NAME 2
#define MULTIBOOT2_TAG_TYPE_MODULE     3
#define MULTIBOOT2_TAG_TYPE_BASIC_MEMINFO 4
#define MULTIBOOT2_TAG_TYPE_BOOTDEV    5
#define MULTIBOOT2_TAG_TYPE_MMAP       6
#define MULTIBOOT2_TAG_TYPE_VBE        7
#define MULTIBOOT2_TAG_TYPE_FRAMEBUFFER 8
#define MULTIBOOT2_TAG_TYPE_ELF_SECTIONS 9
#define MULTIBOOT2_TAG_TYPE_APM        10
#define MULTIBOOT2_TAG_TYPE_EFI32      11
#define MULTIBOOT2_TAG_TYPE_EFI64      12
#define MULTIBOOT2_TAG_TYPE_SMBIOS     13
#define MULTIBOOT2_TAG_TYPE_ACPI_OLD   14
#define MULTIBOOT2_TAG_TYPE_ACPI_NEW   15
#define MULTIBOOT2_TAG_TYPE_NETWORK    16
#define MULTIBOOT2_TAG_TYPE_EFI_MMAP   17
#define MULTIBOOT2_TAG_TYPE_EFI_BS     18
#define MULTIBOOT2_TAG_TYPE_EFI32_IH   19
#define MULTIBOOT2_TAG_TYPE_EFI64_IH   20
#define MULTIBOOT2_TAG_TYPE_LOAD_BASE_ADDR 21

#endif