#include "drivers/serial.h"

void init_serial() {
    outb(COM1 + 1, 0x00);    // Disable all interrupts
    outb(COM1 + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(COM1 + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(COM1 + 1, 0x00);    //                  (hi byte)
    outb(COM1 + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(COM1 + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(COM1 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

int serial_received() {
    return inb(COM1 + 5) & 1;
}

char read_serial() {
    while (serial_received() == 0);

    return inb(COM1);
}

int is_transmit_empty() {
    return inb(COM1 + 5) & 0x20;
}

void write_serial(char a) {
    while (is_transmit_empty() == 0);

    outb(COM1, a);
}

void serial_print(const char *str) {
    while (*str) {
        write_serial(*str++);
    }
}

void int_to_ascii(int value, char* str, int base) {
    char *p = str;
    char *p1, *p2;
    int tmp_value;

    if (value < 0 && base == 10) {
        *p++ = '-';
        value = -value;
    }

    p1 = p;

    do {
        tmp_value = value;
        value /= base;
        *p++ = "0123456789ABCDEF"[tmp_value - value * base];
    } while (value);

    *p = '\0';

    p2 = p - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }
}

void hex_to_ascii64(uint64_t value, char* str) {
    const char *hex_digits = "0123456789ABCDEF";
    char *p = str;
    int shift = 64 - 4;
    int started = 0;

    for (; shift >= 0; shift -= 4) {
        char digit = (value >> shift) & 0xF;
        if (digit != 0 || started) {
            *p++ = hex_digits[digit];
            started = 1;
        }
    }

    if (!started) {
        *p++ = '0';
    }

    *p = '\0';
}

void debug_print(const char *msg) {
    serial_print(msg);
}