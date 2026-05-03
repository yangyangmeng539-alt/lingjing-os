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

#define MULTIBOOT1_BOOTLOADER_MAGIC 0x2BADB002
#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36D76289

#define MULTIBOOT1_INFO_VBE 0x00000800
#define MULTIBOOT1_INFO_FRAMEBUFFER 0x00001000

#define MULTIBOOT2_TAG_TYPE_END 0
#define MULTIBOOT2_TAG_TYPE_FRAMEBUFFER 8

#define LINGJING_REQUESTED_FB_WIDTH 800
#define LINGJING_REQUESTED_FB_HEIGHT 600
#define LINGJING_REQUESTED_FB_BPP 32

typedef struct multiboot1_info {
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
} multiboot1_info_t;

typedef struct multiboot2_info_header {
    unsigned int total_size;
    unsigned int reserved;
} multiboot2_info_header_t;

typedef struct multiboot2_tag {
    unsigned int type;
    unsigned int size;
} multiboot2_tag_t;

typedef struct multiboot2_framebuffer_tag {
    unsigned int type;
    unsigned int size;
    unsigned int framebuffer_addr_low;
    unsigned int framebuffer_addr_high;
    unsigned int framebuffer_pitch;
    unsigned int framebuffer_width;
    unsigned int framebuffer_height;
    unsigned char framebuffer_bpp;
    unsigned char framebuffer_type;
    unsigned short reserved;
} multiboot2_framebuffer_tag_t;

static unsigned int bootinfo_magic = 0;
static unsigned int bootinfo_expected_magic = MULTIBOOT1_BOOTLOADER_MAGIC;
static unsigned int bootinfo_info_addr = 0;
static unsigned int bootinfo_flags = 0;
static int bootinfo_valid = 0;
static int bootinfo_magic_ok = 0;
static int bootinfo_magic_warning = 0;
static int bootinfo_framebuffer_present = 0;
static const char* bootinfo_protocol = "unknown";
static unsigned int bootinfo_mb2_total_size = 0;
static unsigned int bootinfo_mb2_reserved = 0;
static unsigned int bootinfo_mb2_tag_count = 0;
static unsigned int bootinfo_mb2_fb_tag_addr = 0;

static unsigned int bootinfo_fb_addr_low = 0;
static unsigned int bootinfo_fb_addr_high = 0;
static unsigned int bootinfo_fb_pitch = 0;
static unsigned int bootinfo_fb_width = 0;
static unsigned int bootinfo_fb_height = 0;
static unsigned int bootinfo_fb_bpp = 0;
static unsigned int bootinfo_fb_type = 0;

static unsigned int bootinfo_align8(unsigned int value) {
    return (value + 7U) & 0xFFFFFFF8U;
}

static void bootinfo_clear_framebuffer_cache(void) {
    bootinfo_framebuffer_present = 0;
    bootinfo_fb_addr_low = 0;
    bootinfo_fb_addr_high = 0;
    bootinfo_fb_pitch = 0;
    bootinfo_fb_width = 0;
    bootinfo_fb_height = 0;
    bootinfo_fb_bpp = 0;
    bootinfo_fb_type = 0;
}

static void bootinfo_cache_framebuffer(
    unsigned int addr_low,
    unsigned int addr_high,
    unsigned int pitch,
    unsigned int width,
    unsigned int height,
    unsigned int bpp,
    unsigned int type
) {
    bootinfo_framebuffer_present = 1;
    bootinfo_fb_addr_low = addr_low;
    bootinfo_fb_addr_high = addr_high;
    bootinfo_fb_pitch = pitch;
    bootinfo_fb_width = width;
    bootinfo_fb_height = height;
    bootinfo_fb_bpp = bpp;
    bootinfo_fb_type = type;
}

static void bootinfo_parse_multiboot1(unsigned int info_addr) {
    multiboot1_info_t* mbi = (multiboot1_info_t*)info_addr;

    bootinfo_valid = 1;
    bootinfo_protocol = "multiboot1";
    bootinfo_flags = mbi->flags;

    if ((mbi->flags & MULTIBOOT1_INFO_FRAMEBUFFER) != 0) {
        bootinfo_cache_framebuffer(
            mbi->framebuffer_addr_low,
            mbi->framebuffer_addr_high,
            mbi->framebuffer_pitch,
            mbi->framebuffer_width,
            mbi->framebuffer_height,
            (unsigned int)mbi->framebuffer_bpp,
            (unsigned int)mbi->framebuffer_type
        );
    }
}

static void bootinfo_parse_multiboot2(unsigned int info_addr) {
    multiboot2_info_header_t* header = (multiboot2_info_header_t*)info_addr;
    unsigned int cursor = 0;
    unsigned int end = 0;

    bootinfo_valid = 1;
    bootinfo_protocol = "multiboot2";
    bootinfo_flags = 0;
    bootinfo_mb2_total_size = header->total_size;
    bootinfo_mb2_reserved = header->reserved;
    bootinfo_mb2_tag_count = 0;
    bootinfo_mb2_fb_tag_addr = 0;

    if (header->total_size < 16U) {
        bootinfo_valid = 0;
        return;
    }

    cursor = info_addr + 8U;
    end = info_addr + header->total_size;

    while (cursor + 8U <= end) {
        multiboot2_tag_t* tag = (multiboot2_tag_t*)cursor;

        if (tag->size < 8U) {
            bootinfo_valid = 0;
            return;
        }

        bootinfo_mb2_tag_count++;

        if (tag->type == MULTIBOOT2_TAG_TYPE_END) {
            return;
        }

        if (tag->type == MULTIBOOT2_TAG_TYPE_FRAMEBUFFER && tag->size >= 32U) {
            multiboot2_framebuffer_tag_t* fb = (multiboot2_framebuffer_tag_t*)cursor;

            bootinfo_mb2_fb_tag_addr = cursor;
            bootinfo_cache_framebuffer(
                fb->framebuffer_addr_low,
                fb->framebuffer_addr_high,
                fb->framebuffer_pitch,
                fb->framebuffer_width,
                fb->framebuffer_height,
                (unsigned int)fb->framebuffer_bpp,
                (unsigned int)fb->framebuffer_type
            );
        }

        cursor += bootinfo_align8(tag->size);
    }
}

void bootinfo_init(unsigned int magic, unsigned int info_addr) {
    bootinfo_magic = magic;
    bootinfo_expected_magic = MULTIBOOT1_BOOTLOADER_MAGIC;
    bootinfo_info_addr = info_addr;
    bootinfo_flags = 0;
    bootinfo_valid = 0;
    bootinfo_magic_ok = 0;
    bootinfo_magic_warning = 0;
    bootinfo_protocol = "unknown";
    bootinfo_mb2_total_size = 0;
    bootinfo_mb2_reserved = 0;
    bootinfo_mb2_tag_count = 0;
    bootinfo_mb2_fb_tag_addr = 0;

    bootinfo_clear_framebuffer_cache();

    if (magic == MULTIBOOT1_BOOTLOADER_MAGIC) {
        bootinfo_expected_magic = MULTIBOOT1_BOOTLOADER_MAGIC;
        bootinfo_magic_ok = 1;
        bootinfo_protocol = "multiboot1";
    } else if (magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
        bootinfo_expected_magic = MULTIBOOT2_BOOTLOADER_MAGIC;
        bootinfo_magic_ok = 1;
        bootinfo_protocol = "multiboot2";
    } else {
        bootinfo_magic_warning = 1;
    }

    if (info_addr == 0) {
        bootinfo_valid = 0;
        return;
    }

    if (magic == MULTIBOOT1_BOOTLOADER_MAGIC) {
        bootinfo_parse_multiboot1(info_addr);
        return;
    }

    if (magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
        bootinfo_parse_multiboot2(info_addr);
        return;
    }

    bootinfo_valid = 0;
}

void bootinfo_status(void) {
    platform_print("Bootinfo status:\n");

    platform_print("  protocol:   ");
    platform_print(bootinfo_protocol);
    platform_print("\n");

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

    platform_print("  protocol:         ");
    platform_print(bootinfo_protocol);
    platform_print("\n");

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

static void bootinfo_raw_multiboot1(void) {
    multiboot1_info_t* mbi = (multiboot1_info_t*)bootinfo_info_addr;

    platform_print("Multiboot1 raw fields:\n");

    platform_print("  flag mem:    ");
    platform_print((bootinfo_flags & 0x00000001) ? "yes\n" : "no\n");

    platform_print("  flag bootdev:");
    platform_print((bootinfo_flags & 0x00000002) ? " yes\n" : " no\n");

    platform_print("  flag cmdline:");
    platform_print((bootinfo_flags & 0x00000004) ? " yes\n" : " no\n");

    platform_print("  flag mods:   ");
    platform_print((bootinfo_flags & 0x00000008) ? "yes\n" : "no\n");

    platform_print("  flag mmap:   ");
    platform_print((bootinfo_flags & 0x00000040) ? "yes\n" : "no\n");

    platform_print("  flag vbe:    ");
    platform_print((bootinfo_flags & MULTIBOOT1_INFO_VBE) ? "yes\n" : "no\n");

    platform_print("  flag fb:     ");
    platform_print((bootinfo_flags & MULTIBOOT1_INFO_FRAMEBUFFER) ? "yes\n" : "no\n");

    platform_print("Raw VBE fields:\n");

    platform_print("  vbe control: ");
    platform_print_hex32(mbi->vbe_control_info);
    platform_print("\n");

    platform_print("  vbe modeinfo: ");
    platform_print_hex32(mbi->vbe_mode_info);
    platform_print("\n");

    platform_print("  vbe mode:    ");
    platform_print_hex32((unsigned int)mbi->vbe_mode);
    platform_print("\n");

    platform_print("  vbe if seg:  ");
    platform_print_hex32((unsigned int)mbi->vbe_interface_seg);
    platform_print("\n");

    platform_print("  vbe if off:  ");
    platform_print_hex32((unsigned int)mbi->vbe_interface_off);
    platform_print("\n");

    platform_print("  vbe if len:  ");
    platform_print_uint((unsigned int)mbi->vbe_interface_len);
    platform_print("\n");

    platform_print("Raw framebuffer fields:\n");

    platform_print("  fb addr low: ");
    platform_print_hex32(mbi->framebuffer_addr_low);
    platform_print("\n");

    platform_print("  fb addr high: ");
    platform_print_hex32(mbi->framebuffer_addr_high);
    platform_print("\n");

    platform_print("  fb pitch:    ");
    platform_print_uint(mbi->framebuffer_pitch);
    platform_print("\n");

    platform_print("  fb width:    ");
    platform_print_uint(mbi->framebuffer_width);
    platform_print("\n");

    platform_print("  fb height:   ");
    platform_print_uint(mbi->framebuffer_height);
    platform_print("\n");

    platform_print("  fb bpp:      ");
    platform_print_uint((unsigned int)mbi->framebuffer_bpp);
    platform_print("\n");

    platform_print("  fb type:     ");
    platform_print_uint((unsigned int)mbi->framebuffer_type);
    platform_print("\n");

    platform_print("  fb reserved: ");
    platform_print_hex32((unsigned int)mbi->reserved);
    platform_print("\n");
}

static void bootinfo_raw_multiboot2(void) {
    unsigned int cursor = 0;
    unsigned int end = 0;
    unsigned int index = 0;

    platform_print("Multiboot2 raw fields:\n");

    platform_print("  total size:  ");
    platform_print_uint(bootinfo_mb2_total_size);
    platform_print("\n");

    platform_print("  reserved:    ");
    platform_print_hex32(bootinfo_mb2_reserved);
    platform_print("\n");

    platform_print("  tag count:   ");
    platform_print_uint(bootinfo_mb2_tag_count);
    platform_print("\n");

    platform_print("  fb tag addr: ");
    platform_print_hex32(bootinfo_mb2_fb_tag_addr);
    platform_print("\n");

    if (bootinfo_mb2_total_size < 16U) {
        platform_print("  result:      blocked\n");
        platform_print("  reason:      invalid multiboot2 total size\n");
        return;
    }

    cursor = bootinfo_info_addr + 8U;
    end = bootinfo_info_addr + bootinfo_mb2_total_size;

    platform_print("Multiboot2 tags:\n");

    while (cursor + 8U <= end) {
        multiboot2_tag_t* tag = (multiboot2_tag_t*)cursor;

        platform_print("  tag ");
        platform_print_uint(index);
        platform_print(": type=");
        platform_print_uint(tag->type);
        platform_print(" size=");
        platform_print_uint(tag->size);
        platform_print(" addr=");
        platform_print_hex32(cursor);
        platform_print("\n");

        if (tag->type == MULTIBOOT2_TAG_TYPE_FRAMEBUFFER && tag->size >= 32U) {
            multiboot2_framebuffer_tag_t* fb = (multiboot2_framebuffer_tag_t*)cursor;

            platform_print("    framebuffer addr low:  ");
            platform_print_hex32(fb->framebuffer_addr_low);
            platform_print("\n");

            platform_print("    framebuffer addr high: ");
            platform_print_hex32(fb->framebuffer_addr_high);
            platform_print("\n");

            platform_print("    framebuffer pitch:     ");
            platform_print_uint(fb->framebuffer_pitch);
            platform_print("\n");

            platform_print("    framebuffer width:     ");
            platform_print_uint(fb->framebuffer_width);
            platform_print("\n");

            platform_print("    framebuffer height:    ");
            platform_print_uint(fb->framebuffer_height);
            platform_print("\n");

            platform_print("    framebuffer bpp:       ");
            platform_print_uint((unsigned int)fb->framebuffer_bpp);
            platform_print("\n");

            platform_print("    framebuffer type:      ");
            platform_print_uint((unsigned int)fb->framebuffer_type);
            platform_print("\n");
        }

        if (tag->type == MULTIBOOT2_TAG_TYPE_END) {
            return;
        }

        if (tag->size < 8U) {
            return;
        }

        cursor += bootinfo_align8(tag->size);
        index++;
    }
}

void bootinfo_raw(void) {
    platform_print("Bootinfo raw:\n");

    platform_print("  protocol:    ");
    platform_print(bootinfo_protocol);
    platform_print("\n");

    platform_print("  magic:       ");
    platform_print_hex32(bootinfo_magic);
    platform_print("\n");

    platform_print("  expected:    ");
    platform_print_hex32(bootinfo_expected_magic);
    platform_print("\n");

    platform_print("  magic ok:    ");
    platform_print(bootinfo_magic_ok ? "yes\n" : "no\n");

    platform_print("  info addr:   ");
    platform_print_hex32(bootinfo_info_addr);
    platform_print("\n");

    platform_print("  valid:       ");
    platform_print(bootinfo_valid ? "yes\n" : "no\n");

    platform_print("  flags:       ");
    platform_print_hex32(bootinfo_flags);
    platform_print("\n");

    if (!bootinfo_valid || bootinfo_info_addr == 0) {
        platform_print("  result:      blocked\n");
        platform_print("  reason:      multiboot info missing\n");
        return;
    }

    if (bootinfo_magic == MULTIBOOT1_BOOTLOADER_MAGIC) {
        bootinfo_raw_multiboot1();
    } else if (bootinfo_magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
        bootinfo_raw_multiboot2();
    } else {
        platform_print("  result:      blocked\n");
        platform_print("  reason:      unknown boot protocol\n");
        return;
    }

    platform_print("Cached framebuffer:\n");

    platform_print("  present:     ");
    platform_print(bootinfo_framebuffer_present ? "yes\n" : "no\n");

    platform_print("  addr low:    ");
    platform_print_hex32(bootinfo_fb_addr_low);
    platform_print("\n");

    platform_print("  addr high:   ");
    platform_print_hex32(bootinfo_fb_addr_high);
    platform_print("\n");

    platform_print("  width:       ");
    platform_print_uint(bootinfo_fb_width);
    platform_print("\n");

    platform_print("  height:      ");
    platform_print_uint(bootinfo_fb_height);
    platform_print("\n");

    platform_print("  bpp:         ");
    platform_print_uint(bootinfo_fb_bpp);
    platform_print("\n");

    platform_print("  pitch:       ");
    platform_print_uint(bootinfo_fb_pitch);
    platform_print("\n");

    platform_print("  type:        ");
    platform_print_uint(bootinfo_fb_type);
    platform_print("\n");

    platform_print("  result:      ready\n");
}

void bootinfo_check(void) {
    platform_print("Bootinfo check:\n");

    platform_print("  protocol: ");
    platform_print(bootinfo_protocol);
    platform_print("\n");

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

    platform_print("  protocol:        ");
    platform_print(bootinfo_protocol);
    platform_print("\n");

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

    platform_print("  mode:            multiboot1-safe + multiboot2-framebuffer-probe\n");

    platform_print("  result:          ");

    if (!bootinfo_valid) {
        platform_print("blocked\n");
        return;
    }

    platform_print("ready\n");
}

void bootinfo_probe(void) {
    platform_print("Boot handoff probe:\n");

    platform_print("  expected mb1 magic:      ");
    platform_print_hex32(MULTIBOOT1_BOOTLOADER_MAGIC);
    platform_print("\n");

    platform_print("  expected mb2 magic:      ");
    platform_print_hex32(MULTIBOOT2_BOOTLOADER_MAGIC);
    platform_print("\n");

    platform_print("  protocol:                ");
    platform_print(bootinfo_protocol);
    platform_print("\n");

    platform_print("  asm eax initial raw:     ");
    platform_print_hex32(boot_probe_magic_initial);
    platform_print("\n");

    platform_print("  asm ebx initial raw:     ");
    platform_print_hex32(boot_probe_info_initial);
    platform_print("\n");

    platform_print("  asm magic readback pre:  ");
    platform_print_hex32(boot_probe_magic_readback_before_stack);
    platform_print("\n");

    platform_print("  asm info readback pre:   ");
    platform_print_hex32(boot_probe_info_readback_before_stack);
    platform_print("\n");

    platform_print("  asm magic after stack:   ");
    platform_print_hex32(boot_probe_magic_after_stack);
    platform_print("\n");

    platform_print("  asm info after stack:    ");
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

    platform_print("  raw handoff:             ");

    if (boot_probe_magic_initial == MULTIBOOT1_BOOTLOADER_MAGIC) {
        platform_print("multiboot1 clean\n");
    } else if (boot_probe_magic_initial == MULTIBOOT2_BOOTLOADER_MAGIC) {
        platform_print("multiboot2 clean\n");
    } else if (boot_probe_magic_initial == 0x2BADB0FF) {
        platform_print("multiboot1 low-byte repaired\n");
    } else {
        platform_print("unknown raw magic\n");
    }

    platform_print("  normalized handoff:      ");

    if (bootinfo_magic == MULTIBOOT1_BOOTLOADER_MAGIC) {
        platform_print("multiboot1 ok\n");
    } else if (bootinfo_magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
        platform_print("multiboot2 ok\n");
    } else {
        platform_print("unknown\n");
    }

    platform_print("  result:                  ");

    if (bootinfo_valid && bootinfo_magic_ok) {
        platform_print("ready\n");
        return;
    }

    platform_print("blocked\n");
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