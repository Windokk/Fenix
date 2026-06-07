extern long_mode_start

extern p4_table
extern p3_table
extern p2_table
extern p1_table
extern kernel_stack_top
extern gdt64.pointer
extern gdt64.kernel_code

%define KERNEL_VMA          0xFFFF800000000000
%define PAGE_SIZE           0x1000
%define KERNEL_STACK_SIZE   PAGE_SIZE*2

section .efi64_entry
bits 64
DEFAULT ABS
global start_efi64
start_efi64:

    mov rsp, (kernel_stack_top - KERNEL_VMA)
    
    push rax
    push rbx

    call setup_page_tables

    mov rcx, p4_table - KERNEL_VMA
    mov cr3, rcx

    pop rbx        ; Restore original rbx value into rbx
    pop rax        ; Restore original rax value into rax

    ; Move them to rdi and rsi respectively
    mov rdi, rax   ; Transfer original rax to rdi
    mov rsi, rbx   ; Transfer original rbx to rsi

    ; jump to refresh RIP and new GDT
    mov rax, long_mode_start
    jmp rax

setup_page_tables:
    ; PML4[0] -> p3_table  (identity)
    mov rax, p3_table - KERNEL_VMA
    or rax, 0b11
    mov [p4_table - KERNEL_VMA], rax

    ; PML4[256] -> same p3_table  (higher-half at 0xFFFF800000000000)
    mov [p4_table - KERNEL_VMA + 256 * 8], rax

    ; p3_table[0] -> p2_table
    mov rax, p2_table - KERNEL_VMA
    or rax, 0b11
    mov [p3_table - KERNEL_VMA], rax

    ; Map 4 x 2MB huge pages (0-8MB) in p2_table.
    ; PDE flags: Present(1) + Writable(2) + PageSize(0x80) = 0x83
    ; This covers the kernel binary AND wherever GRUB places the multiboot struct.
    mov rcx, 0
.map_huge:
    mov rax, 0x200000
    mul rcx
    or rax, 0x83
    mov [p2_table - KERNEL_VMA + rcx * 8], rax
    inc rcx
    cmp rcx, 4
    jne .map_huge

    ret