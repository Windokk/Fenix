; C-defined functions that this code relies on
extern interrupt_handler



section .text
bits 64

%macro isr_stub_err 1
global vector%1
vector%1:
    cli
    ; Store the interrupt number in the highest four bytes of the error code
    ; This way we can always increment rsp by 8 before iretq and no memory is wasted.
    mov dword [rsp + 4], %1
    jmp isr_common_stub
%endmacro

%macro isr_stub_noerr 1
global vector%1
vector%1:
    cli
    ; For consistency with the err variant
    push qword 0
    mov dword [rsp + 4], %1
    jmp isr_common_stub
%endmacro

%macro isr_stub_irq 1
global vector%1
vector%1:
    cli
    push qword 0
    mov dword [rsp + 4], %1
    jmp isr_common_stub
%endmacro

%macro isr_stub_nop 1
global vector%1
vector%1:
    iretq
%endmacro


isr_common_stub:
    ; save registers
    
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov rax, cr2
    push rax
    
    ; call handler
    mov rdi, rsp
    call interrupt_handler

    add rsp, 8

    ; restore registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 8     ; Cleans up the pushed error code and pushed ISR number

    sti
    iretq

;===============================================================================
; Pre-defined vectors used in the x86-64 IDT (Long mode)
;
; Stack state:
;	-> push all system control registers
;   -> push all general purpose registers
;   -> push trap number
;   -> push error number - not all interrupts have one but this ensure I can use
;         a single C struct for that :)
; Killed registers:
;   None
;===============================================================================
isr_stub_noerr 0
isr_stub_noerr 1
isr_stub_noerr 2
isr_stub_noerr 3
isr_stub_noerr 4
isr_stub_noerr 5
isr_stub_noerr 6
isr_stub_noerr 7
isr_stub_err 8
isr_stub_noerr 9
isr_stub_err 10
isr_stub_err 11
isr_stub_err 12
isr_stub_err 13
isr_stub_err 14
isr_stub_noerr 15
isr_stub_noerr 16
isr_stub_err 17
isr_stub_noerr 18
isr_stub_noerr 19
isr_stub_noerr 20
isr_stub_noerr 21
isr_stub_noerr 22
isr_stub_noerr 23
isr_stub_noerr 24
isr_stub_noerr 25
isr_stub_noerr 26
isr_stub_noerr 27
isr_stub_noerr 28
isr_stub_noerr 29
isr_stub_err 30
isr_stub_noerr 31

isr_stub_irq 32
isr_stub_irq 33

isr_stub_irq 34
isr_stub_irq 35
isr_stub_irq 36
isr_stub_irq 37
isr_stub_irq 38
isr_stub_irq 39
isr_stub_irq 40
isr_stub_irq 41
isr_stub_irq 42
isr_stub_irq 43

isr_stub_irq 44

isr_stub_irq 64

isr_stub_irq 240

%assign i 0x41
%rep 0xAF          ; 0xEF - 0x41 + 1 = 175
    isr_stub_irq i
    %assign i i+1
%endrep

section .data
global msi_stubs
msi_stubs:
%assign i 0x41
%rep 0xAF
    dq vector%+i
    %assign i i+1
%endrep