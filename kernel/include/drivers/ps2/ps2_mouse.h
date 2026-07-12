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