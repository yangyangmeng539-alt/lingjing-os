#include "security.h"
#include "screen.h"
#include "platform.h"

#define SECURITY_LOG_MAX 8

static int security_policy_enabled = 1;
static int security_module_protection_enabled = 1;
static int security_intent_permission_enabled = 1;
static int security_audit_enabled = 1;
static int security_user_control = 1;
static int security_strict_mode = 0;
static int security_block_network = 0;
static int security_block_file = 0;
static int security_block_ai = 0;
static int security_block_video_intent = 0;
static int security_block_ai_intent = 0;
static int security_block_external_modules = 0;
static unsigned int security_allowed_count = 0;
static unsigned int security_blocked_count = 0;
static unsigned int security_decision_count = 0;
static int security_sandbox_on = 1;
static unsigned int security_sandbox_profile_id = 0;
static unsigned int security_sandbox_switches = 0;
static unsigned int security_rule_mode = 0;
static unsigned int security_rule_checks = 0;
static const char* security_rule_last_name = "none";
static const char* security_rule_last_decision = "none";

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
    security_user_control = 1;
    security_strict_mode = 0;
    security_block_network = 0;
    security_block_file = 0;
    security_block_ai = 0;
    security_block_video_intent = 0;
    security_block_ai_intent = 0;
    security_block_external_modules = 0;
    security_allowed_count = 0;
    security_blocked_count = 0;
    security_decision_count = 0;
    security_sandbox_on = 1;
    security_sandbox_profile_id = 0;
    security_sandbox_switches = 0;
    security_rule_mode = 0;
    security_rule_checks = 0;
    security_rule_last_name = "none";
    security_rule_last_decision = "none";

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

    platform_print("  user control:      ");
    platform_print(security_user_control ? "enabled\n" : "disabled\n");

    platform_print("  mode:              ");
    platform_print(security_policy_mode());
    platform_print("\n");

    platform_print("  network policy:    ");
    platform_print(security_network_policy());
    platform_print("\n");

    platform_print("  file policy:       ");
    platform_print(security_file_policy());
    platform_print("\n");

    platform_print("  ai policy:         ");
    platform_print(security_ai_policy());
    platform_print("\n");

    platform_print("  external modules:  ");
    platform_print(security_external_module_policy());
    platform_print("\n");

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

    platform_print("  user control:      ");
    platform_print(security_user_control ? "ok\n" : "broken\n");

    platform_print("  mode:              ");
    platform_print(security_policy_mode());
    platform_print("\n");

    platform_print("  decisions:         ");
    platform_print_uint(security_policy_decision_count());
    platform_print("\n");

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

    security_decision_count++;

    if (intent_name == 0) {
        security_blocked_count++;
        security_audit("blocked intent: null");
        return 0;
    }

    if (security_block_network && security_str_equal(intent_name, "network")) {
        security_blocked_count++;
        security_audit("blocked intent: network");
        return 0;
    }

    if (security_block_ai_intent && security_str_equal(intent_name, "ai")) {
        security_blocked_count++;
        security_audit("blocked intent: ai");
        return 0;
    }

    if (security_block_video_intent && security_str_equal(intent_name, "video")) {
        security_blocked_count++;
        security_audit("blocked intent: video");
        return 0;
    }

    security_allowed_count++;
    security_audit("intent check passed");
    return 1;
}

int security_check_module_load(const char* module_name) {
    if (!security_policy_enabled) {
        return 1;
    }

    security_decision_count++;

    if (module_name == 0) {
        security_blocked_count++;
        security_audit("blocked module load: null");
        return 0;
    }

    if (security_block_external_modules &&
        (security_str_equal(module_name, "gui") ||
         security_str_equal(module_name, "net") ||
         security_str_equal(module_name, "ai"))) {
        security_blocked_count++;
        security_audit("blocked external module load");
        return 0;
    }

    security_allowed_count++;
    security_audit("module load check passed");
    return 1;
}

int security_check_module_unload(const char* module_name, const char* module_type) {
    if (!security_policy_enabled) {
        return 1;
    }

    security_decision_count++;

    if (module_name == 0 || module_type == 0) {
        security_blocked_count++;
        security_audit("blocked module unload: null");
        return 0;
    }

    if (security_module_protection_enabled && !security_str_equal(module_type, "external")) {
        security_blocked_count++;
        security_audit("blocked core module unload");
        return 0;
    }

    security_allowed_count++;
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

const char* security_policy_mode(void) {
    if (security_strict_mode) {
        return "strict";
    }

    return "open";
}

const char* security_network_policy(void) {
    if (security_block_network) {
        return "blocked";
    }

    return "allowed";
}

const char* security_external_module_policy(void) {
    if (security_block_external_modules) {
        return "blocked";
    }

    return "allowed";
}

const char* security_file_policy(void) {
    if (security_block_file) {
        return "blocked";
    }

    return "allowed";
}

const char* security_ai_policy(void) {
    if (security_block_ai) {
        return "blocked";
    }

    return "allowed";
}

int security_user_control_enabled(void) {
    return security_user_control;
}

int security_policy_network_blocked(void) {
    return security_block_network;
}

int security_policy_external_modules_blocked(void) {
    return security_block_external_modules;
}

int security_policy_file_blocked(void) {
    return security_block_file;
}

int security_policy_ai_blocked(void) {
    return security_block_ai;
}

unsigned int security_policy_allowed_count(void) {
    return security_allowed_count;
}

unsigned int security_policy_blocked_count(void) {
    return security_blocked_count;
}

unsigned int security_policy_decision_count(void) {
    return security_decision_count;
}

void security_policy_open(void) {
    security_user_control = 1;
    security_strict_mode = 0;
    security_block_network = 0;
    security_block_file = 0;
    security_block_ai = 0;
    security_block_video_intent = 0;
    security_block_ai_intent = 0;
    security_block_external_modules = 0;
    security_sandbox_on = 1;
    security_sandbox_profile_id = 0;
    security_rule_mode = 0;
    security_audit("policy set open");
}

void security_policy_strict(void) {
    security_user_control = 1;
    security_strict_mode = 1;
    security_block_network = 1;
    security_block_file = 1;
    security_block_ai = 1;
    security_block_video_intent = 1;
    security_block_ai_intent = 1;
    security_block_external_modules = 1;
    security_sandbox_on = 1;
    security_sandbox_profile_id = 2;
    security_rule_mode = 2;
    security_audit("policy set strict");
}

void security_policy_block_network(void) {
    security_user_control = 1;
    security_block_network = 1;
    security_audit("network policy blocked");
}

void security_policy_allow_network(void) {
    security_user_control = 1;
    security_block_network = 0;
    security_audit("network policy allowed");
}

void security_policy_block_file(void) {
    security_user_control = 1;
    security_block_file = 1;
    security_audit("file capability blocked");
}

void security_policy_allow_file(void) {
    security_user_control = 1;
    security_block_file = 0;
    security_audit("file capability allowed");
}

void security_policy_block_ai(void) {
    security_user_control = 1;
    security_block_ai = 1;
    security_audit("ai capability blocked");
}

void security_policy_allow_ai(void) {
    security_user_control = 1;
    security_block_ai = 0;
    security_audit("ai capability allowed");
}

void security_policy_report(void) {
    platform_print("User control policy:\n");

    platform_print("  user control:      ");
    platform_print(security_user_control ? "enabled\n" : "disabled\n");

    platform_print("  mode:              ");
    platform_print(security_policy_mode());
    platform_print("\n");

    platform_print("  network:           ");
    platform_print(security_network_policy());
    platform_print("\n");

    platform_print("  external modules:  ");
    platform_print(security_external_module_policy());
    platform_print("\n");

    platform_print("  file:              ");
    platform_print(security_file_policy());
    platform_print("\n");

    platform_print("  ai:                ");
    platform_print(security_ai_policy());
    platform_print("\n");

    platform_print("  decisions:         ");
    platform_print_uint(security_decision_count);
    platform_print("\n");

    platform_print("  allowed:           ");
    platform_print_uint(security_allowed_count);
    platform_print("\n");

    platform_print("  blocked:           ");
    platform_print_uint(security_blocked_count);
    platform_print("\n");

    platform_print("  result:            ready\n");
}

void security_policy_check_network(void) {
    platform_print("Network policy check:\n");

    platform_print("  target:  network\n");
    platform_print("  result:  ");

    if (security_check_intent("network")) {
        platform_print("allowed\n");
    } else {
        platform_print("blocked\n");
    }
}


int security_check_capability(const char* cap_name, const char* permission) {
    security_decision_count++;

    if (cap_name == 0 || permission == 0) {
        security_blocked_count++;
        security_audit("blocked capability: null");
        return 0;
    }

    if (security_block_network &&
        (security_str_equal(permission, "network") ||
         security_str_equal(cap_name, "net.http"))) {
        security_blocked_count++;
        security_audit("blocked capability: network");
        return 0;
    }

    if (security_block_file &&
        (security_str_equal(permission, "file") ||
         security_str_equal(cap_name, "file.read"))) {
        security_blocked_count++;
        security_audit("blocked capability: file");
        return 0;
    }

    if (security_block_ai &&
        (security_str_equal(permission, "intent") ||
         security_str_equal(cap_name, "ai.assist"))) {
        security_blocked_count++;
        security_audit("blocked capability: ai");
        return 0;
    }

    security_allowed_count++;
    security_audit("capability check passed");
    return 1;
}

void security_policy_check_capability(const char* cap_name, const char* permission) {
    platform_print("Capability policy check:\n");

    platform_print("  capability: ");
    platform_print(cap_name);
    platform_print("\n");

    platform_print("  permission: ");
    platform_print(permission);
    platform_print("\n");

    platform_print("  result:     ");

    if (security_check_capability(cap_name, permission)) {
        platform_print("allowed\n");
    } else {
        platform_print("blocked\n");
    }
}


const char* security_video_intent_policy(void) {
    if (security_block_video_intent) {
        return "blocked";
    }

    return "allowed";
}

const char* security_ai_intent_policy(void) {
    if (security_block_ai_intent) {
        return "blocked";
    }

    return "allowed";
}

const char* security_network_intent_policy(void) {
    if (security_block_network) {
        return "blocked";
    }

    return "allowed";
}

int security_policy_video_intent_blocked(void) {
    return security_block_video_intent;
}

int security_policy_ai_intent_blocked(void) {
    return security_block_ai_intent;
}

void security_policy_block_video_intent(void) {
    security_user_control = 1;
    security_block_video_intent = 1;
    security_audit("video intent blocked");
}

void security_policy_allow_video_intent(void) {
    security_user_control = 1;
    security_block_video_intent = 0;
    security_audit("video intent allowed");
}

void security_policy_block_ai_intent(void) {
    security_user_control = 1;
    security_block_ai_intent = 1;
    security_audit("ai intent blocked");
}

void security_policy_allow_ai_intent(void) {
    security_user_control = 1;
    security_block_ai_intent = 0;
    security_audit("ai intent allowed");
}

void security_policy_check_intent(const char* intent_name) {
    platform_print("Intent policy check:\n");

    platform_print("  intent:  ");
    platform_print(intent_name);
    platform_print("\n");

    platform_print("  result:  ");

    if (security_check_intent(intent_name)) {
        platform_print("allowed\n");
    } else {
        platform_print("blocked\n");
    }
}


void security_policy_block_external_modules(void) {
    security_user_control = 1;
    security_block_external_modules = 1;
    security_audit("external modules blocked");
}

void security_policy_allow_external_modules(void) {
    security_user_control = 1;
    security_block_external_modules = 0;
    security_audit("external modules allowed");
}

void security_policy_check_module(const char* module_name, const char* module_type) {
    platform_print("Module policy check:\n");

    platform_print("  module: ");
    platform_print(module_name);
    platform_print("\n");

    platform_print("  type:   ");
    platform_print(module_type);
    platform_print("\n");

    platform_print("  result: ");

    if (security_check_module_load(module_name)) {
        platform_print("allowed\n");
    } else {
        platform_print("blocked\n");
    }
}


const char* security_sandbox_profile(void) {
    if (security_sandbox_profile_id == 2) {
        return "locked";
    }

    if (security_sandbox_profile_id == 1) {
        return "safe";
    }

    return "open";
}

const char* security_sandbox_state(void) {
    if (security_sandbox_on) {
        return "enabled";
    }

    return "disabled";
}

int security_sandbox_enabled(void) {
    return security_sandbox_on;
}

unsigned int security_sandbox_switch_count(void) {
    return security_sandbox_switches;
}

void security_sandbox_profile_open(void) {
    security_user_control = 1;
    security_strict_mode = 0;
    security_sandbox_on = 1;
    security_sandbox_profile_id = 0;
    security_block_network = 0;
    security_block_file = 0;
    security_block_ai = 0;
    security_block_video_intent = 0;
    security_block_ai_intent = 0;
    security_block_external_modules = 0;
    security_rule_mode = 0;
    security_sandbox_switches++;
    security_audit("sandbox profile open");
}

void security_sandbox_profile_safe(void) {
    security_user_control = 1;
    security_strict_mode = 0;
    security_sandbox_on = 1;
    security_sandbox_profile_id = 1;
    security_block_network = 0;
    security_block_file = 0;
    security_block_ai = 1;
    security_block_video_intent = 0;
    security_block_ai_intent = 1;
    security_block_external_modules = 1;
    security_rule_mode = 1;
    security_sandbox_switches++;
    security_audit("sandbox profile safe");
}

void security_sandbox_profile_locked(void) {
    security_user_control = 1;
    security_strict_mode = 1;
    security_sandbox_on = 1;
    security_sandbox_profile_id = 2;
    security_block_network = 1;
    security_block_file = 1;
    security_block_ai = 1;
    security_block_video_intent = 1;
    security_block_ai_intent = 1;
    security_block_external_modules = 1;
    security_rule_mode = 2;
    security_sandbox_switches++;
    security_audit("sandbox profile locked");
}

void security_sandbox_profile_reset(void) {
    security_sandbox_profile_open();
    security_audit("sandbox profile reset");
}

int security_check_sandbox(void) {
    security_decision_count++;

    if (!security_sandbox_on) {
        security_blocked_count++;
        security_audit("sandbox disabled");
        return 0;
    }

    security_allowed_count++;
    security_audit("sandbox check passed");
    return 1;
}

void security_sandbox_report(void) {
    platform_print("Sandbox profile:\n");

    platform_print("  state:       ");
    platform_print(security_sandbox_state());
    platform_print("\n");

    platform_print("  profile:     ");
    platform_print(security_sandbox_profile());
    platform_print("\n");

    platform_print("  network:     ");
    platform_print(security_network_policy());
    platform_print("\n");

    platform_print("  file:        ");
    platform_print(security_file_policy());
    platform_print("\n");

    platform_print("  ai:          ");
    platform_print(security_ai_policy());
    platform_print("\n");

    platform_print("  external:    ");
    platform_print(security_external_module_policy());
    platform_print("\n");

    platform_print("  switches:    ");
    platform_print_uint(security_sandbox_switches);
    platform_print("\n");

    platform_print("  result:      ready\n");
}


const char* security_audit_last_event(void) {
    if (security_log_count == 0) {
        return "none";
    }

    return security_logs[security_log_count - 1];
}

const char* security_decision_summary(void) {
    if (security_blocked_count > 0) {
        return "has-blocked";
    }

    if (security_allowed_count > 0) {
        return "all-allowed";
    }

    return "no-decision";
}

const char* security_decision_health(void) {
    if (!security_policy_enabled) {
        return "disabled";
    }

    if (!security_user_control) {
        return "bad";
    }

    return "ok";
}

unsigned int security_audit_log_count(void) {
    return security_log_count;
}

void security_decision_reset_counters(void) {
    security_allowed_count = 0;
    security_blocked_count = 0;
    security_decision_count = 0;
    security_audit("decision counters reset");
}

void security_decision_log_report(void) {
    platform_print("Policy decision log:\n");

    platform_print("  health:     ");
    platform_print(security_decision_health());
    platform_print("\n");

    platform_print("  summary:    ");
    platform_print(security_decision_summary());
    platform_print("\n");

    platform_print("  decisions:  ");
    platform_print_uint(security_decision_count);
    platform_print("\n");

    platform_print("  allowed:    ");
    platform_print_uint(security_allowed_count);
    platform_print("\n");

    platform_print("  blocked:    ");
    platform_print_uint(security_blocked_count);
    platform_print("\n");

    platform_print("  audit logs: ");
    platform_print_uint(security_log_count);
    platform_print("\n");

    platform_print("  last:       ");
    platform_print(security_audit_last_event());
    platform_print("\n");

    platform_print("  result:     ready\n");
}

void security_policy_trace_report(void) {
    platform_print("Policy trace:\n");

    platform_print("  control:    ");
    platform_print(security_user_control_enabled() ? "enabled\n" : "disabled\n");

    platform_print("  mode:       ");
    platform_print(security_policy_mode());
    platform_print("\n");

    platform_print("  sandbox:    ");
    platform_print(security_sandbox_profile());
    platform_print("\n");

    platform_print("  network:    ");
    platform_print(security_network_policy());
    platform_print("\n");

    platform_print("  file:       ");
    platform_print(security_file_policy());
    platform_print("\n");

    platform_print("  ai:         ");
    platform_print(security_ai_policy());
    platform_print("\n");

    platform_print("  external:   ");
    platform_print(security_external_module_policy());
    platform_print("\n");

    platform_print("  last event: ");
    platform_print(security_audit_last_event());
    platform_print("\n");

    platform_print("  result:     traced\n");
}


const char* security_rule_default_policy(void) {
    if (security_rule_mode == 2) {
        return "deny";
    }

    if (security_rule_mode == 1) {
        return "ask";
    }

    return "allow";
}

const char* security_rule_last_target(void) {
    return security_rule_last_name;
}

const char* security_rule_last_result(void) {
    return security_rule_last_decision;
}

unsigned int security_rule_check_count(void) {
    return security_rule_checks;
}

void security_rule_default_allow(void) {
    security_user_control = 1;
    security_rule_mode = 0;
    security_audit("rule default allow");
}

void security_rule_default_ask(void) {
    security_user_control = 1;
    security_rule_mode = 1;
    security_audit("rule default ask");
}

void security_rule_default_deny(void) {
    security_user_control = 1;
    security_rule_mode = 2;
    security_audit("rule default deny");
}

void security_rule_reset(void) {
    security_rule_mode = 0;
    security_rule_checks = 0;
    security_rule_last_name = "none";
    security_rule_last_decision = "none";
    security_audit("rule table reset");
}

int security_check_rule(const char* target) {
    security_decision_count++;
    security_rule_checks++;

    if (target == 0) {
        security_blocked_count++;
        security_rule_last_name = "null";
        security_rule_last_decision = "blocked";
        security_audit("blocked rule: null");
        return 0;
    }

    security_rule_last_name = target;

    if (security_rule_mode == 2) {
        security_blocked_count++;
        security_rule_last_decision = "blocked";
        security_audit("blocked rule: default deny");
        return 0;
    }

    if (security_rule_mode == 1) {
        security_allowed_count++;
        security_rule_last_decision = "ask-allowed";
        security_audit("rule ask: prototype allowed");
        return 1;
    }

    security_allowed_count++;
    security_rule_last_decision = "allowed";
    security_audit("rule allowed");
    return 1;
}

void security_rule_report(void) {
    platform_print("User rule table:\n");

    platform_print("  default:    ");
    platform_print(security_rule_default_policy());
    platform_print("\n");

    platform_print("  checks:     ");
    platform_print_uint(security_rule_checks);
    platform_print("\n");

    platform_print("  last rule:  ");
    platform_print(security_rule_last_name);
    platform_print("\n");

    platform_print("  last result:");
    platform_print(security_rule_last_decision);
    platform_print("\n");

    platform_print("  decisions:  ");
    platform_print_uint(security_decision_count);
    platform_print("\n");

    platform_print("  allowed:    ");
    platform_print_uint(security_allowed_count);
    platform_print("\n");

    platform_print("  blocked:    ");
    platform_print_uint(security_blocked_count);
    platform_print("\n");

    platform_print("  result:     ready\n");
}
