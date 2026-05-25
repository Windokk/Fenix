#pragma once

#include "common.h"

extern int32_t mouse_x;
extern int32_t mouse_y;

// Mouse Commands
#define MOUSE_CMD_RESET               0xFF
#define MOUSE_CMD_ENABLE_REPORTING    0xF4

void mouse_enable(void);
void mouse_handle_irq(void);

void mouse_write(uint8_t command);
uint8_t mouse_read(void);

uint8_t mouse_button_state(void);
int     mouse_left_pressed(void);
int     mouse_right_pressed(void);
int     mouse_middle_pressed(void);