#include "tasks/mouse_task.h"

#include "input/input_subsys.h"

#include "drivers/usb/xhci.h"
#include "drivers/hpet.h"

extern semaphore_t mouse_sem;

void mouse_task(void)
{
    while (1)
    {
        sem_wait(&mouse_sem);

        int32_t mouse_dx = __atomic_exchange_n(&mouse_state.pending_dx, 0, __ATOMIC_SEQ_CST);
        int32_t mouse_dy = __atomic_exchange_n(&mouse_state.pending_dy, 0, __ATOMIC_SEQ_CST);
        int32_t mouse_dwheel = __atomic_exchange_n(&mouse_state.pending_wheel, 0, __ATOMIC_SEQ_CST);

        if(mouse_dx || mouse_dy)
        {
            input_mouse_move(mouse_dx, mouse_dy);
        }

        if(mouse_dwheel)
            input_mouse_wheel(mouse_dwheel);

        uint8_t pressed = __atomic_exchange_n(&mouse_state.buttons_pressed, 0, __ATOMIC_SEQ_CST);
        uint8_t released = __atomic_exchange_n(&mouse_state.buttons_released, 0, __ATOMIC_SEQ_CST);

        if(pressed & (1 << 0)){
            input_mouse_button(0, true);
        }
        if(released & (1 << 0)){
            input_mouse_button(0, false);
        }

        if(pressed & (1 << 1)){
            input_mouse_button(1, true);
        }
        if(released & (1 << 1)){
            input_mouse_button(1, false);
        }

        if(pressed & (1 << 2)){
            input_mouse_button(2, true);
        }
        if(released & (1 << 2)){
            input_mouse_button(2, false);
        }
    }
}