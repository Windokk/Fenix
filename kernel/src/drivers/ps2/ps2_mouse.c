#include "drivers/ps2/ps2_mouse.h"

#include "input/input_subsys.h"

#include "interrupts/pic.h"
#include "interrupts/idt.h"

#include "drivers/ps2/ps2.h"
#include "drivers/video.h"

#include "graphics/gfx.h"

#include "stdio.h"

// ---------------------------------------------------------------------------

void ps2_mouse_write(uint8_t command) {
    if (!ps2_wait_input_ready()) {
        KLOG_ERROR("PS/2", "Controller not ready for input");
        return;
    }
    outb(PORT_PS2_CMD, 0xD4);
    if (!ps2_wait_input_ready()) {
        KLOG_ERROR("PS/2", "Controller not ready to forward to mouse");
        return;
    }
    outb(PORT_PS2_RW_DATA, command);
}

uint8_t ps2_mouse_read(void) {
    if (!ps2_wait_output_ready()) {
        KLOG_ERROR("PS/2", "Controller output not ready");
        return 0xFF;
    }
    return inb(PORT_PS2_RW_DATA);
}

void ps2_mouse_enable(void) {
    disable_interrupts();

    ps2_flush_output_buffer();
    ps2_write_command(PS2_CMD_ENABLE_PORT2);

    // Reset mouse
    ps2_write_command(0xD4);
    ps2_write_data(MOUSE_CMD_RESET);
    uint8_t ack = ps2_read_data();
    if (ack != 0xFA) {
        KLOG_ERROR("MOUSE","Mouse reset failed (ack=0x%X)", ack);
        enable_interrupts();
        return;
    }
    // Consume BAT complete (0xAA) and device ID (0x00)
    ps2_flush();

    // Enable data reporting
    ps2_write_command(0xD4);
    ps2_write_data(MOUSE_CMD_ENABLE_REPORTING);
    ack = ps2_read_data();
    if (ack != 0xFA) {
        KLOG_ERROR("MOUSE","Mouse enable reporting failed (ack=0x%X)", ack);
        enable_interrupts();
        return;
    }

    KLOG_INFO("MOUSE","Mouse enabled");

    pic_unmask_irq(PIC_PS2_MOUSE_INTERRUPT);
    enable_interrupts();
}

static uint8_t mouse_packet[3];
static uint8_t mouse_index = 0;

void ps2_mouse_handle_irq(void) {
    uint8_t status = inb(0x64);
    if (!(status & 0x20)) {
        pic_send_eoi(PIC_PS2_MOUSE_INTERRUPT);
        return;
    }

    uint8_t data = inb(0x60);

    // Validate alignment on first byte: bit 3 must always be set
    if (mouse_index == 0 && !(data & 0x08)) {
        pic_send_eoi(PIC_PS2_MOUSE_INTERRUPT);
        return;
    }

    mouse_packet[mouse_index++] = data;

    if (mouse_index == 3) {
        mouse_index = 0;

        int32_t delta_x = (mouse_packet[0] & 0x10)
                          ? (int32_t)(mouse_packet[1] | 0xFFFFFF00)
                          : (int32_t)mouse_packet[1];
        int32_t delta_y = (mouse_packet[0] & 0x20)
                          ? (int32_t)(mouse_packet[2] | 0xFFFFFF00)
                          : (int32_t)mouse_packet[2];

        input_mouse_move(delta_x, delta_y);

        uint8_t mouse_buttons = mouse_packet[0] & 0x07;

        if(mouse_buttons & (1 << 0)){
            input_mouse_button(0, true);
        }
        else{
            input_mouse_button(0, false);
        }


        if(mouse_buttons & (1 << 1)){
            input_mouse_button(1, true);
        }
        else{
            input_mouse_button(1, false);
        }


        if(mouse_buttons & (1 << 2)){
            input_mouse_button(2, true);
        }
        else{
            input_mouse_button(2, false);
        }
    }

    pic_send_eoi(PIC_PS2_MOUSE_INTERRUPT);
}
