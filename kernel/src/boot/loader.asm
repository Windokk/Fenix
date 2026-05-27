extern long_mode_start

%define KERNEL_VMA          0xFFFF800000000000
%define PAGE_SIZE           0x1000
%define KERNEL_STACK_SIZE   PAGE_SIZE*2

section .text
bits 32
global start
start:
    
    ; save eax and ebx (grub magic number and multiboot info) into edi and esi
    ; per GCC C ABI param specs 
    ; as it will be passed down to kernel main as arguments

    mov esp, (kernel_stack_top - KERNEL_VMA)

    push eax
    push ebx

    call detect_cpuid

    call check_long_mode

    call setup_page_tables

    pop ebx        ; Restore original ebx value into ebx
    pop eax        ; Restore original eax value into eax

    ; Move them to edi and esi respectively
    mov edi, eax   ; Transfer original eax to edi
    mov esi, ebx   ; Transfer original ebx to esi

    call enable_paging

    ; load the 64-bit GDT
    lgdt [(gdt64.pointer - KERNEL_VMA)]

    ; jump to refresh RIP and new GDT
    jmp gdt64.kernel_code:(long_mode_start - KERNEL_VMA)
    

no_cpuid:
    hlt

    jmp no_cpuid


no_longmode:
    
    hlt

    jmp no_longmode

; Check if CPUID is supported by attempting to flip the ID bit (bit 21) in
; the FLAGS register. If we can flip it, CPUID is available.

detect_cpuid:

    ; Copy FLAGS in to EAX via stack
    pushfd
    pop eax

    ; Copy to ECX as well for comparing later on
    mov ecx, eax

    ; Flip the ID bit
    xor eax, 1 << 21

    ; Copy EAX to FLAGS via the stack
    push eax
    popfd

    ; Copy FLAGS back to EAX (with the flipped bit if CPUID is supported)
    pushfd
    pop eax

    ; Restore FLAGS from the old version stored in ECX (i.e. flipping the ID bit
    ; back if it was ever flipped).
    push ecx
    popfd

    ; Compare EAX and ECX. If they are equal then that means the bit wasn't
    ; flipped, and CPUID isn't supported.
    xor eax, ecx
    jz no_cpuid
    ret


check_long_mode:
    mov eax, 0x80000000    ; Set the A-register to 0x80000000.
    cpuid                  ; CPU identification.
    cmp eax, 0x80000001    ; Compare the A-register with 0x80000001.
    jb no_longmode         ; It is less, there is no long mode.

    ; Now that we know that extended function is available we can use it to detect long mode:

    mov eax, 0x80000001    ; Set the A-register to 0x80000001.
    cpuid                  ; CPU identification.
    test edx, 1 << 29      ; Test if the LM-bit, which is bit 29, is set in the D-register.
    jz no_longmode         ; They aren't, there is no long mode.
    ret


setup_page_tables:
    ; PML4[0] -> p3_table  (identity)
    mov eax, p3_table - KERNEL_VMA
    or eax, 0b11
    mov [p4_table - KERNEL_VMA], eax

    ; PML4[256] -> same p3_table  (higher-half at 0xFFFF800000000000)
    mov [p4_table - KERNEL_VMA + 256 * 8], eax

    ; p3_table[0] -> p2_table
    mov eax, p2_table - KERNEL_VMA
    or eax, 0b11
    mov [p3_table - KERNEL_VMA], eax

    ; Map 4 x 2MB huge pages (0-8MB) in p2_table.
    ; PDE flags: Present(1) + Writable(2) + PageSize(0x80) = 0x83
    ; This covers the kernel binary AND wherever GRUB places the multiboot struct.
    mov ecx, 0
.map_huge:
    mov eax, 0x200000
    mul ecx
    or eax, 0x83
    mov [p2_table - KERNEL_VMA + ecx * 8], eax
    inc ecx
    cmp ecx, 4
    jne .map_huge

    ret

enable_paging:
    ; load P4 to cr3 register (cpu uses this to access the P4 table)
    mov eax, (p4_table - KERNEL_VMA)
    mov cr3, eax

    ; enable Physical Address Extension (PAE-flag) in cr4
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; set the long mode bit in the EFER MSR (model specific register)
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; enable paging in the cr0 register
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax


    mov eax, cr0
    and ax, 0xFFFB		;clear coprocessor emulation CR0.EM
    or ax, 0x2			;set coprocessor monitoring  CR0.MP
    mov cr0, eax
    mov eax, cr4
    or ax, 3 << 9		;set CR4.OSFXSR and CR4.OSXMMEXCPT at the same time
    mov cr4, eax

    ret






section .bss
align 4096
global p4_table
p4_table resq 512
global p3_table
p3_table resq 512
global p2_table
p2_table resq 512
global p1_table
p1_table resq 512


section .data
align 4096
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


section .data
global gdt64
global gdt64.tss
global gdt64.pointer
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