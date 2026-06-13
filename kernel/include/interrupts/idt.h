#pragma once

#include "common.h"

#define IDT_ENTRIES 256

typedef struct opts {
    uint8_t stack_OK  : 3;
    uint8_t ZEROS     : 5;
    uint8_t gate_type : 1;
    uint8_t ONES      : 3;
    uint8_t ZERO      : 1;
    uint8_t DPL       : 2;
    uint8_t present   : 1;
} __attribute__((packed)) opts_t;

typedef struct idt_gate {
    uint16_t ptr_low;
    uint16_t selector;
    opts_t opts;
    uint16_t ptr_mid;
    uint32_t ptr_high;

    uint8_t  _1_reserved : 8;
    uint8_t  _type       : 5;
    uint32_t _2_reserved : 19;
} __attribute__((packed)) idt_gate_t;

typedef struct idt_register {
    uint16_t length;
    uint64_t base;
} __attribute__((packed)) idt_register_t;

extern void vector0(void);
extern void vector1(void);
extern void vector2(void);
extern void vector3(void);
extern void vector4(void);
extern void vector5(void);
extern void vector6(void);
extern void vector7(void);
extern void vector8(void);
extern void vector10(void);
extern void vector11(void);
extern void vector12(void);
extern void vector13(void);
extern void vector14(void);
extern void vector16(void);
extern void vector17(void);
extern void vector18(void);
extern void vector19(void);
extern void vector20(void);
extern void vector21(void);
/* PIT interrupt */
extern void vector32(void);
/* keyboard interrupt */
extern void vector33(void);

extern void vector34(void);
extern void vector35(void);
extern void vector36(void);
extern void vector37(void);
extern void vector38(void);
extern void vector39(void);
extern void vector40(void);
extern void vector41(void);
extern void vector42(void);
extern void vector43(void);

/* Mouse  interrupt */
extern void vector44(void);

/* LAPIC Timer / Scheduler tick */
extern void vector64(void);

extern void vector240(void);

struct isr_ctx_struct {
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;
	uint64_t r8;
	uint64_t rbp;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rdx;
	uint64_t rcx;
	uint64_t rbx;
	uint64_t rax;

	// Contains error code and interrupt number for exceptions
	// Contains syscall number for syscalls
	// Contains just the interrupt number otherwise
	uint32_t error_code;
	uint32_t int_no;
	// Interrupt stack frame
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
};
typedef struct isr_ctx_struct isr_ctx_t;

void enable_interrupts();

uintptr_t disable_interrupts();

void restore_interrupts(uintptr_t interrupts_state);

void idt_init();

void load_idt();