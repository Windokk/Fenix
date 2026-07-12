#include "input/input_subsys.h"

#include "cpu/smp.h"

#include "drivers/video.h"

#include "graphics/gfx.h"

#include "stdio.h"

// Ring buffer
#define KB_BUF_SIZE 64

static key_event_t kb_buffer[KB_BUF_SIZE];
static uint8_t     kb_head    = 0;
static uint8_t     kb_tail    = 0;
task_t            *kb_waiter  = NULL;

static bool left_shift_held     = false;
static bool right_shift_held    = false;
static bool left_ctrl_held      = false;
static bool right_ctrl_held     = false;
static bool left_alt_held       = false;
static bool right_alt_held      = false;
static bool caps_lock_on        = false;

int32_t mouse_x, mouse_y;
int32_t mouse_wheel;
uint8_t mouse_buttons;

semaphore_t kb_sem;

void init_input(){
    sem_init(&kb_sem, 0);
}

void input_mouse_move(int8_t dx, int8_t dy){

    mouse_x = CLAMP(mouse_x + dx, 0, video_info.width);
    mouse_y = CLAMP(mouse_y + dy, 0, video_info.height);

    gfx_restore_cursor_bg();

    gfx_save_cursor_bg(mouse_x, mouse_y);
    gfx_draw_cursor(mouse_x, mouse_y);
}

void input_mouse_wheel(int8_t dw){
    mouse_wheel = mouse_wheel + dw;
}

void input_mouse_button(uint8_t btn, bool pressed){
    if(pressed){
        mouse_buttons |= (1 << btn);
    }
    else{
        mouse_buttons &= ~(1 << btn);
    }
}

void input_key_event(key_event_t key_evt){
    
    left_shift_held = key_evt.lshift;
    right_shift_held = key_evt.rshift;
    left_ctrl_held = key_evt.lctrl;
    right_ctrl_held = key_evt.rctrl;
    left_alt_held = key_evt.lalt;
    right_alt_held = key_evt.ralt;
    caps_lock_on = key_evt.caps_lock;

    if (!key_evt.released) {
        uint8_t next = (kb_head + 1) % KB_BUF_SIZE;
        if (next == kb_tail) return; /* full — drop oldest */
        kb_buffer[kb_head] = key_evt;
        kb_head = next;
        sem_signal(&kb_sem);
    }
}

void input_mouse_get_pos(int32_t* x, int32_t* y){
    *x = mouse_x;
    *y = mouse_y;
}

bool input_key_event_read(key_event_t *out){
    if (kb_head == kb_tail) return false;
    *out = kb_buffer[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUF_SIZE;
    return true;
}

char input_key_char_read(){
    key_event_t ev;
    while(1){
        sem_wait(&kb_sem);
        if(input_key_event_read(&ev))
            return ev.ascii;
    }
}

void input_key_modifiers_read(uint8_t *out) {
    *out = (left_shift_held << 6) |  (right_shift_held << 5) | (left_ctrl_held << 4) | 
        (right_ctrl_held << 3) | (left_alt_held << 2) | (right_alt_held << 1) | (caps_lock_on << 0);
}

bool input_key_available(){
    return kb_head != kb_tail;
}