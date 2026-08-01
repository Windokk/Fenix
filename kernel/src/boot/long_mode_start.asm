%define GDT_KERNEL_DATA 0x10
%define GDT_TSS         0x28 

extern kernel_premain
extern kernel_main

extern p4_table
extern p3_table
extern p2_table
extern p1_table
extern kernel_stack_top
extern gdt64
extern gdt64.tss
extern gdt64_pointer
extern tss64
extern tss64.rsp0

%define KERNEL_VMA          0xFFFF800000000000

section .text
bits 64
global long_mode_start
long_mode_start:
    cli  ; Clear interrupts (for initialization phase)

    mov rax, flush_rip ; Load the address of flush_rip

    ; If it is mapped, jump to flush_rip
    jmp rax
    
flush_rip:

    mov rax, gdt64_pointer
    lgdt [rax]

    ; Reload CS with our own kernel code selector (0x08).
    ; A near jmp/mov cannot change CS in 64-bit mode — only a far
    ; jump/call/iretq reloads the CS descriptor cache. Without this,
    ; CS keeps whatever selector the EFI/GRUB entry left active, which
    ; becomes a dangling reference into OUR GDT once lgdt above runs.
    push qword 0x08            ; kernel code selector (matches gdt64.kernel_code)
    lea rax, [rel .reload_cs]
    push rax
    retfq
.reload_cs:

    mov ax, GDT_KERNEL_DATA
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; setup new stack
    mov rsp, kernel_stack_top
    mov rax, tss64.rsp0
    mov [rax], rsp

    ; enable 64bit TSS
    mov rbx, gdt64              
    add rbx, gdt64.tss
    mov rax, tss64
    mov word [rbx + 2], ax
    shr rax, 16
    mov byte [rbx + 4], al
    shr rax, 8
    mov byte [rbx + 7], al
    shr rax, 8
    mov dword [rbx + 8], eax

    mov ax, GDT_TSS
    ltr ax

    call kernel_premain

    call kernel_main
    
    hlt

