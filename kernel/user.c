#include "user.h"
#include "platform.h"

#define USER_PROGRAM_MAX 4

#define USER_KERNEL_CODE_SELECTOR 0x08
#define USER_KERNEL_DATA_SELECTOR 0x10
#define USER_CODE_SELECTOR        0x1B
#define USER_DATA_SELECTOR        0x23

#define USER_STACK_TOP            0x003FF000
#define USER_STACK_SIZE           0x00001000
#define USER_ENTRY_POINT          0x00000000
#define USER_EFLAGS_IF            0x00000200

typedef struct user_program {
    unsigned int id;
    const char* name;
    const char* state;
    unsigned int entry;
} user_program_t;

typedef struct user_segment_metadata {
    unsigned int kernel_code;
    unsigned int kernel_data;
    unsigned int user_code;
    unsigned int user_data;
    unsigned int prepared;
} user_segment_metadata_t;

typedef struct user_stack_metadata {
    unsigned int top;
    unsigned int size;
    unsigned int bottom;
    unsigned int prepared;
} user_stack_metadata_t;

typedef struct user_entry_frame {
    unsigned int entry;
    unsigned int user_stack;
    unsigned int user_code;
    unsigned int user_data;
    unsigned int eflags;
    unsigned int prepared;
} user_entry_frame_t;

typedef struct user_boundary_metadata {
    unsigned int ring3_disabled;
    unsigned int syscall_ready;
    unsigned int paging_required;
    unsigned int checked;
} user_boundary_metadata_t;

static user_program_t user_programs_table[USER_PROGRAM_MAX];

static user_segment_metadata_t user_segments_meta;
static user_stack_metadata_t user_stack_meta;
static user_entry_frame_t user_entry_meta;
static user_boundary_metadata_t user_boundary_meta;

static int user_init_done = 0;
static int user_ready = 0;
static int user_broken = 0;

static unsigned int user_program_count = 0;
static unsigned int user_entry_count = 0;
static unsigned int user_metadata_checks = 0;
static unsigned int user_prepare_count = 0;
static unsigned int user_boundary_broken = 0;
static unsigned int user_frame_broken = 0;
static unsigned int user_stack_broken = 0;

static const char* user_mode = "ring3 preparation metadata";

static void user_programs_reset(void) {
    user_programs_table[0].id = 0;
    user_programs_table[0].name = "init";
    user_programs_table[0].state = "metadata";
    user_programs_table[0].entry = USER_ENTRY_POINT;

    user_programs_table[1].id = 1;
    user_programs_table[1].name = "shell-user";
    user_programs_table[1].state = "metadata";
    user_programs_table[1].entry = USER_ENTRY_POINT;

    user_programs_table[2].id = 2;
    user_programs_table[2].name = "intent-user";
    user_programs_table[2].state = "metadata";
    user_programs_table[2].entry = USER_ENTRY_POINT;

    user_programs_table[3].id = 3;
    user_programs_table[3].name = "reserved";
    user_programs_table[3].state = "reserved";
    user_programs_table[3].entry = USER_ENTRY_POINT;

    user_program_count = USER_PROGRAM_MAX;
    user_entry_count = 3;
}

static void user_segments_prepare(void) {
    user_segments_meta.kernel_code = USER_KERNEL_CODE_SELECTOR;
    user_segments_meta.kernel_data = USER_KERNEL_DATA_SELECTOR;
    user_segments_meta.user_code = USER_CODE_SELECTOR;
    user_segments_meta.user_data = USER_DATA_SELECTOR;
    user_segments_meta.prepared = 1;
}

static void user_stack_prepare(void) {
    user_stack_meta.top = USER_STACK_TOP;
    user_stack_meta.size = USER_STACK_SIZE;
    user_stack_meta.bottom = USER_STACK_TOP - USER_STACK_SIZE;
    user_stack_meta.prepared = 1;
}

static void user_entry_frame_prepare(void) {
    user_entry_meta.entry = USER_ENTRY_POINT;
    user_entry_meta.user_stack = USER_STACK_TOP;
    user_entry_meta.user_code = USER_CODE_SELECTOR;
    user_entry_meta.user_data = USER_DATA_SELECTOR;
    user_entry_meta.eflags = USER_EFLAGS_IF;
    user_entry_meta.prepared = 1;
}

static void user_boundary_prepare(void) {
    user_boundary_meta.ring3_disabled = 1;
    user_boundary_meta.syscall_ready = 1;
    user_boundary_meta.paging_required = 1;
    user_boundary_meta.checked = 1;
}

static int user_selector_is_ring3(unsigned int selector) {
    if ((selector & 0x3) != 0x3) {
        return 0;
    }

    return 1;
}

static int user_boundary_ok(void) {
    if (user_boundary_broken) {
        return 0;
    }

    if (!user_segments_meta.prepared) {
        return 0;
    }

    if (!user_stack_meta.prepared) {
        return 0;
    }

    if (!user_entry_meta.prepared) {
        return 0;
    }

    if (!user_boundary_meta.checked) {
        return 0;
    }

    if (!user_selector_is_ring3(user_segments_meta.user_code)) {
        return 0;
    }

    if (!user_selector_is_ring3(user_segments_meta.user_data)) {
        return 0;
    }

    if (user_stack_meta.size == 0) {
        return 0;
    }

    if (user_stack_meta.bottom >= user_stack_meta.top) {
        return 0;
    }

    if (user_entry_meta.user_stack != user_stack_meta.top) {
        return 0;
    }

    if (user_entry_meta.user_code != user_segments_meta.user_code) {
        return 0;
    }

    if (user_entry_meta.user_data != user_segments_meta.user_data) {
        return 0;
    }

    if ((user_entry_meta.eflags & USER_EFLAGS_IF) == 0) {
        return 0;
    }

    if (!user_boundary_meta.syscall_ready) {
        return 0;
    }

    if (!user_boundary_meta.paging_required) {
        return 0;
    }

    return 1;
}

static int user_is_page_aligned(unsigned int value) {
    return (value & 0xFFF) == 0;
}

static int user_frame_ok(void) {
    if (user_frame_broken) {
        return 0;
    }

    if (!user_entry_meta.prepared) {
        return 0;
    }

    if (user_entry_meta.user_stack != user_stack_meta.top) {
        return 0;
    }

    if (!user_is_page_aligned(user_entry_meta.user_stack)) {
        return 0;
    }

    if (!user_is_page_aligned(user_stack_meta.bottom)) {
        return 0;
    }

    if (user_entry_meta.user_code != user_segments_meta.user_code) {
        return 0;
    }

    if (user_entry_meta.user_data != user_segments_meta.user_data) {
        return 0;
    }

    if (!user_selector_is_ring3(user_entry_meta.user_code)) {
        return 0;
    }

    if (!user_selector_is_ring3(user_entry_meta.user_data)) {
        return 0;
    }

    if ((user_entry_meta.eflags & USER_EFLAGS_IF) == 0) {
        return 0;
    }

    return 1;
}

static int user_stack_ok(void) {
    if (user_stack_broken) {
        return 0;
    }

    if (!user_stack_meta.prepared) {
        return 0;
    }

    if (user_stack_meta.size == 0) {
        return 0;
    }

    if (!user_is_page_aligned(user_stack_meta.top)) {
        return 0;
    }

    if (!user_is_page_aligned(user_stack_meta.bottom)) {
        return 0;
    }

    if (user_stack_meta.bottom >= user_stack_meta.top) {
        return 0;
    }

    if ((user_stack_meta.top - user_stack_meta.bottom) != user_stack_meta.size) {
        return 0;
    }

    if (user_stack_meta.top > 0x003FFFFF) {
        return 0;
    }

    if (user_stack_meta.bottom < 0x00001000) {
        return 0;
    }

    return 1;
}

void user_init(void) {
    user_init_done = 1;
    user_ready = 1;
    user_broken = 0;

    user_metadata_checks = 0;
    user_prepare_count = 0;
    user_boundary_broken = 0;
    user_frame_broken = 0;
    user_stack_broken = 0;

    user_programs_reset();
    user_segments_prepare();
    user_stack_prepare();
    user_entry_frame_prepare();
    user_boundary_prepare();
}

int user_is_ready(void) {
    if (!user_init_done) {
        return 0;
    }

    if (!user_ready) {
        return 0;
    }

    if (user_broken) {
        return 0;
    }

    if (user_program_count == 0) {
        return 0;
    }

    if (user_entry_count == 0) {
        return 0;
    }

    if (!user_segments_meta.prepared) {
        return 0;
    }

    if (!user_stack_ok()) {
        return 0;
    }

    if (!user_frame_ok()) {
        return 0;
    }

    if (!user_boundary_ok()) {
        return 0;
    }

    return 1;
}

int user_doctor_ok(void) {
    return user_is_ready();
}

const char* user_get_state(void) {
    return user_is_ready() ? "ready" : "broken";
}

const char* user_get_mode(void) {
    return user_mode;
}

unsigned int user_get_program_count(void) {
    return user_program_count;
}

unsigned int user_get_entry_count(void) {
    return user_entry_count;
}

void user_status(void) {
    platform_print("User mode layer:\n");

    platform_print("  state:      ");
    platform_print(user_get_state());
    platform_print("\n");

    platform_print("  mode:       ");
    platform_print(user_get_mode());
    platform_print("\n");

    platform_print("  programs:   ");
    platform_print_uint(user_get_program_count());
    platform_print("\n");

    platform_print("  entries:    ");
    platform_print_uint(user_get_entry_count());
    platform_print("\n");

    platform_print("  checks:     ");
    platform_print_uint(user_metadata_checks);
    platform_print("\n");

    platform_print("  prepares:   ");
    platform_print_uint(user_prepare_count);
    platform_print("\n");

    platform_print("  segments:   ");
    platform_print(user_segments_meta.prepared ? "prepared\n" : "missing\n");

    platform_print("  stack:      ");
    platform_print(user_stack_meta.prepared ? "prepared\n" : "missing\n");

    platform_print("  frame:      ");
    platform_print(user_entry_meta.prepared ? "prepared\n" : "missing\n");

    platform_print("  boundary:   ");
    platform_print(user_boundary_meta.checked ? "checked\n" : "missing\n");

    platform_print("  ring3:      disabled\n");
    platform_print("  syscall:    int 0x80 ready\n");
}

void user_check(void) {
    user_metadata_checks++;

    platform_print("User mode check:\n");

    platform_print("  init:     ");
    platform_print(user_init_done ? "ok\n" : "missing\n");

    platform_print("  programs: ");
    platform_print(user_program_count > 0 ? "ok\n" : "missing\n");

    platform_print("  entries:  ");
    platform_print(user_entry_count > 0 ? "ok\n" : "missing\n");

    platform_print("  segments: ");
    platform_print(user_segments_meta.prepared ? "ok\n" : "missing\n");

    platform_print("  stack:    ");
    platform_print(user_stack_meta.prepared ? "ok\n" : "missing\n");

    platform_print("  frame:    ");
    platform_print(user_entry_meta.prepared ? "ok\n" : "missing\n");

    platform_print("  boundary: ");
    platform_print(user_boundary_meta.checked ? "ok\n" : "missing\n");

    platform_print("  ring3:    disabled\n");

    platform_print("  result:   ");
    platform_print(user_doctor_ok() ? "ok\n" : "broken\n");
}

void user_programs(void) {
    platform_print("User programs:\n");

    for (int i = 0; i < USER_PROGRAM_MAX; i++) {
        platform_print("  ");
        platform_print_uint(user_programs_table[i].id);
        platform_print("    ");
        platform_print(user_programs_table[i].name);
        platform_print("    ");
        platform_print(user_programs_table[i].state);
        platform_print("    entry ");
        platform_print_hex32(user_programs_table[i].entry);
        platform_print("\n");
    }
}

void user_entries(void) {
    platform_print("User entries:\n");

    for (int i = 0; i < USER_PROGRAM_MAX; i++) {
        if (user_programs_table[i].state[0] == 'm') {
            platform_print("  ");
            platform_print_uint(user_programs_table[i].id);
            platform_print("    ");
            platform_print(user_programs_table[i].name);
            platform_print("    entry ");
            platform_print_hex32(user_programs_table[i].entry);
            platform_print("\n");
        }
    }
}

void user_segments(void) {
    platform_print("User segments:\n");

    platform_print("  kernel cs: ");
    platform_print_hex32(user_segments_meta.kernel_code);
    platform_print("\n");

    platform_print("  kernel ds: ");
    platform_print_hex32(user_segments_meta.kernel_data);
    platform_print("\n");

    platform_print("  user cs:   ");
    platform_print_hex32(user_segments_meta.user_code);
    platform_print("\n");

    platform_print("  user ds:   ");
    platform_print_hex32(user_segments_meta.user_data);
    platform_print("\n");

    platform_print("  prepared:  ");
    platform_print(user_segments_meta.prepared ? "yes\n" : "no\n");
}

void user_stack(void) {
    platform_print("User stack:\n");

    platform_print("  top:      ");
    platform_print_hex32(user_stack_meta.top);
    platform_print("\n");

    platform_print("  bottom:   ");
    platform_print_hex32(user_stack_meta.bottom);
    platform_print("\n");

    platform_print("  size:     ");
    platform_print_uint(user_stack_meta.size);
    platform_print("\n");

    platform_print("  prepared: ");
    platform_print(user_stack_meta.prepared ? "yes\n" : "no\n");

    platform_print("  broken:   ");
    platform_print(user_stack_broken ? "yes\n" : "no\n");

    platform_print("  result:   ");
    platform_print(user_stack_ok() ? "ok\n" : "broken\n");
}

void user_stack_check(void) {
    platform_print("User stack check:\n");

    platform_print("  prepared: ");
    platform_print(user_stack_meta.prepared ? "ok\n" : "missing\n");

    platform_print("  top align:");
    platform_print(user_is_page_aligned(user_stack_meta.top) ? "ok\n" : "bad\n");

    platform_print("  bottom align:");
    platform_print(user_is_page_aligned(user_stack_meta.bottom) ? "ok\n" : "bad\n");

    platform_print("  range:    ");
    platform_print(user_stack_meta.bottom < user_stack_meta.top ? "ok\n" : "bad\n");

    platform_print("  size:     ");
    platform_print((user_stack_meta.top - user_stack_meta.bottom) == user_stack_meta.size ? "ok\n" : "bad\n");

    platform_print("  mapped:   ");
    platform_print((user_stack_meta.top <= 0x003FFFFF && user_stack_meta.bottom >= 0x00001000) ? "identity\n" : "bad\n");

    platform_print("  broken:   ");
    platform_print(user_stack_broken ? "yes\n" : "no\n");

    platform_print("  result:   ");
    platform_print(user_stack_ok() ? "ok\n" : "broken\n");
}

void user_stack_break(void) {
    user_stack_broken = 1;
    user_stack_meta.prepared = 0;

    platform_print("user stack broken.\n");
}

void user_stack_fix(void) {
    user_stack_broken = 0;
    user_stack_prepare();
    user_entry_frame_prepare();

    platform_print("user stack fixed.\n");
}

void user_frame(void) {
    platform_print("User entry frame:\n");

    platform_print("  entry:    ");
    platform_print_hex32(user_entry_meta.entry);
    platform_print("\n");

    platform_print("  stack:    ");
    platform_print_hex32(user_entry_meta.user_stack);
    platform_print("\n");

    platform_print("  user cs:  ");
    platform_print_hex32(user_entry_meta.user_code);
    platform_print("\n");

    platform_print("  user ds:  ");
    platform_print_hex32(user_entry_meta.user_data);
    platform_print("\n");

    platform_print("  eflags:   ");
    platform_print_hex32(user_entry_meta.eflags);
    platform_print("\n");

    platform_print("  prepared: ");
    platform_print(user_entry_meta.prepared ? "yes\n" : "no\n");

    platform_print("  broken:   ");
    platform_print(user_frame_broken ? "yes\n" : "no\n");

    platform_print("  result:   ");
    platform_print(user_frame_ok() ? "ok\n" : "broken\n");
}

void user_frame_check(void) {
    platform_print("User frame check:\n");

    platform_print("  prepared: ");
    platform_print(user_entry_meta.prepared ? "ok\n" : "missing\n");

    platform_print("  stack top:");
    platform_print(user_entry_meta.user_stack == user_stack_meta.top ? "ok\n" : "bad\n");

    platform_print("  stack align:");
    platform_print(user_is_page_aligned(user_entry_meta.user_stack) ? "ok\n" : "bad\n");

    platform_print("  bottom align:");
    platform_print(user_is_page_aligned(user_stack_meta.bottom) ? "ok\n" : "bad\n");

    platform_print("  user cs:  ");
    platform_print(user_entry_meta.user_code == user_segments_meta.user_code ? "ok\n" : "bad\n");

    platform_print("  user ds:  ");
    platform_print(user_entry_meta.user_data == user_segments_meta.user_data ? "ok\n" : "bad\n");

    platform_print("  cs ring:  ");
    platform_print(user_selector_is_ring3(user_entry_meta.user_code) ? "ring3\n" : "bad\n");

    platform_print("  ds ring:  ");
    platform_print(user_selector_is_ring3(user_entry_meta.user_data) ? "ring3\n" : "bad\n");

    platform_print("  eflags IF:");
    platform_print((user_entry_meta.eflags & USER_EFLAGS_IF) ? "on\n" : "off\n");

    platform_print("  broken:   ");
    platform_print(user_frame_broken ? "yes\n" : "no\n");

    platform_print("  result:   ");
    platform_print(user_frame_ok() ? "ok\n" : "broken\n");
}

void user_frame_break(void) {
    user_frame_broken = 1;
    user_entry_meta.prepared = 0;

    platform_print("user entry frame broken.\n");
}

void user_frame_fix(void) {
    user_frame_broken = 0;
    user_entry_frame_prepare();

    platform_print("user entry frame fixed.\n");
}

void user_boundary(void) {
    platform_print("User/kernel boundary:\n");

    platform_print("  ring3:     ");
    platform_print(user_boundary_meta.ring3_disabled ? "disabled\n" : "enabled\n");

    platform_print("  syscall:   ");
    platform_print(user_boundary_meta.syscall_ready ? "ready\n" : "missing\n");

    platform_print("  paging:    ");
    platform_print(user_boundary_meta.paging_required ? "required\n" : "not-required\n");

    platform_print("  checked:   ");
    platform_print(user_boundary_meta.checked ? "yes\n" : "no\n");

    platform_print("  broken:    ");
    platform_print(user_boundary_broken ? "yes\n" : "no\n");

    platform_print("  user cs:   ");
    platform_print(user_selector_is_ring3(user_segments_meta.user_code) ? "ring3\n" : "bad\n");

    platform_print("  user ds:   ");
    platform_print(user_selector_is_ring3(user_segments_meta.user_data) ? "ring3\n" : "bad\n");

    platform_print("  stack:     ");
    platform_print((user_stack_meta.prepared && user_stack_meta.bottom < user_stack_meta.top) ? "ok\n" : "bad\n");

    platform_print("  frame:     ");
    platform_print(user_entry_meta.prepared ? "ok\n" : "bad\n");

    platform_print("  result:    ");
    platform_print(user_boundary_ok() ? "ok\n" : "broken\n");
}

void user_boundary_check(void) {
    platform_print("User boundary check:\n");

    platform_print("  user cs ring: ");
    platform_print(user_selector_is_ring3(user_segments_meta.user_code) ? "ok\n" : "bad\n");

    platform_print("  user ds ring: ");
    platform_print(user_selector_is_ring3(user_segments_meta.user_data) ? "ok\n" : "bad\n");

    platform_print("  stack range:  ");
    platform_print((user_stack_meta.prepared && user_stack_meta.bottom < user_stack_meta.top) ? "ok\n" : "bad\n");

    platform_print("  entry frame:  ");
    platform_print(user_entry_meta.prepared ? "ok\n" : "bad\n");

    platform_print("  syscall:      ");
    platform_print(user_boundary_meta.syscall_ready ? "ok\n" : "bad\n");

    platform_print("  paging:       ");
    platform_print(user_boundary_meta.paging_required ? "required\n" : "bad\n");

    platform_print("  broken:       ");
    platform_print(user_boundary_broken ? "yes\n" : "no\n");

    platform_print("  result:       ");
    platform_print(user_boundary_ok() ? "ok\n" : "broken\n");
}

void user_boundary_break(void) {
    user_boundary_broken = 1;
    user_boundary_meta.checked = 0;

    platform_print("user boundary broken.\n");
}

void user_boundary_fix(void) {
    user_boundary_broken = 0;
    user_frame_broken = 0;
    user_stack_broken = 0;

    user_segments_prepare();
    user_stack_prepare();
    user_entry_frame_prepare();
    user_boundary_prepare();

    platform_print("user boundary fixed.\n");
}

void user_prepare(void) {
    user_boundary_broken = 0;
    user_frame_broken = 0;
    user_stack_broken = 0;

    user_segments_prepare();
    user_stack_prepare();
    user_entry_frame_prepare();
    user_boundary_prepare();
    user_prepare_count++;

    platform_print("user mode preparation refreshed.\n");
}

void user_stats(void) {
    platform_print("User stats:\n");

    platform_print("  programs: ");
    platform_print_uint(user_program_count);
    platform_print("\n");

    platform_print("  entries:  ");
    platform_print_uint(user_entry_count);
    platform_print("\n");

    platform_print("  checks:   ");
    platform_print_uint(user_metadata_checks);
    platform_print("\n");

    platform_print("  prepares: ");
    platform_print_uint(user_prepare_count);
    platform_print("\n");

    platform_print("  ring3:    disabled\n");
}

void user_doctor(void) {
    platform_print("User mode doctor:\n");

    platform_print("  init:      ");
    platform_print(user_init_done ? "ok\n" : "missing\n");

    platform_print("  ready:     ");
    platform_print(user_ready ? "ok\n" : "bad\n");

    platform_print("  broken:    ");
    platform_print(user_broken ? "yes\n" : "no\n");

    platform_print("  programs:  ");
    platform_print_uint(user_program_count);
    platform_print("\n");

    platform_print("  entries:   ");
    platform_print_uint(user_entry_count);
    platform_print("\n");

    platform_print("  segments:  ");
    platform_print(user_segments_meta.prepared ? "ok\n" : "missing\n");

    platform_print("  stack:     ");
    platform_print(user_stack_ok() ? "ok\n" : "broken\n");

    platform_print("  frame:     ");
    platform_print(user_frame_ok() ? "ok\n" : "broken\n");

    platform_print("  boundary:  ");
    platform_print(user_boundary_ok() ? "ok\n" : "broken\n");

    platform_print("  checks:    ");
    platform_print_uint(user_metadata_checks);
    platform_print("\n");

    platform_print("  prepares:  ");
    platform_print_uint(user_prepare_count);
    platform_print("\n");

    platform_print("  ring3:     disabled\n");
    platform_print("  result:    ");
    platform_print(user_doctor_ok() ? "ok\n" : "broken\n");
}

void user_break(void) {
    user_broken = 1;
    user_ready = 0;

    platform_print("user mode layer broken.\n");
}

void user_fix(void) {
    user_init_done = 1;
    user_ready = 1;
    user_broken = 0;
    user_boundary_broken = 0;

    if (user_program_count == 0 || user_entry_count == 0) {
        user_programs_reset();
    }

    user_segments_prepare();
    user_stack_prepare();
    user_entry_frame_prepare();
    user_boundary_prepare();

    platform_print("user mode layer fixed.\n");
}