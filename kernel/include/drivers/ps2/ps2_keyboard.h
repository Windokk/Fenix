#pragma once

#include "common.h"
#include "tasks/task.h"

/* Modifier flag bits returned by keyboard_modifiers() */
#define KB_MOD_SHIFT  (1 << 0)
#define KB_MOD_CTRL   (1 << 1)
#define KB_MOD_ALT    (1 << 2)
#define KB_MOD_CAPS   (1 << 3)

typedef struct {
    uint8_t scancode;  /* PS/2 Set 1 make code (release bit stripped) */
    char    ascii;     /* Translated character, 0 if non-printable */
    bool    released;  /* true = key-up event */
    bool    shift;
    bool    ctrl;
    bool    alt;
    bool    caps_lock;
} key_event_t;

extern task_t *kb_waiter;

void    ps2_keyboard_enable(void);
void    ps2_keyboard_handle_irq(void);

bool    ps2_keyboard_has_input(void);
bool    ps2_keyboard_get_event(key_event_t *out); /* non-blocking pop */
char    ps2_keyboard_getchar(void);               /* blocking — spins until a key is available */
uint8_t ps2_keyboard_modifiers(void);             /* current modifier bitmask */