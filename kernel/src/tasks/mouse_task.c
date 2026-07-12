#include "tasks/mouse_task.h"

#include "input/input_subsys.h"

#include "drivers/usb/xhci.h"

extern semaphore_t mouse_sem;

void mouse_task(void)
{
    while (1) {
        sem_wait(&mouse_sem);

        if(!mouse_state.dirty){
            continue;
        }

        if(mouse_state.buttons & (1 << 0)){
            input_mouse_button(0, true);
        }
        else{
            input_mouse_button(0, false);
        }

        if(mouse_state.buttons & (1 << 1)){
            input_mouse_button(1, true);
        }
        else{
            input_mouse_button(1, false);
        }

        if(mouse_state.buttons & (1 << 2)){
            input_mouse_button(2, true);
        }
        else{
            input_mouse_button(2, false);
        }

        input_mouse_move(mouse_state.pending_dx, mouse_state.pending_dy);

        input_mouse_wheel(mouse_state.pending_wheel);

        mouse_state.buttons = 0;
        mouse_state.pending_dx = 0;
        mouse_state.pending_dy = 0;
        mouse_state.pending_wheel = 0;
    }
}