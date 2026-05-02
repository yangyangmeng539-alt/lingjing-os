#include "intent.h"
#include "screen.h"
#include "module.h"
#include "security.h"
#include "platform.h"
#include "capability.h"

#define MAX_INTENT_MODULES 4
#define INTENT_HISTORY_MAX 8

typedef struct intent_module_requirement {
    const char* module;
    const char* capability;
    const char* permission;
} intent_module_requirement_t;

typedef struct intent_entry {
    const char* name;
    const char* description;
    intent_module_requirement_t requires[MAX_INTENT_MODULES];
    int require_count;
} intent_entry_t;

typedef struct intent_task_context {
    unsigned int task_id;
    const char* intent_name;
    const char* state;
    const char* bound_modules[MAX_INTENT_MODULES];
    const char* bound_capabilities[MAX_INTENT_MODULES];
    int bound_count;
    unsigned int started_at;
    unsigned int cleanup_count;
    int active;
} intent_task_context_t;

static const char* current_intent = "none";
static int current_intent_running = 0;
static int allow_video = 1;
static int allow_ai = 1;
static int allow_network = 1;
static int intent_locked = 0;

static const char* intent_history[INTENT_HISTORY_MAX];
static int intent_history_count = 0;

static intent_task_context_t current_context;
static unsigned int next_intent_task_id = 100;
static unsigned int intent_context_sequence = 0;
static unsigned int intent_context_cleanup_total = 0;

static intent_entry_t intent_table[] = {
    {
        "video",
        "video intent",
        {
            {"gui", "gui.display", "display"},
            {"net", "net.http", "network"},
            {"ai", "ai.assist", "intent"},
            {0, 0, 0}
        },
        3
    },
    {
        "ai",
        "ai intent",
        {
            {"ai", "ai.assist", "intent"},
            {0, 0, 0},
            {0, 0, 0},
            {0, 0, 0}
        },
        1
    },
    {
        "network",
        "network intent",
        {
            {"net", "net.http", "network"},
            {0, 0, 0},
            {0, 0, 0},
            {0, 0, 0}
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

static void intent_context_clear(void) {
    current_context.task_id = 0;
    current_context.intent_name = "none";
    current_context.state = "none";
    current_context.bound_count = 0;
    current_context.started_at = 0;
    current_context.cleanup_count = intent_context_cleanup_total;
    current_context.active = 0;

    for (int i = 0; i < MAX_INTENT_MODULES; i++) {
        current_context.bound_modules[i] = 0;
        current_context.bound_capabilities[i] = 0;
    }
}

static void intent_context_create(const intent_entry_t* intent) {
    current_context.task_id = next_intent_task_id;
    next_intent_task_id++;

    intent_context_sequence++;

    current_context.intent_name = intent->name;
    current_context.state = "running";
    current_context.bound_count = intent->require_count;
    current_context.started_at = intent_context_sequence;
    current_context.cleanup_count = intent_context_cleanup_total;
    current_context.active = 1;

    for (int i = 0; i < MAX_INTENT_MODULES; i++) {
        current_context.bound_modules[i] = 0;
        current_context.bound_capabilities[i] = 0;
    }

    for (int i = 0; i < intent->require_count; i++) {
        current_context.bound_modules[i] = intent->requires[i].module;
        current_context.bound_capabilities[i] = intent->requires[i].capability;
    }
}

static void intent_context_cleanup(void) {
    if (!current_context.active) {
        intent_context_clear();
        return;
    }

    intent_context_cleanup_total++;

    current_context.state = "stopped";
    current_context.cleanup_count = intent_context_cleanup_total;
    current_context.active = 0;
}

static void intent_context_show(void) {
    platform_print("Intent task context:\n");

    if (!current_context.active) {
        platform_print("  active:       no\n");
        platform_print("  taskId:       none\n");
        platform_print("  intent:       none\n");
        platform_print("  state:        none\n");
        platform_print("  cleanup:      ");
        platform_print_uint(intent_context_cleanup_total);
        platform_print("\n");
        return;
    }

    platform_print("  active:       yes\n");

    platform_print("  taskId:       ");
    platform_print_uint(current_context.task_id);
    platform_print("\n");

    platform_print("  intent:       ");
    platform_print(current_context.intent_name);
    platform_print("\n");

    platform_print("  state:        ");
    platform_print(current_context.state);
    platform_print("\n");

    platform_print("  startedAt:    ");
    platform_print_uint(current_context.started_at);
    platform_print("\n");

    platform_print("  bound modules:\n");

    for (int i = 0; i < current_context.bound_count; i++) {
        platform_print("    ");
        platform_print(current_context.bound_modules[i]);
        platform_print(" -> provides ");
        platform_print(module_get_provides(current_context.bound_modules[i]));
        platform_print("\n");
    }

    platform_print("  bound capabilities:\n");

    for (int i = 0; i < current_context.bound_count; i++) {
        platform_print("    ");
        platform_print(current_context.bound_capabilities[i]);
        platform_print(" -> provider ");
        platform_print(capability_get_provider(current_context.bound_capabilities[i]));
        platform_print("\n");
    }
}

void intent_context_report(void) {
    platform_print("Intent context report:\n");

    if (!current_context.active) {
        platform_print("  active:       no\n");
        platform_print("  taskId:       none\n");
        platform_print("  intent:       none\n");
        platform_print("  state:        none\n");
        platform_print("  cleanup:      ");
        platform_print_uint(intent_context_cleanup_total);
        platform_print("\n");
        return;
    }

    platform_print("  active:       yes\n");

    platform_print("  taskId:       ");
    platform_print_uint(current_context.task_id);
    platform_print("\n");

    platform_print("  intent:       ");
    platform_print(current_context.intent_name);
    platform_print("\n");

    platform_print("  state:        ");
    platform_print(current_context.state);
    platform_print("\n");

    platform_print("  startedAt:    ");
    platform_print_uint(current_context.started_at);
    platform_print("\n");

    platform_print("  modules:      ");

    for (int i = 0; i < current_context.bound_count; i++) {
        platform_print(current_context.bound_modules[i]);

        if (i < current_context.bound_count - 1) {
            platform_print(", ");
        }
    }

    platform_print("\n");

    platform_print("  capabilities: ");

    for (int i = 0; i < current_context.bound_count; i++) {
        platform_print(current_context.bound_capabilities[i]);

        if (i < current_context.bound_count - 1) {
            platform_print(", ");
        }
    }

    platform_print("\n");
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
    platform_print("Intent history:\n");

    if (intent_history_count == 0) {
        platform_print("  empty\n");
        return;
    }

    for (int i = 0; i < intent_history_count; i++) {
        platform_print("  ");
        platform_print(intent_history[i]);
        platform_print("\n");
    }
}

static void intent_history_clear(void) {
    for (int i = 0; i < INTENT_HISTORY_MAX; i++) {
        intent_history[i] = 0;
    }

    intent_history_count = 0;

    platform_print("Intent history cleared.\n");
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

static int intent_can_execute(void) {
    if (intent_locked) {
        platform_print("intent blocked: intent system locked.\n");
        return 0;
    }

    if (module_has_broken_dependencies()) {
        platform_print("intent blocked: module dependency broken.\n");
        platform_print("run modulecheck first.\n");
        return 0;
    }

    return 1;
}

static int intent_can_start(const char* name) {
    if (current_intent_running == 0) {
        return 1;
    }

    if (str_equal_local(current_intent, name)) {
        platform_print("intent already running: ");
        platform_print(name);
        platform_print("\n");
        return 0;
    }

    platform_print("intent already running: ");
    platform_print(current_intent);
    platform_print("\n");
    platform_print("stop it first.\n");

    return 0;
}

static int intent_capabilities_exist(const intent_entry_t* intent) {
    for (int i = 0; i < intent->require_count; i++) {
        const char* capability_name = intent->requires[i].capability;

        if (!capability_exists(capability_name)) {
            platform_print("intent blocked: missing capability ");
            platform_print(capability_name);
            platform_print("\n");
            return 0;
        }
    }

    return 1;
}

static int intent_capabilities_ready(const intent_entry_t* intent) {
    for (int i = 0; i < intent->require_count; i++) {
        const char* capability_name = intent->requires[i].capability;

        if (!capability_is_ready(capability_name)) {
            platform_print("intent blocked: capability not ready ");
            platform_print(capability_name);
            platform_print("\n");
            return 0;
        }
    }

    return 1;
}

static void intent_print_capability_plan(const intent_entry_t* intent) {
    platform_print("  required capabilities:\n");

    for (int i = 0; i < intent->require_count; i++) {
        const char* capability_name = intent->requires[i].capability;

        platform_print("    ");
        platform_print(capability_name);
        platform_print(" -> provider ");
        platform_print(capability_get_provider(capability_name));
        platform_print(" : ");

        if (!capability_exists(capability_name)) {
            platform_print("missing\n");
        } else if (capability_is_ready(capability_name)) {
            platform_print("ready\n");
        } else {
            platform_print("blocked\n");
        }
    }
}

static void intent_require_module(const char* name) {
    if (module_exists(name)) {
        platform_print("module already ready: ");
        platform_print(name);
        platform_print("\n");
        return;
    }

    module_load_mock(name);
}

static void intent_load_required_modules(const intent_entry_t* intent) {
    for (int i = 0; i < intent->require_count; i++) {
        intent_require_module(intent->requires[i].module);
    }
}

static void intent_print_permissions(const intent_entry_t* intent) {
    platform_print("checking permissions...\n");

    for (int i = 0; i < intent->require_count; i++) {
        platform_print("permission ");
        platform_print(intent->requires[i].capability);
        platform_print(": ");
        platform_print(intent->requires[i].permission);
        platform_print(" allowed\n");
    }
}

static void intent_print_required_modules_inline(const intent_entry_t* intent) {
    platform_print("requires modules: ");

    for (int i = 0; i < intent->require_count; i++) {
        platform_print(intent->requires[i].module);

        if (i < intent->require_count - 1) {
            platform_print(", ");
        }
    }

    platform_print("\n");
}

static void intent_print_required_capabilities_inline(const intent_entry_t* intent) {
    platform_print("requires capabilities: ");

    for (int i = 0; i < intent->require_count; i++) {
        platform_print(intent->requires[i].capability);

        if (i < intent->require_count - 1) {
            platform_print(", ");
        }
    }

    platform_print("\n");
}

static void intent_audit(void) {
    platform_print("Intent audit:\n");

    platform_print("  single intent guard: enabled\n");
    platform_print("  permission check:    mock\n");
    platform_print("  capability check:    enabled\n");
    platform_print("  task context:        enabled\n");
    platform_print("  module lifecycle:    load/unload\n");
    platform_print("  history:             enabled\n");

    platform_print("  lock:                ");
    platform_print(intent_locked ? "locked\n" : "unlocked\n");

    platform_print("  allow video:         ");
    platform_print(allow_video ? "yes\n" : "no\n");

    platform_print("  allow ai:            ");
    platform_print(allow_ai ? "yes\n" : "no\n");

    platform_print("  allow network:       ");
    platform_print(allow_network ? "yes\n" : "no\n");

    platform_print("  current intent:      ");

    if (current_intent_running == 0) {
        platform_print("none\n");
    } else {
        platform_print(current_intent);
        platform_print("\n");
    }

    platform_print("  task context:        ");
    platform_print(current_context.active ? "active\n" : "none\n");
}

void intent_policy(void) {
    platform_print("Intent policy:\n");

    platform_print("  video:     ");
    platform_print(intent_is_allowed("video") ? "allowed\n" : "denied\n");

    platform_print("  ai:        ");
    platform_print(intent_is_allowed("ai") ? "allowed\n" : "denied\n");

    platform_print("  network:   ");
    platform_print(intent_is_allowed("network") ? "allowed\n" : "denied\n");

    platform_print("  guard:     single-intent\n");
    platform_print("  lifecycle: load/unload\n");
    platform_print("  capability: enabled\n");
    platform_print("  context:   enabled\n");

    platform_print("  lock:      ");
    platform_print(intent_locked ? "locked\n" : "unlocked\n");
}

void intent_doctor(void) {
    int deps_broken = module_has_broken_dependencies();

    platform_print("Intent doctor:\n");

    platform_print("  policy:       ok\n");

    platform_print("  capability:   enabled\n");

    platform_print("  context:      enabled\n");

    platform_print("  lock:         ");
    platform_print(intent_locked ? "locked\n" : "unlocked\n");

    platform_print("  dependencies: ");
    platform_print(deps_broken ? "broken\n" : "ok\n");

    platform_print("  current:      ");

    if (current_intent_running == 0) {
        platform_print("none\n");
    } else {
        platform_print(current_intent);
        platform_print("\n");
    }

    platform_print("  task context: ");

    if (current_context.active) {
        platform_print("active\n");
    } else {
        platform_print("none\n");
    }

    if (intent_locked) {
        platform_print("  result:       blocked\n");
        platform_print("  reason:       intent system locked\n");
        return;
    }

    if (deps_broken) {
        platform_print("  result:       blocked\n");
        platform_print("  reason:       module dependency broken\n");
        return;
    }

    platform_print("  result:       ready\n");
}

static void intent_lock(void) {
    intent_locked = 1;
    platform_print("intent system locked.\n");
}

static void intent_unlock(void) {
    intent_locked = 0;
    platform_print("intent system unlocked.\n");
}

static void intent_reset(void) {
    platform_print("Intent reset:\n");

    if (current_intent_running != 0) {
        const intent_entry_t* intent = intent_find(current_intent);

        if (intent != 0) {
            intent_stop_entry(intent);
        } else {
            platform_print("current intent not found: ");
            platform_print(current_intent);
            platform_print("\n");
        }
    }

    current_intent = "none";
    current_intent_running = 0;

    allow_video = 1;
    allow_ai = 1;
    allow_network = 1;
    intent_locked = 0;

    for (int i = 0; i < INTENT_HISTORY_MAX; i++) {
        intent_history[i] = 0;
    }

    intent_history_count = 0;
    intent_context_clear();

    platform_print("  runtime cleared.\n");
    platform_print("  permissions restored.\n");
    platform_print("  lock cleared.\n");
    platform_print("  history cleared.\n");
    platform_print("  task context cleared.\n");
    platform_print("intent system reset.\n");
}

static void intent_allow_command(const char* name) {
    const intent_entry_t* intent = intent_find(name);

    if (intent == 0) {
        platform_print("intent not found: ");
        platform_print(name);
        platform_print("\n");
        return;
    }

    intent_set_allowed(name, 1);

    platform_print("intent allowed: ");
    platform_print(name);
    platform_print("\n");
}

static void intent_deny_command(const char* name) {
    const intent_entry_t* intent = intent_find(name);

    if (intent == 0) {
        platform_print("intent not found: ");
        platform_print(name);
        platform_print("\n");
        return;
    }

    if (current_intent_running != 0 && str_equal_local(current_intent, name)) {
        platform_print("cannot deny running intent: ");
        platform_print(name);
        platform_print("\n");
        platform_print("stop it first.\n");
        return;
    }

    intent_set_allowed(name, 0);

    platform_print("intent denied: ");
    platform_print(name);
    platform_print("\n");
}

void intent_list(void) {
    platform_print("Available intents:\n");

    for (int i = 0; i < intent_count; i++) {
        platform_print("  ");
        platform_print(intent_table[i].name);
        platform_print("    capabilities: ");

        for (int j = 0; j < intent_table[i].require_count; j++) {
            platform_print(intent_table[i].requires[j].capability);

            if (j < intent_table[i].require_count - 1) {
                platform_print(", ");
            }
        }

        platform_print("\n");
    }

    platform_print("  status\n");
    platform_print("  context\n");
    platform_print("  history\n");
    platform_print("  clear-history\n");
    platform_print("  audit\n");
    platform_print("  policy\n");
    platform_print("  doctor\n");
    platform_print("  lock\n");
    platform_print("  unlock\n");
    platform_print("  reset\n");
    platform_print("  allow <intent>\n");
    platform_print("  deny <intent>\n");
    platform_print("  permissions\n");
    platform_print("  plan <intent>\n");
    platform_print("  run <intent>\n");
    platform_print("  stop           stops current intent\n");
    platform_print("  stop <intent>  stops selected intent\n");
    platform_print("  restart        restarts current intent\n");
    platform_print("  switch <intent> switches current intent\n");
}

void intent_permissions(void) {
    platform_print("Intent permissions:\n");

    for (int i = 0; i < intent_count; i++) {
        platform_print("  ");
        platform_print(intent_table[i].name);
        platform_print(": ");

        for (int j = 0; j < intent_table[i].require_count; j++) {
            platform_print(intent_table[i].requires[j].capability);
            platform_print("=");
            platform_print(intent_table[i].requires[j].permission);

            if (j < intent_table[i].require_count - 1) {
                platform_print(", ");
            }
        }

        platform_print("\n");
    }
}

const char* intent_get_current_name(void) {
    return current_intent;
}

int intent_is_running(void) {
    return current_intent_running;
}

void intent_status(void) {
    platform_print("Current intent:\n");

    if (current_intent_running == 0) {
        platform_print("  none\n");
        return;
    }

    platform_print("  ");
    platform_print(current_intent);
    platform_print(" running\n");

    const intent_entry_t* intent = intent_find(current_intent);

    if (intent == 0) {
        return;
    }

    platform_print("  taskId: ");

    if (current_context.active) {
        platform_print_uint(current_context.task_id);
        platform_print("\n");
    } else {
        platform_print("none\n");
    }

    platform_print("  modules: ");

    for (int i = 0; i < intent->require_count; i++) {
        platform_print(intent->requires[i].module);

        if (i < intent->require_count - 1) {
            platform_print(", ");
        }
    }

    platform_print("\n");

    platform_print("  capabilities: ");

    for (int i = 0; i < intent->require_count; i++) {
        platform_print(intent->requires[i].capability);

        if (i < intent->require_count - 1) {
            platform_print(", ");
        }
    }

    platform_print("\n");

    platform_print("  context: ");

    if (current_context.active) {
        platform_print("active\n");
    } else {
        platform_print("none\n");
    }
}

static void intent_plan(const intent_entry_t* intent) {
    platform_print("Intent plan: ");
    platform_print(intent->name);
    platform_print("\n");

    platform_print("  modules:\n");

    for (int i = 0; i < intent->require_count; i++) {
        platform_print("    ");
        platform_print(intent->requires[i].module);
        platform_print(" -> provides ");
        platform_print(module_get_provides(intent->requires[i].module));
        platform_print("\n");
    }

    intent_print_capability_plan(intent);

    platform_print("  permissions:\n");

    for (int i = 0; i < intent->require_count; i++) {
        platform_print("    ");
        platform_print(intent->requires[i].capability);
        platform_print(": ");
        platform_print(intent->requires[i].permission);
        platform_print("\n");
    }

    platform_print("  dependencies:\n");

    for (int i = 0; i < intent->require_count; i++) {
        const char* module_name = intent->requires[i].module;
        const char* depends = module_get_depends(module_name);

        platform_print("    ");
        platform_print(module_name);
        platform_print(" -> ");
        platform_print(depends);
        platform_print(" : ");

        if (module_dependency_ok(module_name)) {
            platform_print("ok\n");
        } else {
            platform_print("broken\n");
        }
    }

    platform_print("  task context:\n");
    platform_print("    create on run\n");
    platform_print("    bind modules\n");
    platform_print("    bind capabilities\n");
    platform_print("    cleanup on stop\n");

    platform_print("  lifecycle:\n");
    platform_print("    check capabilities\n");
    platform_print("    load provider modules on start\n");
    platform_print("    verify capabilities ready\n");
    platform_print("    create task context\n");
    platform_print("    unload modules on stop\n");
}

static int intent_run_precheck(const intent_entry_t* intent) {
    if (!intent_can_execute()) {
        return 0;
    }

    if (!security_check_intent(intent->name)) {
        platform_print("intent blocked by security: ");
        platform_print(intent->name);
        platform_print("\n");
        return 0;
    }

    if (!intent_is_allowed(intent->name)) {
        platform_print("intent denied: ");
        platform_print(intent->name);
        platform_print("\n");
        return 0;
    }

    if (!intent_can_start(intent->name)) {
        return 0;
    }

    if (!intent_capabilities_exist(intent)) {
        return 0;
    }

    return 1;
}

static void intent_run_entry(const intent_entry_t* intent) {
    if (!intent_run_precheck(intent)) {
        return;
    }

    platform_print("Intent run: ");
    platform_print(intent->name);
    platform_print("\n");

    platform_print("using plan: ");
    platform_print(intent->name);
    platform_print("\n");

    intent_print_required_modules_inline(intent);
    intent_print_required_capabilities_inline(intent);
    intent_print_permissions(intent);

    platform_print("checking capability registry...\n");
    intent_print_capability_plan(intent);

    platform_print("loading provider modules...\n");
    intent_load_required_modules(intent);

    platform_print("verifying capabilities...\n");

    if (!intent_capabilities_ready(intent)) {
        platform_print("intent blocked: capability verification failed.\n");
        return;
    }

    intent_context_create(intent);

    platform_print("creating task context...\n");
    platform_print("  taskId: ");
    platform_print_uint(current_context.task_id);
    platform_print("\n");

    platform_print("  state:  ");
    platform_print(current_context.state);
    platform_print("\n");

    current_intent = intent->name;
    current_intent_running = 1;

    if (str_equal_local(intent->name, "video")) {
        intent_history_push("run video");
    } else if (str_equal_local(intent->name, "ai")) {
        intent_history_push("run ai");
    } else if (str_equal_local(intent->name, "network")) {
        intent_history_push("run network");
    }

    platform_print("intent ready.\n");
}

static void intent_stop_entry(const intent_entry_t* intent) {
    platform_print("Intent stop: ");
    platform_print(intent->name);
    platform_print("\n");

    platform_print("cleaning task context...\n");
    intent_context_cleanup();

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

    platform_print("intent stopped.\n");
}

static void intent_stop_current(void) {
    if (current_intent_running == 0) {
        platform_print("no intent running.\n");
        return;
    }

    const intent_entry_t* intent = intent_find(current_intent);

    if (intent == 0) {
        platform_print("current intent not found: ");
        platform_print(current_intent);
        platform_print("\n");
        current_intent = "none";
        current_intent_running = 0;
        intent_context_cleanup();
        return;
    }

    intent_stop_entry(intent);
}

static void intent_restart_current(void) {
    if (!intent_can_execute()) {
        return;
    }

    if (current_intent_running == 0) {
        platform_print("no intent running.\n");
        return;
    }

    const char* restart_name = current_intent;
    const intent_entry_t* intent = intent_find(restart_name);

    if (intent == 0) {
        platform_print("current intent not found: ");
        platform_print(restart_name);
        platform_print("\n");
        current_intent = "none";
        current_intent_running = 0;
        intent_context_cleanup();
        return;
    }

    platform_print("Intent restart: ");
    platform_print(restart_name);
    platform_print("\n");

    intent_stop_entry(intent);
    intent_run_entry(intent);
}

static void intent_switch_to(const char* name) {
    const intent_entry_t* target = 0;
    const intent_entry_t* current = 0;
    const char* old_intent_name = current_intent;

    if (!intent_can_execute()) {
        return;
    }

    target = intent_find(name);

    if (target == 0) {
        platform_print("intent switch not supported: ");
        platform_print(name);
        platform_print("\n");
        return;
    }

    if (current_intent_running != 0 && str_equal_local(current_intent, name)) {
        platform_print("intent already running: ");
        platform_print(name);
        platform_print("\n");
        return;
    }

    if (!security_check_intent(target->name)) {
        platform_print("intent blocked by security: ");
        platform_print(target->name);
        platform_print("\n");
        return;
    }

    if (!intent_is_allowed(target->name)) {
        platform_print("intent denied: ");
        platform_print(target->name);
        platform_print("\n");
        return;
    }

    if (!intent_capabilities_exist(target)) {
        return;
    }

    platform_print("Intent switch: ");

    if (current_intent_running != 0) {
        platform_print(current_intent);
    } else {
        platform_print("none");
    }

    platform_print(" -> ");
    platform_print(name);
    platform_print("\n");

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

    if (current_intent_running != 0) {
        current = intent_find(old_intent_name);

        if (current != 0) {
            intent_stop_entry(current);
        } else {
            current_intent = "none";
            current_intent_running = 0;
            intent_context_cleanup();
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

    if (str_equal_local(name, "context")) {
        intent_context_show();
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
            platform_print("intent plan not supported: ");
            platform_print(intent_name);
            platform_print("\n");
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
            platform_print("intent run not supported: ");
            platform_print(intent_name);
            platform_print("\n");
            return;
        }

        intent_run_entry(intent);
        return;
    }

    if (str_starts_with_local(name, "stop ")) {
        const char* intent_name = skip_token_local(name);
        const intent_entry_t* intent = intent_find(intent_name);

        if (intent == 0) {
            platform_print("intent stop not supported: ");
            platform_print(intent_name);
            platform_print("\n");
            return;
        }

        intent_stop_entry(intent);
        return;
    }

    const intent_entry_t* legacy_intent = intent_find(name);

    if (legacy_intent != 0) {
        platform_print("deprecated: use intent run ");
        platform_print(name);
        platform_print("\n");
        intent_run_entry(legacy_intent);
        return;
    }

    platform_print("intent not found: ");
    platform_print(name);
    platform_print("\n");
}