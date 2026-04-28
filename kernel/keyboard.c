#include "keyboard.h"
#include "idt.h"
#include "io.h"

#define KEYBOARD_BUFFER_SIZE 128

static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static int keyboard_read_index = 0;
static int keyboard_write_index = 0;

static char scancode_to_char(unsigned char scancode) {
    static char map[128] = {
        0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
        '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
        0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
        0, '\\','z','x','c','v','b','n','m',',','.','/',
        0, '*', 0, ' '
    };

    if (scancode < 128) {
        return map[scancode];
    }

    return 0;
}

static void keyboard_push_char(char c) {
    int next_index = (keyboard_write_index + 1) % KEYBOARD_BUFFER_SIZE;

    if (next_index == keyboard_read_index) {
        return;
    }

    keyboard_buffer[keyboard_write_index] = c;
    keyboard_write_index = next_index;
}

char keyboard_read_char(void) {
    if (keyboard_read_index == keyboard_write_index) {
        return 0;
    }

    char c = keyboard_buffer[keyboard_read_index];
    keyboard_read_index = (keyboard_read_index + 1) % KEYBOARD_BUFFER_SIZE;

    return c;
}

static void keyboard_interrupt_handler(registers_t* regs) {
    (void)regs;

    unsigned char scancode = inb(0x60);

    if ((scancode & 0x80) != 0) {
        return;
    }

    char c = scancode_to_char(scancode);

    if (c != 0) {
        keyboard_push_char(c);
    }
}

void keyboard_init(void) {
    keyboard_read_index = 0;
    keyboard_write_index = 0;

    register_interrupt_handler(33, keyboard_interrupt_handler);
}