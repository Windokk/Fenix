#pragma once

#ifndef __SERIAL_H
#define __SERIAL_H

typedef _Bool bool;

// Define fixed-width unsigned types
typedef unsigned char      uint8_t;   // 8-bit unsigned
typedef unsigned short     uint16_t;  // 16-bit unsigned
typedef unsigned int       uint32_t;  // 32-bit unsigned
typedef unsigned long long uint64_t;  // 64-bit unsigned

// Define fixed-width signed types
typedef signed char        int8_t;    // 8-bit signed
typedef signed short       int16_t;   // 16-bit signed
typedef signed int         int32_t;   // 32-bit signed
typedef signed long long   int64_t;   // 64-bit signed

// Define max-width types
typedef unsigned long long uintmax_t; // Maximum-width unsigned type
typedef long long          intmax_t;  // Maximum-width signed type

// Define pointer-sized types (adapt to architecture)
typedef unsigned long      uintptr_t; // Unsigned integer of pointer size
typedef long               intptr_t;  // Signed integer of pointer size

typedef unsigned long size_t;

#define true 1
#define false 0

#define NULL ((void *)0)

#define COM1 0x3F8

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline void outp(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline void outd(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint16_t inp(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint32_t ind(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void init_serial();

int serial_received();

char read_serial();

int is_transmit_empty();

void write_serial(char a);

void serial_print(const char *str);

void int_to_ascii(int value, char* str, int base);

void hex_to_ascii64(uint64_t value, char* str);


void debug_print(const char *msg);

#endif