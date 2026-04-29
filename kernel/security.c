#include "security.h"
#include "screen.h"
#include "platform.h"

#define SECURITY_LOG_MAX 8

static int security_policy_enabled = 1;
static int security_module_protection_enabled = 1;
static int security_intent_permission_enabled = 1;
static int security_audit_enabled = 1;

static const char* security_logs[SECURITY_LOG_MAX];
static int security_log_count = 0;

static int security_str_equal(const char* a, const char* b) {
    int i = 0;

    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }

        i++;
    }

    return a[i] == '\0' && b[i] == '\0';
}

void security_init(void) {
    security_policy_enabled = 1;
    security_module_protection_enabled = 1;
    security_intent_permission_enabled = 1;
    security_audit_enabled = 1;

    security_log_count = 0;

    for (int i = 0; i < SECURITY_LOG_MAX; i++) {
        security_logs[i] = 0;
    }

    security_audit("security initialized");
}

void security_audit(const char* event) {
    if (!security_audit_enabled) {
        return;
    }

    if (security_log_count < SECURITY_LOG_MAX) {
        security_logs[security_log_count] = event;
        security_log_count++;
        return;
    }

    for (int i = 1; i < SECURITY_LOG_MAX; i++) {
        security_logs[i - 1] = security_logs[i];
    }

    security_logs[SECURITY_LOG_MAX - 1] = event;
}

void security_status(void) {
    platform_print("Security:\n");

    platform_print("  policy:            ");
    platform_print(security_policy_enabled ? "enabled\n" : "disabled\n");

    platform_print("  module protection: ");
    platform_print(security_module_protection_enabled ? "enabled\n" : "disabled\n");

    platform_print("  intent permission: ");
    platform_print(security_intent_permission_enabled ? "enabled\n" : "disabled\n");

    platform_print("  audit:             ");
    platform_print(security_audit_enabled ? "enabled\n" : "disabled\n");

    platform_print("  trust level:       prototype\n");
}

void security_check(void) {
    platform_print("Security check:\n");

    platform_print("  policy:            ");
    platform_print(security_policy_enabled ? "ok\n" : "broken\n");

    platform_print("  module protection: ");
    platform_print(security_module_protection_enabled ? "ok\n" : "broken\n");

    platform_print("  intent permission: ");
    platform_print(security_intent_permission_enabled ? "ok\n" : "broken\n");

    platform_print("  audit:             ");
    platform_print(security_audit_enabled ? "ok\n" : "broken\n");

    platform_print("  result:            ");
    platform_print(security_doctor_ok() ? "ok\n" : "broken\n");
}

void security_log(void) {
    platform_print("Security log:\n");

    if (security_log_count == 0) {
        platform_print("  empty\n");
        return;
    }

    for (int i = 0; i < security_log_count; i++) {
        platform_print("  ");
        platform_print(security_logs[i]);
        platform_print("\n");
    }
}

void security_clear_log(void) {
    for (int i = 0; i < SECURITY_LOG_MAX; i++) {
        security_logs[i] = 0;
    }

    security_log_count = 0;

    platform_print("Security log cleared.\n");
}

int security_check_intent(const char* intent_name) {
    if (!security_policy_enabled) {
        return 1;
    }

    if (intent_name == 0) {
        security_audit("blocked intent: null");
        return 0;
    }

    security_audit("intent check passed");
    return 1;
}

int security_check_module_load(const char* module_name) {
    if (!security_policy_enabled) {
        return 1;
    }

    if (module_name == 0) {
        security_audit("blocked module load: null");
        return 0;
    }

    security_audit("module load check passed");
    return 1;
}

int security_check_module_unload(const char* module_name, const char* module_type) {
    if (!security_policy_enabled) {
        return 1;
    }

    if (module_name == 0 || module_type == 0) {
        security_audit("blocked module unload: null");
        return 0;
    }

    if (security_module_protection_enabled && !security_str_equal(module_type, "external")) {
        security_audit("blocked core module unload");
        return 0;
    }

    security_audit("module unload check passed");
    return 1;
}

int security_doctor_ok(void) {
    if (!security_policy_enabled) {
        return 0;
    }

    if (!security_module_protection_enabled) {
        return 0;
    }

    if (!security_intent_permission_enabled) {
        return 0;
    }

    if (!security_audit_enabled) {
        return 0;
    }

    return 1;
}