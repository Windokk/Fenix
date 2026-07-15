section .multiboot_header

%define MULTIBOOT2_HEADER_MAGIC                  0xe85250d6
%define MULTIBOOT_HEADER_TAG_OPTIONAL            1
%define MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS       3
%define MULTIBOOT_HEADER_TAG_FRAMEBUFFER         5
%define MULTIBOOT_HEADER_TAG_EFI_BS              7
%define MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS_EFI64 9
%define MULTIBOOT_HEADER_TAG_END                 0
%define GRUB_MULTIBOOT_ARCHITECTURE_I386         0
%define KERNEL_VMA                               0xFFFF800000000000

extern start_bios32
extern start_efi64

multiboot_header:
    dd   MULTIBOOT2_HEADER_MAGIC
    dd   GRUB_MULTIBOOT_ARCHITECTURE_I386
    dd   multiboot_header_end - multiboot_header
    dd   -(MULTIBOOT2_HEADER_MAGIC + GRUB_MULTIBOOT_ARCHITECTURE_I386 + (multiboot_header_end - multiboot_header))
framebuffer_tag_start:  
    dw MULTIBOOT_HEADER_TAG_FRAMEBUFFER
    dw MULTIBOOT_HEADER_TAG_OPTIONAL
    dd framebuffer_tag_end - framebuffer_tag_start
    dd 1920
    dd 1080
    dd 32
framebuffer_tag_end:
    db 0    ; padding
    db 0    ; padding
    db 0    ; padding
    db 0    ; padding
entry_address_bios_tag_start:  
    dw  MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS
    dw  0
    dd  entry_address_bios_tag_end - entry_address_bios_tag_start
    dd  (start_bios32 - KERNEL_VMA)
entry_address_bios_tag_end:
    db 0    ; padding
    db 0    ; padding
    db 0    ; padding
    db 0    ; padding
boot_services_efi_tag_start:
    dw MULTIBOOT_HEADER_TAG_EFI_BS
    dw 0
    dd 8
boot_services_efi_tag_end:
entry_address_efi_tag_start:
    dw  MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS_EFI64
    dw  0
    dd  entry_address_efi_tag_end - entry_address_efi_tag_start
    dd  start_efi64
entry_address_efi_tag_end:
    db 0    ; padding
    db 0    ; padding
    db 0    ; padding
    db 0    ; padding
    dw MULTIBOOT_HEADER_TAG_END
    dw 0
    dd 0
multiboot_header_end: