#include "ring3.h"
#include "platform.h"
#include "gdt.h"
#include "idt.h"
#include "syscall.h"

#define RING3_KERNEL_CODE_SELECTOR 0x08
#define RING3_KERNEL_DATA_SELECTOR 0x10
#define RING3_USER_CODE_SELECTOR   0x1B
#define RING3_USER_DATA_SELECTOR   0x23
#define RING3_TSS_SELECTOR         0x28

#define RING3_KERNEL_STACK_TOP     0x00130000
#define RING3_KERNEL_STACK_SIZE    0x00001000

#define RING3_USER_ENTRY           ((unsigned int)ring3_user_stub_asm)
#define RING3_USER_STACK_TOP       0x003FF000
#define RING3_EFLAGS              0x00000002

extern void ring3_enter_user_mode(unsigned int eip,
                                  unsigned int user_cs,
                                  unsigned int eflags,
                                  unsigned int user_esp,
                                  unsigned int user_ds);

extern void ring3_user_stub_asm(void);
extern void ring3_user_syscall_stub_asm(void);

typedef struct ring3_tss_metadata {
    unsigned int selector;
    unsigned int ss0;
    unsigned int esp0;
    unsigned int prepared;
    unsigned int loaded;
    unsigned int load_count;
    unsigned int clear_count;
} ring3_tss_metadata_t;

typedef struct ring3_stack_metadata {
    unsigned int kernel_stack_top;
    unsigned int kernel_stack_bottom;
    unsigned int kernel_stack_size;
    unsigned int prepared;
} ring3_stack_metadata_t;

typedef struct ring3_entry_frame {
    unsigned int user_entry;
    unsigned int user_stack;
    unsigned int user_code;
    unsigned int user_data;
    unsigned int eflags;
    unsigned int prepared;
} ring3_entry_frame_t;

typedef struct ring3_gdt_metadata {
    unsigned int kernel_code;
    unsigned int kernel_data;
    unsigned int user_code;
    unsigned int user_data;
    unsigned int user_code_dpl;
    unsigned int user_data_dpl;
    unsigned int prepared;
    unsigned int installed;
    unsigned int install_count;
    unsigned int clear_count;
} ring3_gdt_metadata_t;

typedef struct ring3_page_metadata {
    unsigned int stub_addr;
    unsigned int stub_page;
    unsigned int stack_top;
    unsigned int stack_page;
    unsigned int present;
    unsigned int writable;
    unsigned int user;
    unsigned int prepared;
    unsigned int prepare_count;
    unsigned int clear_count;
} ring3_page_metadata_t;

typedef struct ring3_syscall_metadata {
    unsigned int vector;
    unsigned int syscall_id;
    unsigned int arg1;
    unsigned int arg2;
    unsigned int arg3;
    unsigned int prepared;
    unsigned int prepare_count;
    unsigned int clear_count;
} ring3_syscall_metadata_t;

typedef struct ring3_syscall_frame {
    unsigned int vector;
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;
    unsigned int valid;
    unsigned int dryrun_count;
} ring3_syscall_frame_t;

typedef struct ring3_syscall_stub_metadata {
    unsigned int entry;
    unsigned int vector;
    unsigned int syscall_id;
    unsigned int arg1;
    unsigned int arg2;
    unsigned int arg3;
    unsigned int prepared;
    unsigned int prepare_count;
    unsigned int clear_count;
} ring3_syscall_stub_metadata_t;

typedef struct ring3_syscall_exec_metadata {
    unsigned int armed;
    unsigned int attempts;
    unsigned int blocked;
    unsigned int staged;
    unsigned int executed;
    unsigned int disarmed_count;

    unsigned int real_armed;
    unsigned int real_attempts;
    unsigned int real_blocked;
    unsigned int real_disarmed_count;

    unsigned int gate_installed;
    unsigned int gate_install_count;
    unsigned int gate_clear_count;
} ring3_syscall_exec_metadata_t;

typedef struct ring3_iret_frame {
    unsigned int ss;
    unsigned int esp;
    unsigned int eflags;
    unsigned int cs;
    unsigned int eip;
    unsigned int valid;
} ring3_iret_frame_t;

static int ring3_init_done = 0;
static int ring3_ready = 0;
static int ring3_broken = 0;

static unsigned int ring3_check_count = 0;
static unsigned int ring3_fix_count = 0;
static unsigned int ring3_enter_attempts = 0;
static unsigned int ring3_enter_blocked = 0;
static unsigned int ring3_enter_staged = 0;
static unsigned int ring3_transition_enabled = 0;

static unsigned int ring3_dryrun_count = 0;
static unsigned int ring3_stub_checks = 0;
static unsigned int ring3_realenter_attempts = 0;
static unsigned int ring3_realenter_blocked = 0;
static unsigned int ring3_realenter_armed = 0;
static unsigned int ring3_realenter_executed = 0;
static unsigned int ring3_real_hardware_installed = 0;
static unsigned int ring3_hw_install_count = 0;
static unsigned int ring3_hw_clear_count = 0;

static unsigned int ring3_gdt_user_ready = 0;
static unsigned int ring3_tss_loaded = 0;
static unsigned int ring3_user_page_ready = 0;

static ring3_iret_frame_t ring3_last_iret_frame;

static ring3_tss_metadata_t ring3_tss_meta;
static ring3_stack_metadata_t ring3_stack_meta;
static ring3_entry_frame_t ring3_frame_meta;
static ring3_gdt_metadata_t ring3_gdt_meta;
static ring3_page_metadata_t ring3_page_meta;
static ring3_syscall_metadata_t ring3_syscall_meta;
static ring3_syscall_frame_t ring3_syscall_frame;
static ring3_syscall_stub_metadata_t ring3_syscall_stub_meta;
static ring3_syscall_exec_metadata_t ring3_syscall_exec_meta;
static unsigned int ring3_selected_stub_entry = 0;
static unsigned int ring3_syscall_stub_selected = 0;

static const char* ring3_mode = "metadata prototype";

static int ring3_guard_ok(void);

static int ring3_is_page_aligned(unsigned int value) {
    return (value & 0xFFF) == 0;
}

static int ring3_selector_is_ring3(unsigned int selector) {
    return (selector & 0x3) == 0x3;
}

static unsigned int ring3_get_stub_address(void) {
    if (ring3_selected_stub_entry != 0) {
        return ring3_selected_stub_entry;
    }

    return (unsigned int)ring3_user_stub_asm;
}

static unsigned int ring3_get_safe_stub_address(void) {
    return (unsigned int)ring3_user_stub_asm;
}

static unsigned int ring3_get_syscall_stub_address(void) {
    return (unsigned int)ring3_user_syscall_stub_asm;
}

static int ring3_stub_ok(void) {
    if (ring3_get_stub_address() == 0) {
        return 0;
    }

    if (ring3_frame_meta.user_entry != ring3_get_stub_address()) {
        return 0;
    }

    return 1;
}

static void ring3_prepare_tss(void) {
    ring3_tss_meta.selector = RING3_TSS_SELECTOR;
    ring3_tss_meta.ss0 = RING3_KERNEL_DATA_SELECTOR;
    ring3_tss_meta.esp0 = RING3_KERNEL_STACK_TOP;
    ring3_tss_meta.prepared = 1;
}

static void ring3_prepare_stack(void) {
    ring3_stack_meta.kernel_stack_top = RING3_KERNEL_STACK_TOP;
    ring3_stack_meta.kernel_stack_size = RING3_KERNEL_STACK_SIZE;
    ring3_stack_meta.kernel_stack_bottom = RING3_KERNEL_STACK_TOP - RING3_KERNEL_STACK_SIZE;
    ring3_stack_meta.prepared = 1;
}

static void ring3_prepare_frame(void) {
    ring3_frame_meta.user_entry = ring3_get_stub_address();
    ring3_frame_meta.user_stack = RING3_USER_STACK_TOP;
    ring3_frame_meta.user_code = RING3_USER_CODE_SELECTOR;
    ring3_frame_meta.user_data = RING3_USER_DATA_SELECTOR;
    ring3_frame_meta.eflags = RING3_EFLAGS;
    ring3_frame_meta.prepared = 1;
}

static void ring3_prepare_gdt_metadata(void) {
    ring3_gdt_meta.kernel_code = RING3_KERNEL_CODE_SELECTOR;
    ring3_gdt_meta.kernel_data = RING3_KERNEL_DATA_SELECTOR;
    ring3_gdt_meta.user_code = RING3_USER_CODE_SELECTOR;
    ring3_gdt_meta.user_data = RING3_USER_DATA_SELECTOR;
    ring3_gdt_meta.user_code_dpl = RING3_USER_CODE_SELECTOR & 0x3;
    ring3_gdt_meta.user_data_dpl = RING3_USER_DATA_SELECTOR & 0x3;
    ring3_gdt_meta.prepared = 1;
}

static int ring3_gdt_ok(void) {
    if (!ring3_gdt_user_ready) {
        return 0;
    }

    if (!ring3_gdt_meta.prepared) {
        return 0;
    }

    if (!ring3_gdt_meta.installed) {
        return 0;
    }

    if (!gdt_user_segments_installed()) {
        return 0;
    }

    if (ring3_gdt_meta.kernel_code != RING3_KERNEL_CODE_SELECTOR) {
        return 0;
    }

    if (ring3_gdt_meta.kernel_data != RING3_KERNEL_DATA_SELECTOR) {
        return 0;
    }

    if (ring3_gdt_meta.user_code != RING3_USER_CODE_SELECTOR) {
        return 0;
    }

    if (ring3_gdt_meta.user_data != RING3_USER_DATA_SELECTOR) {
        return 0;
    }

    if (ring3_gdt_meta.user_code_dpl != 3) {
        return 0;
    }

    if (ring3_gdt_meta.user_data_dpl != 3) {
        return 0;
    }

    return 1;
}

static unsigned int ring3_page_base(unsigned int address) {
    return address & 0xFFFFF000;
}

static void ring3_prepare_page_metadata(void) {
    ring3_page_meta.stub_addr = ring3_get_stub_address();
    ring3_page_meta.stub_page = ring3_page_base(ring3_page_meta.stub_addr);
    ring3_page_meta.stack_top = ring3_frame_meta.user_stack;
    ring3_page_meta.stack_page = ring3_page_base(ring3_frame_meta.user_stack);
    ring3_page_meta.present = 1;
    ring3_page_meta.writable = 1;
    ring3_page_meta.user = 1;
    ring3_page_meta.prepared = 1;
    ring3_page_meta.prepare_count++;

    ring3_user_page_ready = 1;
}

static int ring3_page_ok(void) {
    if (!ring3_user_page_ready) {
        return 0;
    }

    if (!ring3_page_meta.prepared) {
        return 0;
    }

    if (ring3_page_meta.stub_addr == 0) {
        return 0;
    }

    if (ring3_page_meta.stub_addr != ring3_get_stub_address()) {
        return 0;
    }

    if (ring3_page_meta.stub_page != ring3_page_base(ring3_get_stub_address())) {
        return 0;
    }

    if (ring3_page_meta.stack_top != ring3_frame_meta.user_stack) {
        return 0;
    }

    if (ring3_page_meta.stack_page != ring3_page_base(ring3_frame_meta.user_stack)) {
        return 0;
    }

    if (!ring3_page_meta.present) {
        return 0;
    }

    if (!ring3_page_meta.writable) {
        return 0;
    }

    if (!ring3_page_meta.user) {
        return 0;
    }

    return 1;
}

static int ring3_syscall_ok(void) {
    if (!ring3_syscall_meta.prepared) {
        return 0;
    }

    if (ring3_syscall_meta.vector != 0x80) {
        return 0;
    }

    if (ring3_syscall_meta.syscall_id != 0) {
        return 0;
    }

    return 1;
}

static void ring3_clear_syscall_frame(void) {
    ring3_syscall_frame.vector = 0;
    ring3_syscall_frame.eax = 0;
    ring3_syscall_frame.ebx = 0;
    ring3_syscall_frame.ecx = 0;
    ring3_syscall_frame.edx = 0;
    ring3_syscall_frame.valid = 0;
    ring3_syscall_frame.dryrun_count = 0;
}

static void ring3_build_syscall_frame(void) {
    ring3_syscall_frame.vector = ring3_syscall_meta.vector;
    ring3_syscall_frame.eax = ring3_syscall_meta.syscall_id;
    ring3_syscall_frame.ebx = ring3_syscall_meta.arg1;
    ring3_syscall_frame.ecx = ring3_syscall_meta.arg2;
    ring3_syscall_frame.edx = ring3_syscall_meta.arg3;
    ring3_syscall_frame.valid = 1;
}

static int ring3_syscall_frame_ok(void) {
    if (!ring3_syscall_frame.valid) {
        return 0;
    }

    if (ring3_syscall_frame.vector != 0x80) {
        return 0;
    }

    if (ring3_syscall_frame.eax != ring3_syscall_meta.syscall_id) {
        return 0;
    }

    if (ring3_syscall_frame.ebx != ring3_syscall_meta.arg1) {
        return 0;
    }

    if (ring3_syscall_frame.ecx != ring3_syscall_meta.arg2) {
        return 0;
    }

    if (ring3_syscall_frame.edx != ring3_syscall_meta.arg3) {
        return 0;
    }

    return 1;
}

static int ring3_syscall_stub_ok(void) {
    if (!ring3_syscall_stub_meta.prepared) {
        return 0;
    }

    if (ring3_syscall_stub_meta.entry != ring3_get_syscall_stub_address()) {
        return 0;
    }

    if (ring3_syscall_stub_meta.vector != 0x80) {
        return 0;
    }

    if (ring3_syscall_stub_meta.syscall_id != ring3_syscall_meta.syscall_id) {
        return 0;
    }

    if (ring3_syscall_stub_meta.arg1 != ring3_syscall_meta.arg1) {
        return 0;
    }

    if (ring3_syscall_stub_meta.arg2 != ring3_syscall_meta.arg2) {
        return 0;
    }

    if (ring3_syscall_stub_meta.arg3 != ring3_syscall_meta.arg3) {
        return 0;
    }

    return 1;
}

static int ring3_syscall_exec_gate_ok(void) {
    if (!ring3_syscall_ok()) {
        return 0;
    }

    if (!ring3_syscall_frame_ok()) {
        return 0;
    }

    if (!ring3_syscall_stub_ok()) {
        return 0;
    }

    return 1;
}

static int ring3_syscall_gate_ok(void) {
    if (!ring3_syscall_exec_meta.gate_installed) {
        return 0;
    }

    return 1;
}

static int ring3_syscall_real_gate_ok(void) {
    if (!ring3_syscall_exec_gate_ok()) {
        return 0;
    }

    if (!ring3_syscall_stub_selected) {
        return 0;
    }

    if (!ring3_syscall_exec_meta.armed) {
        return 0;
    }

    if (!ring3_syscall_exec_meta.real_armed) {
        return 0;
    }

    if (!ring3_syscall_gate_ok()) {
        return 0;
    }

    return 1;
}

static int ring3_tss_ok(void) {
    if (!ring3_tss_meta.prepared) {
        return 0;
    }

    if (ring3_tss_meta.selector != RING3_TSS_SELECTOR) {
        return 0;
    }

    if (ring3_tss_meta.ss0 != RING3_KERNEL_DATA_SELECTOR) {
        return 0;
    }

    if (ring3_tss_meta.esp0 != RING3_KERNEL_STACK_TOP) {
        return 0;
    }

    return 1;
}

static int ring3_tss_loaded_ok(void) {
    if (!ring3_tss_ok()) {
        return 0;
    }

    if (!ring3_tss_loaded) {
        return 0;
    }

    if (!ring3_tss_meta.loaded) {
        return 0;
    }

    if (!gdt_tss_loaded()) {
        return 0;
    }

    return 1;
}

static int ring3_stack_ok(void) {
    if (!ring3_stack_meta.prepared) {
        return 0;
    }

    if (ring3_stack_meta.kernel_stack_size == 0) {
        return 0;
    }

    if (ring3_stack_meta.kernel_stack_bottom >= ring3_stack_meta.kernel_stack_top) {
        return 0;
    }

    if ((ring3_stack_meta.kernel_stack_top - ring3_stack_meta.kernel_stack_bottom) != ring3_stack_meta.kernel_stack_size) {
        return 0;
    }

    if (!ring3_is_page_aligned(ring3_stack_meta.kernel_stack_top)) {
        return 0;
    }

    if (!ring3_is_page_aligned(ring3_stack_meta.kernel_stack_bottom)) {
        return 0;
    }

    return 1;
}

static int ring3_frame_ok(void) {
    if (!ring3_frame_meta.prepared) {
        return 0;
    }

    if (!ring3_selector_is_ring3(ring3_frame_meta.user_code)) {
        return 0;
    }

    if (!ring3_selector_is_ring3(ring3_frame_meta.user_data)) {
        return 0;
    }

    if (!ring3_stub_ok()) {
        return 0;
    }

    if (!ring3_is_page_aligned(ring3_frame_meta.user_stack)) {
        return 0;
    }

    if ((ring3_frame_meta.eflags & 0x00000002) == 0) {
        return 0;
    }

    return 1;
}

static void ring3_clear_iret_frame(void) {
    ring3_last_iret_frame.ss = 0;
    ring3_last_iret_frame.esp = 0;
    ring3_last_iret_frame.eflags = 0;
    ring3_last_iret_frame.cs = 0;
    ring3_last_iret_frame.eip = 0;
    ring3_last_iret_frame.valid = 0;
}

static void ring3_build_iret_frame(void) {
    ring3_last_iret_frame.ss = ring3_frame_meta.user_data;
    ring3_last_iret_frame.esp = ring3_frame_meta.user_stack;
    ring3_last_iret_frame.eflags = ring3_frame_meta.eflags;
    ring3_last_iret_frame.cs = ring3_frame_meta.user_code;
    ring3_last_iret_frame.eip = ring3_frame_meta.user_entry;
    ring3_last_iret_frame.valid = 1;
}

static int ring3_iret_frame_ok(void) {
    if (!ring3_last_iret_frame.valid) {
        return 0;
    }

    if ((ring3_last_iret_frame.ss & 0x3) != 0x3) {
        return 0;
    }

    if ((ring3_last_iret_frame.cs & 0x3) != 0x3) {
        return 0;
    }

    if ((ring3_last_iret_frame.eflags & 0x00000002) == 0) {
        return 0;
    }

    if (!ring3_is_page_aligned(ring3_last_iret_frame.esp)) {
        return 0;
    }

    return 1;
}

static int ring3_metadata_gate_ready(void) {
    if (!ring3_transition_enabled) {
        return 0;
    }

    if (!ring3_guard_ok()) {
        return 0;
    }

    if (!ring3_stub_ok()) {
        return 0;
    }

    if (!ring3_iret_frame_ok()) {
        return 0;
    }

    if (!ring3_gdt_ok()) {
        return 0;
    }

    if (!ring3_tss_loaded_ok()) {
        return 0;
    }

    if (!ring3_page_ok()) {
        return 0;
    }

    return 1;
}

static int ring3_real_hardware_ready(void) {
    if (!ring3_metadata_gate_ready()) {
        return 0;
    }

    if (!ring3_real_hardware_installed) {
        return 0;
    }

    return 1;
}

static int ring3_guard_ok(void) {
    if (!ring3_init_done) {
        return 0;
    }

    if (!ring3_ready) {
        return 0;
    }

    if (ring3_broken) {
        return 0;
    }

    if (!ring3_tss_ok()) {
        return 0;
    }

    if (!ring3_stack_ok()) {
        return 0;
    }

    if (!ring3_frame_ok()) {
        return 0;
    }

    return 1;
}

void ring3_init(void) {
    ring3_init_done = 1;
    ring3_ready = 1;
    ring3_broken = 0;

    ring3_check_count = 0;
    ring3_fix_count = 0;

    ring3_enter_attempts = 0;
    ring3_enter_blocked = 0;
    ring3_enter_staged = 0;
    ring3_transition_enabled = 0;
    ring3_dryrun_count = 0;
    ring3_stub_checks = 0;
    ring3_realenter_attempts = 0;
    ring3_realenter_blocked = 0;
    ring3_realenter_armed = 0;
    ring3_realenter_executed = 0;

    ring3_gdt_user_ready = 0;
    ring3_tss_loaded = 0;
    ring3_user_page_ready = 0;

    ring3_gdt_meta.kernel_code = 0;
    ring3_gdt_meta.kernel_data = 0;
    ring3_gdt_meta.user_code = 0;
    ring3_gdt_meta.user_data = 0;
    ring3_gdt_meta.user_code_dpl = 0;
    ring3_gdt_meta.user_data_dpl = 0;
    ring3_gdt_meta.prepared = 0;
    ring3_gdt_meta.installed = 0;
    ring3_gdt_meta.install_count = 0;
    ring3_gdt_meta.clear_count = 0;

    ring3_tss_meta.loaded = 0;
    ring3_tss_meta.load_count = 0;
    ring3_tss_meta.clear_count = 0;

    ring3_page_meta.stub_addr = 0;
    ring3_page_meta.stub_page = 0;
    ring3_page_meta.stack_top = 0;
    ring3_page_meta.stack_page = 0;
    ring3_page_meta.present = 0;
    ring3_page_meta.writable = 0;
    ring3_page_meta.user = 0;
    ring3_page_meta.prepared = 0;
    ring3_page_meta.prepare_count = 0;
    ring3_page_meta.clear_count = 0;

    ring3_syscall_meta.vector = 0;
    ring3_syscall_meta.syscall_id = 0;
    ring3_syscall_meta.arg1 = 0;
    ring3_syscall_meta.arg2 = 0;
    ring3_syscall_meta.arg3 = 0;
    ring3_syscall_meta.prepared = 0;
    ring3_syscall_meta.prepare_count = 0;
    ring3_syscall_meta.clear_count = 0;

    ring3_clear_syscall_frame();

    ring3_syscall_stub_meta.entry = 0;
    ring3_syscall_stub_meta.vector = 0;
    ring3_syscall_stub_meta.syscall_id = 0;
    ring3_syscall_stub_meta.arg1 = 0;
    ring3_syscall_stub_meta.arg2 = 0;
    ring3_syscall_stub_meta.arg3 = 0;
    ring3_syscall_stub_meta.prepared = 0;
    ring3_syscall_stub_meta.prepare_count = 0;
    ring3_syscall_stub_meta.clear_count = 0;

    ring3_syscall_exec_meta.armed = 0;
    ring3_syscall_exec_meta.attempts = 0;
    ring3_syscall_exec_meta.blocked = 0;
    ring3_syscall_exec_meta.staged = 0;
    ring3_syscall_exec_meta.executed = 0;
    ring3_syscall_exec_meta.disarmed_count = 0;

    ring3_syscall_exec_meta.real_armed = 0;
    ring3_syscall_exec_meta.real_attempts = 0;
    ring3_syscall_exec_meta.real_blocked = 0;
    ring3_syscall_exec_meta.real_disarmed_count = 0;

    ring3_syscall_exec_meta.gate_installed = 0;
    ring3_syscall_exec_meta.gate_install_count = 0;
    ring3_syscall_exec_meta.gate_clear_count = 0;

    ring3_selected_stub_entry = 0;
    ring3_syscall_stub_selected = 0;

    ring3_clear_iret_frame();

    ring3_prepare_tss();
    ring3_prepare_stack();
    ring3_prepare_frame();
}

int ring3_is_ready(void) {
    if (!ring3_init_done) {
        return 0;
    }

    if (!ring3_ready) {
        return 0;
    }

    if (ring3_broken) {
        return 0;
    }

    if (!ring3_tss_ok()) {
        return 0;
    }

    if (!ring3_stack_ok()) {
        return 0;
    }

    if (!ring3_frame_ok()) {
        return 0;
    }

    return 1;
}

int ring3_doctor_ok(void) {
    return ring3_is_ready();
}

const char* ring3_get_state(void) {
    return ring3_is_ready() ? "ready" : "broken";
}

const char* ring3_get_mode(void) {
    return ring3_mode;
}

void ring3_status(void) {
    platform_print("Ring3 layer:\n");

    platform_print("  state:       ");
    platform_print(ring3_get_state());
    platform_print("\n");

    platform_print("  mode:        ");
    platform_print(ring3_get_mode());
    platform_print("\n");

    platform_print("  tss:         ");
    platform_print(ring3_tss_ok() ? "prepared\n" : "broken\n");

    platform_print("  kernel stack:");
    platform_print(ring3_stack_ok() ? "prepared\n" : "broken\n");

    platform_print("  entry frame: ");
    platform_print(ring3_frame_ok() ? "prepared\n" : "broken\n");

    platform_print("  guard:       ");
    platform_print(ring3_guard_ok() ? "ready\n" : "blocked\n");

    platform_print("  real switch: ");
    platform_print(ring3_transition_enabled ? "enabled\n" : "disabled\n");

    platform_print("  checks:      ");
    platform_print_uint(ring3_check_count);
    platform_print("\n");

    platform_print("  fixes:       ");
    platform_print_uint(ring3_fix_count);
    platform_print("\n");

    platform_print("  attempts:    ");
    platform_print_uint(ring3_enter_attempts);
    platform_print("\n");

    platform_print("  staged:      ");
    platform_print_uint(ring3_enter_staged);
    platform_print("\n");

    platform_print("  dryrun:      ");
    platform_print_uint(ring3_dryrun_count);
    platform_print("\n");

    platform_print("  iret frame:  ");
    platform_print(ring3_iret_frame_ok() ? "ready\n" : "empty\n");

    platform_print("  user stub:   ");
    platform_print(ring3_stub_ok() ? "ready\n" : "broken\n");

    platform_print("  gdt user:    ");
    platform_print(ring3_gdt_ok() ? "installed\n" : "missing\n");

    platform_print("  tss loaded:  ");
    platform_print(ring3_tss_loaded_ok() ? "yes\n" : "no\n");

    platform_print("  user page:   ");
    platform_print(ring3_page_ok() ? "ready\n" : "missing\n");

    platform_print("  realenter:   ");
    platform_print_uint(ring3_realenter_attempts);
    platform_print("\n");

    platform_print("  real blocked:");
    platform_print_uint(ring3_realenter_blocked);
    platform_print("\n");

    platform_print("  real hw:     ");
    platform_print(ring3_real_hardware_installed ? "installed\n" : "missing\n");

    platform_print("  hw installs: ");
    platform_print_uint(ring3_hw_install_count);
    platform_print("\n");

    platform_print("  hw clears:   ");
    platform_print_uint(ring3_hw_clear_count);
    platform_print("\n");

    platform_print("  executed:    ");
    platform_print_uint(ring3_realenter_executed);
    platform_print("\n");

    platform_print("  syscall:    ");
    platform_print(ring3_syscall_ok() ? "prepared\n" : "missing\n");

    platform_print("  syscallstub:");
    platform_print(ring3_syscall_stub_ok() ? "prepared\n" : "missing\n");

    platform_print("  stub mode:  ");
    platform_print(ring3_syscall_stub_selected ? "syscall\n" : "safe\n");

    platform_print("  sys armed:  ");
    platform_print(ring3_syscall_exec_meta.armed ? "yes\n" : "no\n");

    platform_print("  sys real:   ");
    platform_print(ring3_syscall_exec_meta.real_armed ? "yes\n" : "no\n");

    platform_print("  sys gate:   ");
    platform_print(ring3_syscall_gate_ok() ? "installed\n" : "missing\n");
}

void ring3_tss(void) {
    platform_print("Ring3 TSS metadata:\n");

    platform_print("  selector: ");
    platform_print_hex32(ring3_tss_meta.selector);
    platform_print("\n");

    platform_print("  ss0:      ");
    platform_print_hex32(ring3_tss_meta.ss0);
    platform_print("\n");

    platform_print("  esp0:     ");
    platform_print_hex32(ring3_tss_meta.esp0);
    platform_print("\n");

    platform_print("  prepared: ");
    platform_print(ring3_tss_meta.prepared ? "yes\n" : "no\n");

    platform_print("  desc:     ");
    platform_print(gdt_tss_descriptor_installed() ? "installed\n" : "missing\n");

    platform_print("  loaded:   ");
    platform_print(ring3_tss_loaded_ok() ? "yes\n" : "no\n");

    platform_print("  ltr:      ");
    platform_print(gdt_tss_loaded() ? "executed\n" : "not executed\n");

    platform_print("  result:   ");
    platform_print(ring3_tss_ok() ? "ok\n" : "broken\n");
}

void ring3_tss_check(void) {
    platform_print("Ring3 TSS check:\n");

    platform_print("  selector: ");
    platform_print(ring3_tss_meta.selector == RING3_TSS_SELECTOR ? "ok\n" : "bad\n");

    platform_print("  ss0:      ");
    platform_print(ring3_tss_meta.ss0 == RING3_KERNEL_DATA_SELECTOR ? "ok\n" : "bad\n");

    platform_print("  esp0:     ");
    platform_print(ring3_tss_meta.esp0 == RING3_KERNEL_STACK_TOP ? "ok\n" : "bad\n");

    platform_print("  prepared: ");
    platform_print(ring3_tss_meta.prepared ? "yes\n" : "no\n");

    platform_print("  desc:     ");
    platform_print(gdt_tss_descriptor_installed() ? "installed\n" : "missing\n");

    platform_print("  loaded:   ");
    platform_print(ring3_tss_loaded_ok() ? "yes\n" : "no\n");

    platform_print("  load cnt: ");
    platform_print_uint(ring3_tss_meta.load_count);
    platform_print("\n");

    platform_print("  clear cnt:");
    platform_print_uint(ring3_tss_meta.clear_count);
    platform_print("\n");

    platform_print("  ltr:      ");
    platform_print(gdt_tss_loaded() ? "executed\n" : "not executed\n");

    platform_print("  result:   ");
    platform_print(ring3_tss_ok() ? "ok\n" : "broken\n");
}

void ring3_tss_load(void) {
    platform_print("Ring3 TSS load:\n");

    if (!ring3_tss_ok()) {
        platform_print("  result: blocked\n");
        platform_print("  reason: TSS metadata not ready\n");
        return;
    }

    ring3_tss_loaded = 1;
    ring3_tss_meta.loaded = 1;
    ring3_tss_meta.load_count++;

    platform_print("  selector: ");
    platform_print_hex32(ring3_tss_meta.selector);
    platform_print("\n");

    platform_print("  ltr:      not executed\n");
    platform_print("  loaded:   yes\n");
    platform_print("  result:   ready\n");
}

void ring3_tss_install(void) {
    platform_print("Ring3 TSS install:\n");

    if (!ring3_tss_ok()) {
        platform_print("  result: blocked\n");
        platform_print("  reason: TSS metadata not ready\n");
        return;
    }

    gdt_install_tss(ring3_tss_meta.esp0);

    ring3_tss_loaded = 1;
    ring3_tss_meta.loaded = 1;
    ring3_tss_meta.load_count++;

    platform_print("  selector: ");
    platform_print_hex32(ring3_tss_meta.selector);
    platform_print("\n");

    platform_print("  esp0:     ");
    platform_print_hex32(ring3_tss_meta.esp0);
    platform_print("\n");

    platform_print("  desc:     ");
    platform_print(gdt_tss_descriptor_installed() ? "installed\n" : "missing\n");

    platform_print("  ltr:      ");
    platform_print(gdt_tss_loaded() ? "executed\n" : "failed\n");

    platform_print("  loaded:   ");
    platform_print(ring3_tss_loaded_ok() ? "yes\n" : "no\n");

    platform_print("  result:   ");
    platform_print(ring3_tss_loaded_ok() ? "ready\n" : "broken\n");
}

void ring3_tss_clear(void) {
    ring3_tss_loaded = 0;
    ring3_tss_meta.loaded = 0;
    ring3_tss_meta.clear_count++;

    platform_print("ring3 TSS metadata load cleared.\n");
}

void ring3_gdt(void) {
    platform_print("Ring3 GDT metadata:\n");

    platform_print("  kernel cs: ");
    platform_print_hex32(ring3_gdt_meta.kernel_code);
    platform_print("\n");

    platform_print("  kernel ds: ");
    platform_print_hex32(ring3_gdt_meta.kernel_data);
    platform_print("\n");

    platform_print("  user cs:   ");
    platform_print_hex32(ring3_gdt_meta.user_code);
    platform_print("\n");

    platform_print("  user ds:   ");
    platform_print_hex32(ring3_gdt_meta.user_data);
    platform_print("\n");

    platform_print("  cs dpl:    ");
    platform_print_uint(ring3_gdt_meta.user_code_dpl);
    platform_print("\n");

    platform_print("  ds dpl:    ");
    platform_print_uint(ring3_gdt_meta.user_data_dpl);
    platform_print("\n");

    platform_print("  prepared:  ");
    platform_print(ring3_gdt_meta.prepared ? "yes\n" : "no\n");

    platform_print("  installed: ");
    platform_print(ring3_gdt_meta.installed ? "yes\n" : "no\n");

    platform_print("  hw check:  ");
    platform_print(gdt_user_segments_installed() ? "ok\n" : "missing\n");

    platform_print("  install cnt:");
    platform_print_uint(ring3_gdt_meta.install_count);
    platform_print("\n");

    platform_print("  clear cnt: ");
    platform_print_uint(ring3_gdt_meta.clear_count);
    platform_print("\n");

    platform_print("  result:    ");
    platform_print(ring3_gdt_ok() ? "ok\n" : "missing\n");
}

void ring3_gdt_check(void) {
    platform_print("Ring3 GDT check:\n");

    platform_print("  kernel cs: ");
    platform_print(ring3_gdt_meta.kernel_code == RING3_KERNEL_CODE_SELECTOR ? "ok\n" : "bad\n");

    platform_print("  kernel ds: ");
    platform_print(ring3_gdt_meta.kernel_data == RING3_KERNEL_DATA_SELECTOR ? "ok\n" : "bad\n");

    platform_print("  user cs:   ");
    platform_print(ring3_gdt_meta.user_code == RING3_USER_CODE_SELECTOR ? "ok\n" : "bad\n");

    platform_print("  user ds:   ");
    platform_print(ring3_gdt_meta.user_data == RING3_USER_DATA_SELECTOR ? "ok\n" : "bad\n");

    platform_print("  cs dpl:    ");
    platform_print(ring3_gdt_meta.user_code_dpl == 3 ? "ring3\n" : "bad\n");

    platform_print("  ds dpl:    ");
    platform_print(ring3_gdt_meta.user_data_dpl == 3 ? "ring3\n" : "bad\n");

    platform_print("  prepared:  ");
    platform_print(ring3_gdt_meta.prepared ? "yes\n" : "no\n");

    platform_print("  installed: ");
    platform_print(ring3_gdt_meta.installed ? "yes\n" : "no\n");

    platform_print("  hw check:  ");
    platform_print(gdt_user_segments_installed() ? "ok\n" : "missing\n");

    platform_print("  result:    ");
    platform_print(ring3_gdt_ok() ? "ok\n" : "missing\n");
}

void ring3_gdt_prepare(void) {
    ring3_prepare_gdt_metadata();

    platform_print("ring3 GDT user segment metadata prepared.\n");
}

void ring3_gdt_install(void) {
    platform_print("Ring3 GDT install:\n");

    if (!ring3_gdt_meta.prepared) {
        ring3_prepare_gdt_metadata();
    }

    gdt_install_user_segments();

    ring3_gdt_meta.installed = 1;
    ring3_gdt_meta.install_count++;
    ring3_gdt_user_ready = 1;

    platform_print("  user cs:   ");
    platform_print_hex32(ring3_gdt_meta.user_code);
    platform_print("\n");

    platform_print("  user ds:   ");
    platform_print_hex32(ring3_gdt_meta.user_data);
    platform_print("\n");

    platform_print("  lgdt:      refreshed\n");

    platform_print("  result:    ");
    platform_print(ring3_gdt_ok() ? "installed\n" : "broken\n");
}

void ring3_gdt_clear(void) {
    ring3_gdt_user_ready = 0;
    ring3_gdt_meta.installed = 0;
    ring3_gdt_meta.clear_count++;

    platform_print("ring3 GDT user segment readiness cleared.\n");
}

void ring3_page(void) {
    platform_print("Ring3 user page metadata:\n");

    platform_print("  stub addr: ");
    platform_print_hex32(ring3_page_meta.stub_addr);
    platform_print("\n");

    platform_print("  stub page: ");
    platform_print_hex32(ring3_page_meta.stub_page);
    platform_print("\n");

    platform_print("  stack top: ");
    platform_print_hex32(ring3_page_meta.stack_top);
    platform_print("\n");

    platform_print("  stack page:");
    platform_print_hex32(ring3_page_meta.stack_page);
    platform_print("\n");

    platform_print("  present:   ");
    platform_print(ring3_page_meta.present ? "yes\n" : "no\n");

    platform_print("  writable:  ");
    platform_print(ring3_page_meta.writable ? "yes\n" : "no\n");

    platform_print("  user:      ");
    platform_print(ring3_page_meta.user ? "yes\n" : "no\n");

    platform_print("  prepared:  ");
    platform_print(ring3_page_meta.prepared ? "yes\n" : "no\n");

    platform_print("  installed: metadata-only\n");

    platform_print("  result:    ");
    platform_print(ring3_page_ok() ? "ok\n" : "missing\n");
}

void ring3_page_check(void) {
    platform_print("Ring3 user page check:\n");

    platform_print("  stub addr: ");
    platform_print(ring3_page_meta.stub_addr == ring3_get_stub_address() ? "ok\n" : "bad\n");

    platform_print("  stub page: ");
    platform_print(ring3_page_meta.stub_page == ring3_page_base(ring3_get_stub_address()) ? "ok\n" : "bad\n");

    platform_print("  stack top: ");
    platform_print(ring3_page_meta.stack_top == ring3_frame_meta.user_stack ? "ok\n" : "bad\n");

    platform_print("  stack page:");
    platform_print(ring3_page_meta.stack_page == ring3_page_base(ring3_frame_meta.user_stack) ? "ok\n" : "bad\n");

    platform_print("  present:   ");
    platform_print(ring3_page_meta.present ? "yes\n" : "no\n");

    platform_print("  writable:  ");
    platform_print(ring3_page_meta.writable ? "yes\n" : "no\n");

    platform_print("  user:      ");
    platform_print(ring3_page_meta.user ? "yes\n" : "no\n");

    platform_print("  prepared:  ");
    platform_print(ring3_page_meta.prepared ? "yes\n" : "no\n");

    platform_print("  installed: metadata-only\n");

    platform_print("  result:    ");
    platform_print(ring3_page_ok() ? "ok\n" : "missing\n");
}

void ring3_page_prepare(void) {
    platform_print("Ring3 user page prepare:\n");

    if (!ring3_stub_ok()) {
        platform_print("  result: blocked\n");
        platform_print("  reason: user stub not ready\n");
        return;
    }

    if (!ring3_frame_ok()) {
        platform_print("  result: blocked\n");
        platform_print("  reason: ring3 frame not ready\n");
        return;
    }

    ring3_prepare_page_metadata();

    platform_print("  stub page: ");
    platform_print_hex32(ring3_page_meta.stub_page);
    platform_print("\n");

    platform_print("  stack page:");
    platform_print_hex32(ring3_page_meta.stack_page);
    platform_print("\n");

    platform_print("  user bit:  metadata-only\n");
    platform_print("  result:    ready\n");
}

void ring3_page_clear(void) {
    ring3_user_page_ready = 0;

    ring3_page_meta.present = 0;
    ring3_page_meta.writable = 0;
    ring3_page_meta.user = 0;
    ring3_page_meta.prepared = 0;
    ring3_page_meta.clear_count++;

    platform_print("ring3 user page metadata cleared.\n");
}

void ring3_hw(void) {
    platform_print("Ring3 hardware gate:\n");

    platform_print("  switch:     ");
    platform_print(ring3_transition_enabled ? "enabled\n" : "disabled\n");

    platform_print("  guard:      ");
    platform_print(ring3_guard_ok() ? "ready\n" : "blocked\n");

    platform_print("  iret frame: ");
    platform_print(ring3_iret_frame_ok() ? "ready\n" : "missing\n");

    platform_print("  user stub:  ");
    platform_print(ring3_stub_ok() ? "ready\n" : "broken\n");

    platform_print("  gdt user:   ");
    platform_print(ring3_gdt_ok() ? "installed\n" : "missing\n");

    platform_print("  tss loaded: ");
    platform_print(ring3_tss_loaded_ok() ? "yes\n" : "no\n");

    platform_print("  user page:  ");
    platform_print(ring3_page_ok() ? "ready\n" : "missing\n");

    platform_print("  real hw:    ");
    platform_print(ring3_real_hardware_installed ? "installed\n" : "missing\n");

    platform_print("  installs:   ");
    platform_print_uint(ring3_hw_install_count);
    platform_print("\n");

    platform_print("  clears:     ");
    platform_print_uint(ring3_hw_clear_count);
    platform_print("\n");

    platform_print("  result:     ");
    platform_print(ring3_real_hardware_ready() ? "ready\n" : "blocked\n");
}

void ring3_hw_check(void) {
    platform_print("Ring3 hardware check:\n");

    platform_print("  switch:     ");
    platform_print(ring3_transition_enabled ? "ok\n" : "disabled\n");

    platform_print("  guard:      ");
    platform_print(ring3_guard_ok() ? "ok\n" : "bad\n");

    platform_print("  iret frame: ");
    platform_print(ring3_iret_frame_ok() ? "ok\n" : "missing\n");

    platform_print("  user stub:  ");
    platform_print(ring3_stub_ok() ? "ok\n" : "bad\n");

    platform_print("  gdt user:   ");
    platform_print(ring3_gdt_ok() ? "ok\n" : "missing\n");

    platform_print("  tss loaded: ");
    platform_print(ring3_tss_loaded_ok() ? "ok\n" : "missing\n");

    platform_print("  user page:  ");
    platform_print(ring3_page_ok() ? "ok\n" : "missing\n");

    platform_print("  metadata:   ");
    platform_print(ring3_metadata_gate_ready() ? "ready\n" : "blocked\n");

    platform_print("  real hw:    ");
    platform_print(ring3_real_hardware_installed ? "installed\n" : "missing\n");

    platform_print("  result:     ");
    platform_print(ring3_real_hardware_ready() ? "ready\n" : "blocked\n");
}

void ring3_hw_install(void) {
    platform_print("Ring3 hardware install gate:\n");

    if (!ring3_metadata_gate_ready()) {
        platform_print("  result: blocked\n");
        platform_print("  reason: ring3 metadata gate not ready\n");
        return;
    }

    ring3_real_hardware_installed = 1;
    ring3_hw_install_count++;

    platform_print("  real hw: installed\n");
    platform_print("  result:  ready\n");
}

void ring3_hw_clear(void) {
    ring3_real_hardware_installed = 0;
    ring3_hw_clear_count++;

    platform_print("ring3 real hardware gate cleared.\n");
}

void ring3_syscall(void) {
    platform_print("Ring3 syscall metadata:\n");

    platform_print("  vector:    ");
    platform_print_hex32(ring3_syscall_meta.vector);
    platform_print("\n");

    platform_print("  syscall:   ");
    platform_print_uint(ring3_syscall_meta.syscall_id);
    platform_print("\n");

    platform_print("  arg1:      ");
    platform_print_uint(ring3_syscall_meta.arg1);
    platform_print("\n");

    platform_print("  arg2:      ");
    platform_print_uint(ring3_syscall_meta.arg2);
    platform_print("\n");

    platform_print("  arg3:      ");
    platform_print_uint(ring3_syscall_meta.arg3);
    platform_print("\n");

    platform_print("  prepared:  ");
    platform_print(ring3_syscall_meta.prepared ? "yes\n" : "no\n");

    platform_print("  execute:   no\n");

    platform_print("  dryrun:    ");
    platform_print(ring3_syscall_frame_ok() ? "ready\n" : "empty\n");

    platform_print("  result:    ");
    platform_print(ring3_syscall_ok() ? "ok\n" : "missing\n");
}

void ring3_syscall_check(void) {
    platform_print("Ring3 syscall check:\n");

    platform_print("  vector:    ");
    platform_print(ring3_syscall_meta.vector == 0x80 ? "ok\n" : "bad\n");

    platform_print("  syscall:   ");
    platform_print(ring3_syscall_meta.syscall_id == 0 ? "SYS_PING\n" : "bad\n");

    platform_print("  prepared:  ");
    platform_print(ring3_syscall_meta.prepared ? "yes\n" : "no\n");

    platform_print("  prepares:  ");
    platform_print_uint(ring3_syscall_meta.prepare_count);
    platform_print("\n");

    platform_print("  clears:    ");
    platform_print_uint(ring3_syscall_meta.clear_count);
    platform_print("\n");

    platform_print("  execute:   no\n");

    platform_print("  dryrun:    ");
    platform_print(ring3_syscall_frame_ok() ? "ready\n" : "empty\n");

    platform_print("  result:    ");
    platform_print(ring3_syscall_ok() ? "ok\n" : "missing\n");
}

void ring3_syscall_prepare(void) {
    ring3_syscall_meta.vector = 0x80;
    ring3_syscall_meta.syscall_id = 0;
    ring3_syscall_meta.arg1 = 11;
    ring3_syscall_meta.arg2 = 22;
    ring3_syscall_meta.arg3 = 33;
    ring3_syscall_meta.prepared = 1;
    ring3_syscall_meta.prepare_count++;

    platform_print("ring3 syscall metadata prepared.\n");
}

void ring3_syscall_clear(void) {
    ring3_syscall_meta.vector = 0;
    ring3_syscall_meta.syscall_id = 0;
    ring3_syscall_meta.arg1 = 0;
    ring3_syscall_meta.arg2 = 0;
    ring3_syscall_meta.arg3 = 0;
    ring3_syscall_meta.prepared = 0;
    ring3_syscall_meta.clear_count++;

    platform_print("ring3 syscall metadata cleared.\n");
}

void ring3_syscall_dryrun(void) {
    ring3_syscall_frame.dryrun_count++;

    platform_print("Ring3 syscall dryrun:\n");

    platform_print("  dryrun:  ");
    platform_print_uint(ring3_syscall_frame.dryrun_count);
    platform_print("\n");

    platform_print("  syscall: ");
    platform_print(ring3_syscall_ok() ? "prepared\n" : "missing\n");

    if (!ring3_syscall_ok()) {
        platform_print("  result:  blocked\n");
        platform_print("  reason:  ring3 syscall metadata not ready\n");
        return;
    }

    ring3_build_syscall_frame();

    platform_print("  int:     ");
    platform_print_hex32(ring3_syscall_frame.vector);
    platform_print("\n");

    platform_print("  eax:     ");
    platform_print_uint(ring3_syscall_frame.eax);
    platform_print("\n");

    platform_print("  ebx:     ");
    platform_print_uint(ring3_syscall_frame.ebx);
    platform_print("\n");

    platform_print("  ecx:     ");
    platform_print_uint(ring3_syscall_frame.ecx);
    platform_print("\n");

    platform_print("  edx:     ");
    platform_print_uint(ring3_syscall_frame.edx);
    platform_print("\n");

    platform_print("  execute: no\n");

    platform_print("  result:  ");
    platform_print(ring3_syscall_frame_ok() ? "staged\n" : "broken\n");
}

void ring3_syscall_stub(void) {
    platform_print("Ring3 syscall stub metadata:\n");

    platform_print("  entry:     ");
    platform_print_hex32(ring3_syscall_stub_meta.entry);
    platform_print("\n");

    platform_print("  safe:      ");
    platform_print_hex32(ring3_get_safe_stub_address());
    platform_print("\n");

    platform_print("  syscall stub:");
    platform_print_hex32(ring3_get_syscall_stub_address());
    platform_print("\n");

    platform_print("  selected:  ");
    platform_print(ring3_syscall_stub_selected ? "syscall\n" : "safe\n");

    platform_print("  vector:    ");
    platform_print_hex32(ring3_syscall_stub_meta.vector);
    platform_print("\n");

    platform_print("  syscall:   ");
    platform_print_uint(ring3_syscall_stub_meta.syscall_id);
    platform_print("\n");

    platform_print("  arg1:      ");
    platform_print_uint(ring3_syscall_stub_meta.arg1);
    platform_print("\n");

    platform_print("  arg2:      ");
    platform_print_uint(ring3_syscall_stub_meta.arg2);
    platform_print("\n");

    platform_print("  arg3:      ");
    platform_print_uint(ring3_syscall_stub_meta.arg3);
    platform_print("\n");

    platform_print("  prepared:  ");
    platform_print(ring3_syscall_stub_meta.prepared ? "yes\n" : "no\n");

    platform_print("  execute:   no\n");

    platform_print("  result:    ");
    platform_print(ring3_syscall_stub_ok() ? "ok\n" : "missing\n");
}

void ring3_syscall_stub_check(void) {
    platform_print("Ring3 syscall stub check:\n");

    platform_print("  entry:     ");
    platform_print(ring3_syscall_stub_meta.entry == ring3_get_syscall_stub_address() ? "ok\n" : "bad\n");

    platform_print("  vector:    ");
    platform_print(ring3_syscall_stub_meta.vector == 0x80 ? "ok\n" : "bad\n");

    platform_print("  syscall:   ");
    platform_print(ring3_syscall_stub_meta.syscall_id == ring3_syscall_meta.syscall_id ? "ok\n" : "bad\n");

    platform_print("  arg1:      ");
    platform_print(ring3_syscall_stub_meta.arg1 == ring3_syscall_meta.arg1 ? "ok\n" : "bad\n");

    platform_print("  arg2:      ");
    platform_print(ring3_syscall_stub_meta.arg2 == ring3_syscall_meta.arg2 ? "ok\n" : "bad\n");

    platform_print("  arg3:      ");
    platform_print(ring3_syscall_stub_meta.arg3 == ring3_syscall_meta.arg3 ? "ok\n" : "bad\n");

    platform_print("  prepared:  ");
    platform_print(ring3_syscall_stub_meta.prepared ? "yes\n" : "no\n");

    platform_print("  prepares:  ");
    platform_print_uint(ring3_syscall_stub_meta.prepare_count);
    platform_print("\n");

    platform_print("  clears:    ");
    platform_print_uint(ring3_syscall_stub_meta.clear_count);
    platform_print("\n");

    platform_print("  execute:   no\n");

    platform_print("  result:    ");
    platform_print(ring3_syscall_stub_ok() ? "ok\n" : "missing\n");
}

void ring3_syscall_stub_prepare(void) {
    platform_print("Ring3 syscall stub prepare:\n");

    if (!ring3_syscall_ok()) {
        platform_print("  result: blocked\n");
        platform_print("  reason: ring3 syscall metadata not ready\n");
        return;
    }

    if (!ring3_syscall_frame_ok()) {
        ring3_build_syscall_frame();
    }

    if (!ring3_syscall_frame_ok()) {
        platform_print("  result: blocked\n");
        platform_print("  reason: ring3 syscall frame not ready\n");
        return;
    }

    ring3_syscall_stub_meta.entry = ring3_get_syscall_stub_address();
    ring3_syscall_stub_meta.vector = ring3_syscall_frame.vector;
    ring3_syscall_stub_meta.syscall_id = ring3_syscall_frame.eax;
    ring3_syscall_stub_meta.arg1 = ring3_syscall_frame.ebx;
    ring3_syscall_stub_meta.arg2 = ring3_syscall_frame.ecx;
    ring3_syscall_stub_meta.arg3 = ring3_syscall_frame.edx;
    ring3_syscall_stub_meta.prepared = 1;
    ring3_syscall_stub_meta.prepare_count++;

    platform_print("  entry:  ");
    platform_print_hex32(ring3_syscall_stub_meta.entry);
    platform_print("\n");

    platform_print("  int:    ");
    platform_print_hex32(ring3_syscall_stub_meta.vector);
    platform_print("\n");

    platform_print("  syscall:");
    platform_print_uint(ring3_syscall_stub_meta.syscall_id);
    platform_print("\n");

    platform_print("  execute:no\n");
    platform_print("  result: ready\n");
}

void ring3_syscall_stub_clear(void) {
    ring3_syscall_stub_meta.entry = 0;
    ring3_syscall_stub_meta.vector = 0;
    ring3_syscall_stub_meta.syscall_id = 0;
    ring3_syscall_stub_meta.arg1 = 0;
    ring3_syscall_stub_meta.arg2 = 0;
    ring3_syscall_stub_meta.arg3 = 0;
    ring3_syscall_stub_meta.prepared = 0;
    ring3_syscall_stub_meta.clear_count++;

    platform_print("ring3 syscall stub metadata cleared.\n");
}

void ring3_syscall_stub_select(void) {
    platform_print("Ring3 syscall stub select:\n");

    if (!ring3_syscall_stub_ok()) {
        platform_print("  result: blocked\n");
        platform_print("  reason: syscall stub metadata not ready\n");
        return;
    }

    ring3_selected_stub_entry = ring3_get_syscall_stub_address();
    ring3_syscall_stub_selected = 1;

    ring3_prepare_frame();
    ring3_clear_iret_frame();

    platform_print("  selected: syscall\n");

    platform_print("  entry:    ");
    platform_print_hex32(ring3_selected_stub_entry);
    platform_print("\n");

    platform_print("  result:   ready\n");
}

void ring3_syscall_stub_unselect(void) {
    ring3_selected_stub_entry = 0;
    ring3_syscall_stub_selected = 0;
    ring3_syscall_exec_meta.real_armed = 0;

    ring3_prepare_frame();
    ring3_clear_iret_frame();

    platform_print("ring3 syscall stub unselected.\n");
}

void ring3_syscall_arm(void) {
    platform_print("Ring3 syscall arm:\n");

    if (!ring3_syscall_exec_gate_ok()) {
        platform_print("  result: blocked\n");
        platform_print("  reason: ring3 syscall execution gate not ready\n");
        return;
    }

    ring3_syscall_exec_meta.armed = 1;

    platform_print("  armed:  yes\n");
    platform_print("  result: ready\n");
}

void ring3_syscall_disarm(void) {
    ring3_syscall_exec_meta.armed = 0;
    ring3_syscall_exec_meta.real_armed = 0;
    ring3_syscall_exec_meta.disarmed_count++;
    ring3_syscall_exec_meta.real_disarmed_count++;

    platform_print("ring3 syscall execution disarmed.\n");
}

void ring3_syscall_exec(void) {
    ring3_syscall_exec_meta.attempts++;

    platform_print("Ring3 syscall exec:\n");

    platform_print("  attempt: ");
    platform_print_uint(ring3_syscall_exec_meta.attempts);
    platform_print("\n");

    platform_print("  armed:   ");
    platform_print(ring3_syscall_exec_meta.armed ? "yes\n" : "no\n");

    platform_print("  syscall: ");
    platform_print(ring3_syscall_ok() ? "prepared\n" : "missing\n");

    platform_print("  dryrun:  ");
    platform_print(ring3_syscall_frame_ok() ? "ready\n" : "empty\n");

    platform_print("  stub:    ");
    platform_print(ring3_syscall_stub_ok() ? "prepared\n" : "missing\n");

    if (!ring3_syscall_exec_gate_ok()) {
        ring3_syscall_exec_meta.blocked++;

        platform_print("  result:  blocked\n");
        platform_print("  reason:  ring3 syscall execution gate not ready\n");
        return;
    }

    if (!ring3_syscall_exec_meta.armed) {
        ring3_syscall_exec_meta.blocked++;

        platform_print("  result:  blocked\n");
        platform_print("  reason:  ring3 syscall execution disarmed\n");
        return;
    }

    ring3_syscall_exec_meta.staged++;

    platform_print("  int:     ");
    platform_print_hex32(ring3_syscall_frame.vector);
    platform_print("\n");

    platform_print("  eax:     ");
    platform_print_uint(ring3_syscall_frame.eax);
    platform_print("\n");

    platform_print("  ebx:     ");
    platform_print_uint(ring3_syscall_frame.ebx);
    platform_print("\n");

    platform_print("  ecx:     ");
    platform_print_uint(ring3_syscall_frame.ecx);
    platform_print("\n");

    platform_print("  edx:     ");
    platform_print_uint(ring3_syscall_frame.edx);
    platform_print("\n");

    platform_print("  execute: no\n");
    platform_print("  result:  staged\n");
}

void ring3_syscall_real(void) {
    platform_print("Ring3 syscall real gate:\n");

    platform_print("  syscall:   ");
    platform_print(ring3_syscall_ok() ? "prepared\n" : "missing\n");

    platform_print("  dryrun:    ");
    platform_print(ring3_syscall_frame_ok() ? "ready\n" : "empty\n");

    platform_print("  stub:      ");
    platform_print(ring3_syscall_stub_ok() ? "prepared\n" : "missing\n");

    platform_print("  selected:  ");
    platform_print(ring3_syscall_stub_selected ? "syscall\n" : "safe\n");

    platform_print("  exec arm:  ");
    platform_print(ring3_syscall_exec_meta.armed ? "yes\n" : "no\n");

    platform_print("  real arm:  ");
    platform_print(ring3_syscall_exec_meta.real_armed ? "yes\n" : "no\n");

    platform_print("  idt gate:  ");
    platform_print(ring3_syscall_gate_ok() ? "installed\n" : "missing\n");

    platform_print("  attempts:  ");
    platform_print_uint(ring3_syscall_exec_meta.real_attempts);
    platform_print("\n");

    platform_print("  blocked:   ");
    platform_print_uint(ring3_syscall_exec_meta.real_blocked);
    platform_print("\n");

    platform_print("  result:    ");
    platform_print(ring3_syscall_real_gate_ok() ? "ready\n" : "blocked\n");
}

void ring3_syscall_real_arm(void) {
    platform_print("Ring3 syscall real arm:\n");

    if (!ring3_syscall_exec_gate_ok()) {
        platform_print("  result: blocked\n");
        platform_print("  reason: ring3 syscall execution gate not ready\n");
        return;
    }

    if (!ring3_syscall_stub_selected) {
        platform_print("  result: blocked\n");
        platform_print("  reason: syscall stub not selected\n");
        return;
    }

    if (!ring3_syscall_gate_ok()) {
        platform_print("  result: blocked\n");
        platform_print("  reason: ring3 syscall IDT gate not installed\n");
        return;
    }

    ring3_syscall_exec_meta.real_armed = 1;

    platform_print("  real armed: yes\n");
    platform_print("  result:     ready\n");
}

void ring3_syscall_real_disarm(void) {
    ring3_syscall_exec_meta.real_armed = 0;
    ring3_syscall_exec_meta.real_disarmed_count++;

    platform_print("ring3 syscall real gate disarmed.\n");
}

void ring3_syscall_gate(void) {
    platform_print("Ring3 syscall IDT gate:\n");

    platform_print("  vector:    ");
    platform_print_hex32(0x80);
    platform_print("\n");

    platform_print("  dpl:       ring3\n");
    platform_print("  flags:     0xEE\n");

    platform_print("  installed: ");
    platform_print(ring3_syscall_exec_meta.gate_installed ? "yes\n" : "no\n");

    platform_print("  installs:  ");
    platform_print_uint(ring3_syscall_exec_meta.gate_install_count);
    platform_print("\n");

    platform_print("  clears:    ");
    platform_print_uint(ring3_syscall_exec_meta.gate_clear_count);
    platform_print("\n");

    platform_print("  result:    ");
    platform_print(ring3_syscall_gate_ok() ? "ok\n" : "missing\n");
}

void ring3_syscall_gate_install(void) {
    idt_install_syscall_gate();

    ring3_syscall_exec_meta.gate_installed = 1;
    ring3_syscall_exec_meta.gate_install_count++;

    platform_print("ring3 syscall IDT gate installed.\n");
}

void ring3_syscall_gate_clear(void) {
    ring3_syscall_exec_meta.gate_installed = 0;
    ring3_syscall_exec_meta.gate_clear_count++;

    platform_print("ring3 syscall IDT gate readiness cleared.\n");
}

void ring3_syscall_result(void) {
    unsigned int frame_valid = syscall_get_last_frame_valid();
    unsigned int frame_vector = syscall_get_last_frame_vector();
    unsigned int frame_eax = syscall_get_last_frame_eax();
    unsigned int frame_ebx = syscall_get_last_frame_ebx();
    unsigned int frame_ecx = syscall_get_last_frame_ecx();
    unsigned int frame_edx = syscall_get_last_frame_edx();

    unsigned int return_valid = syscall_get_last_return_valid();
    unsigned int return_id = syscall_get_last_return_id();
    unsigned int return_value = syscall_get_last_return_value();
    const char* return_status = syscall_get_last_return_status();

    platform_print("Ring3 syscall roundtrip result:\n");

    platform_print("  frame:     ");
    platform_print(frame_valid ? "valid\n" : "empty\n");

    platform_print("  vector:    ");
    platform_print_hex32(frame_vector);
    platform_print("\n");

    platform_print("  eax/id:    ");
    platform_print_uint(frame_eax);
    platform_print("\n");

    platform_print("  ebx/arg1:  ");
    platform_print_uint(frame_ebx);
    platform_print("\n");

    platform_print("  ecx/arg2:  ");
    platform_print_uint(frame_ecx);
    platform_print("\n");

    platform_print("  edx/arg3:  ");
    platform_print_uint(frame_edx);
    platform_print("\n");

    platform_print("  ret valid: ");
    platform_print(return_valid ? "yes\n" : "no\n");

    platform_print("  ret id:    ");
    platform_print_uint(return_id);
    platform_print("\n");

    platform_print("  ret value: ");
    platform_print_uint(return_value);
    platform_print("\n");

    platform_print("  ret status:");
    platform_print(return_status);
    platform_print("\n");

    platform_print("  result:    ");

    if (!frame_valid) {
        platform_print("empty\n");
        return;
    }

    if (!return_valid) {
        platform_print("no return\n");
        return;
    }

    if (frame_vector != 0x80) {
        platform_print("bad vector\n");
        return;
    }

    platform_print("ok\n");
}

void ring3_stack(void) {
    platform_print("Ring3 kernel stack:\n");

    platform_print("  top:      ");
    platform_print_hex32(ring3_stack_meta.kernel_stack_top);
    platform_print("\n");

    platform_print("  bottom:   ");
    platform_print_hex32(ring3_stack_meta.kernel_stack_bottom);
    platform_print("\n");

    platform_print("  size:     ");
    platform_print_uint(ring3_stack_meta.kernel_stack_size);
    platform_print("\n");

    platform_print("  prepared: ");
    platform_print(ring3_stack_meta.prepared ? "yes\n" : "no\n");

    platform_print("  result:   ");
    platform_print(ring3_stack_ok() ? "ok\n" : "broken\n");
}

void ring3_frame(void) {
    platform_print("Ring3 entry frame:\n");

    platform_print("  entry:    ");
    platform_print_hex32(ring3_frame_meta.user_entry);
    platform_print("\n");

    platform_print("  stub:     ");
    platform_print_hex32(ring3_get_stub_address());
    platform_print("\n");

    platform_print("  safe:     ");
    platform_print_hex32(ring3_get_safe_stub_address());
    platform_print("\n");

    platform_print("  syscall:  ");
    platform_print_hex32(ring3_get_syscall_stub_address());
    platform_print("\n");

    platform_print("  selected: ");
    platform_print(ring3_syscall_stub_selected ? "syscall\n" : "safe\n");

    platform_print("  stack:    ");
    platform_print_hex32(ring3_frame_meta.user_stack);
    platform_print("\n");

    platform_print("  user cs:  ");
    platform_print_hex32(ring3_frame_meta.user_code);
    platform_print("\n");

    platform_print("  user ds:  ");
    platform_print_hex32(ring3_frame_meta.user_data);
    platform_print("\n");

    platform_print("  eflags:   ");
    platform_print_hex32(ring3_frame_meta.eflags);
    platform_print("\n");

    platform_print("  prepared: ");
    platform_print(ring3_frame_meta.prepared ? "yes\n" : "no\n");

    platform_print("  result:   ");
    platform_print(ring3_frame_ok() ? "ok\n" : "broken\n");
}

void ring3_stub(void) {
    platform_print("Ring3 user stub:\n");

    platform_print("  address:  ");
    platform_print_hex32(ring3_get_stub_address());
    platform_print("\n");

    platform_print("  safe:     ");
    platform_print_hex32(ring3_get_safe_stub_address());
    platform_print("\n");

    platform_print("  syscall:  ");
    platform_print_hex32(ring3_get_syscall_stub_address());
    platform_print("\n");

    platform_print("  selected: ");
    platform_print(ring3_syscall_stub_selected ? "syscall\n" : "safe\n");

    platform_print("  frame eip:");
    platform_print_hex32(ring3_frame_meta.user_entry);
    platform_print("\n");

    platform_print("  behavior: hlt loop\n");
    platform_print("  execute:  no\n");

    platform_print("  result:   ");
    platform_print(ring3_stub_ok() ? "ok\n" : "broken\n");
}

void ring3_stub_check(void) {
    ring3_stub_checks++;

    platform_print("Ring3 stub check:\n");

    platform_print("  checks:   ");
    platform_print_uint(ring3_stub_checks);
    platform_print("\n");

    platform_print("  address:  ");
    platform_print(ring3_get_stub_address() != 0 ? "ok\n" : "missing\n");

    platform_print("  frame eip:");
    platform_print(ring3_frame_meta.user_entry == ring3_get_stub_address() ? "ok\n" : "bad\n");

    platform_print("  prepared: ");
    platform_print(ring3_frame_meta.prepared ? "yes\n" : "no\n");

    platform_print("  execute:  no\n");

    platform_print("  result:   ");
    platform_print(ring3_stub_ok() ? "ok\n" : "broken\n");
}

void ring3_arm(void) {
    platform_print("Ring3 arm:\n");

    if (!ring3_metadata_gate_ready()) {
        platform_print("  result: blocked\n");
        platform_print("  reason: ring3 metadata gate not ready\n");
        return;
    }

    if (!ring3_real_hardware_installed) {
        platform_print("  result: blocked\n");
        platform_print("  reason: real hardware gate not installed\n");
        return;
    }

    ring3_realenter_armed = 1;

    platform_print("  armed:  yes\n");
    platform_print("  result: ready\n");
}

void ring3_disarm(void) {
    ring3_realenter_armed = 0;

    platform_print("Ring3 disarmed.\n");
}

void ring3_realenter(void) {
    ring3_realenter_attempts++;

    platform_print("Ring3 real enter:\n");

    platform_print("  attempt:    ");
    platform_print_uint(ring3_realenter_attempts);
    platform_print("\n");

    platform_print("  switch:     ");
    platform_print(ring3_transition_enabled ? "enabled\n" : "disabled\n");

    platform_print("  armed:      ");
    platform_print(ring3_realenter_armed ? "yes\n" : "no\n");

    platform_print("  guard:      ");
    platform_print(ring3_guard_ok() ? "ready\n" : "blocked\n");

    if (!ring3_iret_frame_ok()) {
        ring3_build_iret_frame();
    }

    platform_print("  iret frame: ");
    platform_print(ring3_iret_frame_ok() ? "ready\n" : "broken\n");

    platform_print("  user stub:  ");
    platform_print(ring3_stub_ok() ? "ready\n" : "broken\n");

    platform_print("  gdt user:   ");
    platform_print(ring3_gdt_ok() ? "installed\n" : "missing\n");

    platform_print("  tss loaded: ");
    platform_print(ring3_tss_loaded_ok() ? "yes\n" : "no\n");

    platform_print("  user page:  ");
    platform_print(ring3_page_ok() ? "ready\n" : "missing\n");

    if (!ring3_real_hardware_ready()) {
        ring3_realenter_blocked++;

        platform_print("  result:     blocked\n");
        platform_print("  reason:     real ring3 hardware not installed\n");
        return;
    }

    if (!ring3_realenter_armed) {
        ring3_realenter_blocked++;

        platform_print("  result:     blocked\n");
        platform_print("  reason:     ring3 realenter disarmed\n");
        return;
    }

    if (ring3_syscall_stub_selected) {
        ring3_syscall_exec_meta.real_attempts++;

        platform_print("  syscall:    selected\n");
        platform_print("  sys arm:    ");
        platform_print(ring3_syscall_exec_meta.armed ? "yes\n" : "no\n");

        platform_print("  sys real:   ");
        platform_print(ring3_syscall_exec_meta.real_armed ? "yes\n" : "no\n");

        if (!ring3_syscall_exec_gate_ok()) {
            ring3_syscall_exec_meta.real_blocked++;
            ring3_realenter_blocked++;

            platform_print("  result:     blocked\n");
            platform_print("  reason:     ring3 syscall execution gate not ready\n");
            return;
        }

        if (!ring3_syscall_exec_meta.armed) {
            ring3_syscall_exec_meta.real_blocked++;
            ring3_realenter_blocked++;

            platform_print("  result:     blocked\n");
            platform_print("  reason:     ring3 syscall execution disarmed\n");
            return;
        }

        if (!ring3_syscall_exec_meta.real_armed) {
            ring3_syscall_exec_meta.real_blocked++;
            ring3_realenter_blocked++;

            platform_print("  result:     blocked\n");
            platform_print("  reason:     ring3 syscall real gate disarmed\n");
            return;
        }

        if (!ring3_syscall_gate_ok()) {
            ring3_syscall_exec_meta.real_blocked++;
            ring3_realenter_blocked++;

            platform_print("  result:     blocked\n");
            platform_print("  reason:     ring3 syscall IDT gate not installed\n");
            return;
        }

        ring3_syscall_exec_meta.executed++;
    }

    platform_print("  eip:        ");
    platform_print_hex32(ring3_last_iret_frame.eip);
    platform_print("\n");

    platform_print("  cs:         ");
    platform_print_hex32(ring3_last_iret_frame.cs);
    platform_print("\n");

    platform_print("  eflags:     ");
    platform_print_hex32(ring3_last_iret_frame.eflags);
    platform_print("\n");

    platform_print("  esp:        ");
    platform_print_hex32(ring3_last_iret_frame.esp);
    platform_print("\n");

    platform_print("  ss:         ");
    platform_print_hex32(ring3_last_iret_frame.ss);
    platform_print("\n");

    platform_print("  result:     executing iretd\n");

    ring3_realenter_executed++;

    ring3_enter_user_mode(
        ring3_last_iret_frame.eip,
        ring3_last_iret_frame.cs,
        ring3_last_iret_frame.eflags,
        ring3_last_iret_frame.esp,
        ring3_last_iret_frame.ss
    );

    platform_print("  result:     returned unexpectedly\n");
}

void ring3_guard(void) {
    platform_print("Ring3 transition guard:\n");

    platform_print("  init:        ");
    platform_print(ring3_init_done ? "ok\n" : "missing\n");

    platform_print("  ready:       ");
    platform_print(ring3_ready ? "ok\n" : "bad\n");

    platform_print("  broken:      ");
    platform_print(ring3_broken ? "yes\n" : "no\n");

    platform_print("  tss:         ");
    platform_print(ring3_tss_ok() ? "ok\n" : "broken\n");

    platform_print("  kernel stack:");
    platform_print(ring3_stack_ok() ? "ok\n" : "broken\n");

    platform_print("  entry frame: ");
    platform_print(ring3_frame_ok() ? "ok\n" : "broken\n");

    platform_print("  real switch: ");
    platform_print(ring3_transition_enabled ? "enabled\n" : "disabled\n");

    platform_print("  result:      ");
    platform_print(ring3_guard_ok() ? "ready\n" : "blocked\n");
}

void ring3_enable_switch(void) {
    platform_print("Ring3 switch enable:\n");

    if (!ring3_guard_ok()) {
        platform_print("  result: blocked\n");
        platform_print("  reason: ring3 guard not ready\n");
        return;
    }

    ring3_transition_enabled = 1;

    platform_print("  real switch: enabled\n");
    platform_print("  result: staged\n");
}

void ring3_disable_switch(void) {
    ring3_transition_enabled = 0;

    platform_print("Ring3 switch disabled.\n");
}

void ring3_enter(void) {
    ring3_enter_attempts++;

    platform_print("Ring3 enter:\n");

    platform_print("  attempt: ");
    platform_print_uint(ring3_enter_attempts);
    platform_print("\n");

    platform_print("  guard:   ");
    platform_print(ring3_guard_ok() ? "ready\n" : "blocked\n");

    if (!ring3_guard_ok()) {
        ring3_enter_blocked++;

        platform_print("  result:  blocked\n");
        platform_print("  reason:  ring3 guard not ready\n");
        return;
    }

    if (!ring3_transition_enabled) {
        ring3_enter_blocked++;

        platform_print("  result:  blocked\n");
        platform_print("  reason:  real ring3 transition disabled\n");
        return;
    }

    ring3_enter_staged++;

    platform_print("  result:  staged\n");
    platform_print("  reason:  real iret transition not implemented\n");
}

void ring3_dryrun(void) {
    ring3_dryrun_count++;

    platform_print("Ring3 iret dryrun:\n");

    platform_print("  dryrun: ");
    platform_print_uint(ring3_dryrun_count);
    platform_print("\n");

    platform_print("  guard:  ");
    platform_print(ring3_guard_ok() ? "ready\n" : "blocked\n");

    if (!ring3_guard_ok()) {
        ring3_enter_blocked++;

        platform_print("  result: blocked\n");
        platform_print("  reason: ring3 guard not ready\n");
        return;
    }

    ring3_build_iret_frame();

    platform_print("  frame layout:\n");
    platform_print("    ss      ");
    platform_print_hex32(ring3_last_iret_frame.ss);
    platform_print("\n");

    platform_print("    esp     ");
    platform_print_hex32(ring3_last_iret_frame.esp);
    platform_print("\n");

    platform_print("    eflags  ");
    platform_print_hex32(ring3_last_iret_frame.eflags);
    platform_print("\n");

    platform_print("    cs      ");
    platform_print_hex32(ring3_last_iret_frame.cs);
    platform_print("\n");

    platform_print("    eip     ");
    platform_print_hex32(ring3_last_iret_frame.eip);
    platform_print("\n");

    platform_print("    stub    ");
    platform_print_hex32(ring3_get_stub_address());
    platform_print("\n");

    platform_print("  iret:   not executed\n");

    platform_print("  result: ");
    platform_print(ring3_iret_frame_ok() ? "staged\n" : "broken\n");
}

void ring3_check(void) {
    ring3_check_count++;

    platform_print("Ring3 check:\n");

    platform_print("  init:        ");
    platform_print(ring3_init_done ? "ok\n" : "missing\n");

    platform_print("  ready:       ");
    platform_print(ring3_ready ? "ok\n" : "bad\n");

    platform_print("  broken:      ");
    platform_print(ring3_broken ? "yes\n" : "no\n");

    platform_print("  tss:         ");
    platform_print(ring3_tss_ok() ? "ok\n" : "broken\n");

    platform_print("  kernel stack:");
    platform_print(ring3_stack_ok() ? "ok\n" : "broken\n");

    platform_print("  entry frame: ");
    platform_print(ring3_frame_ok() ? "ok\n" : "broken\n");

    platform_print("  guard:       ");
    platform_print(ring3_guard_ok() ? "ready\n" : "blocked\n");

    platform_print("  real switch: ");
    platform_print(ring3_transition_enabled ? "enabled\n" : "disabled\n");

    platform_print("  result:      ");
    platform_print(ring3_doctor_ok() ? "ok\n" : "broken\n");
}

void ring3_doctor(void) {
    platform_print("Ring3 doctor:\n");

    platform_print("  init:        ");
    platform_print(ring3_init_done ? "ok\n" : "missing\n");

    platform_print("  ready:       ");
    platform_print(ring3_ready ? "ok\n" : "bad\n");

    platform_print("  broken:      ");
    platform_print(ring3_broken ? "yes\n" : "no\n");

    platform_print("  tss:         ");
    platform_print(ring3_tss_ok() ? "ok\n" : "broken\n");

    platform_print("  kernel stack:");
    platform_print(ring3_stack_ok() ? "ok\n" : "broken\n");

    platform_print("  entry frame: ");
    platform_print(ring3_frame_ok() ? "ok\n" : "broken\n");

    platform_print("  guard:       ");
    platform_print(ring3_guard_ok() ? "ready\n" : "blocked\n");

    platform_print("  checks:      ");
    platform_print_uint(ring3_check_count);
    platform_print("\n");

    platform_print("  fixes:       ");
    platform_print_uint(ring3_fix_count);
    platform_print("\n");

    platform_print("  attempts:    ");
    platform_print_uint(ring3_enter_attempts);
    platform_print("\n");

    platform_print("  blocked:     ");
    platform_print_uint(ring3_enter_blocked);
    platform_print("\n");

    platform_print("  staged:      ");
    platform_print_uint(ring3_enter_staged);
    platform_print("\n");

    platform_print("  dryrun:      ");
    platform_print_uint(ring3_dryrun_count);
    platform_print("\n");

    platform_print("  gdt user:    ");
    platform_print(ring3_gdt_ok() ? "installed\n" : "missing\n");

    platform_print("  gdt installs:");
    platform_print_uint(ring3_gdt_meta.install_count);
    platform_print("\n");

    platform_print("  gdt clears:  ");
    platform_print_uint(ring3_gdt_meta.clear_count);
    platform_print("\n");

    platform_print("  tss loads:   ");
    platform_print_uint(ring3_tss_meta.load_count);
    platform_print("\n");

    platform_print("  tss clears:  ");
    platform_print_uint(ring3_tss_meta.clear_count);
    platform_print("\n");

    platform_print("  user page:   ");
    platform_print(ring3_page_ok() ? "ready\n" : "missing\n");

    platform_print("  page prep:   ");
    platform_print_uint(ring3_page_meta.prepare_count);
    platform_print("\n");

    platform_print("  page clear:  ");
    platform_print_uint(ring3_page_meta.clear_count);
    platform_print("\n");

    platform_print("  realenter:   ");
    platform_print_uint(ring3_realenter_attempts);
    platform_print("\n");

    platform_print("  real blocked:");
    platform_print_uint(ring3_realenter_blocked);
    platform_print("\n");

    platform_print("  real hw:     ");
    platform_print(ring3_real_hardware_installed ? "installed\n" : "missing\n");

    platform_print("  syscall:    ");
    platform_print(ring3_syscall_ok() ? "prepared\n" : "missing\n");

    platform_print("  syscall dry:");
    platform_print(ring3_syscall_frame_ok() ? "ready\n" : "empty\n");

    platform_print("  syscallstub:");
    platform_print(ring3_syscall_stub_ok() ? "prepared\n" : "missing\n");

    platform_print("  stub mode:  ");
    platform_print(ring3_syscall_stub_selected ? "syscall\n" : "safe\n");

    platform_print("  sys prep:   ");
    platform_print_uint(ring3_syscall_meta.prepare_count);
    platform_print("\n");

    platform_print("  sys clear:  ");
    platform_print_uint(ring3_syscall_meta.clear_count);
    platform_print("\n");

    platform_print("  sys dryrun: ");
    platform_print_uint(ring3_syscall_frame.dryrun_count);
    platform_print("\n");

    platform_print("  stub prep:  ");
    platform_print_uint(ring3_syscall_stub_meta.prepare_count);
    platform_print("\n");

    platform_print("  sys armed:  ");
    platform_print(ring3_syscall_exec_meta.armed ? "yes\n" : "no\n");

    platform_print("  sys attempts:");
    platform_print_uint(ring3_syscall_exec_meta.attempts);
    platform_print("\n");

    platform_print("  sys blocked:");
    platform_print_uint(ring3_syscall_exec_meta.blocked);
    platform_print("\n");

    platform_print("  sys staged: ");
    platform_print_uint(ring3_syscall_exec_meta.staged);
    platform_print("\n");

    platform_print("  sys exec:   ");
    platform_print_uint(ring3_syscall_exec_meta.executed);
    platform_print("\n");

    platform_print("  sys disarm: ");
    platform_print_uint(ring3_syscall_exec_meta.disarmed_count);
    platform_print("\n");

    platform_print("  sys real:   ");
    platform_print(ring3_syscall_exec_meta.real_armed ? "yes\n" : "no\n");

    platform_print("  real tries: ");
    platform_print_uint(ring3_syscall_exec_meta.real_attempts);
    platform_print("\n");

    platform_print("  real blocks:");
    platform_print_uint(ring3_syscall_exec_meta.real_blocked);
    platform_print("\n");

    platform_print("  real disarm:");
    platform_print_uint(ring3_syscall_exec_meta.real_disarmed_count);
    platform_print("\n");

    platform_print("  sys gate:   ");
    platform_print(ring3_syscall_gate_ok() ? "installed\n" : "missing\n");

    platform_print("  gate inst:  ");
    platform_print_uint(ring3_syscall_exec_meta.gate_install_count);
    platform_print("\n");

    platform_print("  gate clear: ");
    platform_print_uint(ring3_syscall_exec_meta.gate_clear_count);
    platform_print("\n");

    platform_print("  hw installs: ");
    platform_print_uint(ring3_hw_install_count);
    platform_print("\n");

    platform_print("  hw clears:   ");
    platform_print_uint(ring3_hw_clear_count);
    platform_print("\n");

    platform_print("  executed:    ");
    platform_print_uint(ring3_realenter_executed);
    platform_print("\n");

    platform_print("  real switch: ");
    platform_print(ring3_transition_enabled ? "enabled\n" : "disabled\n");

    platform_print("  result:      ");
    platform_print(ring3_doctor_ok() ? "ok\n" : "broken\n");
}

void ring3_break(void) {
    ring3_broken = 1;
    ring3_ready = 0;
    ring3_frame_meta.prepared = 0;

    platform_print("ring3 layer broken.\n");
}

void ring3_fix(void) {
    ring3_broken = 0;
    ring3_ready = 1;
    ring3_fix_count++;
    ring3_transition_enabled = 0;
    ring3_enter_staged = 0;
    ring3_dryrun_count = 0;
    ring3_stub_checks = 0;
    ring3_realenter_attempts = 0;
    ring3_realenter_blocked = 0;
    ring3_realenter_armed = 0;
    ring3_realenter_executed = 0;
    ring3_real_hardware_installed = 0;
    ring3_hw_install_count = 0;
    ring3_hw_clear_count = 0;

    ring3_gdt_user_ready = 0;
    ring3_tss_loaded = 0;
    ring3_user_page_ready = 0;

    ring3_gdt_meta.kernel_code = 0;
    ring3_gdt_meta.kernel_data = 0;
    ring3_gdt_meta.user_code = 0;
    ring3_gdt_meta.user_data = 0;
    ring3_gdt_meta.user_code_dpl = 0;
    ring3_gdt_meta.user_data_dpl = 0;
    ring3_gdt_meta.prepared = 0;
    ring3_gdt_meta.installed = 0;
    ring3_gdt_meta.install_count = 0;
    ring3_gdt_meta.clear_count = 0;

    ring3_tss_meta.loaded = 0;
    ring3_tss_meta.load_count = 0;
    ring3_tss_meta.clear_count = 0;

    ring3_page_meta.stub_addr = 0;
    ring3_page_meta.stub_page = 0;
    ring3_page_meta.stack_top = 0;
    ring3_page_meta.stack_page = 0;
    ring3_page_meta.present = 0;
    ring3_page_meta.writable = 0;
    ring3_page_meta.user = 0;
    ring3_page_meta.prepared = 0;
    ring3_page_meta.prepare_count = 0;
    ring3_page_meta.clear_count = 0;

    ring3_syscall_meta.vector = 0;
    ring3_syscall_meta.syscall_id = 0;
    ring3_syscall_meta.arg1 = 0;
    ring3_syscall_meta.arg2 = 0;
    ring3_syscall_meta.arg3 = 0;
    ring3_syscall_meta.prepared = 0;
    ring3_syscall_meta.prepare_count = 0;
    ring3_syscall_meta.clear_count = 0;

    ring3_clear_syscall_frame();

    ring3_syscall_stub_meta.entry = 0;
    ring3_syscall_stub_meta.vector = 0;
    ring3_syscall_stub_meta.syscall_id = 0;
    ring3_syscall_stub_meta.arg1 = 0;
    ring3_syscall_stub_meta.arg2 = 0;
    ring3_syscall_stub_meta.arg3 = 0;
    ring3_syscall_stub_meta.prepared = 0;
    ring3_syscall_stub_meta.prepare_count = 0;
    ring3_syscall_stub_meta.clear_count = 0;

    ring3_syscall_exec_meta.armed = 0;
    ring3_syscall_exec_meta.attempts = 0;
    ring3_syscall_exec_meta.blocked = 0;
    ring3_syscall_exec_meta.staged = 0;
    ring3_syscall_exec_meta.executed = 0;
    ring3_syscall_exec_meta.disarmed_count = 0;

    ring3_syscall_exec_meta.real_armed = 0;
    ring3_syscall_exec_meta.real_attempts = 0;
    ring3_syscall_exec_meta.real_blocked = 0;
    ring3_syscall_exec_meta.real_disarmed_count = 0;

    ring3_syscall_exec_meta.gate_installed = 0;
    ring3_syscall_exec_meta.gate_install_count = 0;
    ring3_syscall_exec_meta.gate_clear_count = 0;

    ring3_selected_stub_entry = 0;
    ring3_syscall_stub_selected = 0;

    ring3_clear_iret_frame();

    ring3_prepare_tss();
    ring3_prepare_stack();
    ring3_prepare_frame();

    platform_print("ring3 layer fixed.\n");
}