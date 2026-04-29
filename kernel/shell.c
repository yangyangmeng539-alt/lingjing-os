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
#include "lang.h"
#include "platform.h"
#include "scheduler.h"

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

    platform_put_char(digit);
}

static void shell_prompt(void) {
    platform_print("> ");
}

static void shell_handle_mem(void) {
    platform_print("Memory:\n");

    platform_print("  next alloc: ");
    platform_print_hex32(memory_get_placement_address());
    platform_print("\n");

    platform_print("  allocator:  bump\n");
}

static void shell_handle_uptime(void) {
    platform_print("Uptime:\n");

    platform_print("  ticks:   ");
    platform_print_uint(platform_ticks());
    platform_print("\n");

    platform_print("  seconds: ");
    platform_print_uint(platform_seconds());
    platform_print("\n");
}

static void shell_handle_intent(const char* cmd) {
    const char* name = cmd + 7;

    if (name[0] == '\0') {
        platform_print("usage: intent <name>\n");
        return;
    }

    intent_run(name);
}

static void shell_handle_unload(const char* cmd) {
    const char* name = cmd + 7;

    if (name[0] == '\0') {
        platform_print("usage: unload <module>\n");
        return;
    }

    module_unload_mock(name);
}

static void shell_handle_load(const char* cmd) {
    const char* name = cmd + 5;

    if (name[0] == '\0') {
        platform_print("usage: load <module>\n");
        return;
    }

    module_load_mock(name);
}

static void shell_handle_moduleinfo(const char* cmd) {
    const char* name = cmd + 11;

    if (name[0] == '\0') {
        platform_print("usage: moduleinfo <name>\n");
        return;
    }

    module_info(name);
}

static void shell_handle_moduledeps(const char* cmd) {
    const char* name = cmd + 11;

    if (name[0] == '\0') {
        platform_print("usage: moduledeps <name>\n");
        return;
    }

    module_deps(name);
}

static void shell_handle_modulebreak(const char* cmd) {
    const char* name = cmd + 12;

    if (name[0] == '\0') {
        platform_print("usage: modulebreak <name>\n");
        return;
    }

    module_break_dependency(name);
}

static void shell_handle_modulefix(const char* cmd) {
    const char* name = cmd + 10;

    if (name[0] == '\0') {
        platform_print("usage: modulefix <name>\n");
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
    platform_print("Lingjing OS System Info\n");

    platform_print("  version:        ");
    platform_print(LINGJING_VERSION);
    platform_print("\n");

    platform_print("  stage:          ");
    platform_print(LINGJING_STAGE);
    platform_print("\n");

    platform_print("  arch:           ");
    platform_print(LINGJING_ARCH);
    platform_print("\n");

    platform_print("  boot:           ");
    platform_print(LINGJING_BOOT);
    platform_print("\n");

    platform_print("  intent layer:   ");
    platform_print(LINGJING_INTENT_LAYER);
    platform_print("\n");

    platform_print("  platform:       ");
    platform_print(platform_get_name());
    platform_print("\n");

    platform_print("  display:        ");
    platform_print(platform_get_display());
    platform_print("\n");

    platform_print("  timer:          ");
    platform_print(platform_get_timer());
    platform_print("\n");

    platform_print("  input:          ");
    platform_print(platform_get_input());
    platform_print("\n");

    platform_print("  gdt:            ok\n");
    platform_print("  idt:            ok\n");
    platform_print("  keyboard irq:   ok\n");
    platform_print("  timer irq:      ok\n");
    platform_print("  memory:         bump allocator\n");

    platform_print("  uptime seconds: ");
    platform_print_uint(platform_seconds());
    platform_print("\n");

    platform_print("  next alloc:     ");
    platform_print_hex32(memory_get_placement_address());
    platform_print("\n");
}

static void shell_handle_dashboard(void) {
    platform_print("Lingjing Dashboard\n");
    platform_print("------------------\n");

    platform_print("uptime seconds: ");
    platform_print_uint(platform_seconds());
    platform_print("\n");

    platform_print("scheduler ticks:");
    platform_print_uint(scheduler_get_ticks());
    platform_print("\n");

    platform_print("scheduler mode: ");
    platform_print(scheduler_get_mode());
    platform_print("\n");

    platform_print("active task:    ");
    platform_print(scheduler_get_active_task());
    platform_print("\n");

    platform_print("platform:       ");
    platform_print(platform_get_name());
    platform_print("\n");

    platform_print("display:        ");
    platform_print(platform_get_display());
    platform_print("\n");

    platform_print("next alloc:     ");
    platform_print_hex32(memory_get_placement_address());
    platform_print("\n");

    platform_print("intent layer:   enabled\n");

    platform_print("security:       ");
    if (security_doctor_ok()) {
        platform_print("ok\n");
    } else {
        platform_print("broken\n");
    }

    platform_print("language:       ");
    platform_print(lang_get_current_name());
    platform_print("\n");

    platform_print("dependency:     ");
    if (module_has_broken_dependencies()) {
        platform_print("broken\n");
    } else {
        platform_print("ok\n");
    }

    platform_print("task health:    ");
    if (scheduler_has_broken_tasks()) {
        platform_print("broken\n");
    } else {
        platform_print("ok\n");
    }

    platform_print("tasks:          ");
    platform_print_uint((unsigned int)scheduler_task_count());
    platform_print("\n\n");

    intent_status();
    platform_print("\n");

    scheduler_list_tasks();
    platform_print("\n");

    module_list();
}

static void shell_handle_dash(void) {
    int module_broken = module_has_broken_dependencies();
    int task_broken = scheduler_has_broken_tasks();
    int security_ok = security_doctor_ok();
    int lang_ok = lang_doctor_ok();
    int platform_ok = platform_doctor_ok();

    platform_print("Lingjing Dash\n");
    platform_print("-------------\n");

    platform_print("up:       ");
    platform_print_uint(platform_seconds());
    platform_print("s\n");

    platform_print("next:     ");
    platform_print_hex32(memory_get_placement_address());
    platform_print("\n");

    platform_print("platform: ");
    platform_print(platform_get_name());
    platform_print("\n");

    platform_print("display:  ");
    platform_print(platform_get_display());
    platform_print("\n");

    platform_print("intent:   ");
    if (intent_is_running()) {
        platform_print(intent_get_current_name());
    } else {
        platform_print("none");
    }
    platform_print("\n");

    platform_print("modules:  ");
    platform_print_uint((unsigned int)module_count_loaded());
    platform_print("\n");

    platform_print("tasks:    ");
    platform_print_uint((unsigned int)scheduler_task_count());
    platform_print("\n");

    platform_print("deps:     ");
    platform_print(module_broken ? "bad\n" : "ok\n");

    platform_print("task:     ");
    platform_print(task_broken ? "bad\n" : "ok\n");

    platform_print("security: ");
    platform_print(security_ok ? "ok\n" : "bad\n");

    platform_print("lang:     ");
    platform_print(lang_get_current_name());
    platform_print("\n");

    platform_print("platform health: ");
    platform_print(platform_ok ? "ok\n" : "bad\n");

    platform_print("doc:      ");
    if (module_broken || task_broken || !security_ok || !lang_ok || !platform_ok) {
        platform_print("bad\n");
    } else {
        platform_print("ready\n");
    }
}

static void shell_handle_security(void) {
    security_status();
}

static void shell_handle_securitycheck(void) {
    security_check();
}

static void shell_handle_securitylog(void) {
    security_log();
}

static void shell_handle_securityclear(void) {
    security_clear_log();
}

static void shell_handle_platform(void) {
    platform_status();
}

static void shell_handle_platformcheck(void) {
    platform_check();
}

static void shell_handle_lang(const char* cmd) {
    if (str_equal(cmd, "lang")) {
        platform_print(lang_get(MSG_LANGUAGE_CURRENT));
        platform_print(": ");
        platform_print(lang_get_current_name());
        platform_print("\n");
        return;
    }

    if (str_equal(cmd, "lang en")) {
        lang_set(LANG_EN);
        platform_print(lang_get(MSG_LANGUAGE_SET_EN));
        platform_print("\n");
        return;
    }

    if (str_equal(cmd, "lang zh")) {
        lang_set(LANG_ZH);
        platform_print(lang_get(MSG_LANGUAGE_SET_ZH));
        platform_print("\n");
        return;
    }

    platform_print("usage: lang | lang en | lang zh\n");
}

static void shell_handle_doctor(void) {
    int module_broken = module_has_broken_dependencies();
    int task_broken = scheduler_has_broken_tasks();
    int security_ok = security_doctor_ok();
    int language_ok = lang_doctor_ok();
    int platform_ok = platform_doctor_ok();

    platform_print("System doctor:\n");

    platform_print("  module dependencies: ");
    if (module_broken) {
        platform_print("broken\n");
    } else {
        platform_print("ok\n");
    }

    platform_print("  task health:         ");
    if (task_broken) {
        platform_print("broken\n");
    } else {
        platform_print("ok\n");
    }

    platform_print("  scheduler:           ");
    if (task_broken) {
        platform_print("broken\n");
    } else {
        platform_print("ok\n");
    }

    platform_print("  scheduler active:    ");
    if (scheduler_has_broken_tasks()) {
        platform_print("broken\n");
    } else {
        platform_print("ok\n");
    }

    platform_print("  security:            ");
    if (security_ok) {
        platform_print("ok\n");
    } else {
        platform_print("broken\n");
    }

    platform_print("  language layer:      ");
    if (language_ok) {
        platform_print("ok\n");
    } else {
        platform_print("broken\n");
    }

    platform_print("  platform layer:      ");
    if (platform_ok) {
        platform_print("ok\n");
    } else {
        platform_print("broken\n");
    }

    platform_print("  current platform:    ");
    platform_print(platform_get_name());
    platform_print("\n");

    platform_print("  current language:    ");
    platform_print(lang_get_current_name());
    platform_print("\n");

    platform_print("  intent system:       ");
    if (module_broken || task_broken || !security_ok || !language_ok || !platform_ok) {
        platform_print("blocked\n");
    } else {
        platform_print("ready\n");
    }

    platform_print("  result:              ");
    if (module_broken || task_broken || !security_ok || !language_ok || !platform_ok) {
        platform_print("blocked\n");
    } else {
        platform_print("ready\n");
    }
}

static void shell_handle_health(void) {
    int module_broken = module_has_broken_dependencies();
    int task_broken = scheduler_has_broken_tasks();
    int security_ok = security_doctor_ok();
    int lang_ok = lang_doctor_ok();
    int platform_ok = platform_doctor_ok();

    platform_print("System health:\n");

    platform_print("  deps:     ");
    platform_print(module_broken ? "bad\n" : "ok\n");

    platform_print("  security: ");
    platform_print(security_ok ? "ok\n" : "bad\n");

    platform_print("  task:     ");
    platform_print(task_broken ? "bad\n" : "ok\n");

    platform_print("  lang:     ");
    platform_print(lang_ok ? "ok\n" : "bad\n");

    platform_print("  platform: ");
    platform_print(platform_ok ? "ok\n" : "bad\n");

    platform_print("  result:   ");

    if (module_broken || task_broken || !security_ok || !lang_ok || !platform_ok) {
        platform_print("bad\n");
    } else {
        platform_print("ok\n");
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
        platform_print("usage: taskstate <id> <state>\n");
        return;
    }

    scheduler_set_task_state(id, state);
}

static void shell_handle_status(void) {
    int module_broken = module_has_broken_dependencies();
    int task_broken = scheduler_has_broken_tasks();

    platform_print("LJ | up ");
    platform_print_uint(platform_seconds());
    platform_print("s | in ");

    if (intent_is_running()) {
        platform_print(intent_get_current_name());
    } else {
        platform_print("none");
    }

    platform_print(" | m");
    platform_print_uint((unsigned int)module_count_loaded());

    platform_print(" t");
    platform_print_uint((unsigned int)scheduler_task_count());

    platform_print(" | s");
    platform_print_uint(scheduler_get_ticks());

    platform_print(" y");
    platform_print_uint(scheduler_get_yields());

    platform_print(" | coop");

    platform_print(" | deps ");
    platform_print(module_broken ? "bad" : "ok");

    platform_print(" | doc ");
    platform_print((module_broken || task_broken) ? "bad" : "ok");

    platform_print("\n");
}

static void shell_handle_version(void) {
    platform_print(LINGJING_NAME);
    platform_print("\n");

    platform_print("  version: ");
    platform_print(LINGJING_VERSION);
    platform_print("\n");

    platform_print("  stage: ");
    platform_print(LINGJING_STAGE);
    platform_print("\n");

    platform_print("  arch: ");
    platform_print(LINGJING_ARCH);
    platform_print("\n");

    platform_print("  boot: ");
    platform_print(LINGJING_BOOT);
    platform_print("\n");

    platform_print("  platform: ");
    platform_print(platform_get_name());
    platform_print("\n");
}

static void shell_handle_sleep(const char* cmd) {
    const char* seconds_text = cmd + 6;
    unsigned int seconds = parse_uint(seconds_text);

    if (seconds == 0) {
        platform_print("usage: sleep <seconds>\n");
        return;
    }

    platform_print("sleeping ");
    platform_print_uint(seconds);
    platform_print(" seconds...\n");

    platform_sleep(seconds);

    platform_print("wake.\n");
}

static void shell_handle_kmalloc(const char* cmd) {
    const char* size_text = cmd + 8;
    unsigned int size = parse_uint(size_text);

    if (size == 0) {
        platform_print("usage: kmalloc <bytes>\n");
        return;
    }

    void* ptr = kmalloc(size);

    platform_print("allocated ");
    platform_print_uint(size);
    platform_print(" bytes at ");
    platform_print_hex32((unsigned int)ptr);
    platform_print("\n");

    platform_print("next alloc: ");
    platform_print_hex32(memory_get_placement_address());
    platform_print("\n");
}

static void shell_handle_kcalloc(const char* cmd) {
    const char* size_text = cmd + 8;
    unsigned int size = parse_uint(size_text);

    if (size == 0) {
        platform_print("usage: kcalloc <bytes>\n");
        return;
    }

    void* ptr = kcalloc(size);

    platform_print("allocated zeroed ");
    platform_print_uint(size);
    platform_print(" bytes at ");
    platform_print_hex32((unsigned int)ptr);
    platform_print("\n");

    platform_print("next alloc: ");
    platform_print_hex32(memory_get_placement_address());
    platform_print("\n");
}

static void shell_handle_peek(const char* cmd) {
    const char* addr_text = cmd + 5;
    unsigned int addr = parse_hex_or_uint(addr_text);

    unsigned char value = *((unsigned char*)addr);

    platform_print("peek ");
    platform_print_hex32(addr);
    platform_print(" = ");
    platform_print_hex32((unsigned int)value);
    platform_print("\n");
}

static void shell_handle_poke(const char* cmd) {
    const char* args = cmd + 5;
    unsigned int addr = parse_hex_or_uint(args);
    const char* value_text = skip_token(args);
    unsigned int value = parse_hex_or_uint(value_text);

    *((unsigned char*)addr) = (unsigned char)(value & 0xFF);

    platform_print("poke ");
    platform_print_hex32(addr);
    platform_print(" = ");
    platform_print_hex32((unsigned int)(value & 0xFF));
    platform_print("\n");
}

static void shell_handle_hexdump(const char* cmd) {
    const char* args = cmd + 8;
    unsigned int addr = parse_hex_or_uint(args);
    const char* len_text = shell_skip_token(args);
    unsigned int len = parse_uint(len_text);

    if (len == 0) {
        platform_print("usage: hexdump <addr> <len>\n");
        return;
    }

    unsigned char* ptr = (unsigned char*)addr;

    platform_print_hex32(addr);
    platform_print(": ");

    for (unsigned int i = 0; i < len; i++) {
        unsigned int value = (unsigned int)ptr[i];
        const char* hex = "0123456789ABCDEF";

        platform_put_char(hex[(value >> 4) & 0xF]);
        platform_put_char(hex[value & 0xF]);

        if (i + 1 < len) {
            platform_put_char(' ');
        }
    }

    platform_print("\n");
}

static void shell_handle_kzero(const char* cmd) {
    const char* args = cmd + 6;
    unsigned int addr = parse_hex_or_uint(args);
    const char* len_text = shell_skip_token(args);
    unsigned int len = parse_uint(len_text);

    if (len == 0) {
        platform_print("usage: kzero <addr> <len>\n");
        return;
    }

    unsigned char* ptr = (unsigned char*)addr;

    for (unsigned int i = 0; i < len; i++) {
        ptr[i] = 0;
    }

    platform_print("zeroed ");
    platform_print_uint(len);
    platform_print(" bytes at ");
    platform_print_hex32(addr);
    platform_print("\n");
}

static void shell_handle_command(const char* cmd) {
    if (str_equal(cmd, "help")) {
        platform_print("commands: help, clear, about, version, sysinfo, dashboard, dash, status, doctor, health, platform, platformcheck, security, securitycheck, securitylog, securityclear, lang, tasks, taskinfo, taskstate, taskcheck, taskdoctor, schedinfo, schedlog, schedclear, schedreset, schedvalidate, schedfix, runqueue, yield, modules, moduleinfo, moduledeps, moduletree, modulecheck, modulebreak, modulefix, load, unload, intent, echo, mem, uptime, sleep, reboot, halt, kmalloc, kcalloc, peek, poke, hexdump, kzero\n");
    } else if (str_equal(cmd, "clear")) {
        platform_clear();
    } else if (str_equal(cmd, "about")) {
        platform_print("Lingjing OS experimental kernel.\n");
    } else if (str_equal(cmd, "version")) {
        shell_handle_version();
    } else if (str_equal(cmd, "sysinfo")) {
        shell_handle_sysinfo();
    } else if (str_equal(cmd, "dashboard")) {
        shell_handle_dashboard();
    } else if (str_equal(cmd, "dash")) {
        shell_handle_dash();
    } else if (str_equal(cmd, "status")) {
        shell_handle_status();
    } else if (str_equal(cmd, "doctor")) {
        shell_handle_doctor();
    } else if (str_equal(cmd, "health")) {
        shell_handle_health();
    } else if (str_equal(cmd, "platform")) {
        shell_handle_platform();
    } else if (str_equal(cmd, "platformcheck")) {
        shell_handle_platformcheck();
    } else if (str_equal(cmd, "lang") || str_starts_with(cmd, "lang ")) {
        shell_handle_lang(cmd);
    } else if (str_equal(cmd, "security")) {
        shell_handle_security();
    } else if (str_equal(cmd, "securitycheck")) {
        shell_handle_securitycheck();
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
        platform_print("rebooting...\n");
        system_reboot();
    } else if (str_equal(cmd, "halt")) {
        platform_print("system halted.\n");
        system_halt();
    } else if (str_equal(cmd, "lingjing")) {
        platform_print("Lingjing core awakened.\n");
        platform_print("My machine, my rules.\n");
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
        platform_print(cmd + 5);
        platform_print("\n");
    } else if (cmd[0] == '\0') {
        return;
    } else {
        platform_print("unknown command: ");
        platform_print(cmd);
        platform_print("\n");
    }
}

void shell_init(void) {
    input_len = 0;
    shell_prompt();
}

void shell_update(void) {
    char c = platform_read_char();

    if (c == 0) {
        return;
    }

    if (c == '\n') {
        platform_put_char('\n');
        input[input_len] = '\0';
        shell_handle_command(input);
        input_len = 0;
        shell_prompt();
    } else if (c == '\b') {
        if (input_len > 0) {
            input_len--;
            platform_put_char('\b');
        }
    } else {
        if (input_len < 127) {
            input[input_len] = c;
            input_len++;
            platform_put_char(c);
        }
    }
}