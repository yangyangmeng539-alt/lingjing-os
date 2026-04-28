#include "intent.h"
#include "screen.h"
#include "module.h"
#include "security.h"

#define MAX_INTENT_MODULES 4

typedef struct intent_module_requirement {
    const char* module;
    const char* permission;
} intent_module_requirement_t;

typedef struct intent_entry {
    const char* name;
    const char* description;
    intent_module_requirement_t requires[MAX_INTENT_MODULES];
    int require_count;
} intent_entry_t;

static const char* current_intent = "none";
static int current_intent_running = 0;
static int allow_video = 1;
static int allow_ai = 1;
static int allow_network = 1;
static int intent_locked = 0;
#define INTENT_HISTORY_MAX 8

static const char* intent_history[INTENT_HISTORY_MAX];
static int intent_history_count = 0;

static intent_entry_t intent_table[] = {
    {
        "video",
        "video intent",
        {
            {"gui", "display"},
            {"net", "network"},
            {"ai", "intent"},
            {0, 0}
        },
        3
    },
    {
        "ai",
        "ai intent",
        {
            {"ai", "intent"},
            {0, 0},
            {0, 0},
            {0, 0}
        },
        1
    },
    {
        "network",
        "network intent",
        {
            {"net", "network"},
            {0, 0},
            {0, 0},
            {0, 0}
        },
        1
    }
};

static int intent_count = sizeof(intent_table) / sizeof(intent_table[0]);
static const intent_entry_t* intent_find(const char* name);
static void intent_stop_entry(const intent_entry_t* intent);

static int str_equal_local(const char* a, const char* b) {
    int i = 0;

    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }

    return a[i] == '\0' && b[i] == '\0';
}

static int str_starts_with_local(const char* text, const char* prefix) {
    int i = 0;

    while (prefix[i] != '\0') {
        if (text[i] != prefix[i]) {
            return 0;
        }
        i++;
    }

    return 1;
}

static void intent_history_push(const char* text) {
    if (intent_history_count < INTENT_HISTORY_MAX) {
        intent_history[intent_history_count] = text;
        intent_history_count++;
        return;
    }

    for (int i = 1; i < INTENT_HISTORY_MAX; i++) {
        intent_history[i - 1] = intent_history[i];
    }

    intent_history[INTENT_HISTORY_MAX - 1] = text;
}

static void intent_history_show(void) {
    screen_print("Intent history:\n");

    if (intent_history_count == 0) {
        screen_print("  empty\n");
        return;
    }

    for (int i = 0; i < intent_history_count; i++) {
        screen_print("  ");
        screen_print(intent_history[i]);
        screen_print("\n");
    }
}

static void intent_history_clear(void) {
    for (int i = 0; i < INTENT_HISTORY_MAX; i++) {
        intent_history[i] = 0;
    }

    intent_history_count = 0;

    screen_print("Intent history cleared.\n");
}

static void intent_audit(void) {
    screen_print("Intent audit:\n");

    screen_print("  single intent guard: enabled\n");
    screen_print("  permission check:    mock\n");
    screen_print("  module lifecycle:    load/unload\n");
    screen_print("  history:             enabled\n");
    screen_print("  lock:                ");
    screen_print(intent_locked ? "locked\n" : "unlocked\n");

    screen_print("  allow video:         ");
    screen_print(allow_video ? "yes\n" : "no\n");

    screen_print("  allow ai:            ");
    screen_print(allow_ai ? "yes\n" : "no\n");

    screen_print("  allow network:       ");
    screen_print(allow_network ? "yes\n" : "no\n");

    screen_print("  current intent:      ");

    if (current_intent_running == 0) {
        screen_print("none\n");
    } else {
        screen_print(current_intent);
        screen_print("\n");
    }
}

static void intent_policy(void) {
    screen_print("Intent policy:\n");

    screen_print("  video:     ");
    screen_print(allow_video ? "allowed\n" : "denied\n");

    screen_print("  ai:        ");
    screen_print(allow_ai ? "allowed\n" : "denied\n");

    screen_print("  network:   ");
    screen_print(allow_network ? "allowed\n" : "denied\n");

    screen_print("  guard:     single-intent\n");
    screen_print("  lifecycle: load/unload\n");
    screen_print("  lock:      ");
    screen_print(intent_locked ? "locked\n" : "unlocked\n");
}

static void intent_doctor(void) {
    int deps_broken = module_has_broken_dependencies();

    screen_print("Intent doctor:\n");

    screen_print("  policy:       ok\n");

    screen_print("  lock:         ");
    screen_print(intent_locked ? "locked\n" : "unlocked\n");

    screen_print("  dependencies: ");
    screen_print(deps_broken ? "broken\n" : "ok\n");

    screen_print("  current:      ");

    if (current_intent_running == 0) {
        screen_print("none\n");
    } else {
        screen_print(current_intent);
        screen_print("\n");
    }

    if (intent_locked) {
        screen_print("  result:       blocked\n");
        screen_print("  reason:       intent system locked\n");
        return;
    }

    if (deps_broken) {
        screen_print("  result:       blocked\n");
        screen_print("  reason:       module dependency broken\n");
        return;
    }

    screen_print("  result:       ready\n");
}

static int intent_can_execute(void) {
    if (intent_locked) {
        screen_print("intent blocked: intent system locked.\n");
        return 0;
    }

    if (module_has_broken_dependencies()) {
        screen_print("intent blocked: module dependency broken.\n");
        screen_print("run modulecheck first.\n");
        return 0;
    }

    return 1;
}

static void intent_lock(void) {
    intent_locked = 1;
    screen_print("intent system locked.\n");
}

static void intent_unlock(void) {
    intent_locked = 0;
    screen_print("intent system unlocked.\n");
}

static void intent_reset(void) {
    screen_print("Intent reset:\n");

    if (current_intent_running != 0) {
        const intent_entry_t* intent = intent_find(current_intent);

        if (intent != 0) {
            intent_stop_entry(intent);
        } else {
            current_intent = "none";
            current_intent_running = 0;
        }
    }

    allow_video = 1;
    allow_ai = 1;
    allow_network = 1;
    intent_locked = 0;

    for (int i = 0; i < INTENT_HISTORY_MAX; i++) {
        intent_history[i] = 0;
    }

    intent_history_count = 0;

    screen_print("  permissions restored.\n");
    screen_print("  lock cleared.\n");
    screen_print("  history cleared.\n");
    screen_print("intent system reset.\n");
}

static const intent_entry_t* intent_find(const char* name) {
    for (int i = 0; i < intent_count; i++) {
        if (str_equal_local(intent_table[i].name, name)) {
            return &intent_table[i];
        }
    }

    return 0;
}

static int intent_is_allowed(const char* name) {
    if (str_equal_local(name, "video")) {
        return allow_video;
    }

    if (str_equal_local(name, "ai")) {
        return allow_ai;
    }

    if (str_equal_local(name, "network")) {
        return allow_network;
    }

    return 0;
}

static void intent_set_allowed(const char* name, int allowed) {
    if (str_equal_local(name, "video")) {
        allow_video = allowed;
        return;
    }

    if (str_equal_local(name, "ai")) {
        allow_ai = allowed;
        return;
    }

    if (str_equal_local(name, "network")) {
        allow_network = allowed;
        return;
    }
}

static void intent_allow_command(const char* name) {
    const intent_entry_t* intent = intent_find(name);

    if (intent == 0) {
        screen_print("intent not found: ");
        screen_print(name);
        screen_print("\n");
        return;
    }

    intent_set_allowed(name, 1);

    screen_print("intent allowed: ");
    screen_print(name);
    screen_print("\n");
}

static void intent_deny_command(const char* name) {
    const intent_entry_t* intent = intent_find(name);

    if (intent == 0) {
        screen_print("intent not found: ");
        screen_print(name);
        screen_print("\n");
        return;
    }

    if (current_intent_running != 0 && str_equal_local(current_intent, name)) {
        screen_print("cannot deny running intent: ");
        screen_print(name);
        screen_print("\n");
        screen_print("stop it first.\n");
        return;
    }

    intent_set_allowed(name, 0);

    screen_print("intent denied: ");
    screen_print(name);
    screen_print("\n");
}

static const char* skip_token_local(const char* text) {
    int i = 0;

    while (text[i] != '\0' && text[i] != ' ') {
        i++;
    }

    while (text[i] == ' ') {
        i++;
    }

    return text + i;
}

static void intent_require_module(const char* name) {
    if (module_exists(name)) {
        screen_print("module already ready: ");
        screen_print(name);
        screen_print("\n");
        return;
    }

    module_load_mock(name);
}

static int intent_can_start(const char* name) {
    if (current_intent_running == 0) {
        return 1;
    }

    if (str_equal_local(current_intent, name)) {
        screen_print("intent already running: ");
        screen_print(name);
        screen_print("\n");
        return 0;
    }

    screen_print("intent already running: ");
    screen_print(current_intent);
    screen_print("\n");
    screen_print("stop it first.\n");

    return 0;
}

void intent_list(void) {
    screen_print("Available intents:\n");

    for (int i = 0; i < intent_count; i++) {
        screen_print("  ");
        screen_print(intent_table[i].name);
        screen_print("    requires: ");

        for (int j = 0; j < intent_table[i].require_count; j++) {
            screen_print(intent_table[i].requires[j].module);

            if (j < intent_table[i].require_count - 1) {
                screen_print(", ");
            }
        }

        screen_print("\n");
    }

    screen_print("  status\n");
    screen_print("  history\n");
    screen_print("  clear-history\n");
    screen_print("  audit\n");
    screen_print("  policy\n");
    screen_print("  doctor\n");
    screen_print("  lock\n");
    screen_print("  unlock\n");
    screen_print("  reset\n");
    screen_print("  allow <intent>\n");
    screen_print("  deny <intent>\n");
    screen_print("  permissions\n");
    screen_print("  plan <intent>\n");
    screen_print("  run <intent>\n");
    screen_print("  stop           stops current intent\n");
    screen_print("  stop <intent>  stops selected intent\n");
    screen_print("  restart        restarts current intent\n");
    screen_print("  switch <intent> switches current intent\n");
}

void intent_permissions(void) {
    screen_print("Intent permissions:\n");

    for (int i = 0; i < intent_count; i++) {
        screen_print("  ");
        screen_print(intent_table[i].name);
        screen_print(":\n");

        for (int j = 0; j < intent_table[i].require_count; j++) {
            screen_print("    ");
            screen_print(intent_table[i].requires[j].module);
            screen_print(": ");
            screen_print(intent_table[i].requires[j].permission);
            screen_print("\n");
        }
    }
}

const char* intent_get_current_name(void) {
    return current_intent;
}

int intent_is_running(void) {
    return current_intent_running;
}

void intent_status(void) {
    screen_print("Current intent:\n");

    if (current_intent_running == 0) {
        screen_print("  none\n");
        return;
    }

    screen_print("  ");
    screen_print(current_intent);
    screen_print(" running\n");

    const intent_entry_t* intent = intent_find(current_intent);

    if (intent == 0) {
        return;
    }

    screen_print("  modules: ");

    for (int i = 0; i < intent->require_count; i++) {
        screen_print(intent->requires[i].module);

        if (i < intent->require_count - 1) {
            screen_print(", ");
        }
    }

    screen_print("\n");
}

static void intent_plan(const intent_entry_t* intent) {
    screen_print("Intent plan: ");
    screen_print(intent->name);
    screen_print("\n");

    screen_print("  requires:\n");

    for (int i = 0; i < intent->require_count; i++) {
        screen_print("    ");
        screen_print(intent->requires[i].module);
        screen_print("\n");
    }

    screen_print("  permissions:\n");

    for (int i = 0; i < intent->require_count; i++) {
        screen_print("    ");
        screen_print(intent->requires[i].module);
        screen_print(": ");
        screen_print(intent->requires[i].permission);
        screen_print("\n");
    }

    screen_print("  dependencies:\n");

    for (int i = 0; i < intent->require_count; i++) {
        const char* module_name = intent->requires[i].module;
        const char* depends = module_get_depends(module_name);

        screen_print("    ");
        screen_print(module_name);
        screen_print(" -> ");
        screen_print(depends);
        screen_print(" : ");

        if (module_dependency_ok(module_name)) {
            screen_print("ok\n");
        } else {
            screen_print("broken\n");
        }
    }

    screen_print("  lifecycle:\n");
    screen_print("    load on start\n");
    screen_print("    unload on stop\n");
}

static void intent_run_entry(const intent_entry_t* intent) {
    if (!intent_can_execute()) {
        return;
    }

    if (!security_check_intent(intent->name)) {
        screen_print("intent blocked by security: ");
        screen_print(intent->name);
        screen_print("\n");
        return;
    }

    if (!intent_is_allowed(intent->name)) {
        screen_print("intent denied: ");
        screen_print(intent->name);
        screen_print("\n");
        return;
    }

    if (!intent_can_start(intent->name)) {
        return;
    }

    screen_print("Intent run: ");
    screen_print(intent->name);
    screen_print("\n");

    screen_print("using plan: ");
    screen_print(intent->name);
    screen_print("\n");

    screen_print("requires: ");

    for (int i = 0; i < intent->require_count; i++) {
        screen_print(intent->requires[i].module);

        if (i < intent->require_count - 1) {
            screen_print(", ");
        }
    }

    screen_print("\n");
    screen_print("checking permissions...\n");

    for (int i = 0; i < intent->require_count; i++) {
        screen_print("permission ");
        screen_print(intent->requires[i].module);
        screen_print(": ");
        screen_print(intent->requires[i].permission);
        screen_print(" allowed\n");
    }

    for (int i = 0; i < intent->require_count; i++) {
        intent_require_module(intent->requires[i].module);
    }

    current_intent = intent->name;
    current_intent_running = 1;

    if (str_equal_local(intent->name, "video")) {
        intent_history_push("run video");
    } else if (str_equal_local(intent->name, "ai")) {
        intent_history_push("run ai");
    } else if (str_equal_local(intent->name, "network")) {
        intent_history_push("run network");
    }

    screen_print("intent ready.\n");
}

static void intent_stop_entry(const intent_entry_t* intent) {
    screen_print("Intent stop: ");
    screen_print(intent->name);
    screen_print("\n");

    for (int i = 0; i < intent->require_count; i++) {
        module_unload_mock(intent->requires[i].module);
    }

    if (str_equal_local(intent->name, "video")) {
        intent_history_push("stop video");
    } else if (str_equal_local(intent->name, "ai")) {
        intent_history_push("stop ai");
    } else if (str_equal_local(intent->name, "network")) {
        intent_history_push("stop network");
    }

    if (str_equal_local(current_intent, intent->name)) {
        current_intent = "none";
        current_intent_running = 0;
    }

    screen_print("intent stopped.\n");
}

static void intent_stop_current(void) {
    if (current_intent_running == 0) {
        screen_print("no intent running.\n");
        return;
    }

    const intent_entry_t* intent = intent_find(current_intent);

    if (intent == 0) {
        screen_print("current intent not found: ");
        screen_print(current_intent);
        screen_print("\n");
        current_intent = "none";
        current_intent_running = 0;
        return;
    }

    intent_stop_entry(intent);
}

static void intent_restart_current(void) {
    if (!intent_can_execute()) {
        return;
    }

    if (current_intent_running == 0) {
        screen_print("no intent running.\n");
        return;
    }

    const char* restart_name = current_intent;
    const intent_entry_t* intent = intent_find(restart_name);

    if (intent == 0) {
        screen_print("current intent not found: ");
        screen_print(restart_name);
        screen_print("\n");
        current_intent = "none";
        current_intent_running = 0;
        return;
    }

    screen_print("Intent restart: ");
    screen_print(restart_name);
    screen_print("\n");

    intent_stop_entry(intent);
    intent_run_entry(intent);
}

static void intent_switch_to(const char* name) {
    if (!intent_can_execute()) {
        return;
    }

    const intent_entry_t* target = intent_find(name);

    if (target == 0) {
        screen_print("intent switch not supported: ");
        screen_print(name);
        screen_print("\n");
        return;
    }

    if (current_intent_running != 0 && str_equal_local(current_intent, name)) {
        screen_print("intent already running: ");
        screen_print(name);
        screen_print("\n");
        return;
    }

    if (current_intent_running != 0) {
        if (str_equal_local(current_intent, "network") && str_equal_local(name, "video")) {
            intent_history_push("switch network -> video");
        } else if (str_equal_local(current_intent, "video") && str_equal_local(name, "network")) {
            intent_history_push("switch video -> network");
        } else if (str_equal_local(current_intent, "ai") && str_equal_local(name, "video")) {
            intent_history_push("switch ai -> video");
        } else if (str_equal_local(current_intent, "video") && str_equal_local(name, "ai")) {
            intent_history_push("switch video -> ai");
        } else if (str_equal_local(current_intent, "network") && str_equal_local(name, "ai")) {
            intent_history_push("switch network -> ai");
        } else if (str_equal_local(current_intent, "ai") && str_equal_local(name, "network")) {
            intent_history_push("switch ai -> network");
        }
    }
    screen_print("Intent switch: ");

    if (current_intent_running != 0) {
        screen_print(current_intent);
    } else {
        screen_print("none");
    }

    screen_print(" -> ");
    screen_print(name);
    screen_print("\n");

    if (current_intent_running != 0) {
        const intent_entry_t* current = intent_find(current_intent);

        if (current != 0) {
            intent_stop_entry(current);
        } else {
            current_intent = "none";
            current_intent_running = 0;
        }
    }

    intent_run_entry(target);
}

void intent_run(const char* name) {
    if (str_equal_local(name, "list")) {
        intent_list();
        return;
    }

    if (str_equal_local(name, "status")) {
        intent_status();
        return;
    }
    if (str_equal_local(name, "history")) {
        intent_history_show();
        return;
    }
    if (str_equal_local(name, "clear-history")) {
        intent_history_clear();
        return;
    }
    if (str_equal_local(name, "audit")) {
        intent_audit();
        return;
    }
    if (str_equal_local(name, "policy")) {
        intent_policy();
        return;
    }
    if (str_equal_local(name, "doctor")) {
        intent_doctor();
        return;
    }
    if (str_equal_local(name, "lock")) {
        intent_lock();
        return;
    }

    if (str_equal_local(name, "unlock")) {
        intent_unlock();
        return;
    }
    if (str_equal_local(name, "reset")) {
        intent_reset();
        return;
    }
    if (str_starts_with_local(name, "allow ")) {
        const char* intent_name = skip_token_local(name);
        intent_allow_command(intent_name);
        return;
    }

    if (str_starts_with_local(name, "deny ")) {
        const char* intent_name = skip_token_local(name);
        intent_deny_command(intent_name);
        return;
    }
    if (str_equal_local(name, "stop")) {
        intent_stop_current();
        return;
    }
    if (str_equal_local(name, "restart")) {
        intent_restart_current();
        return;
    }
    if (str_equal_local(name, "permissions")) {
        intent_permissions();
        return;
    }

    if (str_starts_with_local(name, "plan ")) {
        const char* intent_name = skip_token_local(name);
        const intent_entry_t* intent = intent_find(intent_name);

        if (intent == 0) {
            screen_print("intent plan not supported: ");
            screen_print(intent_name);
            screen_print("\n");
            return;
        }

        intent_plan(intent);
        return;
    }

    if (str_starts_with_local(name, "switch ")) {
        const char* intent_name = skip_token_local(name);
        intent_switch_to(intent_name);
        return;
    }

    if (str_starts_with_local(name, "run ")) {
        const char* intent_name = skip_token_local(name);
        const intent_entry_t* intent = intent_find(intent_name);

        if (intent == 0) {
            screen_print("intent run not supported: ");
            screen_print(intent_name);
            screen_print("\n");
            return;
        }

        intent_run_entry(intent);
        return;
    }

    if (str_starts_with_local(name, "stop ")) {
        const char* intent_name = skip_token_local(name);
        const intent_entry_t* intent = intent_find(intent_name);

        if (intent == 0) {
            screen_print("intent stop not supported: ");
            screen_print(intent_name);
            screen_print("\n");
            return;
        }

        intent_stop_entry(intent);
        return;
    }

    const intent_entry_t* legacy_intent = intent_find(name);

    if (legacy_intent != 0) {
        screen_print("deprecated: use intent run ");
        screen_print(name);
        screen_print("\n");
        intent_run_entry(legacy_intent);
        return;
    }

    screen_print("intent not found: ");
    screen_print(name);
    screen_print("\n");
}