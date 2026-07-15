#include "drivers/ps2/ps2_keyboard.h"

#include "input/input_subsys.h"

#include "interrupts/pic.h"
#include "interrupts/idt.h"

#include "drivers/ps2/ps2.h"

#include "stdio.h"

#include "cpu/smp.h"

// PS/2 Set 1 scancode => ASCII (unshifted / shifted)
// Index = scancode (0x00–0x58), value = [unshifted, shifted]

static const char scancode_map[2][89] = {
    /* unshifted */
    {
        0,    0,    '1',  '2',  '3',  '4',  '5',  '6',  /* 0x00–0x07 */
        '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t', /* 0x08–0x0F */
        'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',  /* 0x10–0x17 */
        'o',  'p',  '[',  ']',  '\n', 0,    'a',  's',  /* 0x18–0x1F */
        'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',  /* 0x20–0x27 */
        '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',  /* 0x28–0x2F */
        'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',  /* 0x30–0x37 */
        0,    ' ',  0,    0,    0,    0,    0,    0,    /* 0x38–0x3F */
        0,    0,    0,    0,    0,    0,    0,    '7',  /* 0x40–0x47 */
        '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',  /* 0x48–0x4F */
        '2',  '3',  '0',  '.',  0,    0,    0,    0,    /* 0x50–0x57 */
        0                                                  /* 0x58 */
    },
    /* shifted */
    {
        0,    0,    '!',  '@',  '#',  '$',  '%',  '^',  /* 0x00–0x07 */
        '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t', /* 0x08–0x0F */
        'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',  /* 0x10–0x17 */
        'O',  'P',  '{',  '}',  '\n', 0,    'A',  'S',  /* 0x18–0x1F */
        'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',  /* 0x20–0x27 */
        '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',  /* 0x28–0x2F */
        'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',  /* 0x30–0x37 */
        0,    ' ',  0,    0,    0,    0,    0,    0,    /* 0x38–0x3F */
        0,    0,    0,    0,    0,    0,    0,    '7',  /* 0x40–0x47 */
        '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',  /* 0x48–0x4F */
        '2',  '3',  '0',  '.',  0,    0,    0,    0,    /* 0x50–0x57 */
        0                                                  /* 0x58 */
    }
};

// Modifier / special key scancodes (Set 1 make codes)
#define SC_LSHIFT   0x2A
#define SC_RSHIFT   0x36
#define SC_CTRL     0x1D
#define SC_ALT      0x38
#define SC_CAPSLOCK 0x3A

// Release codes = make code | 0x80
#define SC_RELEASE  0x80

// Public API
void ps2_keyboard_enable(void) {
    disable_interrupts();
    pic_unmask_irq(PIC_KEYBOARD_INTERRUPT);
    enable_interrupts();
    KLOG_INFO("KEYBOARD","Keyboard enabled");
}

void ps2_keyboard_handle_irq(void) {
    for (int i = 0; i < PS2_TIMEOUT; i++) {
        if (inb(0x64) & PS2_STATUS_OUTPUT_FULL) {
            break;
        }
        else if(i == (PS2_TIMEOUT - 1)){
            return;
        }
        asm volatile("pause");
    }
    uint8_t raw = inb(0x60);

    bool released = (raw & SC_RELEASE) != 0;
    uint8_t sc    = raw & ~SC_RELEASE;

    uint8_t mods = 0;
    input_key_modifiers_read(&mods);

    bool lshift_held = mods & (1 << 6);
    bool rshift_held = mods & (1 << 5);
    bool lctrl_held = mods & (1 << 4);
    bool rctrl_held = mods & (1 << 3);
    bool lalt_held = mods & (1 << 2);
    bool ralt_held = mods & (1 << 1);
    bool caps_lock_on = mods & (1 << 0);

    /* Update modifier state */
    if (sc == SC_LSHIFT){
        lshift_held = !released;
    }
    else if(sc == SC_RSHIFT) {
        rshift_held = !released;
    } 
    else if (sc == SC_CTRL) {
        lctrl_held = !released;
        rctrl_held = !released;
    } 
    else if (sc == SC_ALT) {
        lalt_held = !released;
        ralt_held = !released;
    } 
    else if (sc == SC_CAPSLOCK && !released) {
        caps_lock_on = !caps_lock_on;
    }

    /* Determine ASCII value */
    char ascii = 0;
    if (sc < 89) {
        bool use_upper = (lshift_held || rshift_held) ^ caps_lock_on;
        ascii = scancode_map[use_upper ? 1 : 0][sc];
    }

    key_event_t ev = {
        .scancode  = sc,
        .ascii     = ascii,
        .released  = released,
        .lshift     = lshift_held,
        .rshift     = rshift_held,
        .lctrl      = lctrl_held,
        .rctrl      = rctrl_held,
        .lalt       = lalt_held,
        .ralt       = ralt_held,
        .caps_lock = caps_lock_on,
    };

    if (!released) {
        input_key_event(ev);
    }

    pic_send_eoi(PIC_KEYBOARD_INTERRUPT);
}