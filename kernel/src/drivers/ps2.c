#include "drivers/ps2.h"

#include "stdio.h"

void io_wait(void) {
    outb(0x80, 0x00);  // 0x80 is typically an unused I/O port
}

bool ps2_wait_input_ready(void) {
    for (int i = 0; i < PS2_TIMEOUT; i++) {
        if (!(inb(PORT_PS2_CMD) & PS2_STATUS_INPUT_FULL)) {
            return true;
        }
    }
    return false;  // Timeout
}

bool ps2_wait_output_ready(void) {
    for (int i = 0; i < PS2_TIMEOUT; i++) {
        if (inb(PORT_PS2_CMD) & PS2_STATUS_OUTPUT_FULL) {
            return true;
        }
    }
    return false;  // Timeout
}

void ps2_flush_output_buffer(void) {
    while (inb(PORT_PS2_CMD) & 0x01) {
        inb(PORT_PS2_DATA);  // Read and discard data
    }
}

void ps2_disable_ports(void) {
    outb(PORT_PS2_CMD, PS2_CMD_DISABLE_PORT1);
    ps2_wait_input_ready();
    outb(PORT_PS2_CMD, PS2_CMD_DISABLE_PORT2);
    ps2_wait_input_ready();
}

void ps2_flush(void) {
    ps2_flush_output_buffer();
}

uint8_t ps2_read_config(void) {
    outb(PORT_PS2_CMD, PS2_CMD_READ_CFG);
    ps2_wait_output_ready();
    return inb(PORT_PS2_DATA);
}

void ps2_write_config(uint8_t config) {
    outb(PORT_PS2_CMD, PS2_CMD_WRITE_CFG);
    ps2_wait_input_ready();
    outb(PORT_PS2_DATA, config);
    ps2_wait_input_ready();
}

bool ps2_self_test(void) {
    outb(PORT_PS2_CMD, PS2_CMD_SELF_TEST);
    ps2_wait_output_ready();
    return inb(PORT_PS2_DATA) == PS2_RESP_TEST_PASS;
}

bool ps2_has_second_channel(void) {
    outb(PORT_PS2_CMD, PS2_CMD_ENABLE_PORT2);
    ps2_wait_input_ready();
    uint8_t config = ps2_read_config();
    bool has_second_channel = !(config & 0x20);
    outb(PORT_PS2_CMD, PS2_CMD_DISABLE_PORT2);
    return has_second_channel;
}

bool ps2_test_port1(void) {
    outb(PORT_PS2_CMD, PS2_CMD_TEST_PORT1);
    ps2_wait_output_ready();
    return inb(PORT_PS2_DATA) == 0x00;
}

bool ps2_test_port2(void) {
    outb(PORT_PS2_CMD, PS2_CMD_TEST_PORT2);
    ps2_wait_output_ready();
    return inb(PORT_PS2_DATA) == 0x00;
}

void ps2_enable_ports(bool enable_port1, bool enable_port2) {
    if (enable_port1) {
        outb(PORT_PS2_CMD, PS2_CMD_ENABLE_PORT1);
        ps2_wait_input_ready();
        printf("Enabled PS2 Port 1\n");
    }
    if (enable_port2) {
        outb(PORT_PS2_CMD, PS2_CMD_ENABLE_PORT2);
        ps2_wait_input_ready();
        printf("Enabled PS2 Port 2\n");
    }
}

void ps2_init(void) {
    ps2_disable_ports();
    ps2_flush();

    // Configure controller
    uint8_t config = ps2_read_config();
    config &= ~0x03;  // Disable IRQ1 and IRQ12
    ps2_write_config(config);

    // Perform self-test
    if (!ps2_self_test()) {
        printf("PS/2 controller self-test failed\n");
        return;
    }

    // Check for second channel
    bool dual_channel = ps2_has_second_channel();
    printf("PS/2 controller dual-channel: %s\n", dual_channel ? "yes" : "no");

    // Test ports
    bool port1_working = ps2_test_port1();
    bool port2_working = dual_channel && ps2_test_port2();
    printf("Port 1: %s, Port 2: %s\n",
           port1_working ? "working" : "not working",
           port2_working ? "working" : "not working");

    // Enable working ports
    ps2_enable_ports(port1_working, port2_working);


    // Reconfigure interrupts if needed
    config = ps2_read_config();
    if (port1_working) config |= 0x01;  // Enable IRQ1
    if (port2_working) config |= 0x02;  // Enable IRQ12
    ps2_write_config(config);

    printf("PS/2 initialization complete.\n");
}


/**
 * Writes a command byte to the PS/2 controller command port.
 */
void ps2_write_command(uint8_t command) {
    if (!ps2_wait_input_ready()) {
        printf("Timeout waiting for PS/2 input buffer to be ready (command).\n");
        return;
    }
    outb(PORT_PS2_CMD, command);
}

/**
 * Writes a data byte to the PS/2 controller data port.
 */
void ps2_write_data(uint8_t data) {
    if (!ps2_wait_input_ready()) {
        printf("Timeout waiting for PS/2 input buffer to be ready (data).\n");
        return;
    }
    outb(PORT_PS2_DATA, data);
}

/**
 * Reads a data byte from the PS/2 controller data port.
 */
uint8_t ps2_read_data(void) {
    if (!ps2_wait_output_ready()) {
        printf("Timeout waiting for PS/2 output buffer to be ready.\n");
        return 0xFF;  // Return error code
    }
    return inb(PORT_PS2_DATA);
}
