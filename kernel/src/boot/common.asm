%define KERNEL_VMA          0xFFFF800000000000
%define PAGE_SIZE           0x1000
%define KERNEL_STACK_SIZE   PAGE_SIZE*2

section .data
global gdt64
global gdt64.tss
global gdt64.pointer
global gdt64.kernel_code
gdt64:                           ; Global Descriptor Table (64-bit).
align 8
.null: equ $ - gdt64         ; The null descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 0                         ; Access.
    db 0                         ; Granularity.
    db 0                         ; Base (high).
.kernel_code: equ $ - gdt64         ; The code descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 10011000b                 ; Present=1 + DPL=00 + S=1 (system segment) + Type=1000(Execute only)
    db 00100000b                 ; Granularity, 64 bits flag, limit19:16.
    db 0                         ; Base (high).
.kernel_data: equ $ - gdt64         ; The data descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 10010010b                 ; Present=1 + DPL=00 + S=1 + Type=0010(Read/Write)
    db 00000000b                 ; Granularity.
    db 0                         ; Base (high).
.user_code: equ $ - gdt64         ; The code descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 11111000b                 ; Present=1 + DPL=00 + S=1 (system segment) + Type=1000(Execute only)
    db 00100000b                ; Granularity, 64 bits flag, limit19:16.
    db 0                         ; Base (high).
.user_data: equ $ - gdt64         ; The data descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 11110010b                 ; Present=1 + DPL=00 + S=1 + Type=0010(Read/Write)
    db 00000000b                 ; Granularity.
    db 0                         ; Base (high).
.tss equ $ - gdt64           ; The TSS descriptor
    dw tss_size & 0xFFFF         ; Limit
    dw 0                         ; Base (bytes 0-2)
    db 0                         ; Base (byte 3)
    db 10001001b                 ; Type, present
    db 00000000b                 ; Misc
    db 0                         ; Base (byte 4)
    dd 0                         ; Base (bytes 5-8)
    dd 0                         ; Zero / reserved
.pointer:                    ; The GDT-pointer.
    dw $ - gdt64 - 1             ; Limit.
    dq gdt64                     ; Base.

section .bss
align 16
kernel_stack_bottom:
    resb KERNEL_STACK_SIZE
global kernel_stack_top
kernel_stack_top:

ist_stack_1:
    resb PAGE_SIZE
ist_stack_1_top:

ist_stack_2:
    resb PAGE_SIZE
ist_stack_2_top:

align PAGE_SIZE
global p4_table
p4_table resq 512
global p3_table
p3_table resq 512
global p2_table
p2_table resq 512
global p1_table
p1_table resq 512

section .data
align PAGE_SIZE
global tss64
global tss64.rsp0
tss64:
    dd 0
tss64.rsp0:
    times 3 dq 0 ; RSPn
    dq 0 ; Reserved
interrupt_stack_table:
    dq ist_stack_1_top ; IST1, NMI
    dq ist_stack_2_top ; IST2, Double fault
    dq 0 ; IST3
    dq 0 ; IST4
    dq 0 ; IST5
    dq 0 ; IST6
    dq 0 ; IST7
    dq 0 ; Reserved
    dw 0 ; Reserved
    dw 0 ; I/O Map Base Address
tss_size equ $ - tss64 - 1