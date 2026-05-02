#include "gshell.h"
#include "platform.h"
#include "graphics.h"
#include "module.h"
#include "capability.h"
#include "intent.h"
#include "framebuffer.h"
#include "health.h"
#include "scheduler.h"
#include "timer.h"
#include "memory.h"
#include "paging.h"
#include "syscall.h"
#include "user.h"
#include "ring3.h"

static int gshell_initialized = 0;
static int gshell_broken = 0;

static unsigned int gshell_render_count = 0;
static unsigned int gshell_blocked_count = 0;
static const char* gshell_last_view = "none";
static const char* gshell_mode = "metadata-only";
static const char* gshell_output_mode = "text-vga";
static const char* gshell_output_plan = "text-vga";
static const char* gshell_output_reason = "default VGA text output";
static unsigned int gshell_output_changes = 0;

#define GSHELL_INPUT_BUFFER_SIZE 64

static char gshell_input_buffer[GSHELL_INPUT_BUFFER_SIZE];
static unsigned int gshell_input_len = 0;
static unsigned int gshell_input_events = 0;
static unsigned int gshell_input_enters = 0;
static unsigned int gshell_input_backspaces = 0;
static const char* gshell_input_status_text = "READY";
static const char* gshell_command_name = "NONE";
static const char* gshell_command_result = "NO COMMAND";
static const char* gshell_command_view = "IDLE";
static unsigned int gshell_command_dispatches = 0;
static unsigned int gshell_command_unknown = 0;
static unsigned int gshell_kernel_bridge_refreshes = 0;
#define GSHELL_HISTORY_SIZE 4
#define GSHELL_HISTORY_TEXT_SIZE 64

static char gshell_history_commands[GSHELL_HISTORY_SIZE][GSHELL_HISTORY_TEXT_SIZE];
static char gshell_history_views[GSHELL_HISTORY_SIZE][GSHELL_HISTORY_TEXT_SIZE];
static unsigned int gshell_history_count = 0;
static unsigned int gshell_history_total = 0;
#define GSHELL_RESULT_LOG_SIZE 4
#define GSHELL_RESULT_LOG_TEXT_SIZE 64

static char gshell_result_log_commands[GSHELL_RESULT_LOG_SIZE][GSHELL_RESULT_LOG_TEXT_SIZE];
static char gshell_result_log_results[GSHELL_RESULT_LOG_SIZE][GSHELL_RESULT_LOG_TEXT_SIZE];
static unsigned int gshell_result_log_count = 0;
static unsigned int gshell_result_log_total = 0;

static void gshell_copy_text(char* dst, const char* src, unsigned int max_len);
static void gshell_history_clear(void);
static void gshell_history_push(const char* command, const char* view);
static void gshell_result_log_clear(void);
static void gshell_result_log_push(const char* command, const char* result);

void gshell_init(void) {
    unsigned int i = 0;

    gshell_initialized = 1;
    gshell_broken = 0;
    gshell_render_count = 0;
    gshell_blocked_count = 0;
    gshell_last_view = "none";
    gshell_mode = "metadata-only";

    gshell_output_mode = "text-vga";
    gshell_output_plan = "text-vga";
    gshell_output_reason = "default VGA text output";
    gshell_output_changes = 0;

    gshell_input_len = 0;
    gshell_input_events = 0;
    gshell_input_enters = 0;
    gshell_input_backspaces = 0;
    gshell_input_status_text = "READY";

    gshell_command_name = "NONE";
    gshell_command_result = "NO COMMAND";
    gshell_command_view = "IDLE";
    gshell_command_dispatches = 0;
    gshell_command_unknown = 0;
    gshell_kernel_bridge_refreshes = 0;

    for (i = 0; i < GSHELL_INPUT_BUFFER_SIZE; i++) {
        gshell_input_buffer[i] = '\0';
    }

    gshell_history_clear();
    gshell_result_log_clear();
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

int gshell_output_graphics_planned(void) {
    if (gshell_output_plan[0] == 'g') {
        return 1;
    }

    return 0;
}

const char* gshell_get_output_mode(void) {
    return gshell_output_mode;
}

const char* gshell_get_output_plan(void) {
    return gshell_output_plan;
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

    platform_print("  output:      ");
    platform_print(gshell_output_mode);
    platform_print("\n");

    platform_print("  output plan: ");
    platform_print(gshell_output_plan);
    platform_print("\n");

    platform_print("  graphics:    ");
    platform_print(graphics_is_ready() ? "ready\n" : "not ready\n");

    platform_print("  display:     ");
    platform_print(framebuffer_get_profile());
    platform_print("\n");

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

    platform_print("  output:      ");
    platform_print(gshell_output_mode);
    platform_print("\n");

    platform_print("  output plan: ");
    platform_print(gshell_output_plan);
    platform_print("\n");

    platform_print("  graphics:    ");
    platform_print(graphics_is_ready() ? "ready\n" : "not ready\n");

    platform_print("  display:     ");
    platform_print(framebuffer_get_profile());
    platform_print("\n");

    platform_print("  capability:  ");
    platform_print(capability_is_ready("gui.shell") ? "ready\n" : "blocked\n");

    platform_print("  module deps: ");
    platform_print(module_dependency_ok("gshell") ? "ok\n" : "broken\n");

    platform_print("  intent:      ");
    platform_print(intent_get_current_name());
    platform_print("\n");

    platform_print("  running:     ");
    platform_print(intent_is_running() ? "yes\n" : "no\n");

    platform_print("  self-output: ");
    if (gshell_output_plan[0] == 'g') {
        platform_print("planned\n");
    } else {
        platform_print("not planned\n");
    }

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

void gshell_text_compact(void) {
    int deps_broken = module_has_broken_dependencies();

    gshell_render_begin("text-compact");

    if (!gshell_render_guard()) {
        return;
    }

    platform_clear();

    platform_print("Lingjing OS | text-mode graphical shell\n");
    platform_print("================================================================================\n");

    platform_print("intent: ");
    platform_print(intent_get_current_name());
    platform_print(" | run: ");
    platform_print(intent_is_running() ? "yes" : "no");
    platform_print(" | deps: ");
    platform_print(deps_broken ? "bad" : "ok");
    platform_print(" | gfx: ");
    platform_print(graphics_is_ready() ? "ready" : "bad");
    platform_print("\n");

    platform_print("cap: fb=");
    platform_print(capability_is_ready("gui.framebuffer") ? "ok" : "bad");
    platform_print(" gfx=");
    platform_print(capability_is_ready("gui.graphics") ? "ok" : "bad");
    platform_print(" shell=");
    platform_print(capability_is_ready("gui.shell") ? "ok" : "bad");
    platform_print("\n");

    platform_print("chain: intent -> capability -> module -> ctx -> health -> doctor\n");

    platform_print("display: ");
    platform_print(framebuffer_get_profile());
    platform_print(" | pixel-write: gated\n");

    platform_print("--------------------------------------------------------------------------------\n");

    if (intent_is_running()) {
        platform_print("ctx: active | intent=");
        platform_print(intent_get_current_name());
        platform_print(" | mods=gui,net,ai\n");

        platform_print("caps: gd=gui.display nh=net.http aa=ai.assist\n");
    } else {
        platform_print("ctx: inactive | intent=none | mods=none\n");
        platform_print("caps: none\n");
    }

    platform_print("--------------------------------------------------------------------------------\n");
    platform_print("cmd: gshell textcompact | gshell statusbar | health short | doctor short\n");
    platform_print("================================================================================\n");
    platform_print("result: staged\n");
}

void gshell_statusbar(void) {
    int deps_broken = module_has_broken_dependencies();

    gshell_render_begin("statusbar");

    if (!gshell_render_guard()) {
        return;
    }

    platform_print("LJ | intent=");
    platform_print(intent_get_current_name());
    platform_print(" | run=");
    platform_print(intent_is_running() ? "yes" : "no");
    platform_print(" | deps=");
    platform_print(deps_broken ? "bad" : "ok");
    platform_print(" | gfx=");
    platform_print(graphics_is_ready() ? "ready" : "blocked");
    platform_print(" | shell=");
    platform_print(capability_is_ready("gui.shell") ? "ready" : "blocked");
    platform_print("\n");
}

void gshell_output(void) {
    platform_print("GShell output:\n");

    platform_print("  current: ");
    platform_print(gshell_output_mode);
    platform_print("\n");

    platform_print("  planned: ");
    platform_print(gshell_output_plan);
    platform_print("\n");

    platform_print("  changes: ");
    platform_print_uint(gshell_output_changes);
    platform_print("\n");

    platform_print("  reason:  ");
    platform_print(gshell_output_reason);
    platform_print("\n");

    platform_print("  result:  ready\n");
}

void gshell_output_doctor(void) {
    platform_print("GShell output doctor:\n");

    platform_print("  current output: ");
    platform_print(gshell_output_mode);
    platform_print("\n");

    platform_print("  planned output: ");
    platform_print(gshell_output_plan);
    platform_print("\n");

    platform_print("  text-vga:       ");
    platform_print(gshell_output_mode[0] == 't' ? "active\n" : "inactive\n");

    platform_print("  graphics-self:  ");
    if (gshell_output_plan[0] == 'g') {
        platform_print("planned\n");
    } else {
        platform_print("not planned\n");
    }

    platform_print("  graphics:       ");
    platform_print(graphics_is_ready() ? "ready\n" : "blocked\n");

    platform_print("  text backend:   ");
    platform_print(graphics_text_backend_is_armed() ? "armed\n" : "disarmed\n");

    platform_print("  shell cap:      ");
    platform_print(capability_is_ready("gui.shell") ? "ready\n" : "blocked\n");

    platform_print("  can self output:");

    if (!graphics_is_ready()) {
        platform_print(" no\n");
        platform_print("  reason:         graphics not ready\n");
    } else if (!graphics_text_backend_is_armed()) {
        platform_print(" no\n");
        platform_print("  reason:         graphics text backend disarmed\n");
    } else {
        platform_print(" staged\n");
        platform_print("  reason:         self-output can be tested later\n");
    }

    platform_print("  result:         ready\n");
}

void gshell_output_text(void) {
    gshell_output_plan = "text-vga";
    gshell_output_reason = "user selected VGA text output";
    gshell_output_changes++;

    platform_print("GShell output text:\n");
    platform_print("  planned: text-vga\n");
    platform_print("  real switch: no\n");
    platform_print("  result:  staged\n");
}

void gshell_output_graphics(void) {
    gshell_output_plan = "graphics-self";
    gshell_output_reason = "user planned graphics shell self-output";
    gshell_output_changes++;

    platform_print("GShell output graphics:\n");
    platform_print("  planned: graphics-self\n");
    platform_print("  real switch: no\n");
    platform_print("  warning: requires framebuffer graphics mode later\n");
    platform_print("  result:  staged\n");
}

void gshell_self_check(void) {
    platform_print("GShell self-output check:\n");

    platform_print("  current output: ");
    platform_print(gshell_output_mode);
    platform_print("\n");

    platform_print("  planned output: ");
    platform_print(gshell_output_plan);
    platform_print("\n");

    platform_print("  display:        ");
    platform_print(framebuffer_get_profile());
    platform_print("\n");

    platform_print("  graphics:       ");
    platform_print(graphics_is_ready() ? "ready\n" : "blocked\n");

    platform_print("  shell cap:      ");
    platform_print(capability_is_ready("gui.shell") ? "ready\n" : "blocked\n");

    platform_print("  text backend:   ");
    platform_print(graphics_text_backend_is_armed() ? "armed\n" : "disarmed\n");

    platform_print("  real gate:      ");
    platform_print(graphics_real_is_armed() ? "armed\n" : "disarmed\n");

    platform_print("  framebuffer:    ");
    platform_print(framebuffer_is_ready() ? "ready\n" : "not ready\n");

    platform_print("  self-output:    ");

    if (gshell_output_plan[0] != 'g') {
        platform_print("blocked\n");
        platform_print("  reason:         gshell graphics-self not planned\n");
        return;
    }

    if (!graphics_is_ready()) {
        platform_print("blocked\n");
        platform_print("  reason:         graphics layer not ready\n");
        return;
    }

    if (!capability_is_ready("gui.shell")) {
        platform_print("blocked\n");
        platform_print("  reason:         gui.shell capability blocked\n");
        return;
    }

    if (!graphics_text_backend_is_armed()) {
        platform_print("blocked\n");
        platform_print("  reason:         graphics text backend disarmed\n");
        return;
    }

    platform_print("staged\n");
    platform_print("  reason:         graphics-self output metadata ready\n");
}

void gshell_self_render(void) {
    gshell_render_begin("self-render");

    platform_print("GShell self-render dryrun:\n");

    platform_print("  planned output: ");
    platform_print(gshell_output_plan);
    platform_print("\n");

    platform_print("  display:        ");
    platform_print(framebuffer_get_profile());
    platform_print("\n");

    platform_print("  backend:        graphics text dryrun\n");

    platform_print("  result:         ");

    if (gshell_output_plan[0] != 'g') {
        gshell_blocked_count++;
        platform_print("blocked\n");
        platform_print("  reason:         gshell graphics-self not planned\n");
        return;
    }

    if (!graphics_is_ready()) {
        gshell_blocked_count++;
        platform_print("blocked\n");
        platform_print("  reason:         graphics layer not ready\n");
        return;
    }

    if (!capability_is_ready("gui.shell")) {
        gshell_blocked_count++;
        platform_print("blocked\n");
        platform_print("  reason:         gui.shell capability blocked\n");
        return;
    }

    if (!graphics_text_backend_is_armed()) {
        gshell_blocked_count++;
        platform_print("blocked\n");
        platform_print("  reason:         graphics text backend disarmed\n");
        return;
    }

    graphics_text(16, 16, "Lingjing OS Runtime");
    graphics_text(16, 32, "intent capability module task health doctor");
    graphics_text(16, 48, "graphics-self-output staged");

    platform_print("  self render:    staged\n");
}

void gshell_runtime_check(void) {
    int deps_broken = module_has_broken_dependencies();

    platform_print("Graphical runtime check:\n");

    platform_print("  framebuffer: ");
    platform_print(capability_is_ready("gui.framebuffer") ? "ready\n" : "blocked\n");

    platform_print("  graphics:    ");
    platform_print(capability_is_ready("gui.graphics") ? "ready\n" : "blocked\n");

    platform_print("  display:        ");
    platform_print(framebuffer_get_profile());
    platform_print("\n");

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

static void gshell_copy_text(char* dst, const char* src, unsigned int max_len) {
    unsigned int i = 0;

    if (!dst || max_len == 0) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    while (src[i] != '\0' && i + 1 < max_len) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

static void gshell_history_clear(void) {
    unsigned int i = 0;

    gshell_history_count = 0;
    gshell_history_total = 0;

    for (i = 0; i < GSHELL_HISTORY_SIZE; i++) {
        gshell_history_commands[i][0] = '\0';
        gshell_history_views[i][0] = '\0';
    }
}

static void gshell_history_push(const char* command, const char* view) {
    unsigned int i = 0;

    if (!command || command[0] == '\0') {
        return;
    }

    if (gshell_history_count < GSHELL_HISTORY_SIZE) {
        gshell_copy_text(
            gshell_history_commands[gshell_history_count],
            command,
            GSHELL_HISTORY_TEXT_SIZE
        );

        gshell_copy_text(
            gshell_history_views[gshell_history_count],
            view,
            GSHELL_HISTORY_TEXT_SIZE
        );

        gshell_history_count++;
        gshell_history_total++;
        return;
    }

    for (i = 1; i < GSHELL_HISTORY_SIZE; i++) {
        gshell_copy_text(
            gshell_history_commands[i - 1],
            gshell_history_commands[i],
            GSHELL_HISTORY_TEXT_SIZE
        );

        gshell_copy_text(
            gshell_history_views[i - 1],
            gshell_history_views[i],
            GSHELL_HISTORY_TEXT_SIZE
        );
    }

    gshell_copy_text(
        gshell_history_commands[GSHELL_HISTORY_SIZE - 1],
        command,
        GSHELL_HISTORY_TEXT_SIZE
    );

    gshell_copy_text(
        gshell_history_views[GSHELL_HISTORY_SIZE - 1],
        view,
        GSHELL_HISTORY_TEXT_SIZE
    );

    gshell_history_total++;
}

static void gshell_result_log_clear(void) {
    unsigned int i = 0;

    gshell_result_log_count = 0;
    gshell_result_log_total = 0;

    for (i = 0; i < GSHELL_RESULT_LOG_SIZE; i++) {
        gshell_result_log_commands[i][0] = '\0';
        gshell_result_log_results[i][0] = '\0';
    }
}

static void gshell_result_log_push(const char* command, const char* result) {
    unsigned int i = 0;

    if (!command || command[0] == '\0') {
        return;
    }

    if (gshell_result_log_count < GSHELL_RESULT_LOG_SIZE) {
        gshell_copy_text(
            gshell_result_log_commands[gshell_result_log_count],
            command,
            GSHELL_RESULT_LOG_TEXT_SIZE
        );

        gshell_copy_text(
            gshell_result_log_results[gshell_result_log_count],
            result,
            GSHELL_RESULT_LOG_TEXT_SIZE
        );

        gshell_result_log_count++;
        gshell_result_log_total++;
        return;
    }

    for (i = 1; i < GSHELL_RESULT_LOG_SIZE; i++) {
        gshell_copy_text(
            gshell_result_log_commands[i - 1],
            gshell_result_log_commands[i],
            GSHELL_RESULT_LOG_TEXT_SIZE
        );

        gshell_copy_text(
            gshell_result_log_results[i - 1],
            gshell_result_log_results[i],
            GSHELL_RESULT_LOG_TEXT_SIZE
        );
    }

    gshell_copy_text(
        gshell_result_log_commands[GSHELL_RESULT_LOG_SIZE - 1],
        command,
        GSHELL_RESULT_LOG_TEXT_SIZE
    );

    gshell_copy_text(
        gshell_result_log_results[GSHELL_RESULT_LOG_SIZE - 1],
        result,
        GSHELL_RESULT_LOG_TEXT_SIZE
    );

    gshell_result_log_total++;
}

static int gshell_text_equal(const char* a, const char* b) {
    unsigned int i = 0;

    if (!a || !b) {
        return 0;
    }

    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }

        i++;
    }

    return a[i] == '\0' && b[i] == '\0';
}

static void gshell_dispatch_command(void) {
    gshell_command_dispatches++;

    if (gshell_text_equal(gshell_input_buffer, "")) {
        gshell_command_name = "EMPTY";
        gshell_command_result = "EMPTY";
        gshell_command_view = "IDLE";
        gshell_input_status_text = "ENTER ACCEPTED";
        return;
    }

    if (gshell_text_equal(gshell_input_buffer, "dash")) {
        gshell_command_name = "DASH";
        gshell_command_result = "DASH OK";
        gshell_command_view = "DASH";
        gshell_input_status_text = "COMMAND OK";
        gshell_history_push(gshell_input_buffer, gshell_command_view);
        gshell_result_log_push(gshell_input_buffer, gshell_command_result);
        return;
    }

    if (gshell_text_equal(gshell_input_buffer, "health")) {
        gshell_command_name = "HEALTH";
        gshell_command_result = health_result_ok() ? "HEALTH OK" : "HEALTH BAD";
        gshell_command_view = "HEALTH";
        gshell_input_status_text = health_result_ok() ? "COMMAND OK" : "HEALTH BAD";
        gshell_history_push(gshell_input_buffer, gshell_command_view);
        gshell_result_log_push(gshell_input_buffer, gshell_command_result);
        return;
    }

    if (gshell_text_equal(gshell_input_buffer, "doctor")) {
        gshell_command_name = "DOCTOR";
        gshell_command_result = health_result_ok() ? "DOCTOR OK" : "DOCTOR BAD";
        gshell_command_view = "DOCTOR";
        gshell_input_status_text = health_result_ok() ? "COMMAND OK" : "DOCTOR BAD";
        gshell_history_push(gshell_input_buffer, gshell_command_view);
        gshell_result_log_push(gshell_input_buffer, gshell_command_result);
        return;
    }

    if (gshell_text_equal(gshell_input_buffer, "help")) {
        gshell_command_name = "HELP";
        gshell_command_result = "HELP OK";
        gshell_command_view = "HELP";
        gshell_input_status_text = "COMMAND OK";
        gshell_history_push(gshell_input_buffer, gshell_command_view);
        gshell_result_log_push(gshell_input_buffer, gshell_command_result);
        return;
    }

    if (gshell_text_equal(gshell_input_buffer, "clear")) {
        gshell_command_name = "CLEAR";
        gshell_command_result = "CLEAR OK";
        gshell_command_view = "CLEAR";
        gshell_input_status_text = "COMMAND OK";
        gshell_history_push(gshell_input_buffer, gshell_command_view);
        gshell_result_log_push(gshell_input_buffer, gshell_command_result);
        return;
    }

    if (gshell_text_equal(gshell_input_buffer, "history")) {
        gshell_command_name = "HISTORY";
        gshell_command_result = "HISTORY OK";
        gshell_command_view = "HISTORY";
        gshell_input_status_text = "COMMAND OK";
        gshell_history_push(gshell_input_buffer, gshell_command_view);
        gshell_result_log_push(gshell_input_buffer, gshell_command_result);
        return;
    }

    if (gshell_text_equal(gshell_input_buffer, "histclear")) {
        gshell_history_clear();
        gshell_command_name = "HISTCLEAR";
        gshell_command_result = "HIST CLEARED";
        gshell_command_view = "HISTCLEAR";
        gshell_input_status_text = "COMMAND OK";
        gshell_result_log_push(gshell_input_buffer, gshell_command_result);
        return;
    }

    if (gshell_text_equal(gshell_input_buffer, "log")) {
        gshell_command_name = "LOG";
        gshell_command_result = "LOG OK";
        gshell_command_view = "LOG";
        gshell_input_status_text = "COMMAND OK";
        gshell_history_push(gshell_input_buffer, gshell_command_view);
        gshell_result_log_push(gshell_input_buffer, gshell_command_result);
        return;
    }

    if (gshell_text_equal(gshell_input_buffer, "logclear")) {
        gshell_result_log_clear();
        gshell_command_name = "LOGCLEAR";
        gshell_command_result = "LOG CLEARED";
        gshell_command_view = "LOGCLEAR";
        gshell_input_status_text = "COMMAND OK";
        gshell_history_push(gshell_input_buffer, gshell_command_view);
        gshell_result_log_push(gshell_input_buffer, gshell_command_result);
        return;
    }

    gshell_command_unknown++;
    gshell_command_name = "UNKNOWN";
    gshell_command_result = "UNKNOWN";
    gshell_command_view = "ERROR";
    gshell_input_status_text = "UNKNOWN COMMAND";
    gshell_history_push(gshell_input_buffer, gshell_command_view);
    gshell_result_log_push(gshell_input_buffer, gshell_command_result);
}

static int gshell_graphics_can_render_now(void) {
    if (!gshell_initialized) {
        return 0;
    }

    if (gshell_broken) {
        return 0;
    }

    if (!graphics_is_ready()) {
        return 0;
    }

    if (!framebuffer_is_ready()) {
        return 0;
    }

    if (framebuffer_get_address() == 0) {
        return 0;
    }

    if (framebuffer_get_bpp() != 32) {
        return 0;
    }

    if (framebuffer_get_width() < 640 || framebuffer_get_height() < 480) {
        return 0;
    }

    return 1;
}

void gshell_input_status(void) {
    platform_print("GShell input:\n");

    platform_print("  buffer: ");
    platform_print(gshell_input_buffer);
    platform_print("\n");

    platform_print("  length: ");
    platform_print_uint(gshell_input_len);
    platform_print("\n");

    platform_print("  events: ");
    platform_print_uint(gshell_input_events);
    platform_print("\n");

    platform_print("  enters: ");
    platform_print_uint(gshell_input_enters);
    platform_print("\n");

    platform_print("  backs:  ");
    platform_print_uint(gshell_input_backspaces);
    platform_print("\n");

    platform_print("  status: ");
    platform_print(gshell_input_status_text);
    platform_print("\n");

    platform_print("  command: ");
    platform_print(gshell_command_name);
    platform_print("\n");

    platform_print("  view: ");
    platform_print(gshell_command_view);
    platform_print("\n");

    platform_print("  result text: ");
    platform_print(gshell_command_result);
    platform_print("\n");

    platform_print("  dispatches: ");
    platform_print_uint(gshell_command_dispatches);
    platform_print("\n");

    platform_print("  unknown: ");
    platform_print_uint(gshell_command_unknown);
    platform_print("\n");

    platform_print("  history count: ");
    platform_print_uint(gshell_history_count);
    platform_print("\n");

    platform_print("  history total: ");
    platform_print_uint(gshell_history_total);
    platform_print("\n");

    platform_print("  result log count: ");
    platform_print_uint(gshell_result_log_count);
    platform_print("\n");

    platform_print("  result log total: ");
    platform_print_uint(gshell_result_log_total);
    platform_print("\n");

    platform_print("  render: ");

    if (gshell_graphics_can_render_now()) {
        platform_print("graphics-ready\n");
    } else {
        platform_print("text-safe\n");
    }

    platform_print("  result: ready\n");
}

void gshell_input_char(char c) {
    gshell_input_events++;

    if (c == '\n') {
        gshell_input_enters++;

        gshell_dispatch_command();

        gshell_input_len = 0;
        gshell_input_buffer[0] = '\0';

        if (gshell_graphics_can_render_now()) {
            gshell_graphics_dashboard();
        }

        return;
    }

    if (c == '\b') {
        if (gshell_input_len > 0) {
            gshell_input_len--;
            gshell_input_buffer[gshell_input_len] = '\0';
        }

        gshell_input_backspaces++;
        gshell_input_status_text = "BACKSPACE";

        if (gshell_graphics_can_render_now()) {
            gshell_graphics_dashboard();
        }

        return;
    }

    if (c < 32 || c > 126) {
        gshell_input_status_text = "IGNORED";
        return;
    }

    if (gshell_input_len < GSHELL_INPUT_BUFFER_SIZE - 1) {
        gshell_input_buffer[gshell_input_len] = c;
        gshell_input_len++;
        gshell_input_buffer[gshell_input_len] = '\0';
        gshell_input_status_text = "TYPING";
    } else {
        gshell_input_status_text = "BUFFER FULL";
    }

    if (gshell_graphics_can_render_now()) {
        gshell_graphics_dashboard();
    }
}

static void gshell_uint_to_text(unsigned int value, char* out, unsigned int max_len) {
    char temp[16];
    unsigned int temp_len = 0;
    unsigned int out_index = 0;
    unsigned int digit = 0;

    if (max_len == 0) {
        return;
    }

    if (value == 0) {
        if (max_len > 1) {
            out[0] = '0';
            out[1] = '\0';
        } else {
            out[0] = '\0';
        }

        return;
    }

    while (value > 0 && temp_len < 15) {
        digit = value % 10;
        temp[temp_len] = (char)('0' + digit);
        temp_len++;
        value = value / 10;
    }

    while (temp_len > 0 && out_index + 1 < max_len) {
        temp_len--;
        out[out_index] = temp[temp_len];
        out_index++;
    }

    out[out_index] = '\0';
}

static void gshell_draw_value_text(unsigned int x, unsigned int y, const char* label, const char* value) {
    graphics_text(x, y, label);
    graphics_text(x + 132, y, value);
}

static void gshell_draw_value_uint(unsigned int x, unsigned int y, const char* label, unsigned int value) {
    char value_text[16];

    gshell_uint_to_text(value, value_text, 16);

    graphics_text(x, y, label);
    graphics_text(x + 132, y, value_text);
}

static void gshell_draw_value_ok(unsigned int x, unsigned int y, const char* label, int ok) {
    graphics_text(x, y, label);
    graphics_text(x + 132, y, ok ? "OK" : "BAD");
}

static void gshell_draw_command_view_dash(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    gshell_kernel_bridge_refreshes++;

    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x00004477);
    graphics_rect(x, y + h - 4, w, 4, 0x00004477);
    graphics_rect(x, y, 4, h, 0x00004477);
    graphics_rect(x + w - 4, y, 4, h, 0x00004477);

    graphics_text(x + 24, y + 20, "REAL DASH");

    gshell_draw_value_uint(x + 24, y + 56, "UP", timer_get_seconds());
    gshell_draw_value_uint(x + 24, y + 84, "TICKS", timer_get_ticks());
    gshell_draw_value_uint(x + 24, y + 112, "MODULES", (unsigned int)module_count_loaded());
    gshell_draw_value_uint(x + 24, y + 140, "TASKS", (unsigned int)scheduler_task_count());
    gshell_draw_value_text(x + 24, y + 168, "ACTIVE", scheduler_get_active_task());
    gshell_draw_value_text(x + 24, y + 196, "MODE", scheduler_get_mode());
    gshell_draw_value_uint(x + 24, y + 224, "SYSCALLS", syscall_get_table_count());
    gshell_draw_value_uint(x + 24, y + 252, "BRIDGE", gshell_kernel_bridge_refreshes);

    graphics_rect(x + 24, y + h - 24, w - 48, 10, 0x0000FF66);
}

static void gshell_draw_command_view_health(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    gshell_kernel_bridge_refreshes++;

    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000FF66);
    graphics_rect(x, y + h - 4, w, 4, 0x0000FF66);
    graphics_rect(x, y, 4, h, 0x0000FF66);
    graphics_rect(x + w - 4, y, 4, h, 0x0000FF66);

    graphics_text(x + 24, y + 20, "REAL HEALTH");

    gshell_draw_value_ok(x + 24, y + 56, "DEPS", health_deps_ok());
    gshell_draw_value_ok(x + 24, y + 84, "TASK", health_task_ok());
    gshell_draw_value_ok(x + 24, y + 112, "SECURITY", health_security_ok());
    gshell_draw_value_ok(x + 24, y + 140, "MEMORY", health_memory_ok());
    gshell_draw_value_ok(x + 24, y + 168, "PAGING", health_paging_ok());
    gshell_draw_value_ok(x + 24, y + 196, "SYSCALL", health_syscall_ok());
    gshell_draw_value_ok(x + 24, y + 224, "USER", health_user_ok());
    gshell_draw_value_ok(x + 24, y + 252, "RESULT", health_result_ok());

    graphics_rect(x + 24, y + h - 24, w - 48, 10, health_result_ok() ? 0x0000FF66 : 0x00FFAA00);
}

static void gshell_draw_command_view_doctor(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    gshell_kernel_bridge_refreshes++;

    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x00FFAA00);
    graphics_rect(x, y + h - 4, w, 4, 0x00FFAA00);
    graphics_rect(x, y, 4, h, 0x00FFAA00);
    graphics_rect(x + w - 4, y, 4, h, 0x00FFAA00);

    graphics_text(x + 24, y + 20, "REAL DOCTOR");

    gshell_draw_value_ok(x + 24, y + 56, "MODULE", !module_has_broken_dependencies());
    gshell_draw_value_ok(x + 24, y + 84, "TASK", !scheduler_has_broken_tasks());
    gshell_draw_value_ok(x + 24, y + 112, "SWITCH", health_task_switch_ok());
    gshell_draw_value_ok(x + 24, y + 140, "SYSCALL", health_syscall_ok());
    gshell_draw_value_ok(x + 24, y + 168, "RING3", health_ring3_ok());
    gshell_draw_value_ok(x + 24, y + 196, "MEMORY", health_memory_ok());
    gshell_draw_value_ok(x + 24, y + 224, "PAGING", health_paging_ok());
    gshell_draw_value_ok(x + 24, y + 252, "RESULT", health_result_ok());

    graphics_rect(x + 24, y + h - 24, w - 48, 10, health_result_ok() ? 0x0000FF66 : 0x00FFAA00);
}

static void gshell_draw_command_view_help(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "HELP VIEW");
    graphics_text(x + 24, y + 60, "DASH     SHOW DASH VIEW");
    graphics_text(x + 24, y + 92, "HEALTH   SHOW HEALTH");
    graphics_text(x + 24, y + 124, "DOCTOR   SHOW DOCTOR");
    graphics_text(x + 24, y + 156, "HELP     SHOW COMMANDS");
    graphics_text(x + 24, y + 188, "CLEAR    CLEAR VIEW");
}

static void gshell_draw_command_view_clear(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x00004477);
    graphics_rect(x, y + h - 4, w, 4, 0x00004477);
    graphics_rect(x, y, 4, h, 0x00004477);
    graphics_rect(x + w - 4, y, 4, h, 0x00004477);

    graphics_text(x + 24, y + 20, "CLEAR VIEW");
    graphics_text(x + 24, y + 72, "SCREEN CLEAN");
    graphics_text(x + 24, y + 112, "READY FOR NEXT COMMAND");
}

static void gshell_draw_command_view_error(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x00FFAA00);
    graphics_rect(x, y + h - 4, w, 4, 0x00FFAA00);
    graphics_rect(x, y, 4, h, 0x00FFAA00);
    graphics_rect(x + w - 4, y, 4, h, 0x00FFAA00);

    graphics_text(x + 24, y + 20, "ERROR VIEW");
    graphics_text(x + 24, y + 72, "UNKNOWN COMMAND");
    graphics_text(x + 24, y + 112, "TYPE HELP");
}

static void gshell_draw_command_view_history(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "HISTORY VIEW");

    if (gshell_history_count == 0) {
        graphics_text(x + 24, y + 72, "NO HISTORY");
        return;
    }

    if (gshell_history_count > 0) {
        graphics_text(x + 24, y + 64, gshell_history_commands[0]);
        graphics_text(x + 132, y + 64, gshell_history_views[0]);
    }

    if (gshell_history_count > 1) {
        graphics_text(x + 24, y + 96, gshell_history_commands[1]);
        graphics_text(x + 132, y + 96, gshell_history_views[1]);
    }

    if (gshell_history_count > 2) {
        graphics_text(x + 24, y + 128, gshell_history_commands[2]);
        graphics_text(x + 132, y + 128, gshell_history_views[2]);
    }

    if (gshell_history_count > 3) {
        graphics_text(x + 24, y + 160, gshell_history_commands[3]);
        graphics_text(x + 132, y + 160, gshell_history_views[3]);
    }

    gshell_draw_value_uint(x + 24, y + 220, "COUNT", gshell_history_count);
    gshell_draw_value_uint(x + 24, y + 248, "TOTAL", gshell_history_total);
}

static void gshell_draw_command_view_histclear(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x00004477);
    graphics_rect(x, y + h - 4, w, 4, 0x00004477);
    graphics_rect(x, y, 4, h, 0x00004477);
    graphics_rect(x + w - 4, y, 4, h, 0x00004477);

    graphics_text(x + 24, y + 20, "HISTORY CLEARED");
    graphics_text(x + 24, y + 72, "COUNT 0");
    graphics_text(x + 24, y + 112, "READY FOR NEW COMMANDS");
}

static void gshell_draw_result_log_line(unsigned int x, unsigned int y, unsigned int index) {
    if (index >= gshell_result_log_count) {
        return;
    }

    graphics_text(x, y, gshell_result_log_commands[index]);
    graphics_text(x + 108, y, gshell_result_log_results[index]);
}

static void gshell_draw_command_view_log(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "RESULT LOG");

    if (gshell_result_log_count == 0) {
        graphics_text(x + 24, y + 72, "NO LOG");
        return;
    }

    if (gshell_result_log_count > 0) {
        gshell_draw_result_log_line(x + 24, y + 64, 0);
    }

    if (gshell_result_log_count > 1) {
        gshell_draw_result_log_line(x + 24, y + 96, 1);
    }

    if (gshell_result_log_count > 2) {
        gshell_draw_result_log_line(x + 24, y + 128, 2);
    }

    if (gshell_result_log_count > 3) {
        gshell_draw_result_log_line(x + 24, y + 160, 3);
    }

    gshell_draw_value_uint(x + 24, y + 220, "COUNT", gshell_result_log_count);
    gshell_draw_value_uint(x + 24, y + 248, "TOTAL", gshell_result_log_total);
}

static void gshell_draw_command_view_logclear(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x00004477);
    graphics_rect(x, y + h - 4, w, 4, 0x00004477);
    graphics_rect(x, y, 4, h, 0x00004477);
    graphics_rect(x + w - 4, y, 4, h, 0x00004477);

    graphics_text(x + 24, y + 20, "LOG CLEARED");
    graphics_text(x + 24, y + 72, "COUNT 1");
    graphics_text(x + 24, y + 112, "LOGCLEAR RECORDED");
}

static void gshell_draw_command_view_idle(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x00004477);
    graphics_rect(x, y + h - 4, w, 4, 0x00004477);
    graphics_rect(x, y, 4, h, 0x00004477);
    graphics_rect(x + w - 4, y, 4, h, 0x00004477);

    graphics_text(x + 24, y + 20, "COMMAND VIEW");
    graphics_text(x + 24, y + 72, "TYPE HELP");
    graphics_text(x + 24, y + 112, "LJ SHELL READY");
}

static void gshell_draw_history_line(unsigned int x, unsigned int y, unsigned int index) {
    if (index >= gshell_history_count) {
        return;
    }

    graphics_text(x, y, gshell_history_commands[index]);
    graphics_text(x + 96, y, gshell_history_views[index]);
}

static void gshell_draw_history_panel(unsigned int x, unsigned int y) {
    graphics_text(x, y, "HISTORY");

    if (gshell_history_count == 0) {
        graphics_text(x, y + 32, "NONE");
        return;
    }

    if (gshell_history_count > 0) {
        gshell_draw_history_line(x, y + 32, 0);
    }

    if (gshell_history_count > 1) {
        gshell_draw_history_line(x, y + 56, 1);
    }
}

static void gshell_draw_command_view(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    if (gshell_text_equal(gshell_command_view, "DASH")) {
        gshell_draw_command_view_dash(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "HEALTH")) {
        gshell_draw_command_view_health(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "DOCTOR")) {
        gshell_draw_command_view_doctor(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "HELP")) {
        gshell_draw_command_view_help(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "CLEAR")) {
        gshell_draw_command_view_clear(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "HISTORY")) {
        gshell_draw_command_view_history(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "HISTCLEAR")) {
        gshell_draw_command_view_histclear(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "LOG")) {
        gshell_draw_command_view_log(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "LOGCLEAR")) {
        gshell_draw_command_view_logclear(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "ERROR")) {
        gshell_draw_command_view_error(x, y, w, h);
        return;
    }

    gshell_draw_command_view_idle(x, y, w, h);
}

void gshell_graphics_dashboard(void) {
    unsigned int width = framebuffer_get_width();
    unsigned int height = framebuffer_get_height();
    unsigned int center_x = 0;
    unsigned int center_y = 0;
    unsigned int command_x = 318;
    unsigned int command_y = 116;
    unsigned int command_w = 250;
    unsigned int command_h = 300;

    gshell_render_begin("graphics-dashboard");

    if (!gshell_render_guard()) {
        return;
    }

    if (!framebuffer_is_ready()) {
        gshell_blocked_count++;
        platform_print("  result: blocked\n");
        platform_print("  reason: framebuffer not ready\n");
        return;
    }

    if (framebuffer_get_address() == 0) {
        gshell_blocked_count++;
        platform_print("  result: blocked\n");
        platform_print("  reason: framebuffer address missing\n");
        return;
    }

    if (framebuffer_get_bpp() != 32) {
        gshell_blocked_count++;
        platform_print("  result: blocked\n");
        platform_print("  reason: only 32bpp graphics framebuffer is enabled now\n");
        return;
    }

    if (width < 640 || height < 480) {
        gshell_blocked_count++;
        platform_print("  result: blocked\n");
        platform_print("  reason: framebuffer too small\n");
        return;
    }

    center_x = width / 2;
    center_y = height / 2;

    gshell_mode = "real-framebuffer-dashboard";
    gshell_output_mode = "graphics-self";
    gshell_output_plan = "graphics-self";
    gshell_output_reason = "graphical shell rendered into framebuffer";
    gshell_output_changes++;

    graphics_real_arm();
    graphics_text_arm();

    graphics_clear();

    graphics_rect(18, 18, width - 36, 4, 0x0000AAFF);
    graphics_rect(18, height - 22, width - 36, 4, 0x0000AAFF);
    graphics_rect(18, 18, 4, height - 36, 0x0000AAFF);
    graphics_rect(width - 22, 18, 4, height - 36, 0x0000AAFF);

    graphics_rect(36, 36, width - 72, 54, 0x00112233);
    graphics_rect(36, 36, width - 72, 4, 0x0000AAFF);
    graphics_rect(36, 86, width - 72, 4, 0x00004477);

    graphics_text(54, 54, "LINGJING OS");
    graphics_text(250, 54, "DEV 0.4.6");
    graphics_text(390, 54, "LAYOUT CLEANUP");

    graphics_rect(36, 116, 254, 300, 0x00112233);
    graphics_rect(36, 116, 254, 4, 0x0000AAFF);
    graphics_rect(36, 412, 254, 4, 0x0000AAFF);
    graphics_rect(36, 116, 4, 300, 0x0000AAFF);
    graphics_rect(286, 116, 4, 300, 0x0000AAFF);

    graphics_text(58, 136, "SYSTEM");
    graphics_rect(58, 166, 190, 10, 0x0000FF66);
    graphics_text(58, 188, "PLATFORM BAREMETAL");
    graphics_text(58, 216, "BOOT MB2 GFX");
    graphics_text(58, 244, "FB 800X600X32");
    graphics_text(58, 272, "GRAPHICS READY");
    graphics_text(58, 300, "TEXT READY");
    graphics_text(58, 328, "LOG READY");
    graphics_text(58, 356, "GSHELL ONLINE");

    gshell_draw_command_view(command_x, command_y, command_w, command_h);

    graphics_rect(width - 230, 116, 194, 300, 0x00112233);
    graphics_rect(width - 230, 116, 194, 4, 0x00FFAA00);
    graphics_rect(width - 230, 412, 194, 4, 0x00FFAA00);
    graphics_rect(width - 230, 116, 4, 300, 0x00FFAA00);
    graphics_rect(width - 40, 116, 4, 300, 0x00FFAA00);

    graphics_text(width - 208, 136, "COMMANDS");
    graphics_text(width - 208, 164, "DASH");
    graphics_text(width - 208, 188, "HEALTH");
    graphics_text(width - 208, 212, "DOCTOR");
    graphics_text(width - 208, 236, "HELP");
    graphics_text(width - 208, 260, "CLEAR");
    graphics_text(width - 208, 284, "HISTORY");
    graphics_text(width - 208, 308, "HISTCLEAR");
    graphics_text(width - 208, 332, "LOG");
    graphics_text(width - 208, 356, "LOGCLEAR");

    gshell_draw_history_panel(width - 208, 384);

    graphics_rect(36, height - 150, width - 72, 102, 0x00000000);
    graphics_rect(36, height - 150, width - 72, 4, 0x00004477);
    graphics_rect(36, height - 52, width - 72, 4, 0x00004477);
    graphics_rect(36, height - 150, 4, 102, 0x00004477);
    graphics_rect(width - 40, height - 150, 4, 102, 0x00004477);

    graphics_text(58, height - 130, "GRAPHICAL SHELL COMMAND ZONE");

    graphics_text(58, height - 104, "LJ>");
    graphics_text(106, height - 104, gshell_input_buffer);

    graphics_text(58, height - 80, "STATUS");
    graphics_text(154, height - 80, gshell_input_status_text);

    graphics_rect(106 + (gshell_input_len * 12), height - 106, 10, 16, 0x00FFFFFF);

    graphics_pixel(center_x, center_y, 0x00FFFFFF);
    graphics_pixel(center_x + 1, center_y, 0x00FFFFFF);
    graphics_pixel(center_x - 1, center_y, 0x00FFFFFF);
    graphics_pixel(center_x, center_y + 1, 0x00FFFFFF);
    graphics_pixel(center_x, center_y - 1, 0x00FFFFFF);

    platform_print("  output: graphics-self\n");
    platform_print("  command zone: layout-clean\n");
    platform_print("  result: real-written\n");
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