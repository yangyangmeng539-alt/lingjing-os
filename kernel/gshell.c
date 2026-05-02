#include "gshell.h"
#include "platform.h"
#include "graphics.h"
#include "module.h"
#include "capability.h"
#include "intent.h"

static int gshell_initialized = 0;
static int gshell_broken = 0;

static unsigned int gshell_render_count = 0;
static unsigned int gshell_blocked_count = 0;
static const char* gshell_last_view = "none";
static const char* gshell_mode = "metadata-only";

void gshell_init(void) {
    gshell_initialized = 1;
    gshell_broken = 0;
    gshell_render_count = 0;
    gshell_blocked_count = 0;
    gshell_last_view = "none";
    gshell_mode = "metadata-only";
}

int gshell_is_ready(void) {
    if (!gshell_initialized) {
        return 0;
    }

    if (gshell_broken) {
        return 0;
    }

    if (!graphics_is_ready()) {
        return 0;
    }

    if (!module_dependency_ok("gshell")) {
        return 0;
    }

    if (!capability_is_ready("gui.shell")) {
        return 0;
    }

    return 1;
}

int gshell_is_broken(void) {
    return gshell_broken;
}

unsigned int gshell_get_render_count(void) {
    return gshell_render_count;
}

const char* gshell_get_last_view(void) {
    return gshell_last_view;
}

const char* gshell_get_mode(void) {
    return gshell_mode;
}

void gshell_status(void) {
    platform_print("Graphical shell status:\n");

    platform_print("  initialized: ");
    platform_print(gshell_initialized ? "yes\n" : "no\n");

    platform_print("  broken:      ");
    platform_print(gshell_broken ? "yes\n" : "no\n");

    platform_print("  mode:        ");
    platform_print(gshell_mode);
    platform_print("\n");

    platform_print("  graphics:    ");
    platform_print(graphics_is_ready() ? "ready\n" : "not ready\n");

    platform_print("  capability:  ");
    platform_print(capability_is_ready("gui.shell") ? "ready\n" : "blocked\n");

    platform_print("  module deps: ");
    platform_print(module_dependency_ok("gshell") ? "ok\n" : "broken\n");

    platform_print("  last view:   ");
    platform_print(gshell_last_view);
    platform_print("\n");

    platform_print("  renders:     ");
    platform_print_uint(gshell_render_count);
    platform_print("\n");

    platform_print("  blocked:     ");
    platform_print_uint(gshell_blocked_count);
    platform_print("\n");

    platform_print("  ready:       ");
    platform_print(gshell_is_ready() ? "yes\n" : "no\n");
}

void gshell_check(void) {
    platform_print("Graphical shell check:\n");

    platform_print("  initialized: ");
    platform_print(gshell_initialized ? "ok\n" : "bad\n");

    platform_print("  graphics:    ");
    platform_print(graphics_is_ready() ? "ready\n" : "blocked\n");

    platform_print("  capability:  ");
    platform_print(capability_is_ready("gui.shell") ? "ready\n" : "blocked\n");

    platform_print("  module deps: ");
    platform_print(module_dependency_ok("gshell") ? "ok\n" : "broken\n");

    platform_print("  broken:      ");
    platform_print(gshell_broken ? "yes\n" : "no\n");

    platform_print("  result:      ");

    if (gshell_broken) {
        platform_print("broken\n");
        return;
    }

    if (!gshell_initialized) {
        platform_print("blocked\n");
        return;
    }

    if (!graphics_is_ready()) {
        platform_print("blocked\n");
        return;
    }

    if (!capability_is_ready("gui.shell")) {
        platform_print("blocked\n");
        return;
    }

    if (!module_dependency_ok("gshell")) {
        platform_print("blocked\n");
        return;
    }

    platform_print("ready\n");
}

void gshell_doctor(void) {
    platform_print("Graphical shell doctor:\n");

    platform_print("  layer:       ");
    platform_print(gshell_initialized ? "initialized\n" : "not initialized\n");

    platform_print("  mode:        metadata-only\n");

    platform_print("  graphics:    ");
    platform_print(graphics_is_ready() ? "ready\n" : "not ready\n");

    platform_print("  capability:  ");
    platform_print(capability_is_ready("gui.shell") ? "ready\n" : "blocked\n");

    platform_print("  module deps: ");
    platform_print(module_dependency_ok("gshell") ? "ok\n" : "broken\n");

    platform_print("  intent:      ");
    platform_print(intent_get_current_name());
    platform_print("\n");

    platform_print("  running:     ");
    platform_print(intent_is_running() ? "yes\n" : "no\n");

    platform_print("  broken:      ");
    platform_print(gshell_broken ? "yes\n" : "no\n");

    platform_print("  result:      ");

    if (gshell_broken) {
        platform_print("broken\n");
        return;
    }

    if (!gshell_initialized) {
        platform_print("blocked\n");
        return;
    }

    if (!graphics_is_ready()) {
        platform_print("blocked\n");
        return;
    }

    if (!capability_is_ready("gui.shell")) {
        platform_print("blocked\n");
        return;
    }

    if (!module_dependency_ok("gshell")) {
        platform_print("blocked\n");
        return;
    }

    platform_print("ready\n");
}

static void gshell_render_begin(const char* view) {
    gshell_last_view = view;
    gshell_render_count++;

    platform_print("Graphical shell render:\n");

    platform_print("  view:   ");
    platform_print(view);
    platform_print("\n");

    platform_print("  mode:   ");
    platform_print(gshell_mode);
    platform_print("\n");
}

static int gshell_render_guard(void) {
    if (!gshell_is_ready()) {
        gshell_blocked_count++;

        platform_print("  result: blocked\n");
        platform_print("  reason: graphical shell not ready\n");
        return 0;
    }

    return 1;
}

void gshell_panel(void) {
    gshell_render_begin("panel");

    if (!gshell_render_guard()) {
        return;
    }

    graphics_panel();

    platform_print("  title:  Lingjing OS Runtime\n");
    platform_print("  blocks: intent capability module task health doctor\n");

    platform_print("  intent: ");
    platform_print(intent_get_current_name());
    platform_print("\n");

    platform_print("  running:");
    platform_print(intent_is_running() ? " yes\n" : " no\n");

    platform_print("  gui.shell: ");
    platform_print(capability_is_ready("gui.shell") ? "ready\n" : "blocked\n");

    platform_print("  result: staged\n");
}

void gshell_runtime(void) {
    gshell_render_begin("runtime");

    if (!gshell_render_guard()) {
        return;
    }

    graphics_rect(16, 16, 360, 220, 0x00112233);
    graphics_text(28, 32, "Lingjing Runtime");
    graphics_text(28, 56, "intent -> capability -> module -> task");
    graphics_text(28, 80, "graphics backend: gated");

    platform_print("  runtime chain:\n");
    platform_print("    intent\n");
    platform_print("    capability\n");
    platform_print("    module\n");
    platform_print("    task context\n");
    platform_print("    health / doctor\n");

    platform_print("  current intent: ");
    platform_print(intent_get_current_name());
    platform_print("\n");

    platform_print("  running:        ");
    platform_print(intent_is_running() ? "yes\n" : "no\n");

    intent_context_report();

    platform_print("  result: staged\n");
}

void gshell_intent(void) {
    gshell_render_begin("intent");

    if (!gshell_render_guard()) {
        return;
    }

    graphics_rect(16, 16, 360, 180, 0x00002244);
    graphics_text(28, 32, "Intent Panel");
    graphics_text(28, 56, intent_get_current_name());

    platform_print("  current intent: ");
    platform_print(intent_get_current_name());
    platform_print("\n");

    platform_print("  running:        ");
    platform_print(intent_is_running() ? "yes\n" : "no\n");

    intent_context_report();

    platform_print("  result:         staged\n");
}

void gshell_system(void) {
    int deps_broken = module_has_broken_dependencies();

    gshell_render_begin("system");

    if (!gshell_render_guard()) {
        return;
    }

    graphics_rect(16, 16, 360, 200, 0x00003322);
    graphics_text(28, 32, "System Panel");

    platform_print("  module deps: ");
    platform_print(deps_broken ? "broken\n" : "ok\n");

    platform_print("  health:      ");
    platform_print(deps_broken ? "bad\n" : "ok\n");

    platform_print("  graphics:    ");
    platform_print(graphics_is_ready() ? "ready\n" : "blocked\n");

    platform_print("  gui.shell:   ");
    platform_print(capability_is_ready("gui.shell") ? "ready\n" : "blocked\n");

    platform_print("  result:      staged\n");
}

void gshell_dashboard(void) {
    int deps_broken = module_has_broken_dependencies();

    gshell_render_begin("dashboard");

    if (!gshell_render_guard()) {
        return;
    }

    graphics_rect(8, 8, 480, 300, 0x00101022);
    graphics_text(20, 24, "Lingjing OS Dashboard");
    graphics_text(20, 48, "Intent / Capability / Module / Task / Doctor");

    platform_print("Graphical runtime dashboard:\n");

    platform_print("  intent:      ");
    platform_print(intent_get_current_name());
    platform_print("\n");

    platform_print("  running:     ");
    platform_print(intent_is_running() ? "yes\n" : "no\n");

    platform_print("  module deps: ");
    platform_print(deps_broken ? "broken\n" : "ok\n");

    platform_print("  graphics:    ");
    platform_print(graphics_is_ready() ? "ready\n" : "blocked\n");

    platform_print("  gshell:      ");
    platform_print(gshell_is_ready() ? "ready\n" : "blocked\n");

    platform_print("  gui.shell:   ");
    platform_print(capability_is_ready("gui.shell") ? "ready\n" : "blocked\n");

    platform_print("  gui.graphics:");
    platform_print(capability_is_ready("gui.graphics") ? " ready\n" : " blocked\n");

    platform_print("  gui.fb:      ");
    platform_print(capability_is_ready("gui.framebuffer") ? "ready\n" : "blocked\n");

    intent_context_report();

    platform_print("  result:      staged\n");
}

void gshell_text_panel(void) {
    int deps_broken = module_has_broken_dependencies();

    gshell_render_begin("text-panel");

    if (!gshell_render_guard()) {
        return;
    }

    platform_clear();

    platform_print("+------------------------------------------------------------------------------+\n");
    platform_print("|                            Lingjing OS Runtime                               |\n");
    platform_print("+------------------------------------------------------------------------------+\n");
    platform_print("| mode: text-safe VGA shell                                                    |\n");
    platform_print("| view: graphical shell text panel                                             |\n");
    platform_print("+------------------------------------------------------------------------------+\n");
    platform_print("| Intent                                                                       |\n");
    platform_print("|   current: ");

    platform_print(intent_get_current_name());

    platform_print("\n");
    platform_print("|   running: ");

    platform_print(intent_is_running() ? "yes" : "no");

    platform_print("\n");
    platform_print("+------------------------------------------------------------------------------+\n");
    platform_print("| Runtime Chain                                                                |\n");
    platform_print("|   intent -> capability -> module -> task context -> health -> doctor         |\n");
    platform_print("+------------------------------------------------------------------------------+\n");
    platform_print("| System                                                                       |\n");
    platform_print("|   module deps: ");

    platform_print(deps_broken ? "broken" : "ok");

    platform_print("\n");
    platform_print("|   graphics:    ");

    platform_print(graphics_is_ready() ? "ready" : "blocked");

    platform_print("\n");
    platform_print("|   gui.shell:   ");

    platform_print(capability_is_ready("gui.shell") ? "ready" : "blocked");

    platform_print("\n");
    platform_print("+------------------------------------------------------------------------------+\n");
    platform_print("| Status                                                                       |\n");
    platform_print("|   this is not real framebuffer graphics                                      |\n");
    platform_print("|   this is a text-mode graphical shell prototype                              |\n");
    platform_print("+------------------------------------------------------------------------------+\n");

    platform_print("result: staged\n");
}

void gshell_text_dashboard(void) {
    int deps_broken = module_has_broken_dependencies();

    gshell_render_begin("text-dashboard");

    if (!gshell_render_guard()) {
        return;
    }

    platform_clear();

    platform_print("Lingjing OS Dashboard\n");
    platform_print("================================================================================\n");

    platform_print("Boot / Display\n");
    platform_print("  boot mode:      text-safe\n");
    platform_print("  framebuffer:    guarded\n");
    platform_print("  real graphics:  disabled\n");
    platform_print("  text UI:        enabled\n");
    platform_print("\n");

    platform_print("Runtime\n");
    platform_print("  intent:         ");
    platform_print(intent_get_current_name());
    platform_print("\n");

    platform_print("  running:        ");
    platform_print(intent_is_running() ? "yes" : "no");
    platform_print("\n");

    platform_print("  module deps:    ");
    platform_print(deps_broken ? "broken" : "ok");
    platform_print("\n");

    platform_print("  graphics:       ");
    platform_print(graphics_is_ready() ? "ready" : "blocked");
    platform_print("\n");

    platform_print("  gshell:         ");
    platform_print(gshell_is_ready() ? "ready" : "blocked");
    platform_print("\n");

    platform_print("\n");

    platform_print("Capabilities\n");
    platform_print("  gui.framebuffer:");
    platform_print(capability_is_ready("gui.framebuffer") ? " ready\n" : " blocked\n");

    platform_print("  gui.graphics:   ");
    platform_print(capability_is_ready("gui.graphics") ? "ready\n" : "blocked\n");

    platform_print("  gui.shell:      ");
    platform_print(capability_is_ready("gui.shell") ? "ready\n" : "blocked\n");

    platform_print("\n");

    intent_context_report();

    platform_print("================================================================================\n");
    platform_print("result: staged\n");
}

void gshell_runtime_check(void) {
    int deps_broken = module_has_broken_dependencies();

    platform_print("Graphical runtime check:\n");

    platform_print("  framebuffer: ");
    platform_print(capability_is_ready("gui.framebuffer") ? "ready\n" : "blocked\n");

    platform_print("  graphics:    ");
    platform_print(capability_is_ready("gui.graphics") ? "ready\n" : "blocked\n");

    platform_print("  shell:       ");
    platform_print(capability_is_ready("gui.shell") ? "ready\n" : "blocked\n");

    platform_print("  module deps: ");
    platform_print(deps_broken ? "broken\n" : "ok\n");

    platform_print("  intent:      ");
    platform_print(intent_get_current_name());
    platform_print("\n");

    platform_print("  running:     ");
    platform_print(intent_is_running() ? "yes\n" : "no\n");

    platform_print("  result:      ");

    if (deps_broken) {
        platform_print("blocked\n");
        return;
    }

    if (!capability_is_ready("gui.framebuffer")) {
        platform_print("blocked\n");
        return;
    }

    if (!capability_is_ready("gui.graphics")) {
        platform_print("blocked\n");
        return;
    }

    if (!capability_is_ready("gui.shell")) {
        platform_print("blocked\n");
        return;
    }

    platform_print("ready\n");
}

void gshell_break(void) {
    gshell_broken = 1;

    platform_print("Graphical shell break:\n");
    platform_print("  result: broken\n");
}

void gshell_fix(void) {
    gshell_broken = 0;

    if (!gshell_initialized) {
        gshell_init();
    }

    platform_print("Graphical shell fix:\n");
    platform_print("  result: fixed\n");
}