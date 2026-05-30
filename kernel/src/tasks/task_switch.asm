; task_switch(uint64_t *old_rsp, uint64_t new_rsp)
;   rdi = pointer to store the current rsp into
;   rsi = new rsp to switch to
;
; Saves callee-saved registers on the current stack, swaps rsp,
; then restores callee-saved registers from the new stack and returns
; into the new task (via ret).

global task_switch
global task_trampoline
extern task_exit

section .text

task_trampoline:
    sti
    sub rsp, 8
    call rbx
    call task_exit

task_switch:
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    mov  [rdi], rsp     ; save current stack pointer
    mov  rsp,   rsi     ; switch to new stack

    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rbp
    pop  rbx
    ret
