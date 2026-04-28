#include "screen.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

static unsigned short* const VGA_BUFFER = (unsigned short*)VGA_MEMORY;
static int cursor_row = 0;
static int cursor_col = 0;

static unsigned short vga_entry(char c, unsigned char color) {
    return (unsigned short)c | ((unsigned short)color << 8);
}

void screen_clear(void) {
    unsigned char color = 0x0F;

    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[y * VGA_WIDTH + x] = vga_entry(' ', color);
        }
    }

    cursor_row = 0;
    cursor_col = 0;
}

void screen_put_char(char c) {
    unsigned char color = 0x0F;

    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            VGA_BUFFER[cursor_row * VGA_WIDTH + cursor_col] = vga_entry(' ', color);
        }
    } else {
        VGA_BUFFER[cursor_row * VGA_WIDTH + cursor_col] = vga_entry(c, color);
        cursor_col++;

        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
    }

    if (cursor_row >= VGA_HEIGHT) {
        screen_clear();
    }
}

void screen_print(const char* text) {
    for (int i = 0; text[i] != '\0'; i++) {
        screen_put_char(text[i]);
    }
}
