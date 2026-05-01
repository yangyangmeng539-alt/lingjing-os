#include "module.h"
#include "screen.h"
#include "security.h"
#include "scheduler.h"
#include "platform.h"

#define MAX_MODULES 32

typedef struct module_entry {
    const char* name;
    const char* status;
    const char* type;
    const char* permission;
    const char* depends;
} module_entry_t;

static module_entry_t modules[MAX_MODULES];
static int module_count = 0;

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

void module_init(void) {
    module_count = 0;
}

int module_exists(const char* name) {
    for (int i = 0; i < module_count; i++) {
        if (str_equal_local(modules[i].name, name)) {
            return 1;
        }
    }

    return 0;
}

int module_count_loaded(void) {
    return module_count;
}

void module_register(const char* name, const char* status, const char* type, const char* permission, const char* depends) {
    if (module_count >= MAX_MODULES) {
        return;
    }

    if (module_exists(name)) {
        return;
    }

    modules[module_count].name = name;
    modules[module_count].status = status;
    modules[module_count].type = type;
    modules[module_count].permission = permission;
    modules[module_count].depends = depends;
    module_count++;
}

void module_load_mock(const char* name) {
    if (!security_check_module_load(name)) {
        platform_print("module load blocked by security: ");
        platform_print(name);
        platform_print("\n");
        return;
    }

    if (module_exists(name)) {
        platform_print("module already loaded: ");
        platform_print(name);
        platform_print("\n");
        return;
    }

    if (str_equal_local(name, "gui")) {
        platform_print("loading module: gui\n");
        module_register("gui", "loaded", "external", "display", "screen");
        platform_print("module loaded.\n");
        return;
    }

    if (str_equal_local(name, "net")) {
        platform_print("loading module: net\n");
        module_register("net", "loaded", "external", "network", "timer");
        platform_print("module loaded.\n");
        return;
    }

    if (str_equal_local(name, "ai")) {
        platform_print("loading module: ai\n");
        module_register("ai", "loaded", "external", "intent", "core");
        platform_print("module loaded.\n");
        return;
    }

    platform_print("unknown module: ");
    platform_print(name);
    platform_print("\n");
}

void module_unload_mock(const char* name) {
    int found_index = -1;

    for (int i = 0; i < module_count; i++) {
        if (str_equal_local(modules[i].name, name)) {
            found_index = i;
            break;
        }
    }

    if (found_index < 0) {
        platform_print("module not loaded: ");
        platform_print(name);
        platform_print("\n");
        return;
    }

    if (!security_check_module_unload(name, modules[found_index].type)) {
        platform_print("module unload blocked by security: ");
        platform_print(name);
        platform_print("\n");
        return;
    }

    if (!str_equal_local(modules[found_index].type, "external")) {
        platform_print("cannot unload core module: ");
        platform_print(name);
        platform_print("\n");
        return;
    }

    platform_print("unloading module: ");
    platform_print(name);
    platform_print("\n");

    for (int i = found_index; i < module_count - 1; i++) {
        modules[i] = modules[i + 1];
    }

    module_count--;

    platform_print("module unloaded.\n");
}

void module_list(void) {
    platform_print("Registered modules:\n");

    for (int i = 0; i < module_count; i++) {
        platform_print("  ");
        platform_print(modules[i].name);
        platform_print("    ");
        platform_print(modules[i].status);
        platform_print("\n");
    }
}

void module_info(const char* name) {
    for (int i = 0; i < module_count; i++) {
        if (str_equal_local(modules[i].name, name)) {
            platform_print("Module:\n");

            platform_print("  name:       ");
            platform_print(modules[i].name);
            platform_print("\n");

            platform_print("  status:     ");
            platform_print(modules[i].status);
            platform_print("\n");

            platform_print("  type:       ");
            platform_print(modules[i].type);
            platform_print("\n");

            platform_print("  permission: ");
            platform_print(modules[i].permission);
            platform_print("\n");

            platform_print("  depends:    ");
            platform_print(modules[i].depends);
            platform_print("\n");

            return;
        }
    }

    platform_print("module not found: ");
    platform_print(name);
    platform_print("\n");
}

void module_deps(const char* name) {
    for (int i = 0; i < module_count; i++) {
        if (str_equal_local(modules[i].name, name)) {
            platform_print("Module dependency:\n");

            platform_print("  ");
            platform_print(modules[i].name);
            platform_print(" -> ");
            platform_print(modules[i].depends);
            platform_print("\n");

            return;
        }
    }

    platform_print("module not found: ");
    platform_print(name);
    platform_print("\n");
}

static void module_tree_print_children(const char* parent, int depth) {
    for (int i = 0; i < module_count; i++) {
        if (str_equal_local(modules[i].depends, parent)) {
            for (int s = 0; s < depth; s++) {
                platform_print("  ");
            }

            platform_print(modules[i].name);
            platform_print("\n");

            module_tree_print_children(modules[i].name, depth + 1);
        }
    }
}

void module_tree(void) {
    platform_print("Module tree:\n");

    platform_print("  core\n");
    module_tree_print_children("core", 2);

    for (int i = 0; i < module_count; i++) {
        if (!str_equal_local(modules[i].depends, "none") &&
            !module_exists(modules[i].depends)) {
            platform_print("  ");
            platform_print(modules[i].depends);
            platform_print("\n");

            module_tree_print_children(modules[i].depends, 2);
        }
    }
}

void module_check(void) {
    int ok_count = 0;
    int broken_count = 0;

    platform_print("Module dependency check:\n");

    for (int i = 0; i < module_count; i++) {
        platform_print("  ");
        platform_print(modules[i].name);
        platform_print("    ");

        if (str_equal_local(modules[i].depends, "none")) {
            platform_print("ok");
            ok_count++;
        } else if (module_exists(modules[i].depends)) {
            platform_print("ok depends ");
            platform_print(modules[i].depends);
            ok_count++;
        } else {
            platform_print("broken depends ");
            platform_print(modules[i].depends);
            broken_count++;
        }

        platform_print("\n");
    }

    platform_print("summary:\n");

    platform_print("  total:  ");
    platform_print_uint((unsigned int)module_count);
    platform_print("\n");

    platform_print("  ok:     ");
    platform_print_uint((unsigned int)ok_count);
    platform_print("\n");

    platform_print("  broken: ");
    platform_print_uint((unsigned int)broken_count);
    platform_print("\n");
}

int module_has_broken_dependencies(void) {
    for (int i = 0; i < module_count; i++) {
        if (str_equal_local(modules[i].depends, "none")) {
            continue;
        }

        if (!module_exists(modules[i].depends)) {
            return 1;
        }
    }

    return 0;
}

const char* module_get_depends(const char* name) {
    for (int i = 0; i < module_count; i++) {
        if (str_equal_local(modules[i].name, name)) {
            return modules[i].depends;
        }
    }

    if (str_equal_local(name, "gui")) {
        return "screen";
    }

    if (str_equal_local(name, "net")) {
        return "timer";
    }

    if (str_equal_local(name, "ai")) {
        return "core";
    }

    if (str_equal_local(name, "capability")) {
        return "core";
    }

    return "unknown";
}

int module_dependency_ok(const char* name) {
    const char* depends = module_get_depends(name);

    if (str_equal_local(depends, "none")) {
        return 1;
    }

    if (str_equal_local(depends, "unknown")) {
        return 0;
    }

    return module_exists(depends);
}

void module_break_dependency(const char* name) {
    for (int i = 0; i < module_count; i++) {
        if (str_equal_local(modules[i].name, name)) {
            modules[i].depends = "missing";
            platform_print("module dependency broken: ");
            platform_print(name);
            platform_print("\n");
            return;
        }
    }

    platform_print("module not found: ");
    platform_print(name);
    platform_print("\n");
}

void module_fix_dependency(const char* name) {
    for (int i = 0; i < module_count; i++) {
        if (str_equal_local(modules[i].name, name)) {
            if (str_equal_local(name, "core")) {
                modules[i].depends = "none";
            } else if (str_equal_local(name, "screen")) {
                modules[i].depends = "core";
            } else if (str_equal_local(name, "gdt")) {
                modules[i].depends = "core";
            } else if (str_equal_local(name, "idt")) {
                modules[i].depends = "gdt";
            } else if (str_equal_local(name, "keyboard")) {
                modules[i].depends = "idt";
            } else if (str_equal_local(name, "timer")) {
                modules[i].depends = "idt";
            } else if (str_equal_local(name, "scheduler")) {
                modules[i].depends = "timer";
            } else if (str_equal_local(name, "security")) {
                modules[i].depends = "core";
            } else if (str_equal_local(name, "syscall")) {
                modules[i].depends = "security";
            } else if (str_equal_local(name, "user")) {
                modules[i].depends = "syscall";
            } else if (str_equal_local(name, "ring3")) {
                modules[i].depends = "user";
            } else if (str_equal_local(name, "platform")) {
                modules[i].depends = "core";
            } else if (str_equal_local(name, "lang")) {
                modules[i].depends = "core";
            } else if (str_equal_local(name, "identity")) {
                modules[i].depends = "security";
            } else if (str_equal_local(name, "health")) {
                modules[i].depends = "core";
            } else if (str_equal_local(name, "memory")) {
                modules[i].depends = "core";
            } else if (str_equal_local(name, "paging")) {
                modules[i].depends = "memory";
            } else if (str_equal_local(name, "capability")) {
                modules[i].depends = "core";
            } else if (str_equal_local(name, "shell")) {
                modules[i].depends = "keyboard";
            } else if (str_equal_local(name, "gui")) {
                modules[i].depends = "screen";
            } else if (str_equal_local(name, "net")) {
                modules[i].depends = "timer";
            } else if (str_equal_local(name, "ai")) {
                modules[i].depends = "core";
            } else if (str_equal_local(name, "fs")) {
                modules[i].depends = "paging";
            } else {
                platform_print("no default dependency for module: ");
                platform_print(name);
                platform_print("\n");
                return;
            }

            platform_print("module dependency fixed: ");
            platform_print(name);
            platform_print("\n");
            return;
        }
    }

    platform_print("module not found: ");
    platform_print(name);
    platform_print("\n");
}