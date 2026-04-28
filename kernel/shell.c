#include "shell.h"
#include "screen.h"
#include "keyboard.h"
#include "memory.h"
#include "timer.h"
#include "system.h"
#include "memory.h"
#include "timer.h"
#include "module.h"
#include "intent.h"
#include "version.h"
#include "security.h"

extern unsigned int kernel_stack_marker;

extern unsigned int start;
extern unsigned int end;

static char input[128];
static int input_len = 0;

static int str_equal(const char* a, const char* b) {
    int i = 0;

    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }

    return a[i] == '\0' && b[i] == '\0';
}

static int str_starts_with(const char* text, const char* prefix) {
    int i = 0;

    while (prefix[i] != '\0') {
        if (text[i] != prefix[i]) {
            return 0;
        }
        i++;
    }

    return 1;
}

static unsigned int parse_uint(const char* text) {
    unsigned int value = 0;
    int i = 0;

    while (text[i] >= '0' && text[i] <= '9') {
        value = value * 10 + (unsigned int)(text[i] - '0');
        i++;
    }

    return value;
}

static unsigned int parse_hex_or_uint(const char* text) {
    unsigned int value = 0;
    int i = 0;

    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        i = 2;

        while (1) {
            char c = text[i];

            if (c >= '0' && c <= '9') {
                value = value * 16 + (unsigned int)(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                value = value * 16 + (unsigned int)(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                value = value * 16 + (unsigned int)(c - 'A' + 10);
            } else {
                break;
            }

            i++;
        }

        return value;
    }

    return parse_uint(text);
}

static const char* skip_token(const char* text) {
    int i = 0;

    while (text[i] != '\0' && text[i] != ' ') {
        i++;
    }

    while (text[i] == ' ') {
        i++;
    }

    return text + i;
}

static void print_hex_digit(unsigned int value) {
    char digit = 0;

    if (value < 10) {
        digit = (char)('0' + value);
    } else {
        digit = (char)('A' + (value - 10));
    }

    screen_put_char(digit);
}

static void print_hex32(unsigned int value) {
    screen_print("0x");

    for (int shift = 28; shift >= 0; shift -= 4) {
        print_hex_digit((value >> shift) & 0xF);
    }
}

static void print_uint(unsigned int value) {
    char buffer[16];
    int index = 0;

    if (value == 0) {
        screen_put_char('0');
        return;
    }

    while (value > 0 && index < 16) {
        buffer[index] = (char)('0' + (value % 10));
        value /= 10;
        index++;
    }

    for (int i = index - 1; i >= 0; i--) {
        screen_put_char(buffer[i]);
    }
}

static void shell_prompt(void) {
    screen_print("> ");
}

static void shell_handle_mem(void) {
    screen_print("Memory info:\n");

    screen_print("  kernel start: ");
    print_hex32((unsigned int)&start);
    screen_print("\n");

    screen_print("  kernel end:   ");
    print_hex32((unsigned int)&end);
    screen_print("\n");

    screen_print("  stack near:   ");
    print_hex32(kernel_stack_marker);
    screen_print("\n");

    screen_print("  next alloc:   ");
    print_hex32(memory_get_placement_address());
    screen_print("\n");
}

static void shell_handle_uptime(void) {
    screen_print("Uptime:\n");

    screen_print("  ticks:   ");
    print_uint(timer_get_ticks());
    screen_print("\n");

    screen_print("  seconds: ");
    print_uint(timer_get_seconds());
    screen_print("\n");
}

static void shell_handle_intent(const char* cmd) {
    const char* name = cmd + 7;

    if (name[0] == '\0') {
        screen_print("usage: intent <name>\n");
        return;
    }

    intent_run(name);
}

static void shell_handle_unload(const char* cmd) {
    const char* name = cmd + 7;

    if (name[0] == '\0') {
        screen_print("usage: unload <module>\n");
        return;
    }

    module_unload_mock(name);
}

static void shell_handle_load(const char* cmd) {
    const char* name = cmd + 5;

    if (name[0] == '\0') {
        screen_print("usage: load <module>\n");
        return;
    }

    module_load_mock(name);
}

static void shell_handle_moduleinfo(const char* cmd) {
    const char* name = cmd + 11;

    if (name[0] == '\0') {
        screen_print("usage: moduleinfo <name>\n");
        return;
    }

    module_info(name);
}

static void shell_handle_moduledeps(const char* cmd) {
    const char* name = cmd + 11;

    if (name[0] == '\0') {
        screen_print("usage: moduledeps <name>\n");
        return;
    }

    module_deps(name);
}

static void shell_handle_modulebreak(const char* cmd) {
    const char* name = cmd + 12;

    if (name[0] == '\0') {
        screen_print("usage: modulebreak <name>\n");
        return;
    }

    module_break_dependency(name);
}

static void shell_handle_modulefix(const char* cmd) {
    const char* name = cmd + 10;

    if (name[0] == '\0') {
        screen_print("usage: modulefix <name>\n");
        return;
    }

    module_fix_dependency(name);
}

static void shell_handle_moduletree(void) {
    module_tree();
}

static void shell_handle_modulecheck(void) {
    module_check();
}

static void shell_handle_sysinfo(void) {
    screen_print("Lingjing OS System Info\n");

    screen_print("  version:        ");
    screen_print(LINGJING_VERSION);
    screen_print("\n");

    screen_print("  stage:          ");
    screen_print(LINGJING_STAGE);
    screen_print("\n");

    screen_print("  arch:           ");
    screen_print(LINGJING_ARCH);
    screen_print("\n");

    screen_print("  boot:           ");
    screen_print(LINGJING_BOOT);
    screen_print("\n");

    screen_print("  intent layer:   ");
    screen_print(LINGJING_INTENT_LAYER);
    screen_print("\n");

    screen_print("  gdt:            ok\n");
    screen_print("  idt:            ok\n");
    screen_print("  keyboard irq:   ok\n");
    screen_print("  timer irq:      ok\n");

    screen_print("  memory:         bump allocator\n");

    screen_print("  uptime seconds: ");
    print_uint(timer_get_seconds());
    screen_print("\n");

    screen_print("  next alloc:     ");
    print_hex32(memory_get_placement_address());
    screen_print("\n");
}

static void shell_handle_dashboard(void) {
    screen_print("Lingjing Dashboard\n");
    screen_print("------------------\n");

    screen_print("uptime seconds: ");
    print_uint(timer_get_seconds());
    screen_print("\n");

    screen_print("scheduler ticks:");
    print_uint(scheduler_get_ticks());
    screen_print("\n");

    screen_print("scheduler mode: ");
    screen_print(scheduler_get_mode());
    screen_print("\n");

    screen_print("active task:    ");
    screen_print(scheduler_get_active_task());
    screen_print("\n");

    screen_print("next alloc:     ");
    print_hex32(memory_get_placement_address());
    screen_print("\n");

    screen_print("intent layer:   enabled\n");

    screen_print("dependency:     ");
    if (module_has_broken_dependencies()) {
        screen_print("broken\n");
    } else {
        screen_print("ok\n");
    }

    screen_print("task health:    ");
    if (scheduler_has_broken_tasks()) {
        screen_print("broken\n");
    } else {
        screen_print("ok\n");
    }

    screen_print("tasks:          ");
    print_uint((unsigned int)scheduler_task_count());
    screen_print("\n\n");

    intent_status();
    screen_print("\n");

    scheduler_list_tasks();
    screen_print("\n");

    module_list();
}

static void shell_handle_security(void) {
    security_status();
}

static void shell_handle_securitylog(void) {
    security_log();
}

static void shell_handle_securityclear(void) {
    security_clear_log();
}

static void shell_handle_doctor(void) {
    int module_broken = module_has_broken_dependencies();
    int task_broken = scheduler_has_broken_tasks();
    int security_ok = security_doctor_ok();

    screen_print("System doctor:\n");

    screen_print("  module dependencies: ");
    if (module_broken) {
        screen_print("broken\n");
    } else {
        screen_print("ok\n");
    }

    screen_print("  task health:         ");
    if (task_broken) {
        screen_print("broken\n");
    } else {
        screen_print("ok\n");
    }

    screen_print("  scheduler:           ");
    if (task_broken) {
        screen_print("broken\n");
    } else {
        screen_print("ok\n");
    }

    screen_print("  scheduler active:    ");
    if (scheduler_has_broken_tasks()) {
        screen_print("broken\n");
    } else {
        screen_print("ok\n");
    }

    screen_print("  security:            ");
    if (security_ok) {
        screen_print("ok\n");
    } else {
        screen_print("broken\n");
    }

    screen_print("  intent system:       ");
    if (module_broken || task_broken || !security_ok) {
        screen_print("blocked\n");
    } else {
        screen_print("ready\n");
    }

    screen_print("  result:              ");
    if (module_broken || task_broken || !security_ok) {
        screen_print("blocked\n");
    } else {
        screen_print("ready\n");
    }
}

static void shell_handle_tasks(void) {
    scheduler_list_tasks();
}

static void shell_handle_taskcheck(void) {
    scheduler_check_tasks();
}

static void shell_handle_taskdoctor(void) {
    scheduler_doctor();
}

static void shell_handle_schedvalidate(void) {
    scheduler_validate();
}

static void shell_handle_schedfix(void) {
    scheduler_fix();
}

static void shell_handle_schedinfo(void) {
    scheduler_info();
}

static void shell_handle_schedlog(void) {
    scheduler_log();
}

static void shell_handle_runqueue(void) {
    scheduler_runqueue();
}

static void shell_handle_schedclear(void) {
    scheduler_clear_log();
}

static void shell_handle_schedreset(void) {
    scheduler_reset();
}

static void shell_handle_yield(void) {
    scheduler_yield();
}

static void shell_handle_taskinfo(const char* cmd) {
    const char* id_text = cmd + 9;
    unsigned int id = parse_uint(id_text);

    scheduler_task_info(id);
}

static const char* shell_skip_token(const char* text) {
    int i = 0;

    while (text[i] != '\0' && text[i] != ' ') {
        i++;
    }

    while (text[i] == ' ') {
        i++;
    }

    return text + i;
}

static void shell_handle_taskstate(const char* cmd) {
    const char* id_text = cmd + 10;
    unsigned int id = parse_uint(id_text);
    const char* state = shell_skip_token(id_text);

    if (state[0] == '\0') {
        screen_print("usage: taskstate <id> <state>\n");
        return;
    }

    scheduler_set_task_state(id, state);
}

static void shell_handle_status(void) {
    int module_broken = module_has_broken_dependencies();
    int task_broken = scheduler_has_broken_tasks();

    screen_print("LJ | up ");
    print_uint(timer_get_seconds());
    screen_print("s | in ");

    if (intent_is_running()) {
        screen_print(intent_get_current_name());
    } else {
        screen_print("none");
    }

    screen_print(" | m");
    print_uint((unsigned int)module_count_loaded());

    screen_print(" t");
    print_uint((unsigned int)scheduler_task_count());

    screen_print(" | s");
    print_uint(scheduler_get_ticks());

    screen_print(" y");
    print_uint(scheduler_get_yields());

    screen_print(" | coop");

    screen_print(" | deps ");
    screen_print(module_broken ? "bad" : "ok");

    screen_print(" | doc ");
    screen_print((module_broken || task_broken) ? "bad" : "ok");

    screen_print("\n");
}

static void shell_handle_version(void) {
    screen_print(LINGJING_NAME);
    screen_print("\n");

    screen_print("  version: ");
    screen_print(LINGJING_VERSION);
    screen_print("\n");

    screen_print("  stage: ");
    screen_print(LINGJING_STAGE);
    screen_print("\n");

    screen_print("  arch: ");
    screen_print(LINGJING_ARCH);
    screen_print("\n");

    screen_print("  boot: ");
    screen_print(LINGJING_BOOT);
    screen_print("\n");
}

static void shell_handle_sleep(const char* cmd) {
    const char* seconds_text = cmd + 6;
    unsigned int seconds = parse_uint(seconds_text);

    if (seconds == 0) {
        screen_print("usage: sleep <seconds>\n");
        return;
    }

    screen_print("sleeping ");
    print_uint(seconds);
    screen_print(" seconds...\n");

    timer_sleep(seconds);

    screen_print("wake.\n");
}

static void shell_handle_kmalloc(const char* cmd) {
    const char* size_text = cmd + 8;
    unsigned int size = parse_uint(size_text);

    if (size == 0) {
        screen_print("usage: kmalloc <bytes>\n");
        return;
    }

    unsigned int address = kmalloc(size);

    screen_print("allocated ");
    print_uint(size);
    screen_print(" bytes at ");
    print_hex32(address);
    screen_print("\n");

    screen_print("next alloc: ");
    print_hex32(memory_get_placement_address());
    screen_print("\n");
}

static void shell_handle_kcalloc(const char* cmd) {
    const char* size_text = cmd + 8;
    unsigned int size = parse_uint(size_text);

    if (size == 0) {
        screen_print("usage: kcalloc <bytes>\n");
        return;
    }

    unsigned int address = kcalloc(size);

    screen_print("allocated and zeroed ");
    print_uint(size);
    screen_print(" bytes at ");
    print_hex32(address);
    screen_print("\n");

    screen_print("next alloc: ");
    print_hex32(memory_get_placement_address());
    screen_print("\n");
}

static void shell_handle_peek(const char* cmd) {
    const char* addr_text = cmd + 5;
    unsigned int address = parse_hex_or_uint(addr_text);

    if (address == 0) {
        screen_print("usage: peek <addr>\n");
        return;
    }

    volatile unsigned char* ptr = (volatile unsigned char*)address;
    unsigned int value = (unsigned int)(*ptr);

    screen_print("addr: ");
    print_hex32(address);
    screen_print("\n");

    screen_print("value: ");
    print_uint(value);
    screen_print(" / ");
    print_hex32(value);
    screen_print("\n");

    if (value >= 32 && value <= 126) {
        screen_print("char: ");
        screen_put_char((char)value);
        screen_print("\n");
    }
}

static void shell_handle_poke(const char* cmd) {
    const char* addr_text = cmd + 5;
    const char* value_text = skip_token(addr_text);

    unsigned int address = parse_hex_or_uint(addr_text);
    unsigned int value = parse_hex_or_uint(value_text);

    if (address == 0) {
        screen_print("usage: poke <addr> <value>\n");
        return;
    }

    if (value > 255) {
        screen_print("value must be 0-255\n");
        return;
    }

    volatile unsigned char* ptr = (volatile unsigned char*)address;
    *ptr = (unsigned char)value;

    screen_print("wrote ");
    print_uint(value);
    screen_print(" to ");
    print_hex32(address);
    screen_print("\n");
}

static void shell_handle_hexdump(const char* cmd) {
    const char* addr_text = cmd + 8;
    const char* len_text = skip_token(addr_text);

    unsigned int address = parse_hex_or_uint(addr_text);
    unsigned int length = parse_hex_or_uint(len_text);

    if (address == 0 || length == 0) {
        screen_print("usage: hexdump <addr> <len>\n");
        return;
    }

    volatile unsigned char* ptr = (volatile unsigned char*)address;

    for (unsigned int i = 0; i < length; i++) {
        if ((i % 16) == 0) {
            if (i != 0) {
                screen_print("\n");
            }

            print_hex32(address + i);
            screen_print(": ");
        }

        unsigned int value = (unsigned int)ptr[i];

        print_hex_digit((value >> 4) & 0xF);
        print_hex_digit(value & 0xF);
        screen_put_char(' ');
    }

    screen_print("\n");
}

static void shell_handle_kzero(const char* cmd) {
    const char* addr_text = cmd + 6;
    const char* len_text = skip_token(addr_text);

    unsigned int address = parse_hex_or_uint(addr_text);
    unsigned int length = parse_hex_or_uint(len_text);

    if (address == 0 || length == 0) {
        screen_print("usage: kzero <addr> <len>\n");
        return;
    }

    volatile unsigned char* ptr = (volatile unsigned char*)address;

    for (unsigned int i = 0; i < length; i++) {
        ptr[i] = 0;
    }

    screen_print("zeroed ");
    print_uint(length);
    screen_print(" bytes at ");
    print_hex32(address);
    screen_print("\n");
}

static void shell_handle_command(const char* cmd) {
    if (str_equal(cmd, "help")) {
        screen_print("commands: help, clear, about, version, sysinfo, dashboard, status, doctor, security, securitylog, securityclear, tasks, taskinfo, taskstate, taskcheck, taskdoctor, schedinfo, schedlog, schedclear, schedreset, schedvalidate, schedfix, runqueue, yield, modules, moduleinfo, moduledeps, moduletree, modulecheck, modulebreak, modulefix, load, unload, intent, echo, mem, uptime, sleep, reboot, halt, kmalloc, kcalloc, peek, poke, hexdump, kzero\n");
    } else if (str_equal(cmd, "clear")) {
        screen_clear();
    } else if (str_equal(cmd, "about")) {
        screen_print("Lingjing OS experimental kernel.\n");
    } else if (str_equal(cmd, "version")) {
        shell_handle_version();
    } else if (str_equal(cmd, "sysinfo")) {
        shell_handle_sysinfo();
    } else if (str_equal(cmd, "dashboard")) {
        shell_handle_dashboard();
    } else if (str_equal(cmd, "status")) {
        shell_handle_status();
    } else if (str_equal(cmd, "doctor")) {
        shell_handle_doctor();
    } else if (str_equal(cmd, "security")) {
        shell_handle_security();
    } else if (str_equal(cmd, "securitylog")) {
        shell_handle_securitylog();
    } else if (str_equal(cmd, "securityclear")) {
        shell_handle_securityclear();
    } else if (str_equal(cmd, "tasks")) {
        shell_handle_tasks();
    } else if (str_starts_with(cmd, "taskinfo ")) {
        shell_handle_taskinfo(cmd);
    } else if (str_starts_with(cmd, "taskstate ")) {
        shell_handle_taskstate(cmd);
    } else if (str_equal(cmd, "taskcheck")) {
        shell_handle_taskcheck();
    } else if (str_equal(cmd, "taskdoctor")) {
        shell_handle_taskdoctor();
    } else if (str_equal(cmd, "schedinfo")) {
        shell_handle_schedinfo();
    } else if (str_equal(cmd, "schedlog")) {
        shell_handle_schedlog();
    } else if (str_equal(cmd, "schedclear")) {
        shell_handle_schedclear();
    } else if (str_equal(cmd, "schedreset")) {
        shell_handle_schedreset();
    } else if (str_equal(cmd, "schedvalidate")) {
        shell_handle_schedvalidate();
    } else if (str_equal(cmd, "schedfix")) {
        shell_handle_schedfix();
    } else if (str_equal(cmd, "runqueue")) {
        shell_handle_runqueue();
    } else if (str_equal(cmd, "yield")) {
        shell_handle_yield();
    } else if (str_equal(cmd, "modules")) {
        module_list();
    } else if (str_starts_with(cmd, "moduleinfo ")) {
        shell_handle_moduleinfo(cmd);
    } else if (str_starts_with(cmd, "moduledeps ")) {
        shell_handle_moduledeps(cmd);
    } else if (str_equal(cmd, "moduletree")) {
        shell_handle_moduletree();
    } else if (str_equal(cmd, "modulecheck")) {
        shell_handle_modulecheck();
    } else if (str_starts_with(cmd, "modulebreak ")) {
        shell_handle_modulebreak(cmd);
    } else if (str_starts_with(cmd, "modulefix ")) {
        shell_handle_modulefix(cmd);
    } else if (str_starts_with(cmd, "load ")) {
        shell_handle_load(cmd);
    } else if (str_starts_with(cmd, "unload ")) {
        shell_handle_unload(cmd);
    } else if (str_starts_with(cmd, "intent ")) {
        shell_handle_intent(cmd);
    } else if (str_equal(cmd, "mem")) {
        shell_handle_mem();
    } else if (str_equal(cmd, "uptime")) {
        shell_handle_uptime();
    } else if (str_starts_with(cmd, "sleep ")) {
        shell_handle_sleep(cmd);
    } else if (str_equal(cmd, "reboot")) {
        screen_print("rebooting...\n");
        system_reboot();
    } else if (str_equal(cmd, "halt")) {
        screen_print("system halted.\n");
        system_halt();
    } else if (str_equal(cmd, "lingjing")) {
        screen_print("Lingjing core awakened.\n");
        screen_print("My machine, my rules.\n");
    } else if (str_starts_with(cmd, "kmalloc ")) {
        shell_handle_kmalloc(cmd);
    } else if (str_starts_with(cmd, "kcalloc ")) {
        shell_handle_kcalloc(cmd);
    } else if (str_starts_with(cmd, "peek ")) {
        shell_handle_peek(cmd);
    } else if (str_starts_with(cmd, "poke ")) {
        shell_handle_poke(cmd);
    } else if (str_starts_with(cmd, "hexdump ")) {
        shell_handle_hexdump(cmd);
    } else if (str_starts_with(cmd, "kzero ")) {
        shell_handle_kzero(cmd);
    } else if (cmd[0] == 'e' && cmd[1] == 'c' && cmd[2] == 'h' && cmd[3] == 'o' && cmd[4] == ' ') {
        screen_print(cmd + 5);
        screen_print("\n");
    } else if (cmd[0] == '\0') {
        return;
    } else {
        screen_print("unknown command: ");
        screen_print(cmd);
        screen_print("\n");
    }
}

void shell_init(void) {
    input_len = 0;
    shell_prompt();
}

void shell_update(void) {
    char c = keyboard_read_char();

    if (c == 0) {
        return;
    }

    if (c == '\n') {
        screen_put_char('\n');
        input[input_len] = '\0';
        shell_handle_command(input);
        input_len = 0;
        shell_prompt();
    } else if (c == '\b') {
        if (input_len > 0) {
            input_len--;
            screen_put_char('\b');
        }
    } else {
        if (input_len < 127) {
            input[input_len] = c;
            input_len++;
            screen_put_char(c);
        }
    }
}