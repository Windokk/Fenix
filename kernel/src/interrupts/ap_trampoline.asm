bits 16
org 0x8000

start:
    cli
    cld
    xor ax, ax
    mov ds, ax

    lgdt [gdt_desc_phys]

    mov eax, cr0
    or  eax, 1
    mov cr0, eax
    jmp 0x18:prot32         ; far jump flush prefetch + load CS

align 8
gdt_table:
    dq 0x0000000000000000
    dq 0x00AF9A000000FFFF
    dq 0x00CF92000000FFFF
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:

gdt_desc_phys:
    dw gdt_end - gdt_table - 1
    dd gdt_table

bsp_cr3_phys: dd 0
              dd 0
ap_stack_top: dq 0
ap_entry_fn:  dq 0

bits 32
prot32:
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; copy CR3 from BSP
    mov eax, [bsp_cr3_phys]
    mov cr3, eax

    ; activate PAE + long mode
    mov eax, cr4
    or  eax, (1 << 5) | (1 << 9) | (1 << 10)  ; PAE + OSFXSR + OSXMMEXCPT
    mov cr4, eax

    mov ecx, 0xC0000080     ; IA32_EFER
    rdmsr
    or  eax, (1 << 8)       ; LME
    wrmsr

    mov eax, cr0
    or  eax, (1 << 31)      ; PG
    mov cr0, eax
    jmp 0x08:long64         ; far jump to 64-bit CS

bits 64
DEFAULT ABS
long64:
    mov rdi, [ap_stack_top]   ; first arg (System V ABI)
    mov rsp, rdi
    mov rax, [ap_entry_fn]
    sub rsp, 8     ; align RSP to ABI convention (RSP % 16 == 8 at function entry)
    jmp rax