#include "bootinfo.h"
#include "platform.h"

extern unsigned int boot_multiboot_magic;
extern unsigned int boot_multiboot_info_addr;

extern unsigned int boot_probe_magic_initial;
extern unsigned int boot_probe_info_initial;
extern unsigned int boot_probe_magic_readback_before_stack;
extern unsigned int boot_probe_info_readback_before_stack;
extern unsigned int boot_probe_magic_after_stack;
extern unsigned int boot_probe_info_after_stack;
extern unsigned int boot_probe_stack_top_value;

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002
#define MULTIBOOT_INFO_FRAMEBUFFER 0x00001000

#define LINGJING_REQUESTED_FB_WIDTH 800
#define LINGJING_REQUESTED_FB_HEIGHT 600
#define LINGJING_REQUESTED_FB_BPP 32

typedef struct multiboot_info {
    unsigned int flags;
    unsigned int mem_lower;
    unsigned int mem_upper;
    unsigned int boot_device;
    unsigned int cmdline;
    unsigned int mods_count;
    unsigned int mods_addr;

    unsigned int syms0;
    unsigned int syms1;
    unsigned int syms2;
    unsigned int syms3;

    unsigned int mmap_length;
    unsigned int mmap_addr;

    unsigned int drives_length;
    unsigned int drives_addr;

    unsigned int config_table;
    unsigned int boot_loader_name;
    unsigned int apm_table;

    unsigned int vbe_control_info;
    unsigned int vbe_mode_info;
    unsigned short vbe_mode;
    unsigned short vbe_interface_seg;
    unsigned short vbe_interface_off;
    unsigned short vbe_interface_len;

    unsigned int framebuffer_addr_low;
    unsigned int framebuffer_addr_high;
    unsigned int framebuffer_pitch;
    unsigned int framebuffer_width;
    unsigned int framebuffer_height;
    unsigned char framebuffer_bpp;
    unsigned char framebuffer_type;
    unsigned short reserved;
} multiboot_info_t;

static unsigned int bootinfo_magic = 0;
static unsigned int bootinfo_expected_magic = MULTIBOOT_BOOTLOADER_MAGIC;
static unsigned int bootinfo_info_addr = 0;
static unsigned int bootinfo_flags = 0;
static int bootinfo_valid = 0;
static int bootinfo_magic_ok = 0;
static int bootinfo_magic_warning = 0;
static int bootinfo_framebuffer_present = 0;

static unsigned int bootinfo_fb_addr_low = 0;
static unsigned int bootinfo_fb_addr_high = 0;
static unsigned int bootinfo_fb_pitch = 0;
static unsigned int bootinfo_fb_width = 0;
static unsigned int bootinfo_fb_height = 0;
static unsigned int bootinfo_fb_bpp = 0;
static unsigned int bootinfo_fb_type = 0;

void bootinfo_init(unsigned int magic, unsigned int info_addr) {
    multiboot_info_t* mbi = 0;

    bootinfo_magic = magic;
    bootinfo_expected_magic = MULTIBOOT_BOOTLOADER_MAGIC;
    bootinfo_info_addr = info_addr;
    bootinfo_flags = 0;
    bootinfo_valid = 0;
    bootinfo_magic_ok = 0;
    bootinfo_magic_warning = 0;
    bootinfo_framebuffer_present = 0;

    bootinfo_fb_addr_low = 0;
    bootinfo_fb_addr_high = 0;
    bootinfo_fb_pitch = 0;
    bootinfo_fb_width = 0;
    bootinfo_fb_height = 0;
    bootinfo_fb_bpp = 0;
    bootinfo_fb_type = 0;

    if (magic == MULTIBOOT_BOOTLOADER_MAGIC) {
        bootinfo_magic_ok = 1;
    } else {
        bootinfo_magic_warning = 1;
    }

    if (info_addr == 0) {
        bootinfo_valid = 0;
        return;
    }

    mbi = (multiboot_info_t*)info_addr;

    bootinfo_valid = 1;
    bootinfo_flags = mbi->flags;

    if ((mbi->flags & MULTIBOOT_INFO_FRAMEBUFFER) != 0) {
        bootinfo_framebuffer_present = 1;

        bootinfo_fb_addr_low = mbi->framebuffer_addr_low;
        bootinfo_fb_addr_high = mbi->framebuffer_addr_high;
        bootinfo_fb_pitch = mbi->framebuffer_pitch;
        bootinfo_fb_width = mbi->framebuffer_width;
        bootinfo_fb_height = mbi->framebuffer_height;
        bootinfo_fb_bpp = (unsigned int)mbi->framebuffer_bpp;
        bootinfo_fb_type = (unsigned int)mbi->framebuffer_type;
    }
}

void bootinfo_status(void) {
    platform_print("Bootinfo status:\n");

    platform_print("  magic:      ");
    platform_print_hex32(bootinfo_magic);
    platform_print("\n");

    platform_print("  expected:   ");
    platform_print_hex32(bootinfo_expected_magic);
    platform_print("\n");

    platform_print("  magic ok:   ");
    platform_print(bootinfo_magic_ok ? "yes\n" : "no\n");

    platform_print("  warning:    ");
    platform_print(bootinfo_magic_warning ? "magic mismatch\n" : "none\n");

    platform_print("  info addr:  ");
    platform_print_hex32(bootinfo_info_addr);
    platform_print("\n");

    platform_print("  valid:      ");
    platform_print(bootinfo_valid ? "yes\n" : "no\n");

    platform_print("  flags:      ");
    platform_print_hex32(bootinfo_flags);
    platform_print("\n");

    platform_print("  framebuffer:");
    platform_print(bootinfo_framebuffer_present ? " yes\n" : " no\n");
}

void bootinfo_framebuffer(void) {
    platform_print("Bootinfo framebuffer:\n");

    platform_print("  requested width:  ");
    platform_print_uint(LINGJING_REQUESTED_FB_WIDTH);
    platform_print("\n");

    platform_print("  requested height: ");
    platform_print_uint(LINGJING_REQUESTED_FB_HEIGHT);
    platform_print("\n");

    platform_print("  requested bpp:    ");
    platform_print_uint(LINGJING_REQUESTED_FB_BPP);
    platform_print("\n");

    platform_print("  present:          ");
    platform_print(bootinfo_framebuffer_present ? "yes\n" : "no\n");

    platform_print("  addr low:         ");
    platform_print_hex32(bootinfo_fb_addr_low);
    platform_print("\n");

    platform_print("  addr high:        ");
    platform_print_hex32(bootinfo_fb_addr_high);
    platform_print("\n");

    platform_print("  width:            ");
    platform_print_uint(bootinfo_fb_width);
    platform_print("\n");

    platform_print("  height:           ");
    platform_print_uint(bootinfo_fb_height);
    platform_print("\n");

    platform_print("  bpp:              ");
    platform_print_uint(bootinfo_fb_bpp);
    platform_print("\n");

    platform_print("  pitch:            ");
    platform_print_uint(bootinfo_fb_pitch);
    platform_print("\n");

    platform_print("  type:             ");
    platform_print_uint(bootinfo_fb_type);
    platform_print("\n");

    platform_print("  type name:        ");

    if (bootinfo_fb_type == 0) {
        platform_print("indexed-color\n");
    } else if (bootinfo_fb_type == 1) {
        platform_print("rgb-graphics\n");
    } else if (bootinfo_fb_type == 2) {
        platform_print("text-mode\n");
    } else {
        platform_print("unknown\n");
    }

    platform_print("  actual mode:      ");

    if (bootinfo_fb_width == LINGJING_REQUESTED_FB_WIDTH &&
        bootinfo_fb_height == LINGJING_REQUESTED_FB_HEIGHT &&
        bootinfo_fb_bpp == LINGJING_REQUESTED_FB_BPP) {
        platform_print("requested\n");
    } else {
        platform_print("different\n");
    }
}

void bootinfo_check(void) {
    platform_print("Bootinfo check:\n");

    platform_print("  magic:    ");
    platform_print(bootinfo_magic_ok ? "ok\n" : "warning\n");

    platform_print("  expected: ");
    platform_print_hex32(bootinfo_expected_magic);
    platform_print("\n");

    platform_print("  actual:   ");
    platform_print_hex32(bootinfo_magic);
    platform_print("\n");

    platform_print("  info addr:");
    platform_print(bootinfo_info_addr != 0 ? " ok\n" : " bad\n");

    platform_print("  fb info:  ");
    platform_print(bootinfo_framebuffer_present ? "present\n" : "not present\n");

    platform_print("  result:   ");

    if (!bootinfo_valid) {
        platform_print("blocked\n");
        return;
    }

    platform_print("ready\n");
}

void bootinfo_doctor(void) {
    platform_print("Bootinfo doctor:\n");

    platform_print("  multiboot magic: ");
    platform_print(bootinfo_magic_ok ? "valid\n" : "warning\n");

    platform_print("  expected magic:  ");
    platform_print_hex32(bootinfo_expected_magic);
    platform_print("\n");

    platform_print("  actual magic:    ");
    platform_print_hex32(bootinfo_magic);
    platform_print("\n");

    platform_print("  info structure:  ");
    platform_print(bootinfo_info_addr != 0 ? "present\n" : "missing\n");

    platform_print("  framebuffer:     ");
    platform_print(bootinfo_framebuffer_present ? "reported\n" : "not reported\n");

    platform_print("  requested mode:  ");
    platform_print_uint(LINGJING_REQUESTED_FB_WIDTH);
    platform_print("x");
    platform_print_uint(LINGJING_REQUESTED_FB_HEIGHT);
    platform_print("x");
    platform_print_uint(LINGJING_REQUESTED_FB_BPP);
    platform_print("\n");

    platform_print("  actual mode:     ");
    platform_print_uint(bootinfo_fb_width);
    platform_print("x");
    platform_print_uint(bootinfo_fb_height);
    platform_print("x");
    platform_print_uint(bootinfo_fb_bpp);
    platform_print("\n");

    platform_print("  fb type:         ");

    if (bootinfo_fb_type == 0) {
        platform_print("indexed-color\n");
    } else if (bootinfo_fb_type == 1) {
        platform_print("rgb-graphics\n");
    } else if (bootinfo_fb_type == 2) {
        platform_print("text-mode\n");
    } else {
        platform_print("unknown\n");
    }

    platform_print("  mode:            relaxed-validation + graphics-request\n");

    platform_print("  result:          ");

    if (!bootinfo_valid) {
        platform_print("blocked\n");
        return;
    }

    platform_print("ready\n");
}

void bootinfo_probe(void) {
    platform_print("Boot handoff probe:\n");

    platform_print("  expected magic:          ");
    platform_print_hex32(MULTIBOOT_BOOTLOADER_MAGIC);
    platform_print("\n");

    platform_print("  asm eax initial:         ");
    platform_print_hex32(boot_probe_magic_initial);
    platform_print("\n");

    platform_print("  asm ebx initial:         ");
    platform_print_hex32(boot_probe_info_initial);
    platform_print("\n");

    platform_print("  asm magic readback pre:  ");
    platform_print_hex32(boot_probe_magic_readback_before_stack);
    platform_print("\n");

    platform_print("  asm info readback pre:   ");
    platform_print_hex32(boot_probe_info_readback_before_stack);
    platform_print("\n");

    platform_print("  asm eax after stack:     ");
    platform_print_hex32(boot_probe_magic_after_stack);
    platform_print("\n");

    platform_print("  asm ebx after stack:     ");
    platform_print_hex32(boot_probe_info_after_stack);
    platform_print("\n");

    platform_print("  asm stack top:           ");
    platform_print_hex32(boot_probe_stack_top_value);
    platform_print("\n");

    platform_print("  c boot magic variable:   ");
    platform_print_hex32(boot_multiboot_magic);
    platform_print("\n");

    platform_print("  c boot info variable:    ");
    platform_print_hex32(boot_multiboot_info_addr);
    platform_print("\n");

    platform_print("  bootinfo magic stored:   ");
    platform_print_hex32(bootinfo_magic);
    platform_print("\n");

    platform_print("  bootinfo info stored:    ");
    platform_print_hex32(bootinfo_info_addr);
    platform_print("\n");

    platform_print("  result:                  ");

    if (boot_probe_magic_initial == MULTIBOOT_BOOTLOADER_MAGIC) {
        platform_print("raw magic ok\n");
        return;
    }

    platform_print("raw magic mismatch\n");
}

int bootinfo_is_multiboot_valid(void) {
    return bootinfo_valid;
}

int bootinfo_has_framebuffer(void) {
    return bootinfo_framebuffer_present;
}

unsigned int bootinfo_get_magic(void) {
    return bootinfo_magic;
}

unsigned int bootinfo_get_info_addr(void) {
    return bootinfo_info_addr;
}

unsigned int bootinfo_get_flags(void) {
    return bootinfo_flags;
}

unsigned int bootinfo_get_framebuffer_addr_low(void) {
    return bootinfo_fb_addr_low;
}

unsigned int bootinfo_get_framebuffer_addr_high(void) {
    return bootinfo_fb_addr_high;
}

unsigned int bootinfo_get_framebuffer_pitch(void) {
    return bootinfo_fb_pitch;
}

unsigned int bootinfo_get_framebuffer_width(void) {
    return bootinfo_fb_width;
}

unsigned int bootinfo_get_framebuffer_height(void) {
    return bootinfo_fb_height;
}

unsigned int bootinfo_get_framebuffer_bpp(void) {
    return bootinfo_fb_bpp;
}

unsigned int bootinfo_get_framebuffer_type(void) {
    return bootinfo_fb_type;
}
unsigned int bootinfo_get_requested_width(void) {
    return LINGJING_REQUESTED_FB_WIDTH;
}

unsigned int bootinfo_get_requested_height(void) {
    return LINGJING_REQUESTED_FB_HEIGHT;
}

unsigned int bootinfo_get_requested_bpp(void) {
    return LINGJING_REQUESTED_FB_BPP;
}