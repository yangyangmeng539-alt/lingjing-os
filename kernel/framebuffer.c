#include "framebuffer.h"
#include "platform.h"
#include "bootinfo.h"

static int framebuffer_initialized = 0;
static int framebuffer_prepared = 0;
static int framebuffer_broken = 0;

static unsigned int framebuffer_width = 0;
static unsigned int framebuffer_height = 0;
static unsigned int framebuffer_bpp = 0;
static unsigned int framebuffer_pitch = 0;
static unsigned int framebuffer_address = 0;
static unsigned int framebuffer_type = 0;

static const char* framebuffer_profile_name = "unknown";

static void framebuffer_update_profile(void) {
    framebuffer_profile_name = "unknown";

    if (framebuffer_type == 2 &&
        framebuffer_address == 0x000B8000 &&
        framebuffer_width == 80 &&
        framebuffer_height == 25 &&
        framebuffer_bpp == 16) {
        framebuffer_profile_name = "text-vga-80x25";
        return;
    }

    if (framebuffer_type == 1 &&
        framebuffer_width == 800 &&
        framebuffer_height == 600 &&
        framebuffer_bpp == 32 &&
        framebuffer_address != 0 &&
        framebuffer_address != 0x000B8000) {
        framebuffer_profile_name = "graphics-rgb-800x600x32";
        return;
    }

    if (framebuffer_type == 1) {
        framebuffer_profile_name = "graphics-rgb";
        return;
    }

    if (framebuffer_type == 0) {
        framebuffer_profile_name = "graphics-indexed";
        return;
    }

    if (framebuffer_type == 2) {
        framebuffer_profile_name = "text-mode";
        return;
    }
}

void framebuffer_init(void) {
    framebuffer_initialized = 1;
    framebuffer_prepared = 0;
    framebuffer_broken = 0;

    framebuffer_width = 0;
    framebuffer_height = 0;
    framebuffer_bpp = 0;
    framebuffer_pitch = 0;
    framebuffer_address = 0;
    framebuffer_type = 0;
    framebuffer_profile_name = "unknown";
}

void framebuffer_prepare(void) {
    if (bootinfo_has_framebuffer()) {
        framebuffer_prepare_from_bootinfo();
        return;
    }

    framebuffer_initialized = 1;
    framebuffer_prepared = 1;
    framebuffer_broken = 0;

    framebuffer_width = 640;
    framebuffer_height = 480;
    framebuffer_bpp = 32;
    framebuffer_pitch = framebuffer_width * 4;
    framebuffer_address = 0;
    framebuffer_type = 0;

    framebuffer_update_profile();

    platform_print("Framebuffer prepare:\n");

    platform_print("  source:   fallback metadata\n");
    platform_print("  mode:     metadata-only\n");

    platform_print("  width:    ");
    platform_print_uint(framebuffer_width);
    platform_print("\n");

    platform_print("  height:   ");
    platform_print_uint(framebuffer_height);
    platform_print("\n");

    platform_print("  bpp:      ");
    platform_print_uint(framebuffer_bpp);
    platform_print("\n");

    platform_print("  pitch:    ");
    platform_print_uint(framebuffer_pitch);
    platform_print("\n");

    platform_print("  address:  ");
    platform_print_hex32(framebuffer_address);
    platform_print("\n");

    platform_print("  type:     ");
    platform_print_uint(framebuffer_type);
    platform_print("\n");

    platform_print("  profile:  ");
    platform_print(framebuffer_profile_name);
    platform_print("\n");

    platform_print("  result:   prepared\n");
}

void framebuffer_prepare_from_bootinfo(void) {
    framebuffer_initialized = 1;
    framebuffer_prepared = 1;
    framebuffer_broken = 0;

    framebuffer_width = bootinfo_get_framebuffer_width();
    framebuffer_height = bootinfo_get_framebuffer_height();
    framebuffer_bpp = bootinfo_get_framebuffer_bpp();
    framebuffer_pitch = bootinfo_get_framebuffer_pitch();
    framebuffer_address = bootinfo_get_framebuffer_addr_low();
    framebuffer_type = bootinfo_get_framebuffer_type();

    framebuffer_update_profile();

    platform_print("Framebuffer prepare from bootinfo:\n");

    platform_print("  source:   multiboot framebuffer info\n");

    platform_print("  width:    ");
    platform_print_uint(framebuffer_width);
    platform_print("\n");

    platform_print("  height:   ");
    platform_print_uint(framebuffer_height);
    platform_print("\n");

    platform_print("  bpp:      ");
    platform_print_uint(framebuffer_bpp);
    platform_print("\n");

    platform_print("  pitch:    ");
    platform_print_uint(framebuffer_pitch);
    platform_print("\n");

    platform_print("  address:  ");
    platform_print_hex32(framebuffer_address);
    platform_print("\n");

    platform_print("  addr hi:  ");
    platform_print_hex32(bootinfo_get_framebuffer_addr_high());
    platform_print("\n");

    platform_print("  type:     ");
    platform_print_uint(framebuffer_type);
    platform_print("\n");

    platform_print("  profile:  ");
    platform_print(framebuffer_profile_name);
    platform_print("\n");

    platform_print("  result:   prepared\n");
}

void framebuffer_prepare_silent(void) {
    framebuffer_initialized = 1;
    framebuffer_prepared = 1;
    framebuffer_broken = 0;

    if (bootinfo_has_framebuffer()) {
        framebuffer_width = bootinfo_get_framebuffer_width();
        framebuffer_height = bootinfo_get_framebuffer_height();
        framebuffer_bpp = bootinfo_get_framebuffer_bpp();
        framebuffer_pitch = bootinfo_get_framebuffer_pitch();
        framebuffer_address = bootinfo_get_framebuffer_addr_low();
        framebuffer_type = bootinfo_get_framebuffer_type();

        framebuffer_update_profile();
        return;
    }

    framebuffer_width = 640;
    framebuffer_height = 480;
    framebuffer_bpp = 32;
    framebuffer_pitch = framebuffer_width * 4;
    framebuffer_address = 0;
    framebuffer_type = 0;

    framebuffer_update_profile();
}

void framebuffer_status(void) {
    platform_print("Framebuffer status:\n");

    platform_print("  initialized: ");
    platform_print(framebuffer_initialized ? "yes\n" : "no\n");

    platform_print("  prepared:    ");
    platform_print(framebuffer_prepared ? "yes\n" : "no\n");

    platform_print("  broken:      ");
    platform_print(framebuffer_broken ? "yes\n" : "no\n");

    platform_print("  mode:        metadata-only\n");

    platform_print("  profile:     ");
    platform_print(framebuffer_profile_name);
    platform_print("\n");

    platform_print("  width:       ");
    platform_print_uint(framebuffer_width);
    platform_print("\n");

    platform_print("  height:      ");
    platform_print_uint(framebuffer_height);
    platform_print("\n");

    platform_print("  bpp:         ");
    platform_print_uint(framebuffer_bpp);
    platform_print("\n");

    platform_print("  pitch:       ");
    platform_print_uint(framebuffer_pitch);
    platform_print("\n");

    platform_print("  address:     ");
    platform_print_hex32(framebuffer_address);
    platform_print("\n");

    platform_print("  type:        ");
    platform_print_uint(framebuffer_type);
    platform_print("\n");

    platform_print("  type name:   ");

    if (framebuffer_type == 0) {
        platform_print("indexed-color\n");
    } else if (framebuffer_type == 1) {
        platform_print("rgb-graphics\n");
    } else if (framebuffer_type == 2) {
        platform_print("text-mode\n");
    } else {
        platform_print("unknown\n");
    }
}

void framebuffer_check(void) {
    platform_print("Framebuffer check:\n");

    platform_print("  initialized: ");
    platform_print(framebuffer_initialized ? "ok\n" : "bad\n");

    platform_print("  prepared:    ");
    platform_print(framebuffer_prepared ? "ok\n" : "not prepared\n");

    platform_print("  metadata:    ");

    if (framebuffer_width == 0 || framebuffer_height == 0 || framebuffer_bpp == 0 || framebuffer_pitch == 0) {
        platform_print("empty\n");
    } else {
        platform_print("ready\n");
    }

    platform_print("  broken:      ");
    platform_print(framebuffer_broken ? "yes\n" : "no\n");

    platform_print("  result:      ");

    if (framebuffer_broken) {
        platform_print("broken\n");
        return;
    }

    if (!framebuffer_initialized) {
        platform_print("blocked\n");
        return;
    }

    if (!framebuffer_prepared) {
        platform_print("not prepared\n");
        return;
    }

    platform_print("ready\n");
}

void framebuffer_doctor(void) {
    platform_print("Framebuffer doctor:\n");

    platform_print("  layer:       ");
    platform_print(framebuffer_initialized ? "initialized\n" : "not initialized\n");

    platform_print("  mode:        metadata-only\n");

    platform_print("  prepared:    ");
    platform_print(framebuffer_prepared ? "yes\n" : "no\n");

    platform_print("  broken:      ");
    platform_print(framebuffer_broken ? "yes\n" : "no\n");

    platform_print("  drawable:    no\n");
    platform_print("  reason:      real framebuffer drawing not enabled in dev-0.2.0\n");

    platform_print("  result:      ");

    if (framebuffer_broken) {
        platform_print("broken\n");
        return;
    }

    if (!framebuffer_initialized) {
        platform_print("blocked\n");
        return;
    }

    platform_print("ready\n");
}

void framebuffer_profile(void) {
    platform_print("Framebuffer profile:\n");

    platform_print("  profile: ");
    platform_print(framebuffer_profile_name);
    platform_print("\n");

    platform_print("  type:    ");
    platform_print_uint(framebuffer_type);
    platform_print("\n");

    platform_print("  size:    ");
    platform_print_uint(framebuffer_width);
    platform_print("x");
    platform_print_uint(framebuffer_height);
    platform_print("x");
    platform_print_uint(framebuffer_bpp);
    platform_print("\n");

    platform_print("  pitch:   ");
    platform_print_uint(framebuffer_pitch);
    platform_print("\n");

    platform_print("  address: ");
    platform_print_hex32(framebuffer_address);
    platform_print("\n");

    platform_print("  class:   ");

    if (framebuffer_type == 2) {
        platform_print("text\n");
    } else if (framebuffer_type == 0 || framebuffer_type == 1) {
        platform_print("graphics\n");
    } else {
        platform_print("unknown\n");
    }

    platform_print("  result:  ready\n");
}

void framebuffer_break(void) {
    framebuffer_broken = 1;

    platform_print("Framebuffer break:\n");
    platform_print("  result: broken\n");
}

void framebuffer_fix(void) {
    framebuffer_broken = 0;

    if (!framebuffer_initialized) {
        framebuffer_init();
    }

    platform_print("Framebuffer fix:\n");
    platform_print("  result: fixed\n");
}

int framebuffer_is_ready(void) {
    if (!framebuffer_initialized) {
        return 0;
    }

    if (framebuffer_broken) {
        return 0;
    }

    return 1;
}

int framebuffer_is_broken(void) {
    return framebuffer_broken;
}

unsigned int framebuffer_get_width(void) {
    return framebuffer_width;
}

unsigned int framebuffer_get_height(void) {
    return framebuffer_height;
}

unsigned int framebuffer_get_bpp(void) {
    return framebuffer_bpp;
}

unsigned int framebuffer_get_pitch(void) {
    return framebuffer_pitch;
}

unsigned int framebuffer_get_address(void) {
    return framebuffer_address;
}

unsigned int framebuffer_get_type(void) {
    return framebuffer_type;
}

const char* framebuffer_get_profile(void) {
    return framebuffer_profile_name;
}