#ifndef __MOUSE_H
#define __MOUSE_H 

#include "common.h"

static int32_t mouse_x = 0;
static int32_t mouse_y = 0;

// Mouse Commands
#define MOUSE_CMD_RESET               0xF6
#define MOUSE_CMD_ENABLE_REPORTING    0xF4

void mouse_enable(void);
void mouse_handle_irq(void);

void mouse_write(uint8_t command);
uint8_t mouse_read(void) ;

#endif