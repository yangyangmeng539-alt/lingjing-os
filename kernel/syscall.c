#include "syscall.h"
#include "platform.h"

#define SYSCALL_TABLE_MAX 8

#define SYS_PING   0
#define SYS_UPTIME 1
#define SYS_HEALTH 2
#define SYS_USER   3

typedef struct syscall_entry {
    unsigned int id;
    const char* name;
    const char* status;
} syscall_entry_t;

static syscall_entry_t syscall_table_entries[SYSCALL_TABLE_MAX];

static int syscall_init_done = 0;
static int syscall_ready = 0;
static int syscall_broken = 0;

static unsigned int syscall_table_count = 0;
static unsigned int syscall_dispatch_count = 0;
static unsigned int syscall_unsupported_count = 0;

static const char* syscall_mode = "metadata prototype";

static void syscall_table_reset(void) {
    syscall_table_entries[0].id = SYS_PING;
    syscall_table_entries[0].name = "SYS_PING";
    syscall_table_entries[0].status = "ready";

    syscall_table_entries[1].id = SYS_UPTIME;
    syscall_table_entries[1].name = "SYS_UPTIME";
    syscall_table_entries[1].status = "ready";

    syscall_table_entries[2].id = SYS_HEALTH;
    syscall_table_entries[2].name = "SYS_HEALTH";
    syscall_table_entries[2].status = "ready";

    syscall_table_entries[3].id = SYS_USER;
    syscall_table_entries[3].name = "SYS_USER";
    syscall_table_entries[3].status = "metadata";

    for (int i = 4; i < SYSCALL_TABLE_MAX; i++) {
        syscall_table_entries[i].id = (unsigned int)i;
        syscall_table_entries[i].name = "RESERVED";
        syscall_table_entries[i].status = "reserved";
    }

    syscall_table_count = SYSCALL_TABLE_MAX;
}

static const char* syscall_name_for_id(unsigned int id) {
    if (id >= SYSCALL_TABLE_MAX) {
        return "UNKNOWN";
    }

    return syscall_table_entries[id].name;
}

static const char* syscall_status_for_id(unsigned int id) {
    if (id >= SYSCALL_TABLE_MAX) {
        return "unsupported";
    }

    return syscall_table_entries[id].status;
}

void syscall_init(void) {
    syscall_init_done = 1;
    syscall_ready = 1;
    syscall_broken = 0;

    syscall_dispatch_count = 0;
    syscall_unsupported_count = 0;

    syscall_table_reset();
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

unsigned int syscall_get_unsupported_count(void) {
    return syscall_unsupported_count;
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

    platform_print("  name:   ");
    platform_print(syscall_name_for_id(id));
    platform_print("\n");

    platform_print("  status: ");
    platform_print(syscall_status_for_id(id));
    platform_print("\n");

    if (id == SYS_PING) {
        syscall_dispatch_count++;

        platform_print("  return: pong\n");
        platform_print("  result: ok\n");
        return;
    }

    if (id == SYS_UPTIME) {
        syscall_dispatch_count++;

        platform_print("  return: metadata-only\n");
        platform_print("  result: ok\n");
        return;
    }

    if (id == SYS_HEALTH) {
        syscall_dispatch_count++;

        platform_print("  return: use health command\n");
        platform_print("  result: ok\n");
        return;
    }

    if (id == SYS_USER) {
        syscall_dispatch_count++;

        platform_print("  return: user metadata-only\n");
        platform_print("  result: ok\n");
        return;
    }

    syscall_unsupported_count++;

    platform_print("  result: unsupported\n");
}

void syscall_table(void) {
    platform_print("Syscall table:\n");

    for (int i = 0; i < SYSCALL_TABLE_MAX; i++) {
        platform_print("  ");
        platform_print_uint(syscall_table_entries[i].id);
        platform_print("    ");
        platform_print(syscall_table_entries[i].name);
        platform_print("    ");
        platform_print(syscall_table_entries[i].status);
        platform_print("\n");
    }
}

void syscall_stats(void) {
    platform_print("Syscall stats:\n");

    platform_print("  table:       ");
    platform_print_uint(syscall_table_count);
    platform_print("\n");

    platform_print("  dispatch:    ");
    platform_print_uint(syscall_dispatch_count);
    platform_print("\n");

    platform_print("  unsupported: ");
    platform_print_uint(syscall_unsupported_count);
    platform_print("\n");

    platform_print("  int 0x80:    disabled\n");
    platform_print("  user mode:   metadata-only\n");
}

void syscall_status(void) {
    platform_print("Syscall layer:\n");

    platform_print("  state:       ");
    platform_print(syscall_get_state());
    platform_print("\n");

    platform_print("  mode:        ");
    platform_print(syscall_get_mode());
    platform_print("\n");

    platform_print("  table:       ");
    platform_print_uint(syscall_get_table_count());
    platform_print("\n");

    platform_print("  dispatch:    ");
    platform_print_uint(syscall_get_dispatch_count());
    platform_print("\n");

    platform_print("  unsupported: ");
    platform_print_uint(syscall_get_unsupported_count());
    platform_print("\n");

    platform_print("  int 0x80:    disabled\n");
    platform_print("  user mode:   metadata-only\n");
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

    platform_print("  init:        ");
    platform_print(syscall_init_done ? "ok\n" : "missing\n");

    platform_print("  ready:       ");
    platform_print(syscall_ready ? "ok\n" : "bad\n");

    platform_print("  broken:      ");
    platform_print(syscall_broken ? "yes\n" : "no\n");

    platform_print("  table:       ");
    platform_print_uint(syscall_table_count);
    platform_print("\n");

    platform_print("  dispatch:    ");
    platform_print_uint(syscall_dispatch_count);
    platform_print("\n");

    platform_print("  unsupported: ");
    platform_print_uint(syscall_unsupported_count);
    platform_print("\n");

    platform_print("  int 0x80:    disabled\n");
    platform_print("  user mode:   metadata-only\n");

    platform_print("  result:      ");
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
        syscall_table_reset();
    }

    platform_print("syscall layer fixed.\n");
}