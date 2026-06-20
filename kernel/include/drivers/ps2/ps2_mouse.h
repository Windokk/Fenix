#pragma once

#include "common.h"

extern int32_t mouse_x;
extern int32_t mouse_y;

// Mouse Commands
#define MOUSE_CMD_RESET               0xFF
#define MOUSE_CMD_ENABLE_REPORTING    0xF4

void ps2_mouse_enable(void);
void ps2_mouse_handle_irq(void);

void ps2_mouse_write(uint8_t command);
uint8_t ps2_mouse_read(void);

uint8_t ps2_mouse_button_state(void);
int     ps2_mouse_left_pressed(void);
int     ps2_mouse_right_pressed(void);
int     ps2_mouse_middle_pressed(void);