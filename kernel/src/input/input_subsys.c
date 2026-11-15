#include "input/input_subsys.h"

#include "cpu/smp.h"

#include "drivers/video.h"
#include "drivers/hpet.h"

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

uint32_t mouse_x, mouse_y;
int32_t mouse_wheel;
uint8_t mouse_buttons;

int32_t mouse_x_f = 0;
int32_t mouse_y_f = 0;

uint64_t mouse_last_time = 0;

semaphore_t kb_sem;

extern video_rect_t dirty_rect;

#define V_LOW 300
#define V_HIGH 3000
#define G_MIN 45875
#define G_MAX 126608

void init_input(){
    sem_init(&kb_sem, 0);
}

void input_mouse_move(int32_t dx, int32_t dy){

    uint64_t now = hpet_get_nanos();

    uint64_t dt = max(now - mouse_last_time, 1);

    mouse_last_time = now;

    uint64_t mouse_delta_pos = abs(dx) + abs(dy);

    uint64_t speed = mouse_delta_pos * 1000000000 / (dt == 0 ? 1 : dt);
    
    uint64_t gain = 65536;

    if(!(dt == 0 || dt > 100000000))
    {
        if(speed <= V_LOW)
            gain = G_MIN;
        else if(speed >= V_HIGH)
            gain = G_MAX;
        else
            gain = G_MIN + (G_MAX - G_MIN) * ((int64_t)speed - (int64_t)V_LOW) / ((int64_t)V_HIGH - (int64_t)V_LOW);
    }
    dirty_add(mouse_x,mouse_y,mouse_x+MOUSE_W,mouse_y+MOUSE_H);

    mouse_x_f = CLAMP(mouse_x_f + ((int64_t)dx * (int64_t)gain), 0, (int32_t)(video_info.width - 1) << 16);
    mouse_y_f = CLAMP(mouse_y_f + ((int64_t)dy * (int64_t)gain), 0, (int32_t)(video_info.height - 1) << 16);

    mouse_x = mouse_x_f >> 16;
    mouse_y = mouse_y_f >> 16;

    dirty_add(mouse_x,mouse_y,mouse_x+MOUSE_W,mouse_y+MOUSE_H);
    
    present();
}

void input_mouse_wheel(int32_t dw){
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