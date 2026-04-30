#include "user.h"
#include "platform.h"

#define USER_PROGRAM_MAX 4

static int user_init_done = 0;
static int user_ready = 0;
static int user_broken = 0;

static unsigned int user_program_count = 0;
static unsigned int user_entry_count = 0;

static const char* user_mode = "metadata prototype";

void user_init(void) {
    user_init_done = 1;
    user_ready = 1;
    user_broken = 0;

    user_program_count = USER_PROGRAM_MAX;
    user_entry_count = 0;
}

int user_is_ready(void) {
    if (!user_init_done) {
        return 0;
    }

    if (!user_ready) {
        return 0;
    }

    if (user_broken) {
        return 0;
    }

    if (user_program_count == 0) {
        return 0;
    }

    return 1;
}

int user_doctor_ok(void) {
    return user_is_ready();
}

const char* user_get_state(void) {
    return user_is_ready() ? "ready" : "broken";
}

const char* user_get_mode(void) {
    return user_mode;
}

unsigned int user_get_program_count(void) {
    return user_program_count;
}

unsigned int user_get_entry_count(void) {
    return user_entry_count;
}

void user_status(void) {
    platform_print("User mode layer:\n");

    platform_print("  state:      ");
    platform_print(user_get_state());
    platform_print("\n");

    platform_print("  mode:       ");
    platform_print(user_get_mode());
    platform_print("\n");

    platform_print("  programs:   ");
    platform_print_uint(user_get_program_count());
    platform_print("\n");

    platform_print("  entries:    ");
    platform_print_uint(user_get_entry_count());
    platform_print("\n");

    platform_print("  ring3:      disabled\n");
    platform_print("  syscall:    metadata-only\n");
}

void user_check(void) {
    platform_print("User mode check:\n");

    platform_print("  init:     ");
    platform_print(user_init_done ? "ok\n" : "missing\n");

    platform_print("  programs: ");
    platform_print(user_program_count > 0 ? "ok\n" : "missing\n");

    platform_print("  entry:    metadata\n");

    platform_print("  result:   ");
    platform_print(user_doctor_ok() ? "ok\n" : "broken\n");
}

void user_doctor(void) {
    platform_print("User mode doctor:\n");

    platform_print("  init:      ");
    platform_print(user_init_done ? "ok\n" : "missing\n");

    platform_print("  ready:     ");
    platform_print(user_ready ? "ok\n" : "bad\n");

    platform_print("  broken:    ");
    platform_print(user_broken ? "yes\n" : "no\n");

    platform_print("  programs:  ");
    platform_print_uint(user_program_count);
    platform_print("\n");

    platform_print("  entries:   ");
    platform_print_uint(user_entry_count);
    platform_print("\n");

    platform_print("  ring3:     disabled\n");
    platform_print("  result:    ");
    platform_print(user_doctor_ok() ? "ok\n" : "broken\n");
}

void user_break(void) {
    user_broken = 1;
    user_ready = 0;

    platform_print("user mode layer broken.\n");
}

void user_fix(void) {
    user_init_done = 1;
    user_ready = 1;
    user_broken = 0;

    if (user_program_count == 0) {
        user_program_count = USER_PROGRAM_MAX;
    }

    platform_print("user mode layer fixed.\n");
}