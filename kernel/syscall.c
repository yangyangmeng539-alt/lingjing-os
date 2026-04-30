#include "syscall.h"
#include "platform.h"

#define SYSCALL_TABLE_MAX 8
#define SYS_PING   0
#define SYS_UPTIME 1
#define SYS_HEALTH 2

static int syscall_init_done = 0;
static int syscall_ready = 0;
static int syscall_broken = 0;

static unsigned int syscall_table_count = 0;
static unsigned int syscall_dispatch_count = 0;

static const char* syscall_mode = "metadata prototype";

void syscall_init(void) {
    syscall_init_done = 1;
    syscall_ready = 1;
    syscall_broken = 0;

    syscall_table_count = SYSCALL_TABLE_MAX;
    syscall_dispatch_count = 0;
}

int syscall_is_ready(void) {
    if (!syscall_init_done) {
        return 0;
    }

    if (!syscall_ready) {
        return 0;
    }

    if (syscall_broken) {
        return 0;
    }

    if (syscall_table_count == 0) {
        return 0;
    }

    return 1;
}

int syscall_doctor_ok(void) {
    return syscall_is_ready();
}

const char* syscall_get_state(void) {
    return syscall_is_ready() ? "ready" : "broken";
}

const char* syscall_get_mode(void) {
    return syscall_mode;
}

unsigned int syscall_get_table_count(void) {
    return syscall_table_count;
}

unsigned int syscall_get_dispatch_count(void) {
    return syscall_dispatch_count;
}

void syscall_dispatch(unsigned int id) {
    platform_print("Syscall dispatch:\n");

    if (!syscall_is_ready()) {
        platform_print("  result: blocked\n");
        platform_print("  reason: syscall layer not ready\n");
        return;
    }

    platform_print("  id:     ");
    platform_print_uint(id);
    platform_print("\n");

    if (id == SYS_PING) {
        syscall_dispatch_count++;

        platform_print("  name:   SYS_PING\n");
        platform_print("  return: pong\n");
        platform_print("  result: ok\n");
        return;
    }

    if (id == SYS_UPTIME) {
        syscall_dispatch_count++;

        platform_print("  name:   SYS_UPTIME\n");
        platform_print("  return: metadata-only\n");
        platform_print("  result: ok\n");
        return;
    }

    if (id == SYS_HEALTH) {
        syscall_dispatch_count++;

        platform_print("  name:   SYS_HEALTH\n");
        platform_print("  return: use health command\n");
        platform_print("  result: ok\n");
        return;
    }

    platform_print("  name:   unknown\n");
    platform_print("  result: unsupported\n");
}

void syscall_status(void) {
    platform_print("Syscall layer:\n");

    platform_print("  state:     ");
    platform_print(syscall_get_state());
    platform_print("\n");

    platform_print("  mode:      ");
    platform_print(syscall_get_mode());
    platform_print("\n");

    platform_print("  table:     ");
    platform_print_uint(syscall_get_table_count());
    platform_print("\n");

    platform_print("  dispatch:  ");
    platform_print_uint(syscall_get_dispatch_count());
    platform_print("\n");

    platform_print("  int 0x80:  disabled\n");
    platform_print("  user mode: disabled\n");
}

void syscall_check(void) {
    platform_print("Syscall check:\n");

    platform_print("  init:     ");
    platform_print(syscall_init_done ? "ok\n" : "missing\n");

    platform_print("  table:    ");
    platform_print(syscall_table_count > 0 ? "ok\n" : "missing\n");

    platform_print("  dispatch: metadata\n");

    platform_print("  result:   ");
    platform_print(syscall_doctor_ok() ? "ok\n" : "broken\n");
}

void syscall_doctor(void) {
    platform_print("Syscall doctor:\n");

    platform_print("  init:      ");
    platform_print(syscall_init_done ? "ok\n" : "missing\n");

    platform_print("  ready:     ");
    platform_print(syscall_ready ? "ok\n" : "bad\n");

    platform_print("  broken:    ");
    platform_print(syscall_broken ? "yes\n" : "no\n");

    platform_print("  table:     ");
    platform_print_uint(syscall_table_count);
    platform_print("\n");

    platform_print("  dispatch:  ");
    platform_print_uint(syscall_dispatch_count);
    platform_print("\n");

    platform_print("  int 0x80:  disabled\n");
    platform_print("  user mode: disabled\n");

    platform_print("  result:    ");
    platform_print(syscall_doctor_ok() ? "ok\n" : "broken\n");
}

void syscall_break(void) {
    syscall_broken = 1;
    syscall_ready = 0;

    platform_print("syscall layer broken.\n");
}

void syscall_fix(void) {
    syscall_init_done = 1;
    syscall_ready = 1;
    syscall_broken = 0;

    if (syscall_table_count == 0) {
        syscall_table_count = SYSCALL_TABLE_MAX;
    }

    platform_print("syscall layer fixed.\n");
}