#include "user.h"
#include "platform.h"

#define USER_PROGRAM_MAX 4

typedef struct user_program {
    unsigned int id;
    const char* name;
    const char* state;
    unsigned int entry;
} user_program_t;

static user_program_t user_programs_table[USER_PROGRAM_MAX];

static int user_init_done = 0;
static int user_ready = 0;
static int user_broken = 0;

static unsigned int user_program_count = 0;
static unsigned int user_entry_count = 0;
static unsigned int user_metadata_checks = 0;

static const char* user_mode = "metadata prototype";

static void user_programs_reset(void) {
    user_programs_table[0].id = 0;
    user_programs_table[0].name = "init";
    user_programs_table[0].state = "metadata";
    user_programs_table[0].entry = 0x00000000;

    user_programs_table[1].id = 1;
    user_programs_table[1].name = "shell-user";
    user_programs_table[1].state = "metadata";
    user_programs_table[1].entry = 0x00000000;

    user_programs_table[2].id = 2;
    user_programs_table[2].name = "intent-user";
    user_programs_table[2].state = "metadata";
    user_programs_table[2].entry = 0x00000000;

    user_programs_table[3].id = 3;
    user_programs_table[3].name = "reserved";
    user_programs_table[3].state = "reserved";
    user_programs_table[3].entry = 0x00000000;

    user_program_count = USER_PROGRAM_MAX;
    user_entry_count = 3;
}

void user_init(void) {
    user_init_done = 1;
    user_ready = 1;
    user_broken = 0;

    user_metadata_checks = 0;
    user_programs_reset();
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

    if (user_entry_count == 0) {
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

    platform_print("  checks:     ");
    platform_print_uint(user_metadata_checks);
    platform_print("\n");

    platform_print("  ring3:      disabled\n");
    platform_print("  syscall:    metadata-only\n");
}

void user_check(void) {
    user_metadata_checks++;

    platform_print("User mode check:\n");

    platform_print("  init:     ");
    platform_print(user_init_done ? "ok\n" : "missing\n");

    platform_print("  programs: ");
    platform_print(user_program_count > 0 ? "ok\n" : "missing\n");

    platform_print("  entries:  ");
    platform_print(user_entry_count > 0 ? "ok\n" : "missing\n");

    platform_print("  ring3:    disabled\n");

    platform_print("  result:   ");
    platform_print(user_doctor_ok() ? "ok\n" : "broken\n");
}

void user_programs(void) {
    platform_print("User programs:\n");

    for (int i = 0; i < USER_PROGRAM_MAX; i++) {
        platform_print("  ");
        platform_print_uint(user_programs_table[i].id);
        platform_print("    ");
        platform_print(user_programs_table[i].name);
        platform_print("    ");
        platform_print(user_programs_table[i].state);
        platform_print("    entry ");
        platform_print_hex32(user_programs_table[i].entry);
        platform_print("\n");
    }
}

void user_entries(void) {
    platform_print("User entries:\n");

    for (int i = 0; i < USER_PROGRAM_MAX; i++) {
        if (user_programs_table[i].state[0] == 'm') {
            platform_print("  ");
            platform_print_uint(user_programs_table[i].id);
            platform_print("    ");
            platform_print(user_programs_table[i].name);
            platform_print("    entry ");
            platform_print_hex32(user_programs_table[i].entry);
            platform_print("\n");
        }
    }
}

void user_stats(void) {
    platform_print("User stats:\n");

    platform_print("  programs: ");
    platform_print_uint(user_program_count);
    platform_print("\n");

    platform_print("  entries:  ");
    platform_print_uint(user_entry_count);
    platform_print("\n");

    platform_print("  checks:   ");
    platform_print_uint(user_metadata_checks);
    platform_print("\n");

    platform_print("  ring3:    disabled\n");
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

    platform_print("  checks:    ");
    platform_print_uint(user_metadata_checks);
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

    if (user_program_count == 0 || user_entry_count == 0) {
        user_programs_reset();
    }

    platform_print("user mode layer fixed.\n");
}