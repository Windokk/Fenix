#pragma once

#include "common.h"

#include "tasks/task.h"

extern int32_t mouse_x, mouse_y;
extern int32_t mouse_wheel;
extern uint8_t mouse_buttons;

extern semaphore_t kb_sem;

typedef struct key_event_t{
    uint8_t scancode;
    char    ascii;     /* Translated character, 0 if non-printable */
    bool    released;  /* true = key-up event */
    bool    lshift;
    bool    rshift;
    bool    lctrl;
    bool    rctrl;
    bool    lalt;
    bool    ralt;
    bool    caps_lock;
} key_event_t;

void input_mouse_move(int8_t dx, int8_t dy);
void input_mouse_wheel(int8_t dw);
void input_mouse_button(uint8_t btn, bool pressed);
void input_key_event(key_event_t key_evt);

void input_mouse_get_pos(int32_t* x, int32_t* y);
bool input_key_event_read(key_event_t* out);
char input_key_char_read();
void input_key_modifiers_read(uint8_t* out);
bool input_key_available();

void init_input();