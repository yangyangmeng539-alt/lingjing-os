#include "health.h"
#include "module.h"
#include "scheduler.h"
#include "security.h"
#include "lang.h"
#include "platform.h"
#include "identity.h"
#include "memory.h"
#include "paging.h"
#include "syscall.h"
#include "user.h"

static int health_ready = 0;

void health_init(void) {
    health_ready = 1;
}

int health_deps_ok(void) {
    return module_has_broken_dependencies() ? 0 : 1;
}

int health_task_ok(void) {
    return scheduler_has_broken_tasks() ? 0 : 1;
}

int health_security_ok(void) {
    return security_doctor_ok();
}

int health_lang_ok(void) {
    return lang_doctor_ok();
}

int health_platform_ok(void) {
    return platform_doctor_ok();
}

int health_identity_ok(void) {
    return identity_doctor_ok();
}

int health_memory_ok(void) {
    return memory_doctor_ok();
}

int health_paging_ok(void) {
    return paging_doctor_ok();
}

int health_task_switch_ok(void) {
    return scheduler_task_switch_doctor_ok();
}

int health_syscall_ok(void) {
    return syscall_doctor_ok();
}

int health_user_ok(void) {
    return user_doctor_ok();
}

int health_result_ok(void) {
    if (!health_ready) {
        return 0;
    }

    if (!health_deps_ok()) {
        return 0;
    }

    if (!health_task_ok()) {
        return 0;
    }

    if (!health_security_ok()) {
        return 0;
    }

    if (!health_lang_ok()) {
        return 0;
    }

    if (!health_platform_ok()) {
        return 0;
    }

     if (!health_identity_ok()) {
        return 0;
    }

    if (!health_memory_ok()) {
        return 0;
    }

     if (!health_paging_ok()) {
        return 0;
    }

    if (!health_task_switch_ok()) {
        return 0;
    }

    if (!health_syscall_ok()) {
        return 0;
    }

    if (!health_user_ok()) {
        return 0;
    }

    return 1;
}

void health_print(void) {
    platform_print("System health:\n");

    platform_print("  deps:     ");
    platform_print(health_deps_ok() ? "ok\n" : "bad\n");

    platform_print("  security: ");
    platform_print(health_security_ok() ? "ok\n" : "bad\n");

    platform_print("  task:     ");
    platform_print(health_task_ok() ? "ok\n" : "bad\n");

    platform_print("  lang:     ");
    platform_print(health_lang_ok() ? "ok\n" : "bad\n");

    platform_print("  platform: ");
    platform_print(health_platform_ok() ? "ok\n" : "bad\n");

    platform_print("  identity: ");
    platform_print(health_identity_ok() ? "ok\n" : "bad\n");

    platform_print("  memory:   ");
    platform_print(health_memory_ok() ? "ok\n" : "bad\n");

    platform_print("  paging:   ");
    platform_print(health_paging_ok() ? "ok\n" : "bad\n");

    platform_print("  switch:   ");
    platform_print(health_task_switch_ok() ? "ok\n" : "bad\n");

    platform_print("  syscall:  ");
    platform_print(health_syscall_ok() ? "ok\n" : "bad\n");

    platform_print("  user:     ");
    platform_print(health_user_ok() ? "ok\n" : "bad\n");

    platform_print("  result:   ");
    platform_print(health_result_ok() ? "ok\n" : "bad\n");
}