#include "drivers/mouse.h"

#include "interrupts/pic.h"
#include "interrupts/idt.h"

#include "drivers/ps2.h"

#include "graphics/gfx.h"
#include "drivers/video.h"

#include "stdio.h"

int32_t mouse_x = 0;
int32_t mouse_y = 0;

static uint8_t mouse_buttons = 0;

// ---------------------------------------------------------------------------
// Arrow cursor (16 wide x 16 tall): 0=transparent, 1=white, 2=black outline
// ---------------------------------------------------------------------------
#define CURSOR_W 16
#define CURSOR_H 16

static const uint8_t cursor_bitmap[CURSOR_H][CURSOR_W] = {
    {1,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,2,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,2,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,2,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,2,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,2,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,2,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,1,2,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,1,1,2,0,0,0,0,0,0},
    {1,1,1,1,1,2,2,2,2,2,0,0,0,0,0,0},
    {1,1,2,1,1,2,0,0,0,0,0,0,0,0,0,0},
    {1,2,2,2,1,1,2,0,0,0,0,0,0,0,0,0},
    {2,2,0,2,1,1,2,2,0,0,0,0,0,0,0,0},
    {0,0,0,0,2,1,1,2,0,0,0,0,0,0,0,0},
    {0,0,0,0,2,1,1,2,0,0,0,0,0,0,0,0},
    {0,0,0,0,2,2,2,2,0,0,0,0,0,0,0,0}
};

static uint32_t cursor_bg[CURSOR_H * CURSOR_W];
static int32_t  cursor_saved_x = -1;
static int32_t  cursor_saved_y = -1;

static void cursor_save_bg(int32_t x, int32_t y) {
    uint32_t *fb = (uint32_t *)video_info.virt_addr;
    for (int row = 0; row < CURSOR_H; row++) {
        for (int col = 0; col < CURSOR_W; col++) {
            int px = x + col, py = y + row;
            if (px >= 0 && px < video_info.width &&
                py >= 0 && py < video_info.height)
                cursor_bg[row * CURSOR_W + col] = fb[py * (video_info.pitch / (video_info.bits / 8)) + px];
            else
                cursor_bg[row * CURSOR_W + col] = 0;
        }
    }
    cursor_saved_x = x;
    cursor_saved_y = y;
}

static void cursor_restore_bg(void) {
    if (cursor_saved_x < 0) return;
    uint32_t *fb = (uint32_t *)video_info.virt_addr;
    for (int row = 0; row < CURSOR_H; row++) {
        for (int col = 0; col < CURSOR_W; col++) {
            int px = cursor_saved_x + col, py = cursor_saved_y + row;
            if (px >= 0 && px < video_info.width &&
                py >= 0 && py < video_info.height)
                fb[py * (video_info.pitch / (video_info.bits / 8)) + px] = cursor_bg[row * CURSOR_W + col];
        }
    }
}

static void cursor_draw(int32_t x, int32_t y) {
    uint32_t *fb = (uint32_t *)video_info.virt_addr;
    for (int row = 0; row < CURSOR_H; row++) {
        for (int col = 0; col < CURSOR_W; col++) {
            uint8_t pixel = cursor_bitmap[row][col];
            if (pixel == 0) continue;
            int px = x + col, py = y + row;
            if (px < 0 || px >= video_info.width ||
                py < 0 || py >= video_info.height) continue;
            fb[py * (video_info.pitch / (video_info.bits / 8)) + px] = (pixel == 1) ? 0xFFFFFF : 0x000000;
        }
    }
}

// ---------------------------------------------------------------------------

void mouse_write(uint8_t command) {
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

uint8_t mouse_read(void) {
    if (!ps2_wait_output_ready()) {
        KLOG_ERROR("PS/2", "Controller output not ready");
        return 0xFF;
    }
    return inb(PORT_PS2_RW_DATA);
}

void mouse_enable(void) {
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

void mouse_handle_irq(void) {
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

        mouse_buttons = mouse_packet[0] & 0x07;

        int32_t new_x = mouse_x + delta_x;
        int32_t new_y = mouse_y - delta_y;

        if (new_x < 0) new_x = 0;
        if (new_y < 0) new_y = 0;
        if (new_x >= (int32_t)video_info.width)  new_x = video_info.width  - 1;
        if (new_y >= (int32_t)video_info.height) new_y = video_info.height - 1;

        cursor_restore_bg();

        mouse_x = new_x;
        mouse_y = new_y;

        cursor_save_bg(mouse_x, mouse_y);
        cursor_draw(mouse_x, mouse_y);
    }

    pic_send_eoi(PIC_PS2_MOUSE_INTERRUPT);
}

uint8_t mouse_button_state(void)  { return mouse_buttons; }
int     mouse_left_pressed(void)  { return (mouse_buttons & 0x01) != 0; }
int     mouse_right_pressed(void) { return (mouse_buttons & 0x02) != 0; }
int     mouse_middle_pressed(void){ return (mouse_buttons & 0x04) != 0; }
