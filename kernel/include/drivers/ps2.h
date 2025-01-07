#ifndef __PS2_H
#define __PS2_H

#include "common.h"
#include "drivers/serial.h"


// PS2 IO Ports
#define PORT_PS2_CMD       0x64  // PS/2 controller command port
#define PORT_PS2_DATA      0x60  // PS/2 controller data port
#define PORT_PS2_RW_DATA   0x60  // PS/2 Controller data port

#define PS2_STATUS_OUTPUT_FULL  0x01  // Output buffer status
#define PS2_STATUS_INPUT_FULL   0x02  // Input buffer status

// Timeouts (arbitrary, adjust as needed for your system)
#define PS2_TIMEOUT 100000

// PS2 Commands
#define PS2_CMD_READ_CFG   0x20
#define PS2_CMD_WRITE_CFG  0x60
#define PS2_CMD_SELF_TEST  0xAA
#define PS2_CMD_TEST_PORT1 0xAB
#define PS2_CMD_TEST_PORT2 0xA9
#define PS2_CMD_DISABLE_PORT1 0xAD
#define PS2_CMD_DISABLE_PORT2 0xA7
#define PS2_CMD_ENABLE_PORT1  0xAE
#define PS2_CMD_ENABLE_PORT2  0xA8
#define PS2_CMD_FLUSH_OUTBUF 0xD1  // Not used but shown for reference

// PS2 Responses
#define PS2_RESP_TEST_PASS 0x55
#define PS2_RESP_ACK       0xFA
#define PS2_RESP_TEST_FAIL 0xFC

void io_wait(void);

bool ps2_wait_input_ready(void);

bool ps2_wait_output_ready(void);

void ps2_flush_output_buffer(void);

void ps2_disable_ports(void);

void ps2_flush(void);

uint8_t ps2_read_config(void);

void ps2_write_config(uint8_t config);

bool ps2_self_test(void);

bool ps2_has_second_channel(void);

bool ps2_test_port1(void);

bool ps2_test_port2(void);

void ps2_enable_ports(bool enable_port1, bool enable_port2);

void ps2_init(void);

void ps2_write_command(uint8_t command);

void ps2_write_data(uint8_t data);

uint8_t ps2_read_data(void);


#endif