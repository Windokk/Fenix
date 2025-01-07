#include "drivers/mouse.h"

#include "interrupts/pic.h"
#include "interrupts/idt.h"

#include "drivers/ps2.h"

#include "graphics/gfx.h"

#include "stdio.h"

void mouse_write(uint8_t command) {
    // Wait for the PS/2 controller to be ready to accept input
    if (!ps2_wait_input_ready()) {
        printf("PS/2 Controller not ready for input\n");
        return;
    }

    // Send the "Write to Mouse" command (0xD4) to the PS/2 controller
    outb(PORT_PS2_CMD, 0xD4);

    // Wait again for the PS/2 controller to be ready to accept input
    if (!ps2_wait_input_ready()) {
        printf("PS/2 Controller not ready to send command to mouse\n");
        return;
    }

    // Send the actual command/data to the mouse
    outb(PORT_PS2_RW_DATA, command);
}

uint8_t mouse_read(void) {
    // Wait for the PS/2 controller to have data ready to read
    if (!ps2_wait_output_ready()) {
        printf("PS/2 Controller not ready for output\n");
        return 0xFF; // Return an invalid value to indicate failure
    }

    // Read and return the byte from the PS/2 data port
    return inb(PORT_PS2_RW_DATA);
}

void mouse_enable(void)
{
    // Disable interrupts to avoid race conditions
    disable_interrupts();

    uint8_t ack;

    // Clear the PS/2 controller's output buffer
    ps2_flush_output_buffer();

    // Enable the second PS/2 port
    ps2_write_command(PS2_CMD_ENABLE_PORT2);  // 0xA8 for enabling the second PS/2 port

    // Reset the mouse
    ps2_write_command(0xD4);  // 0xD4: Command signaling the next byte is for the mouse
    ps2_write_data(MOUSE_CMD_RESET);  // 0xFF: Reset command for the mouse
    ack = ps2_read_data();

    if (ack != 0xFA) {
        printf("Mouse reset failed, ack: 0x%X\n", ack);
        enable_interrupts();  // Re-enable interrupts before returning
        return;
    }

    // Enable data reporting from the mouse
    ps2_write_command(0xD4);
    ps2_write_data(MOUSE_CMD_ENABLE_REPORTING);  // 0xF4: Enable data reporting
    ack = ps2_read_data();

    if (ack != 0xFA) {
        printf("Failed to enable mouse data reporting, ack: 0x%X\n", ack);
        enable_interrupts();  // Re-enable interrupts before returning
        return;
    }

    // Print success message
    printf("PS/2 mouse enabled successfully\n");

    // Unmask the mouse interrupt (IRQ12)
    pic_unmask_irq(PIC_PS2_MOUSE_INTERRUPT);

    // Re-enable interrupts
    enable_interrupts();

    printf("Mouse IRQ enabled\n");
}

static uint8_t mouse_packet[3];
static uint8_t mouse_index = 0;

void mouse_handle_irq(void) {

    gfx_draw_filled_rectangle(mouse_x, mouse_y, 10, 10, 0, 0, 0);

    // Check if the incoming byte is from the mouse
    uint8_t status = inb(0x64);
    if (!(status & 0x20)) {
        // Not mouse data, ignore it
        pic_send_eoi(PIC_PS2_MOUSE_INTERRUPT);
        return;
    }

    // Read the data byte from port 0x60
    uint8_t data = inb(0x60);

    // Store the byte in the packet buffer
    mouse_packet[mouse_index++] = data;

    // Process the packet if it is complete
    if (mouse_index == 3) {
        // Verify alignment
        if (!(mouse_packet[0] & 0x08)) {
            // Misaligned packet, discard
            mouse_index = 0;
            pic_send_eoi(PIC_PS2_MOUSE_INTERRUPT);
            return;
        }

        // Extract movement and button states
        int32_t delta_x = (mouse_packet[0] & 0x10) ? (mouse_packet[1] | 0xFFFFFF00) : mouse_packet[1];
        int32_t delta_y = (mouse_packet[0] & 0x20) ? (mouse_packet[2] | 0xFFFFFF00) : mouse_packet[2];
        uint8_t left_button = mouse_packet[0] & 0x01;
        uint8_t right_button = mouse_packet[0] & 0x02;
        uint8_t middle_button = mouse_packet[0] & 0x04;

        // Handle movement
        printf("Mouse moved: X=%d, Y=%d\n", delta_x, delta_y);

        mouse_x += delta_x;
        mouse_y -= delta_y;

        // Handle button states
        if (left_button) {
            //TODO : Implement left click
            printf("Left button pressed\n");
        }
        if (right_button) {
            //TODO : Implement right click
            printf("Right button pressed\n");
        }
        if (middle_button) {
            //TODO : Implement middle click
            printf("Middle button pressed\n");
        }

        // Reset packet index for the next packet
        mouse_index = 0;
    }

    printf("Mouse position: X=%d, Y=%d\n", mouse_x, mouse_y);

    
    gfx_draw_filled_rectangle(mouse_x, mouse_y, 10, 10, 255, 255, 255);

    // Send End of Interrupt (EOI) signal to the PIC
    pic_send_eoi(PIC_PS2_MOUSE_INTERRUPT);
}