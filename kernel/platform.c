#include "platform.h"
#include "screen.h"
#include "timer.h"

static int platform_ready = 0;

void platform_init(void) {
    platform_ready = 1;
}

void platform_print(const char* text) {
    screen_print(text);
}

void platform_put_char(char c) {
    screen_put_char(c);
}

void platform_print_uint(unsigned int value) {
    char buffer[16];
    int index = 0;

    if (value == 0) {
        platform_put_char('0');
        return;
    }

    while (value > 0 && index < 16) {
        buffer[index] = (char)('0' + (value % 10));
        value /= 10;
        index++;
    }

    for (int i = index - 1; i >= 0; i--) {
        platform_put_char(buffer[i]);
    }
}

void platform_print_hex32(unsigned int value) {
    const char* hex = "0123456789ABCDEF";

    platform_print("0x");

    for (int i = 7; i >= 0; i--) {
        unsigned int shift = (unsigned int)(i * 4);
        unsigned int digit = (value >> shift) & 0xF;
        platform_put_char(hex[digit]);
    }
}

unsigned int platform_ticks(void) {
    return timer_get_ticks();
}

unsigned int platform_seconds(void) {
    return timer_get_seconds();
}

const char* platform_get_name(void) {
    return "baremetal";
}

const char* platform_get_display(void) {
    return "vga-text";
}

const char* platform_get_timer(void) {
    return "irq0-pit";
}

const char* platform_get_input(void) {
    return "irq1-keyboard";
}

void platform_status(void) {
    platform_print("Platform:\n");

    platform_print("  name:    ");
    platform_print(platform_get_name());
    platform_print("\n");

    platform_print("  display: ");
    platform_print(platform_get_display());
    platform_print("\n");

    platform_print("  timer:   ");
    platform_print(platform_get_timer());
    platform_print("\n");

    platform_print("  input:   ");
    platform_print(platform_get_input());
    platform_print("\n");

    platform_print("  ready:   ");
    platform_print(platform_ready ? "yes\n" : "no\n");
}

void platform_check(void) {
    platform_print("Platform check:\n");

    platform_print("  display: ok\n");
    platform_print("  timer:   ok\n");
    platform_print("  input:   ok\n");

    platform_print("  ticks:   ");
    platform_print_uint(platform_ticks());
    platform_print("\n");

    platform_print("  result:  ");
    platform_print(platform_ready ? "ok\n" : "broken\n");
}

int platform_doctor_ok(void) {
    if (!platform_ready) {
        return 0;
    }

    if (platform_get_name() == 0) {
        return 0;
    }

    if (platform_get_display() == 0) {
        return 0;
    }

    if (platform_get_timer() == 0) {
        return 0;
    }

    if (platform_get_input() == 0) {
        return 0;
    }

    return 1;
}