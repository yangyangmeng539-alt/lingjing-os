#include "platform.h"
#include "screen.h"
#include "timer.h"
#include "keyboard.h"
#include "system.h"

static int platform_ready = 0;
static int platform_init_done = 0;

static int platform_output_ready = 0;
static int platform_input_ready = 0;
static int platform_timer_ready = 0;
static int platform_power_ready = 0;

static int platform_str_equal(const char* a, const char* b) {
    int i = 0;

    if (a == 0 || b == 0) {
        return 0;
    }

    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }

        i++;
    }

    return a[i] == '\0' && b[i] == '\0';
}

void platform_init(void) {
    platform_output_ready = 1;
    platform_input_ready = 1;
    platform_timer_ready = 1;
    platform_power_ready = 1;

    platform_ready = 1;
    platform_init_done = 1;
}

void platform_print(const char* text) {
    screen_print(text);
}

void platform_put_char(char c) {
    screen_put_char(c);
}

void platform_clear(void) {
    screen_clear();
}

char platform_read_char(void) {
    return keyboard_read_char();
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

void platform_sleep(unsigned int seconds) {
    timer_sleep(seconds);
}

void platform_reboot(void) {
    system_reboot();
}

void platform_halt(void) {
    system_halt();
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

    platform_print("  init:    ");
    platform_print(platform_init_done ? "done\n" : "pending\n");

    platform_print("  ready:   ");
    platform_print(platform_ready ? "yes\n" : "no\n");
}

void platform_check(void) {
    platform_print("Platform check:\n");

    platform_print("  init:    ");
    platform_print(platform_init_done ? "ok\n" : "broken\n");

    platform_print("  output:  ");
    platform_print(platform_output_ready ? "ok\n" : "broken\n");

    platform_print("  display: ");
    platform_print(platform_output_ready ? "ok\n" : "broken\n");

    platform_print("  timer:   ");
    platform_print(platform_timer_ready ? "ok\n" : "broken\n");

    platform_print("  input:   ");
    platform_print(platform_input_ready ? "ok\n" : "broken\n");

    platform_print("  power:   ");
    platform_print(platform_power_ready ? "ok\n" : "broken\n");

    platform_print("  ticks:   ");
    platform_print_uint(platform_ticks());
    platform_print("\n");

    platform_print("  result:  ");
    platform_print(platform_doctor_ok() ? "ok\n" : "broken\n");
}

void platform_deps(void) {
    platform_print("Platform dependencies:\n");

    platform_print("  output:  ");
    platform_print(platform_output_ready ? "platform\n" : "broken\n");

    platform_print("  input:   ");
    platform_print(platform_input_ready ? "platform\n" : "broken\n");

    platform_print("  timer:   ");
    platform_print(platform_timer_ready ? "platform\n" : "broken\n");

    platform_print("  power:   ");
    platform_print(platform_power_ready ? "platform\n" : "broken\n");

    platform_print("  backend: ");
    platform_print(platform_get_name());
    platform_print("\n");

    platform_print("  display: ");
    platform_print(platform_get_display());
    platform_print("\n");

    platform_print("  result:  ");
    platform_print(platform_doctor_ok() ? "ok\n" : "broken\n");
}

void platform_boot_info(void) {
    platform_print("Platform boot:\n");

    platform_print("  backend: ");
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

    platform_print("  init:    ");
    platform_print(platform_init_done ? "done\n" : "pending\n");

    platform_print("  ready:   ");
    platform_print(platform_ready ? "yes\n" : "no\n");
}

void platform_summary(void) {
    platform_print("Platform summary:\n");

    platform_print("  ");
    platform_print(platform_get_name());
    platform_print(" | ");

    platform_print(platform_get_display());
    platform_print(" | ");

    platform_print(platform_get_timer());
    platform_print(" | ");

    platform_print(platform_get_input());
    platform_print(" | ");

    platform_print(platform_doctor_ok() ? "ok\n" : "broken\n");
}

void platform_caps(void) {
    platform_print("Platform capabilities:\n");

    platform_print("  output: ");
    platform_print(platform_output_ready ? "ok\n" : "broken\n");

    platform_print("  input:  ");
    platform_print(platform_input_ready ? "ok\n" : "broken\n");

    platform_print("  timer:  ");
    platform_print(platform_timer_ready ? "ok\n" : "broken\n");

    platform_print("  power:  ");
    platform_print(platform_power_ready ? "ok\n" : "broken\n");

    platform_print("  result: ");
    platform_print(platform_doctor_ok() ? "ok\n" : "broken\n");
}

void platform_break(const char* capability) {
    if (platform_str_equal(capability, "output")) {
        platform_output_ready = 0;
        platform_print("platform capability broken: output\n");
        return;
    }

    if (platform_str_equal(capability, "input")) {
        platform_input_ready = 0;
        platform_print("platform capability broken: input\n");
        return;
    }

    if (platform_str_equal(capability, "timer")) {
        platform_timer_ready = 0;
        platform_print("platform capability broken: timer\n");
        return;
    }

    if (platform_str_equal(capability, "power")) {
        platform_power_ready = 0;
        platform_print("platform capability broken: power\n");
        return;
    }

    if (platform_str_equal(capability, "all")) {
        platform_output_ready = 0;
        platform_input_ready = 0;
        platform_timer_ready = 0;
        platform_power_ready = 0;
        platform_print("platform capability broken: all\n");
        return;
    }

    platform_print("unknown platform capability: ");
    platform_print(capability);
    platform_print("\n");
    platform_print("allowed: output, input, timer, power, all\n");
}

void platform_fix(const char* capability) {
    if (platform_str_equal(capability, "output")) {
        platform_output_ready = 1;
        platform_print("platform capability fixed: output\n");
        return;
    }

    if (platform_str_equal(capability, "input")) {
        platform_input_ready = 1;
        platform_print("platform capability fixed: input\n");
        return;
    }

    if (platform_str_equal(capability, "timer")) {
        platform_timer_ready = 1;
        platform_print("platform capability fixed: timer\n");
        return;
    }

    if (platform_str_equal(capability, "power")) {
        platform_power_ready = 1;
        platform_print("platform capability fixed: power\n");
        return;
    }

    if (platform_str_equal(capability, "all")) {
        platform_output_ready = 1;
        platform_input_ready = 1;
        platform_timer_ready = 1;
        platform_power_ready = 1;
        platform_print("platform capability fixed: all\n");
        return;
    }

    platform_print("unknown platform capability: ");
    platform_print(capability);
    platform_print("\n");
    platform_print("allowed: output, input, timer, power, all\n");
}

int platform_doctor_ok(void) {
    if (!platform_ready) {
        return 0;
    }

    if (!platform_init_done) {
        return 0;
    }

    if (!platform_output_ready) {
        return 0;
    }

    if (!platform_input_ready) {
        return 0;
    }

    if (!platform_timer_ready) {
        return 0;
    }

    if (!platform_power_ready) {
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