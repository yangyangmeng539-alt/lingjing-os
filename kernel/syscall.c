#include "syscall.h"
#include "platform.h"
#include "idt.h"

#define SYSCALL_TABLE_MAX 8

#define SYS_PING   0
#define SYS_UPTIME 1
#define SYS_HEALTH 2
#define SYS_USER   3
#define SYSCALL_RET_OK          0
#define SYSCALL_RET_USER        3
#define SYSCALL_RET_UNSUPPORTED 0xFFFFFFFF

typedef struct syscall_entry {
    unsigned int id;
    const char* name;
    const char* status;
} syscall_entry_t;

typedef struct syscall_frame {
    unsigned int valid;
    unsigned int vector;
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;
    unsigned int int_no;
    unsigned int err_code;
} syscall_frame_t;

typedef struct syscall_return_state {
    unsigned int valid;
    unsigned int id;
    unsigned int value;
    const char* status;
} syscall_return_state_t;

static syscall_entry_t syscall_table_entries[SYSCALL_TABLE_MAX];

static int syscall_init_done = 0;
static int syscall_ready = 0;
static int syscall_broken = 0;

static unsigned int syscall_table_count = 0;
static unsigned int syscall_dispatch_count = 0;
static unsigned int syscall_unsupported_count = 0;
static unsigned int syscall_interrupt_count = 0;
static syscall_frame_t syscall_last_frame;
static syscall_return_state_t syscall_last_return;

static int syscall_interrupt_prepared = 0;
static int syscall_real_int80_enabled = 0;

static const char* syscall_mode = "interrupt metadata prototype";

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

static void syscall_clear_frame(void) {
    syscall_last_frame.valid = 0;
    syscall_last_frame.vector = 0;
    syscall_last_frame.eax = 0;
    syscall_last_frame.ebx = 0;
    syscall_last_frame.ecx = 0;
    syscall_last_frame.edx = 0;
    syscall_last_frame.int_no = 0;
    syscall_last_frame.err_code = 0;
}

static void syscall_clear_return(void) {
    syscall_last_return.valid = 0;
    syscall_last_return.id = 0;
    syscall_last_return.value = 0;
    syscall_last_return.status = "empty";
}

static void syscall_set_return(unsigned int id, unsigned int value, const char* status) {
    syscall_last_return.valid = 1;
    syscall_last_return.id = id;
    syscall_last_return.value = value;
    syscall_last_return.status = status;
}

static void syscall_save_frame(registers_t* regs) {
    if (regs == 0) {
        syscall_clear_frame();
        return;
    }

    syscall_last_frame.valid = 1;
    syscall_last_frame.vector = 0x80;
    syscall_last_frame.eax = regs->eax;
    syscall_last_frame.ebx = regs->ebx;
    syscall_last_frame.ecx = regs->ecx;
    syscall_last_frame.edx = regs->edx;
    syscall_last_frame.int_no = regs->int_no;
    syscall_last_frame.err_code = regs->err_code;
}

static void syscall_int80_handler(registers_t* regs) {
    unsigned int id = regs->eax;

    syscall_interrupt_count++;
    syscall_save_frame(regs);

    platform_print("Syscall int 0x80 handler:\n");
    platform_print("  vector:  0x80\n");
    platform_print("  real:    enabled\n");

    platform_print("  eax:     ");
    platform_print_uint(regs->eax);
    platform_print("\n");

    platform_print("  ebx:     ");
    platform_print_uint(regs->ebx);
    platform_print("\n");

    platform_print("  ecx:     ");
    platform_print_uint(regs->ecx);
    platform_print("\n");

    platform_print("  edx:     ");
    platform_print_uint(regs->edx);
    platform_print("\n");

    syscall_dispatch(id);
    regs->eax = syscall_last_return.value;
    syscall_last_frame.eax = regs->eax;
}

void syscall_init(void) {
    syscall_init_done = 1;
    syscall_ready = 1;
    syscall_broken = 0;

    syscall_dispatch_count = 0;
    syscall_unsupported_count = 0;
    syscall_interrupt_count = 0;
    syscall_clear_frame();
    syscall_clear_return();

    syscall_interrupt_prepared = 1;
    syscall_real_int80_enabled = 1;

    idt_install_syscall_gate();
    register_interrupt_handler(128, syscall_int80_handler);

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

    if (!syscall_interrupt_prepared) {
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

unsigned int syscall_get_interrupt_count(void) {
    return syscall_interrupt_count;
}

unsigned int syscall_get_last_frame_valid(void) {
    return syscall_last_frame.valid;
}

unsigned int syscall_get_last_frame_vector(void) {
    return syscall_last_frame.vector;
}

unsigned int syscall_get_last_frame_eax(void) {
    return syscall_last_frame.eax;
}

unsigned int syscall_get_last_frame_ebx(void) {
    return syscall_last_frame.ebx;
}

unsigned int syscall_get_last_frame_ecx(void) {
    return syscall_last_frame.ecx;
}

unsigned int syscall_get_last_frame_edx(void) {
    return syscall_last_frame.edx;
}

unsigned int syscall_get_last_return_valid(void) {
    return syscall_last_return.valid;
}

unsigned int syscall_get_last_return_id(void) {
    return syscall_last_return.id;
}

unsigned int syscall_get_last_return_value(void) {
    return syscall_last_return.value;
}

const char* syscall_get_last_return_status(void) {
    return syscall_last_return.status;
}

void syscall_dispatch(unsigned int id) {
    platform_print("Syscall dispatch:\n");

    if (!syscall_is_ready()) {
        syscall_set_return(id, SYSCALL_RET_UNSUPPORTED, "blocked");

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
        syscall_set_return(id, SYSCALL_RET_OK, "ok");

        platform_print("  return: pong\n");
        platform_print("  retv:   ");
        platform_print_uint(SYSCALL_RET_OK);
        platform_print("\n");
        platform_print("  result: ok\n");
        return;
    }

    if (id == SYS_UPTIME) {
        syscall_dispatch_count++;
        syscall_set_return(id, SYSCALL_RET_OK, "ok");

        platform_print("  return: metadata-only\n");
        platform_print("  retv:   ");
        platform_print_uint(SYSCALL_RET_OK);
        platform_print("\n");
        platform_print("  result: ok\n");
        return;
    }

    if (id == SYS_HEALTH) {
        syscall_dispatch_count++;
        syscall_set_return(id, SYSCALL_RET_OK, "ok");

        platform_print("  return: use health command\n");
        platform_print("  retv:   ");
        platform_print_uint(SYSCALL_RET_OK);
        platform_print("\n");
        platform_print("  result: ok\n");
        return;
    }

    if (id == SYS_USER) {
        syscall_dispatch_count++;
        syscall_set_return(id, SYSCALL_RET_USER, "ok");

        platform_print("  return: user metadata-only\n");
        platform_print("  retv:   ");
        platform_print_uint(SYSCALL_RET_USER);
        platform_print("\n");
        platform_print("  result: ok\n");
        return;
    }

    syscall_unsupported_count++;
    syscall_set_return(id, SYSCALL_RET_UNSUPPORTED, "unsupported");

    platform_print("  retv:   ");
    platform_print_uint(SYSCALL_RET_UNSUPPORTED);
    platform_print("\n");
    platform_print("  result: unsupported\n");
}

void syscall_interrupt_dispatch(unsigned int id) {
    platform_print("Syscall interrupt path:\n");

    if (!syscall_is_ready()) {
        platform_print("  result: blocked\n");
        platform_print("  reason: syscall layer not ready\n");
        return;
    }

    if (!syscall_interrupt_prepared) {
        platform_print("  result: blocked\n");
        platform_print("  reason: interrupt path not prepared\n");
        return;
    }

    syscall_interrupt_count++;

    platform_print("  vector:  0x80\n");
    platform_print("  real:    disabled(simulated)\n");
    platform_print("  mode:    simulated interrupt path\n");
    platform_print("  id:      ");
    platform_print_uint(id);
    platform_print("\n");

    syscall_dispatch(id);
}

void syscall_trigger_int80(unsigned int id) {
    platform_print("Syscall real int 0x80 trigger:\n");

    if (!syscall_is_ready()) {
        platform_print("  result: blocked\n");
        platform_print("  reason: syscall layer not ready\n");
        return;
    }

    if (!syscall_real_int80_enabled) {
        platform_print("  result: blocked\n");
        platform_print("  reason: real int 0x80 disabled\n");
        return;
    }

    platform_print("  id:      ");
    platform_print_uint(id);
    platform_print("\n");

    __asm__ volatile (
        "mov %0, %%eax\n"
        "int $0x80\n"
        :
        : "r"(id)
        : "eax", "memory"
    );

    platform_print("  result: returned from int 0x80\n");
}

void syscall_trigger_int80_args(unsigned int id, unsigned int arg1, unsigned int arg2, unsigned int arg3) {
    unsigned int ret = 0;

    platform_print("Syscall real int 0x80 trigger args:\n");

    if (!syscall_is_ready()) {
        platform_print("  result: blocked\n");
        platform_print("  reason: syscall layer not ready\n");
        return;
    }

    if (!syscall_real_int80_enabled) {
        platform_print("  result: blocked\n");
        platform_print("  reason: real int 0x80 disabled\n");
        return;
    }

    platform_print("  eax/id:  ");
    platform_print_uint(id);
    platform_print("\n");

    platform_print("  ebx/a:   ");
    platform_print_uint(arg1);
    platform_print("\n");

    platform_print("  ecx/b:   ");
    platform_print_uint(arg2);
    platform_print("\n");

    platform_print("  edx/c:   ");
    platform_print_uint(arg3);
    platform_print("\n");

    __asm__ volatile (
        "push %%ebx\n"
        "movl %1, %%eax\n"
        "movl %2, %%ebx\n"
        "movl %3, %%ecx\n"
        "movl %4, %%edx\n"
        "int $0x80\n"
        "movl %%eax, %0\n"
        "pop %%ebx\n"
        : "=m"(ret)
        : "m"(id), "m"(arg1), "m"(arg2), "m"(arg3)
        : "eax", "ecx", "edx", "memory"
    );

    platform_print("  retv:    ");
    platform_print_uint(ret);
    platform_print("\n");

    platform_print("  result:  returned from int 0x80\n");
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

void syscall_interrupt_status(void) {
    platform_print("Syscall interrupt:\n");

    platform_print("  vector:    0x80\n");

    platform_print("  prepared:  ");
    platform_print(syscall_interrupt_prepared ? "yes\n" : "no\n");

    platform_print("  real int:  ");
    platform_print(syscall_real_int80_enabled ? "enabled\n" : "disabled\n");

    platform_print("  calls:     ");
    platform_print_uint(syscall_interrupt_count);
    platform_print("\n");

    platform_print("  mode:      simulated interrupt path\n");

    platform_print("  result:    ");
    platform_print(syscall_doctor_ok() ? "ok\n" : "broken\n");
}

void syscall_frame(void) {
    platform_print("Syscall frame:\n");

    platform_print("  valid:   ");
    platform_print(syscall_last_frame.valid ? "yes\n" : "no\n");

    platform_print("  vector:  ");
    platform_print_hex32(syscall_last_frame.vector);
    platform_print("\n");

    platform_print("  int_no:  ");
    platform_print_uint(syscall_last_frame.int_no);
    platform_print("\n");

    platform_print("  err:     ");
    platform_print_uint(syscall_last_frame.err_code);
    platform_print("\n");

    platform_print("  eax:     ");
    platform_print_uint(syscall_last_frame.eax);
    platform_print("\n");

    platform_print("  ebx:     ");
    platform_print_uint(syscall_last_frame.ebx);
    platform_print("\n");

    platform_print("  ecx:     ");
    platform_print_uint(syscall_last_frame.ecx);
    platform_print("\n");

    platform_print("  edx:     ");
    platform_print_uint(syscall_last_frame.edx);
    platform_print("\n");
}

void syscall_return_status(void) {
    platform_print("Syscall return:\n");

    platform_print("  valid:  ");
    platform_print(syscall_last_return.valid ? "yes\n" : "no\n");

    platform_print("  id:     ");
    platform_print_uint(syscall_last_return.id);
    platform_print("\n");

    platform_print("  value:  ");
    platform_print_uint(syscall_last_return.value);
    platform_print("\n");

    platform_print("  status: ");
    platform_print(syscall_last_return.status);
    platform_print("\n");
}

void syscall_stats(void) {
    platform_print("Syscall stats:\n");

    platform_print("  table:       ");
    platform_print_uint(syscall_table_count);
    platform_print("\n");

    platform_print("  dispatch:    ");
    platform_print_uint(syscall_dispatch_count);
    platform_print("\n");

    platform_print("  interrupt:   ");
    platform_print_uint(syscall_interrupt_count);
    platform_print("\n");

    platform_print("  unsupported: ");
    platform_print_uint(syscall_unsupported_count);
    platform_print("\n");

    platform_print("  frame valid: ");
    platform_print(syscall_last_frame.valid ? "yes\n" : "no\n");

    platform_print("  return valid: ");
    platform_print(syscall_last_return.valid ? "yes\n" : "no\n");

    platform_print("  int 0x80:    ");
    platform_print(syscall_real_int80_enabled ? "enabled\n" : "disabled\n");

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

    platform_print("  interrupt:   ");
    platform_print_uint(syscall_get_interrupt_count());
    platform_print("\n");

    platform_print("  unsupported: ");
    platform_print_uint(syscall_get_unsupported_count());
    platform_print("\n");

    platform_print("  int 0x80:    ");
    platform_print(syscall_real_int80_enabled ? "enabled\n" : "disabled\n");

    platform_print("  user mode:   metadata-only\n");
}

void syscall_check(void) {
    platform_print("Syscall check:\n");

    platform_print("  init:      ");
    platform_print(syscall_init_done ? "ok\n" : "missing\n");

    platform_print("  table:     ");
    platform_print(syscall_table_count > 0 ? "ok\n" : "missing\n");

    platform_print("  interrupt: ");
    platform_print(syscall_interrupt_prepared ? "prepared\n" : "missing\n");

    platform_print("  dispatch:  metadata\n");

    platform_print("  result:    ");
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

    platform_print("  interrupt:   ");
    platform_print_uint(syscall_interrupt_count);
    platform_print("\n");

    platform_print("  unsupported: ");
    platform_print_uint(syscall_unsupported_count);
    platform_print("\n");

    platform_print("  frame:       ");
    platform_print(syscall_last_frame.valid ? "captured\n" : "empty\n");

    platform_print("  return:      ");
    platform_print(syscall_last_return.valid ? "captured\n" : "empty\n");

    platform_print("  int path:    ");
    platform_print(syscall_interrupt_prepared ? "prepared\n" : "missing\n");

    platform_print("  int 0x80:    ");
    platform_print(syscall_real_int80_enabled ? "enabled\n" : "disabled\n");

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
    syscall_interrupt_prepared = 1;

    if (syscall_table_count == 0) {
        syscall_table_reset();
    }

    platform_print("syscall layer fixed.\n");
}