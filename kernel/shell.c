#include "shell.h"
#include "screen.h"
#include "keyboard.h"
#include "memory.h"
#include "timer.h"
#include "system.h"
#include "memory.h"
#include "paging.h"
#include "timer.h"
#include "module.h"
#include "intent.h"
#include "version.h"
#include "security.h"
#include "syscall.h"
#include "user.h"
#include "ring3.h"
#include "lang.h"
#include "platform.h"
#include "scheduler.h"
#include "health.h"
#include "identity.h"

extern unsigned int kernel_stack_marker;

extern unsigned int start;
extern unsigned int end;

static char input[128];
static int input_len = 0;

static const char* shell_skip_token(const char* text);

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

static void shell_prompt(void) {
    platform_print("> ");
}

static void shell_handle_mem(void) {
    memory_print_status();
}

static void shell_handle_paging(const char* cmd) {
    if (str_equal(cmd, "paging")) {
        paging_status();
        return;
    }

    if (str_equal(cmd, "paging status")) {
        paging_status();
        return;
    }

    if (str_equal(cmd, "paging check")) {
        paging_check();
        return;
    }

    if (str_equal(cmd, "paging doctor")) {
        paging_doctor();
        return;
    }

    if (str_equal(cmd, "paging enable")) {
        paging_enable();
        return;
    }

    if (str_equal(cmd, "paging stats")) {
        paging_stats();
        return;
    }

    if (str_starts_with(cmd, "paging flags ")) {
        const char* addr_text = cmd + 13;
        unsigned int addr = parse_hex_or_uint(addr_text);

        paging_flags(addr);
        return;
    }

    if (str_equal(cmd, "paging flags")) {
        platform_print("usage: paging flags <addr>\n");
        return;
    }

    if (str_starts_with(cmd, "paging map ")) {
        const char* addr_text = cmd + 11;
        unsigned int addr = parse_hex_or_uint(addr_text);

        paging_map_check(addr);
        return;
    }

    if (str_equal(cmd, "paging map")) {
        platform_print("usage: paging map <addr>\n");
        return;
    }

    platform_print("usage: paging | paging status | paging check | paging doctor | paging map <addr> | paging flags <addr> | paging stats | paging enable\n");
}

static void shell_handle_pagingbreak(void) {
    paging_break();
}

static void shell_handle_pagingfix(void) {
    paging_fix();
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

    platform_print("  platform init:  ");
    platform_print(platform_doctor_ok() ? "ok\n" : "broken\n");

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
    platform_print(" | ");
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
    platform_print(health_deps_ok() ? "ok\n" : "bad\n");

    platform_print("task:     ");
    platform_print(health_task_ok() ? "ok\n" : "bad\n");

    platform_print("security: ");
    platform_print(health_security_ok() ? "ok\n" : "bad\n");

    platform_print("lang:     ");
    platform_print(lang_get_current_name());
    platform_print("\n");

    platform_print("platform init: ");
    platform_print(health_platform_ok() ? "ok\n" : "bad\n");

    platform_print("platform health: ");
    platform_print(health_platform_ok() ? "ok\n" : "bad\n");

    platform_print("doc:      ");
    platform_print(health_result_ok() ? "ready\n" : "bad\n");
}

static void shell_handle_security(void) {
    security_status();
}

static void shell_handle_syscall(const char* cmd) {
    if (str_equal(cmd, "syscall")) {
        syscall_status();
        return;
    }

    if (str_equal(cmd, "syscall status")) {
        syscall_status();
        return;
    }

    if (str_equal(cmd, "syscall check")) {
        syscall_check();
        return;
    }

    if (str_equal(cmd, "syscall doctor")) {
        syscall_doctor();
        return;
    }

    if (str_equal(cmd, "syscall table")) {
        syscall_table();
        return;
    }

    if (str_equal(cmd, "syscall stats")) {
        syscall_stats();
        return;
    }

    if (str_equal(cmd, "syscall interrupt")) {
        syscall_interrupt_status();
        return;
    }

    if (str_equal(cmd, "syscall frame")) {
        syscall_frame();
        return;
    }

    if (str_equal(cmd, "syscall ret")) {
        syscall_return_status();
        return;
    }

    if (str_starts_with(cmd, "syscall realargs ")) {
        const char* id_text = cmd + 17;
        unsigned int id = parse_uint(id_text);
        const char* arg1_text = shell_skip_token(id_text);
        unsigned int arg1 = parse_uint(arg1_text);
        const char* arg2_text = shell_skip_token(arg1_text);
        unsigned int arg2 = parse_uint(arg2_text);
        const char* arg3_text = shell_skip_token(arg2_text);
        unsigned int arg3 = parse_uint(arg3_text);

        if (arg1_text[0] == '\0' || arg2_text[0] == '\0' || arg3_text[0] == '\0') {
            platform_print("usage: syscall realargs <id> <a> <b> <c>\n");
            return;
        }

        syscall_trigger_int80_args(id, arg1, arg2, arg3);
        return;
    }

    if (str_equal(cmd, "syscall realargs")) {
        platform_print("usage: syscall realargs <id> <a> <b> <c>\n");
        return;
    }

    if (str_starts_with(cmd, "syscall real ")) {
        const char* id_text = cmd + 13;
        unsigned int id = parse_uint(id_text);

        syscall_trigger_int80(id);
        return;
    }

    if (str_equal(cmd, "syscall real")) {
        platform_print("usage: syscall real <id>\n");
        return;
    }

    if (str_starts_with(cmd, "syscall int ")) {
        const char* id_text = cmd + 12;
        unsigned int id = parse_uint(id_text);

        syscall_interrupt_dispatch(id);
        return;
    }

    if (str_equal(cmd, "syscall int")) {
        platform_print("usage: syscall int <id>\n");
        return;
    }

    if (str_starts_with(cmd, "syscall call ")) {
        const char* id_text = cmd + 13;
        unsigned int id = parse_uint(id_text);

        syscall_dispatch(id);
        return;
    }

    if (str_equal(cmd, "syscall call")) {
        platform_print("usage: syscall call <id>\n");
        return;
    }

    platform_print("usage: syscall | syscall status | syscall check | syscall doctor | syscall table | syscall stats | syscall interrupt | syscall frame | syscall ret | syscall realargs <id> <a> <b> <c> | syscall real <id> | syscall int <id> | syscall call <id>\n");
}

static void shell_handle_user(const char* cmd) {
    if (str_equal(cmd, "user")) {
        user_status();
        return;
    }

    if (str_equal(cmd, "user status")) {
        user_status();
        return;
    }

    if (str_equal(cmd, "user check")) {
        user_check();
        return;
    }

    if (str_equal(cmd, "user doctor")) {
        user_doctor();
        return;
    }

    if (str_equal(cmd, "user programs")) {
        user_programs();
        return;
    }

    if (str_equal(cmd, "user entries")) {
        user_entries();
        return;
    }

    if (str_equal(cmd, "user segments")) {
        user_segments();
        return;
    }

    if (str_equal(cmd, "user stack")) {
        user_stack();
        return;
    }

    if (str_equal(cmd, "user stackcheck")) {
        user_stack_check();
        return;
    }

    if (str_equal(cmd, "user stackbreak")) {
        user_stack_break();
        return;
    }

    if (str_equal(cmd, "user stackfix")) {
        user_stack_fix();
        return;
    }

    if (str_equal(cmd, "user frame")) {
        user_frame();
        return;
    }

    if (str_equal(cmd, "user framecheck")) {
        user_frame_check();
        return;
    }

    if (str_equal(cmd, "user framebreak")) {
        user_frame_break();
        return;
    }

    if (str_equal(cmd, "user framefix")) {
        user_frame_fix();
        return;
    }

    if (str_equal(cmd, "user boundary")) {
        user_boundary();
        return;
    }

    if (str_equal(cmd, "user boundarycheck")) {
        user_boundary_check();
        return;
    }

    if (str_equal(cmd, "user boundarybreak")) {
        user_boundary_break();
        return;
    }

    if (str_equal(cmd, "user boundaryfix")) {
        user_boundary_fix();
        return;
    }

    if (str_equal(cmd, "user prepare")) {
        user_prepare();
        return;
    }

    if (str_equal(cmd, "user stats")) {
        user_stats();
        return;
    }

    platform_print("usage: user | user status | user check | user doctor | user programs | user entries | user segments | user stack | user stackcheck | user stackbreak | user stackfix | user frame | user framecheck | user framebreak | user framefix | user boundary | user boundarycheck | user boundarybreak | user boundaryfix | user prepare | user stats\n");
}

static void shell_handle_userbreak(void) {
    user_break();
}

static void shell_handle_userfix(void) {
    user_fix();
}

static void shell_handle_ring3(const char* cmd) {
    if (str_equal(cmd, "ring3")) {
        ring3_status();
        return;
    }

    if (str_equal(cmd, "ring3 status")) {
        ring3_status();
        return;
    }

    if (str_equal(cmd, "ring3 check")) {
        ring3_check();
        return;
    }

    if (str_equal(cmd, "ring3 doctor")) {
        ring3_doctor();
        return;
    }

    if (str_equal(cmd, "ring3 tss")) {
        ring3_tss();
        return;
    }

    if (str_equal(cmd, "ring3 tsscheck")) {
        ring3_tss_check();
        return;
    }

    if (str_equal(cmd, "ring3 tssload")) {
        ring3_tss_load();
        return;
    }

    if (str_equal(cmd, "ring3 tssinstall")) {
        ring3_tss_install();
        return;
    }

    if (str_equal(cmd, "ring3 tssclear")) {
        ring3_tss_clear();
        return;
    }

    if (str_equal(cmd, "ring3 stack")) {
        ring3_stack();
        return;
    }

    if (str_equal(cmd, "ring3 frame")) {
        ring3_frame();
        return;
    }

    if (str_equal(cmd, "ring3 gdt")) {
        ring3_gdt();
        return;
    }

    if (str_equal(cmd, "ring3 gdtcheck")) {
        ring3_gdt_check();
        return;
    }

    if (str_equal(cmd, "ring3 gdtprepare")) {
        ring3_gdt_prepare();
        return;
    }

    if (str_equal(cmd, "ring3 gdtinstall")) {
        ring3_gdt_install();
        return;
    }

    if (str_equal(cmd, "ring3 gdtclear")) {
        ring3_gdt_clear();
        return;
    }

    if (str_equal(cmd, "ring3 page")) {
        ring3_page();
        return;
    }

    if (str_equal(cmd, "ring3 pagecheck")) {
        ring3_page_check();
        return;
    }

    if (str_equal(cmd, "ring3 pageprepare")) {
        ring3_page_prepare();
        return;
    }

    if (str_equal(cmd, "ring3 pageclear")) {
        ring3_page_clear();
        return;
    }

    if (str_equal(cmd, "ring3 hw")) {
        ring3_hw();
        return;
    }

    if (str_equal(cmd, "ring3 hwcheck")) {
        ring3_hw_check();
        return;
    }

    if (str_equal(cmd, "ring3 hwinstall")) {
        ring3_hw_install();
        return;
    }

    if (str_equal(cmd, "ring3 hwclear")) {
        ring3_hw_clear();
        return;
    }

    if (str_equal(cmd, "ring3 stub")) {
        ring3_stub();
        return;
    }

    if (str_equal(cmd, "ring3 stubcheck")) {
        ring3_stub_check();
        return;
    }

    if (str_equal(cmd, "ring3 guard")) {
        ring3_guard();
        return;
    }

    if (str_equal(cmd, "ring3 enter")) {
        ring3_enter();
        return;
    }

    if (str_equal(cmd, "ring3 dryrun")) {
        ring3_dryrun();
        return;
    }

    if (str_equal(cmd, "ring3 realenter")) {
        ring3_realenter();
        return;
    }

    if (str_equal(cmd, "ring3 arm")) {
        ring3_arm();
        return;
    }

    if (str_equal(cmd, "ring3 disarm")) {
        ring3_disarm();
        return;
    }

    if (str_equal(cmd, "ring3 enableswitch")) {
        ring3_enable_switch();
        return;
    }

    if (str_equal(cmd, "ring3 disableswitch")) {
        ring3_disable_switch();
        return;
    }

    platform_print("usage: ring3 | ring3 status | ring3 check | ring3 doctor | ring3 tss | ring3 tsscheck | ring3 tssload | ring3 tssinstall | ring3 tssclear | ring3 stack | ring3 frame | ring3 gdt | ring3 gdtcheck | ring3 gdtprepare | ring3 gdtinstall | ring3 gdtclear | ring3 page | ring3 pagecheck | ring3 pageprepare | ring3 pageclear | ring3 hw | ring3 hwcheck | ring3 hwinstall | ring3 hwclear | ring3 stub | ring3 stubcheck | ring3 guard | ring3 enter | ring3 dryrun | ring3 realenter | ring3 arm | ring3 disarm | ring3 enableswitch | ring3 disableswitch\n");
}

static void shell_handle_ring3break(void) {
    ring3_break();
}

static void shell_handle_ring3fix(void) {
    ring3_fix();
}

static void shell_handle_syscallbreak(void) {
    syscall_break();
}

static void shell_handle_syscallfix(void) {
    syscall_fix();
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

static void shell_handle_platformdeps(void) {
    platform_deps();
}

static void shell_handle_platformboot(void) {
    platform_boot_info();
}

static void shell_handle_platformsummary(void) {
    platform_summary();
}

static void shell_handle_platformcaps(void) {
    platform_caps();
}

static void shell_handle_platformbreak(const char* cmd) {
    const char* capability = cmd + 14;

    if (capability[0] == '\0') {
        platform_print("usage: platformbreak <capability>\n");
        platform_print("allowed: output, input, timer, power, all\n");
        return;
    }

    platform_break(capability);
}

static void shell_handle_platformfix(const char* cmd) {
    const char* capability = cmd + 12;

    if (capability[0] == '\0') {
        platform_print("usage: platformfix <capability>\n");
        platform_print("allowed: output, input, timer, power, all\n");
        return;
    }

    platform_fix(capability);
}

static void shell_handle_identity(const char* cmd) {
    if (str_equal(cmd, "identity")) {
        identity_status();
        return;
    }

    if (str_equal(cmd, "identity status")) {
        identity_status();
        return;
    }

    if (str_equal(cmd, "identity doctor")) {
        identity_doctor();
        return;
    }

    if (str_equal(cmd, "identity check")) {
        identity_check();
        return;
    }

    if (str_equal(cmd, "identity break")) {
        identity_break();
        return;
    }

    if (str_equal(cmd, "identity fix")) {
        identity_fix();
        return;
    }

    platform_print("usage: identity | identity status | identity doctor | identity check | identity break | identity fix\n");
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
    platform_print("System doctor:\n");

    platform_print("  module dependencies: ");
    platform_print(health_deps_ok() ? "ok\n" : "broken\n");

    platform_print("  task health:         ");
    platform_print(health_task_ok() ? "ok\n" : "broken\n");

    platform_print("  scheduler:           ");
    platform_print(health_task_ok() ? "ok\n" : "broken\n");

    platform_print("  scheduler active:    ");
    platform_print(health_task_ok() ? "ok\n" : "broken\n");

    platform_print("  task switch layer:   ");
    platform_print(health_task_switch_ok() ? "ok\n" : "broken\n");

    platform_print("  syscall layer:       ");
    platform_print(health_syscall_ok() ? "ok\n" : "broken\n");

    platform_print("  user mode layer:     ");
    platform_print(health_user_ok() ? "ok\n" : "broken\n");

    platform_print("  ring3 layer:         ");
    platform_print(health_ring3_ok() ? "ok\n" : "broken\n");

    platform_print("  security:            ");
    platform_print(health_security_ok() ? "ok\n" : "broken\n");

    platform_print("  language layer:      ");
    platform_print(health_lang_ok() ? "ok\n" : "broken\n");

    platform_print("  platform layer:      ");
    platform_print(health_platform_ok() ? "ok\n" : "broken\n");

    platform_print("  identity layer:      ");
    platform_print(health_identity_ok() ? "ok\n" : "broken\n");

    platform_print("  memory layer:        ");
    platform_print(health_memory_ok() ? "ok\n" : "broken\n");

    platform_print("  paging layer:        ");
    platform_print(health_paging_ok() ? "ok\n" : "broken\n");

    platform_print("  current platform:    ");
    platform_print(platform_get_name());
    platform_print("\n");

    platform_print("  current language:    ");
    platform_print(lang_get_current_name());
    platform_print("\n");

    platform_print("  intent system:       ");
    platform_print(health_result_ok() ? "ready\n" : "blocked\n");

    platform_print("  result:              ");
    platform_print(health_result_ok() ? "ready\n" : "blocked\n");
}

static void shell_handle_health(void) {
    health_print();
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

static void shell_handle_taskswitch(void) {
    scheduler_task_switch();
}

static void shell_handle_taskswitchcheck(void) {
    scheduler_task_switch_check();
}

static void shell_handle_taskswitchdoctor(void) {
    scheduler_task_switch_doctor();
}

static void shell_handle_taskswitchbreak(void) {
    scheduler_task_switch_break();
}

static void shell_handle_taskswitchfix(void) {
    scheduler_task_switch_fix();
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

static void shell_handle_taskcreate(const char* cmd) {
    const char* name = cmd + 11;

    if (name[0] == '\0') {
        platform_print("usage: taskcreate <name>\n");
        return;
    }

    scheduler_create_task(name);
}

static void shell_handle_taskkill(const char* cmd) {
    const char* id_text = cmd + 9;
    unsigned int id = parse_uint(id_text);

    scheduler_kill_task(id);
}

static void shell_handle_tasksleep(const char* cmd) {
    const char* id_text = cmd + 10;
    unsigned int id = parse_uint(id_text);

    scheduler_sleep_task(id);
}

static void shell_handle_taskwake(const char* cmd) {
    const char* id_text = cmd + 9;
    unsigned int id = parse_uint(id_text);

    scheduler_wake_task(id);
}

static void shell_handle_taskprio(const char* cmd) {
    const char* id_text = cmd + 9;
    unsigned int id = parse_uint(id_text);
    const char* prio_text = shell_skip_token(id_text);
    unsigned int priority = parse_uint(prio_text);

    if (prio_text[0] == '\0') {
        platform_print("usage: taskprio <id> <priority>\n");
        return;
    }

    scheduler_set_task_priority(id, priority);
}

static void shell_handle_taskexit(const char* cmd) {
    const char* id_text = cmd + 9;
    unsigned int id = parse_uint(id_text);
    const char* code_text = shell_skip_token(id_text);
    unsigned int exit_code = parse_uint(code_text);

    if (code_text[0] == '\0') {
        platform_print("usage: taskexit <id> <code>\n");
        return;
    }

    scheduler_exit_task(id, exit_code);
}

static void shell_handle_taskbreak(const char* cmd) {
    const char* id_text = cmd + 10;
    unsigned int id = parse_uint(id_text);

    scheduler_break_task(id);
}

static void shell_handle_taskfix(const char* cmd) {
    const char* id_text = cmd + 8;
    unsigned int id = parse_uint(id_text);

    scheduler_fix_task(id);
}

static void shell_handle_taskstats(void) {
    scheduler_task_stats();
}

static void shell_handle_status(void) {
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
    platform_print(health_deps_ok() ? "ok" : "bad");

    platform_print(" | doc ");
    platform_print(health_result_ok() ? "ok" : "bad");

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

    unsigned int ptr = kmalloc(size);

    platform_print("allocated ");
    platform_print_uint(size);
    platform_print(" bytes at ");
    platform_print_hex32(ptr);
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

    unsigned int ptr = kcalloc(size);

    platform_print("allocated zeroed ");
    platform_print_uint(size);
    platform_print(" bytes at ");
    platform_print_hex32(ptr);
    platform_print("\n");

    platform_print("next alloc: ");
    platform_print_hex32(memory_get_placement_address());
    platform_print("\n");
}

static void shell_handle_kfree(const char* cmd) {
    const char* addr_text = cmd + 6;
    unsigned int addr = parse_hex_or_uint(addr_text);

    if (addr == 0) {
        platform_print("usage: kfree <addr>\n");
        return;
    }

    kfree(addr);

    platform_print("freed ");
    platform_print_hex32(addr);
    platform_print("\n");

    platform_print("free list: ");
    platform_print_uint(memory_get_free_list_count());
    platform_print("\n");
}

static void shell_handle_heapcheck(void) {
    memory_check();
}

static void shell_handle_heapdoctor(void) {
    memory_doctor();
}

static void shell_handle_heapstats(void) {
    memory_stats();
}

static void shell_handle_heapbreak(void) {
    memory_break();
}

static void shell_handle_heapfix(void) {
    memory_fix();
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
        platform_print("commands: help, clear, about, version, sysinfo, dashboard, dash, status, doctor, health, identity, platform, platformcheck, platformdeps, platformboot, platformsummary, platformcaps, platformbreak, platformfix, security, securitycheck, syscall, syscall table, syscall stats, syscall interrupt, syscall frame, syscall ret, syscall realargs, syscall real, syscall int, syscall call, syscallbreak, syscallfix, user, user programs, user entries, user segments, user stack, user stackcheck, user stackbreak, user stackfix, user frame, user framecheck, user framebreak, user framefix, user boundary, user boundarycheck, user boundarybreak, user boundaryfix, user prepare, user stats, userbreak, userfix, ring3, ring3 check, ring3 doctor, ring3 tss, ring3 tsscheck, ring3 tssload, ring3 tssinstall, ring3 tssclear, ring3 stack, ring3 frame, ring3 gdt, ring3 gdtcheck, ring3 gdtprepare, ring3 gdtinstall, ring3 gdtclear, ring3 page, ring3 pagecheck, ring3 pageprepare, ring3 pageclear, ring3 hw, ring3 hwcheck, ring3 hwinstall, ring3 hwclear, ring3 stub, ring3 stubcheck, ring3 guard, ring3 enter, ring3 dryrun, ring3 realenter, ring3 arm, ring3 disarm, ring3 enableswitch, ring3 disableswitch, ring3break, ring3fix, securitylog, securityclear, lang, tasks, taskinfo, taskstate, taskcreate, taskkill, tasksleep, taskwake, taskprio, taskexit, taskbreak, taskfix, taskstats, taskcheck, taskdoctor, schedinfo, schedlog, schedclear, schedreset, schedvalidate, schedfix, taskswitch, taskswitchcheck, taskswitchdoctor, taskswitchbreak, taskswitchfix, runqueue, yield, modules, moduleinfo, moduledeps, moduletree, modulecheck, modulebreak, modulefix, load, unload, intent, echo, mem, paging, paging map, paging flags, paging stats, paging enable, pagingbreak, pagingfix, uptime, sleep, reboot, halt, kmalloc, kcalloc, kfree, heapcheck, heapdoctor, heapstats, heapbreak, heapfix, peek, poke, hexdump, kzero\n");
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
    } else if (str_equal(cmd, "identity") || str_starts_with(cmd, "identity ")) {
        shell_handle_identity(cmd);
    } else if (str_equal(cmd, "platform")) {
        shell_handle_platform();
    } else if (str_equal(cmd, "platformcheck")) {
        shell_handle_platformcheck();
    } else if (str_equal(cmd, "platformdeps")) {
        shell_handle_platformdeps();
    } else if (str_equal(cmd, "platformboot")) {
        shell_handle_platformboot();
    } else if (str_equal(cmd, "platformsummary")) {
        shell_handle_platformsummary();
    } else if (str_equal(cmd, "platformcaps")) {
        shell_handle_platformcaps();
    } else if (str_starts_with(cmd, "platformbreak ")) {
        shell_handle_platformbreak(cmd);
    } else if (str_equal(cmd, "platformbreak")) {
        platform_print("usage: platformbreak <capability>\n");
        platform_print("allowed: output, input, timer, power, all\n");
    } else if (str_starts_with(cmd, "platformfix ")) {
        shell_handle_platformfix(cmd);
    } else if (str_equal(cmd, "platformfix")) {
        platform_print("usage: platformfix <capability>\n");
        platform_print("allowed: output, input, timer, power, all\n");
    } else if (str_equal(cmd, "lang") || str_starts_with(cmd, "lang ")) {
        shell_handle_lang(cmd);
    } else if (str_equal(cmd, "security")) {
        shell_handle_security();
    } else if (str_equal(cmd, "syscall") || str_starts_with(cmd, "syscall ")) {
        shell_handle_syscall(cmd);
    } else if (str_equal(cmd, "syscallbreak")) {
        shell_handle_syscallbreak();
    } else if (str_equal(cmd, "syscallfix")) {
        shell_handle_syscallfix();
    } else if (str_equal(cmd, "user") || str_starts_with(cmd, "user ")) {
        shell_handle_user(cmd);
    } else if (str_equal(cmd, "userbreak")) {
        shell_handle_userbreak();
    } else if (str_equal(cmd, "userfix")) {
        shell_handle_userfix();
    } else if (str_equal(cmd, "ring3") || str_starts_with(cmd, "ring3 ")) {
        shell_handle_ring3(cmd);
    } else if (str_equal(cmd, "ring3break")) {
        shell_handle_ring3break();
    } else if (str_equal(cmd, "ring3fix")) {
        shell_handle_ring3fix();
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
    } else if (str_starts_with(cmd, "taskcreate ")) {
        shell_handle_taskcreate(cmd);
    } else if (str_equal(cmd, "taskcreate")) {
        platform_print("usage: taskcreate <name>\n");
    } else if (str_starts_with(cmd, "taskkill ")) {
        shell_handle_taskkill(cmd);
    } else if (str_equal(cmd, "taskkill")) {
        platform_print("usage: taskkill <id>\n");
    } else if (str_starts_with(cmd, "tasksleep ")) {
        shell_handle_tasksleep(cmd);
    } else if (str_equal(cmd, "tasksleep")) {
        platform_print("usage: tasksleep <id>\n");
    } else if (str_starts_with(cmd, "taskwake ")) {
        shell_handle_taskwake(cmd);
    } else if (str_equal(cmd, "taskwake")) {
        platform_print("usage: taskwake <id>\n");
    } else if (str_starts_with(cmd, "taskprio ")) {
        shell_handle_taskprio(cmd);
    } else if (str_equal(cmd, "taskprio")) {
        platform_print("usage: taskprio <id> <priority>\n");
    } else if (str_starts_with(cmd, "taskexit ")) {
        shell_handle_taskexit(cmd);
    } else if (str_equal(cmd, "taskexit")) {
        platform_print("usage: taskexit <id> <code>\n");
    } else if (str_starts_with(cmd, "taskbreak ")) {
        shell_handle_taskbreak(cmd);
    } else if (str_equal(cmd, "taskbreak")) {
        platform_print("usage: taskbreak <id>\n");
    } else if (str_starts_with(cmd, "taskfix ")) {
        shell_handle_taskfix(cmd);
    } else if (str_equal(cmd, "taskfix")) {
        platform_print("usage: taskfix <id>\n");
    } else if (str_equal(cmd, "taskstats")) {
        shell_handle_taskstats();
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
    } else if (str_equal(cmd, "taskswitch")) {
        shell_handle_taskswitch();
    } else if (str_equal(cmd, "taskswitchcheck")) {
        shell_handle_taskswitchcheck();
    } else if (str_equal(cmd, "taskswitchdoctor")) {
        shell_handle_taskswitchdoctor();
    } else if (str_equal(cmd, "taskswitchbreak")) {
        shell_handle_taskswitchbreak();
    } else if (str_equal(cmd, "taskswitchfix")) {
        shell_handle_taskswitchfix();
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
    } else if (str_equal(cmd, "paging") || str_starts_with(cmd, "paging ")) {
        shell_handle_paging(cmd);
    } else if (str_equal(cmd, "pagingbreak")) {
        shell_handle_pagingbreak();
    } else if (str_equal(cmd, "pagingfix")) {
        shell_handle_pagingfix();
    } else if (str_equal(cmd, "uptime")) {
        shell_handle_uptime();
    } else if (str_starts_with(cmd, "sleep ")) {
        shell_handle_sleep(cmd);
    } else if (str_equal(cmd, "reboot")) {
        platform_print("rebooting...\n");
        platform_reboot();
    } else if (str_equal(cmd, "halt")) {
        platform_print("system halted.\n");
        platform_halt();
    } else if (str_equal(cmd, "lingjing")) {
        platform_print("Lingjing core awakened.\n");
        platform_print("My machine, my rules.\n");
    } else if (str_starts_with(cmd, "kmalloc ")) {
        shell_handle_kmalloc(cmd);
    } else if (str_starts_with(cmd, "kcalloc ")) {
        shell_handle_kcalloc(cmd);
    } else if (str_starts_with(cmd, "kfree ")) {
        shell_handle_kfree(cmd);
    } else if (str_equal(cmd, "kfree")) {
        platform_print("usage: kfree <addr>\n");
    } else if (str_equal(cmd, "heapcheck")) {
        shell_handle_heapcheck();
    } else if (str_equal(cmd, "heapdoctor")) {
        shell_handle_heapdoctor();
    } else if (str_equal(cmd, "heapstats")) {
        shell_handle_heapstats();
    } else if (str_equal(cmd, "heapbreak")) {
        shell_handle_heapbreak();
    } else if (str_equal(cmd, "heapfix")) {
        shell_handle_heapfix();
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