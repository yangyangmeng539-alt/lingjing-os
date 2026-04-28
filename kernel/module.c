#include "module.h"
#include "screen.h"
#include "security.h"

#define MAX_MODULES 16

static void module_print_uint(unsigned int value) {
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
        screen_print("module load blocked by security: ");
        screen_print(name);
        screen_print("\n");
        return;
    }

    if (module_exists(name)) {
        screen_print("module already loaded: ");
        screen_print(name);
        screen_print("\n");
        return;
    }

    if (str_equal_local(name, "ai")) {
        screen_print("loading module: ai\n");
        module_register("ai", "loaded", "external", "intent", "core");
        screen_print("module loaded.\n");
        return;
    }

    if (str_equal_local(name, "net")) {
        screen_print("loading module: net\n");
        module_register("net", "loaded", "external", "network", "timer");
        screen_print("module loaded.\n");
        return;
    }

    if (str_equal_local(name, "gui")) {
        screen_print("loading module: gui\n");
        module_register("gui", "loaded", "external", "display", "screen");
        screen_print("module loaded.\n");
        return;
    }

    screen_print("module source not found: ");
    screen_print(name);
    screen_print("\n");
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
        screen_print("module not loaded: ");
        screen_print(name);
        screen_print("\n");
        return;
    }

    if (!security_check_module_unload(name, modules[found_index].type)) {
        screen_print("module unload blocked by security: ");
        screen_print(name);
        screen_print("\n");
        return;
    }

    if (!str_equal_local(modules[found_index].type, "external")) {
        screen_print("cannot unload core module: ");
        screen_print(name);
        screen_print("\n");
        return;
    }

    screen_print("unloading module: ");
    screen_print(name);
    screen_print("\n");

    for (int i = found_index; i < module_count - 1; i++) {
        modules[i] = modules[i + 1];
    }

    module_count--;

    screen_print("module unloaded.\n");
}

void module_list(void) {
    screen_print("Registered modules:\n");

    for (int i = 0; i < module_count; i++) {
        screen_print("  ");
        screen_print(modules[i].name);
        screen_print("    ");
        screen_print(modules[i].status);
        screen_print("\n");
    }
}

void module_info(const char* name) {
    for (int i = 0; i < module_count; i++) {
        if (str_equal_local(modules[i].name, name)) {
            screen_print("Module:\n");

            screen_print("  name:       ");
            screen_print(modules[i].name);
            screen_print("\n");

            screen_print("  status:     ");
            screen_print(modules[i].status);
            screen_print("\n");

            screen_print("  type:       ");
            screen_print(modules[i].type);
            screen_print("\n");

            screen_print("  permission: ");
            screen_print(modules[i].permission);
            screen_print("\n");

            screen_print("  depends:    ");
            screen_print(modules[i].depends);
            screen_print("\n");

            return;
        }
    }

    screen_print("module not found: ");
    screen_print(name);
    screen_print("\n");
}

void module_deps(const char* name) {
    for (int i = 0; i < module_count; i++) {
        if (str_equal_local(modules[i].name, name)) {
            screen_print("Module dependency:\n");
            screen_print("  ");
            screen_print(modules[i].name);
            screen_print(" -> ");
            screen_print(modules[i].depends);
            screen_print("\n");
            return;
        }
    }

    screen_print("module not found: ");
    screen_print(name);
    screen_print("\n");
}

static void module_tree_print_children(const char* parent, int depth) {
    for (int i = 0; i < module_count; i++) {
        if (str_equal_local(modules[i].depends, parent)) {
            for (int s = 0; s < depth; s++) {
                screen_print("  ");
            }

            screen_print(modules[i].name);
            screen_print("\n");

            module_tree_print_children(modules[i].name, depth + 1);
        }
    }
}

void module_tree(void) {
    screen_print("Module tree:\n");

    screen_print("  core\n");
    module_tree_print_children("core", 2);

    for (int i = 0; i < module_count; i++) {
        if (!str_equal_local(modules[i].depends, "none") &&
            !module_exists(modules[i].depends)) {
            screen_print("  ");
            screen_print(modules[i].depends);
            screen_print("\n");

            module_tree_print_children(modules[i].depends, 2);
        }
    }
}

void module_check(void) {
    int ok_count = 0;
    int broken_count = 0;

    screen_print("Module dependency check:\n");

    for (int i = 0; i < module_count; i++) {
        screen_print("  ");
        screen_print(modules[i].name);
        screen_print("    ");

        if (str_equal_local(modules[i].depends, "none")) {
            screen_print("ok");
            ok_count++;
        } else if (module_exists(modules[i].depends)) {
            screen_print("ok depends ");
            screen_print(modules[i].depends);
            ok_count++;
        } else {
            screen_print("broken depends ");
            screen_print(modules[i].depends);
            broken_count++;
        }

        screen_print("\n");
    }

    screen_print("summary:\n");

    screen_print("  total:  ");
    module_print_uint((unsigned int)module_count);
    screen_print("\n");

    screen_print("  ok:     ");
    module_print_uint((unsigned int)ok_count);
    screen_print("\n");

    screen_print("  broken: ");
    module_print_uint((unsigned int)broken_count);
    screen_print("\n");
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
            screen_print("module dependency broken: ");
            screen_print(name);
            screen_print("\n");
            return;
        }
    }

    screen_print("module not found: ");
    screen_print(name);
    screen_print("\n");
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
            } else if (str_equal_local(name, "memory")) {
                modules[i].depends = "core";
            } else if (str_equal_local(name, "shell")) {
                modules[i].depends = "keyboard";
            } else if (str_equal_local(name, "gui")) {
                modules[i].depends = "screen";
            } else if (str_equal_local(name, "net")) {
                modules[i].depends = "timer";
            } else if (str_equal_local(name, "ai")) {
                modules[i].depends = "core";
            } else {
                screen_print("no default dependency for module: ");
                screen_print(name);
                screen_print("\n");
                return;
            }

            screen_print("module dependency fixed: ");
            screen_print(name);
            screen_print("\n");
            return;
        }
    }

    screen_print("module not found: ");
    screen_print(name);
    screen_print("\n");
}