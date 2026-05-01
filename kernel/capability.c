#include "capability.h"
#include "platform.h"
#include "module.h"

#define MAX_CAPABILITIES 16

typedef struct capability_entry {
    const char* name;
    const char* provider;
    const char* permission;
    const char* status;
} capability_entry_t;

static capability_entry_t capability_table[MAX_CAPABILITIES];
static int capability_count = 0;
static int capability_initialized = 0;

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

static void capability_register(const char* name, const char* provider, const char* permission, const char* status) {
    if (capability_count >= MAX_CAPABILITIES) {
        return;
    }

    capability_table[capability_count].name = name;
    capability_table[capability_count].provider = provider;
    capability_table[capability_count].permission = permission;
    capability_table[capability_count].status = status;
    capability_count++;
}

void capability_init(void) {
    capability_count = 0;
    capability_initialized = 1;

    capability_register("gui.display", "gui", "display", "available");
    capability_register("net.http", "net", "network", "available");
    capability_register("ai.assist", "ai", "intent", "available");
    capability_register("file.read", "fs", "file", "planned");
    capability_register("sys.health", "health", "diagnostic", "ready");
    capability_register("sys.syscall", "syscall", "syscall", "ready");
    capability_register("user.ring3", "ring3", "ring3", "ready");
}

int capability_exists(const char* name) {
    for (int i = 0; i < capability_count; i++) {
        if (str_equal_local(capability_table[i].name, name)) {
            return 1;
        }
    }

    return 0;
}

const char* capability_get_provider(const char* name) {
    for (int i = 0; i < capability_count; i++) {
        if (str_equal_local(capability_table[i].name, name)) {
            return capability_table[i].provider;
        }
    }

    return "unknown";
}

const char* capability_get_permission(const char* name) {
    for (int i = 0; i < capability_count; i++) {
        if (str_equal_local(capability_table[i].name, name)) {
            return capability_table[i].permission;
        }
    }

    return "unknown";
}

const char* capability_get_status(const char* name) {
    for (int i = 0; i < capability_count; i++) {
        if (str_equal_local(capability_table[i].name, name)) {
            return capability_table[i].status;
        }
    }

    return "unknown";
}

int capability_is_ready(const char* name) {
    const char* provider = capability_get_provider(name);

    if (str_equal_local(provider, "unknown")) {
        return 0;
    }

    if (str_equal_local(provider, "fs")) {
        return 0;
    }

    if (!module_exists(provider)) {
        return 0;
    }

    if (!module_dependency_ok(provider)) {
        return 0;
    }

    return 1;
}

void capability_list(void) {
    platform_print("Capability registry:\n");

    if (!capability_initialized) {
        platform_print("  result: not initialized\n");
        return;
    }

    for (int i = 0; i < capability_count; i++) {
        platform_print("  ");
        platform_print(capability_table[i].name);
        platform_print("    provider: ");
        platform_print(capability_table[i].provider);
        platform_print("    status: ");
        platform_print(capability_table[i].status);
        platform_print("\n");
    }

    platform_print("summary:\n");

    platform_print("  total: ");
    platform_print_uint((unsigned int)capability_count);
    platform_print("\n");
}

void capability_info(const char* name) {
    for (int i = 0; i < capability_count; i++) {
        if (str_equal_local(capability_table[i].name, name)) {
            platform_print("Capability:\n");

            platform_print("  name:       ");
            platform_print(capability_table[i].name);
            platform_print("\n");

            platform_print("  provider:   ");
            platform_print(capability_table[i].provider);
            platform_print("\n");

            platform_print("  permission: ");
            platform_print(capability_table[i].permission);
            platform_print("\n");

            platform_print("  status:     ");
            platform_print(capability_table[i].status);
            platform_print("\n");

            platform_print("  module:     ");
            platform_print(module_exists(capability_table[i].provider) ? "loaded\n" : "not loaded\n");

            platform_print("  ready:      ");
            platform_print(capability_is_ready(name) ? "yes\n" : "no\n");

            return;
        }
    }

    platform_print("capability not found: ");
    platform_print(name);
    platform_print("\n");
}

void capability_check(const char* name) {
    const char* provider = capability_get_provider(name);
    const char* permission = capability_get_permission(name);
    const char* status = capability_get_status(name);

    platform_print("Capability check:\n");

    platform_print("  name:       ");
    platform_print(name);
    platform_print("\n");

    if (!capability_exists(name)) {
        platform_print("  result:     missing\n");
        return;
    }

    platform_print("  provider:   ");
    platform_print(provider);
    platform_print("\n");

    platform_print("  permission: ");
    platform_print(permission);
    platform_print("\n");

    platform_print("  status:     ");
    platform_print(status);
    platform_print("\n");

    platform_print("  module:     ");
    platform_print(module_exists(provider) ? "loaded\n" : "not loaded\n");

    platform_print("  dependency: ");

    if (module_dependency_ok(provider)) {
        platform_print("ok\n");
    } else {
        platform_print("broken\n");
    }

    platform_print("  result:     ");
    platform_print(capability_is_ready(name) ? "ready\n" : "blocked\n");
}

void capability_doctor(void) {
    int missing_provider_count = 0;
    int broken_dependency_count = 0;
    int ready_count = 0;

    platform_print("Capability doctor:\n");

    if (!capability_initialized) {
        platform_print("  registry:   not initialized\n");
        platform_print("  result:     blocked\n");
        return;
    }

    platform_print("  registry:   initialized\n");

    for (int i = 0; i < capability_count; i++) {
        const char* provider = capability_table[i].provider;

        platform_print("  ");
        platform_print(capability_table[i].name);
        platform_print(" -> ");
        platform_print(provider);
        platform_print(" : ");

        if (str_equal_local(provider, "fs")) {
            platform_print("planned\n");
            missing_provider_count++;
            continue;
        }

        if (!module_exists(provider)) {
            platform_print("provider not loaded\n");
            missing_provider_count++;
            continue;
        }

        if (!module_dependency_ok(provider)) {
            platform_print("dependency broken\n");
            broken_dependency_count++;
            continue;
        }

        platform_print("ready\n");
        ready_count++;
    }

    platform_print("summary:\n");

    platform_print("  total:             ");
    platform_print_uint((unsigned int)capability_count);
    platform_print("\n");

    platform_print("  ready:             ");
    platform_print_uint((unsigned int)ready_count);
    platform_print("\n");

    platform_print("  missing provider:  ");
    platform_print_uint((unsigned int)missing_provider_count);
    platform_print("\n");

    platform_print("  broken dependency: ");
    platform_print_uint((unsigned int)broken_dependency_count);
    platform_print("\n");

    platform_print("  result:            ");

    if (broken_dependency_count > 0) {
        platform_print("blocked\n");
        return;
    }

    platform_print("ready\n");
}