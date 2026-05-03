#include "graphics.h"
#include "bootinfo.h"
#include "platform.h"
#include "framebuffer.h"
#include "gshell.h"


static int graphics_initialized = 0;
static int graphics_broken = 0;

static int graphics_real_armed = 0;
static int graphics_real_attempts = 0;
static int graphics_real_writes = 0;
static int graphics_real_blocks = 0;

static unsigned int graphics_draw_count = 0;
static unsigned int graphics_last_x = 0;
static unsigned int graphics_last_y = 0;
static unsigned int graphics_last_w = 0;
static unsigned int graphics_last_h = 0;
static unsigned int graphics_last_color = 0;

static const char* graphics_last_op = "none";
static const char* graphics_mode = "metadata-only";
static const char* graphics_backend_name = "dryrun-backend";
static const char* graphics_font_name = "builtin-5x7-block";
static unsigned int graphics_font_width = 12;
static unsigned int graphics_font_height = 16;
static unsigned int graphics_last_text_width = 0;
static unsigned int graphics_last_text_height = 0;
static int graphics_text_backend_armed = 0;
static unsigned int graphics_text_draw_attempts = 0;
static unsigned int graphics_text_draw_blocks = 0;
static unsigned int graphics_text_draw_staged = 0;

static const char* graphics_boot_current_mode = "text-safe";
static const char* graphics_boot_planned_mode = "text-safe";
static const char* graphics_boot_reason = "default safe VGA text shell";
static unsigned int graphics_boot_plan_changes = 0;
static void graphics_write_pixel32(unsigned int x, unsigned int y, unsigned int color);
static void graphics_fill_rect32(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int color);
static void graphics_set_glyph5x7(unsigned char* rows, unsigned char r0, unsigned char r1, unsigned char r2, unsigned char r3, unsigned char r4, unsigned char r5, unsigned char r6);
static void graphics_get_glyph5x7(char ch, unsigned char* rows);
static void graphics_draw_char5x7(unsigned int x, unsigned int y, char ch, unsigned int color);
static void graphics_draw_text5x7(unsigned int x, unsigned int y, const char* text, unsigned int color);

static unsigned int graphics_text_len(const char* text) {
    unsigned int len = 0;

    while (text[len] != '\0') {
        len++;
    }

    return len;
}

void graphics_init(void) {
    graphics_initialized = 1;
    graphics_broken = 0;

    graphics_real_armed = 0;
    graphics_real_attempts = 0;
    graphics_real_writes = 0;
    graphics_real_blocks = 0;

    graphics_draw_count = 0;
    graphics_last_x = 0;
    graphics_last_y = 0;
    graphics_last_w = 0;
    graphics_last_h = 0;
    graphics_last_color = 0;

    graphics_last_op = "none";
    graphics_mode = "metadata-only";
    graphics_backend_name = "dryrun-backend";

    graphics_font_name = "builtin-5x7-ascii";
    graphics_font_width = 12;
    graphics_font_height = 16;
    graphics_last_text_width = 0;
    graphics_last_text_height = 0;

    graphics_text_backend_armed = 0;
    graphics_text_draw_attempts = 0;
    graphics_text_draw_blocks = 0;
    graphics_text_draw_staged = 0;

    graphics_boot_current_mode = "text-safe";
    graphics_boot_planned_mode = "text-safe";
    graphics_boot_reason = "default safe VGA text shell";
    graphics_boot_plan_changes = 0;
}

int graphics_is_ready(void) {
    if (!graphics_initialized) {
        return 0;
    }

    if (graphics_broken) {
        return 0;
    }

    if (!framebuffer_is_ready()) {
        return 0;
    }

    return 1;
}

int graphics_is_broken(void) {
    return graphics_broken;
}

int graphics_real_is_armed(void) {
    return graphics_real_armed;
}

int graphics_text_backend_is_armed(void) {
    return graphics_text_backend_armed;
}

int graphics_backend_is_ready(void) {
    if (!graphics_is_ready()) {
        return 0;
    }

    return 1;
}

static int graphics_framebuffer_is_graphics_mode(void) {
    unsigned int type = framebuffer_get_type();

    if (!framebuffer_is_ready()) {
        return 0;
    }

    if (framebuffer_get_address() == 0) {
        return 0;
    }

    if (framebuffer_get_width() == 0 || framebuffer_get_height() == 0 || framebuffer_get_bpp() == 0) {
        return 0;
    }

    if (type == 0) {
        return 1;
    }

    if (type == 1) {
        return 1;
    }

    return 0;
}

const char* graphics_get_backend(void) {
    return graphics_backend_name;
}

void graphics_status(void) {
    platform_print("Graphics status:\n");

    platform_print("  initialized: ");
    platform_print(graphics_initialized ? "yes\n" : "no\n");

    platform_print("  broken:      ");
    platform_print(graphics_broken ? "yes\n" : "no\n");

    platform_print("  mode:        ");
    platform_print(graphics_mode);
    platform_print("\n");

    platform_print("  backend:     ");
    platform_print(graphics_backend_name);
    platform_print("\n");

    platform_print("  boot mode:   ");
    platform_print(graphics_boot_current_mode);
    platform_print("\n");

    platform_print("  boot plan:   ");
    platform_print(graphics_boot_planned_mode);
    platform_print("\n");

    platform_print("  font:        ");
    platform_print(graphics_font_name);
    platform_print("\n");

    platform_print("  font cell:   ");
    platform_print_uint(graphics_font_width);
    platform_print("x");
    platform_print_uint(graphics_font_height);
    platform_print("\n");

    platform_print("  text gate:   ");
    platform_print(graphics_text_backend_armed ? "armed\n" : "disarmed\n");

    platform_print("  real armed:  ");
    platform_print(graphics_real_armed ? "yes\n" : "no\n");

    platform_print("  framebuffer: ");
    platform_print(framebuffer_is_ready() ? "ready\n" : "not ready\n");

    platform_print("  fb address:  ");
    platform_print_hex32(framebuffer_get_address());
    platform_print("\n");

    platform_print("  fb type:     ");
    platform_print_uint(framebuffer_get_type());
    platform_print("\n");

    platform_print("  fb profile:  ");
    platform_print(framebuffer_get_profile());
    platform_print("\n");

    platform_print("  fb mode:     ");

    if (framebuffer_get_type() == 0) {
        platform_print("indexed-color\n");
    } else if (framebuffer_get_type() == 1) {
        platform_print("rgb-graphics\n");
    } else if (framebuffer_get_type() == 2) {
        platform_print("text-mode\n");
    } else {
        platform_print("unknown\n");
    }

    platform_print("  last op:     ");
    platform_print(graphics_last_op);
    platform_print("\n");

    platform_print("  draw count:  ");
    platform_print_uint(graphics_draw_count);
    platform_print("\n");

    platform_print("  real tries:  ");
    platform_print_uint((unsigned int)graphics_real_attempts);
    platform_print("\n");

    platform_print("  real writes: ");
    platform_print_uint((unsigned int)graphics_real_writes);
    platform_print("\n");

    platform_print("  real blocks: ");
    platform_print_uint((unsigned int)graphics_real_blocks);
    platform_print("\n");

    platform_print("  text tries:  ");
    platform_print_uint(graphics_text_draw_attempts);
    platform_print("\n");

    platform_print("  text staged: ");
    platform_print_uint(graphics_text_draw_staged);
    platform_print("\n");

    platform_print("  text blocks: ");
    platform_print_uint(graphics_text_draw_blocks);
    platform_print("\n");

    platform_print("  last x:      ");
    platform_print_uint(graphics_last_x);
    platform_print("\n");

    platform_print("  last y:      ");
    platform_print_uint(graphics_last_y);
    platform_print("\n");

    platform_print("  last w:      ");
    platform_print_uint(graphics_last_w);
    platform_print("\n");

    platform_print("  last h:      ");
    platform_print_uint(graphics_last_h);
    platform_print("\n");

    platform_print("  last color:  ");
    platform_print_hex32(graphics_last_color);
    platform_print("\n");

    platform_print("  text width:  ");
    platform_print_uint(graphics_last_text_width);
    platform_print("\n");

    platform_print("  text height: ");
    platform_print_uint(graphics_last_text_height);
    platform_print("\n");
}

void graphics_check(void) {
    platform_print("Graphics check:\n");

    platform_print("  initialized: ");
    platform_print(graphics_initialized ? "ok\n" : "bad\n");

    platform_print("  framebuffer: ");
    platform_print(framebuffer_is_ready() ? "ready\n" : "not ready\n");

    platform_print("  backend:     ");
    platform_print(graphics_backend_is_ready() ? "ready\n" : "blocked\n");

    platform_print("  real gate:   ");
    platform_print(graphics_real_armed ? "armed\n" : "disarmed\n");

    platform_print("  broken:      ");
    platform_print(graphics_broken ? "yes\n" : "no\n");

    platform_print("  result:      ");

    if (graphics_broken) {
        platform_print("broken\n");
        return;
    }

    if (!graphics_initialized) {
        platform_print("blocked\n");
        return;
    }

    if (!framebuffer_is_ready()) {
        platform_print("blocked\n");
        return;
    }

    platform_print("ready\n");
}

void graphics_doctor(void) {
    platform_print("Graphics doctor:\n");

    platform_print("  layer:       ");
    platform_print(graphics_initialized ? "initialized\n" : "not initialized\n");

    platform_print("  mode:        ");
    platform_print(graphics_mode);
    platform_print("\n");

    platform_print("  backend:     ");
    platform_print(graphics_backend_name);
    platform_print("\n");

    platform_print("  framebuffer: ");
    platform_print(framebuffer_is_ready() ? "ready\n" : "not ready\n");

    platform_print("  fb address:  ");
    platform_print_hex32(framebuffer_get_address());
    platform_print("\n");

    platform_print("  real gate:   ");
    platform_print(graphics_real_armed ? "armed\n" : "disarmed\n");

    platform_print("  drawable:    dryrun\n");
    platform_print("  real draw:   gated\n");

    platform_print("  broken:      ");
    platform_print(graphics_broken ? "yes\n" : "no\n");

    platform_print("  result:      ");

    if (graphics_broken) {
        platform_print("broken\n");
        return;
    }

    if (!graphics_initialized) {
        platform_print("blocked\n");
        return;
    }

    if (!framebuffer_is_ready()) {
        platform_print("blocked\n");
        return;
    }

    platform_print("ready\n");
}

void graphics_backend(void) {
    platform_print("Graphics backend:\n");

    platform_print("  name:        ");
    platform_print(graphics_backend_name);
    platform_print("\n");

    platform_print("  mode:        ");
    platform_print(graphics_mode);
    platform_print("\n");

    platform_print("  framebuffer: ");
    platform_print(framebuffer_is_ready() ? "ready\n" : "not ready\n");

    platform_print("  width:       ");
    platform_print_uint(framebuffer_get_width());
    platform_print("\n");

    platform_print("  height:      ");
    platform_print_uint(framebuffer_get_height());
    platform_print("\n");

    platform_print("  bpp:         ");
    platform_print_uint(framebuffer_get_bpp());
    platform_print("\n");

    platform_print("  pitch:       ");
    platform_print_uint(framebuffer_get_pitch());
    platform_print("\n");

    platform_print("  address:     ");
    platform_print_hex32(framebuffer_get_address());
    platform_print("\n");

    platform_print("  fb type:     ");
    platform_print_uint(framebuffer_get_type());
    platform_print("\n");

    platform_print("  fb profile:  ");
    platform_print(framebuffer_get_profile());
    platform_print("\n");

    platform_print("  fb mode:     ");

    if (framebuffer_get_type() == 0) {
        platform_print("indexed-color\n");
    } else if (framebuffer_get_type() == 1) {
        platform_print("rgb-graphics\n");
    } else if (framebuffer_get_type() == 2) {
        platform_print("text-mode\n");
    } else {
        platform_print("unknown\n");
    }

    platform_print("  real gate:   ");
    platform_print(graphics_real_armed ? "armed\n" : "disarmed\n");

    platform_print("  safe write:  ");

    if (!graphics_real_armed) {
        platform_print("blocked\n");
    } else if (!graphics_framebuffer_is_graphics_mode()) {
        platform_print("blocked text-mode\n");
    } else if (framebuffer_get_address() == 0) {
        platform_print("blocked no-address\n");
    } else {
        platform_print("possible\n");
    }

    platform_print("  result:      ");
    platform_print(graphics_backend_is_ready() ? "ready\n" : "blocked\n");
}

void graphics_backend_check(void) {
    platform_print("Graphics backend check:\n");

    platform_print("  graphics:    ");
    platform_print(graphics_initialized ? "ok\n" : "bad\n");

    platform_print("  framebuffer: ");
    platform_print(framebuffer_is_ready() ? "ready\n" : "not ready\n");

    platform_print("  fb prepared: ");
    platform_print(framebuffer_get_width() > 0 && framebuffer_get_height() > 0 ? "yes\n" : "no\n");

    platform_print("  fb address:  ");
    platform_print_hex32(framebuffer_get_address());
    platform_print("\n");

    platform_print("  fb type:     ");
    platform_print_uint(framebuffer_get_type());
    platform_print("\n");

    platform_print("  fb profile:  ");
    platform_print(framebuffer_get_profile());
    platform_print("\n");

    platform_print("  graphics fb: ");
    platform_print(graphics_framebuffer_is_graphics_mode() ? "yes\n" : "no\n");

    platform_print("  real gate:   ");
    platform_print(graphics_real_armed ? "armed\n" : "disarmed\n");

    platform_print("  safe write:  ");

    if (graphics_real_armed && framebuffer_get_address() != 0 && graphics_framebuffer_is_graphics_mode()) {
        platform_print("possible\n");
    } else {
        platform_print("blocked\n");
    }

    platform_print("  result:      ");

    if (!graphics_backend_is_ready()) {
        platform_print("blocked\n");
        return;
    }

    platform_print("ready\n");
}

void graphics_backend_doctor(void) {
    platform_print("Graphics backend doctor:\n");

    platform_print("  backend:       ");
    platform_print(graphics_backend_name);
    platform_print("\n");

    platform_print("  metadata draw: ok\n");
    platform_print("  real write:    gated\n");

    platform_print("  framebuffer:   ");
    platform_print(framebuffer_is_ready() ? "ready\n" : "not ready\n");

    platform_print("  fb address:    ");
    platform_print_hex32(framebuffer_get_address());
    platform_print("\n");

    platform_print("  fb type:       ");
    platform_print_uint(framebuffer_get_type());
    platform_print("\n");

    platform_print("  fb profile:    ");
    platform_print(framebuffer_get_profile());
    platform_print("\n");

    platform_print("  graphics fb:   ");
    platform_print(graphics_framebuffer_is_graphics_mode() ? "yes\n" : "no\n");

    platform_print("  real armed:    ");
    platform_print(graphics_real_armed ? "yes\n" : "no\n");

    platform_print("  reason:        ");

    if (!graphics_real_armed) {
        platform_print("real write gate is disarmed\n");
    } else if (!graphics_framebuffer_is_graphics_mode()) {
        platform_print("framebuffer is text mode, real pixel write blocked\n");
    } else if (framebuffer_get_address() == 0) {
        platform_print("framebuffer address is metadata-only\n");
    } else {
        platform_print("real write path can be tested later\n");
    }

    platform_print("  result:        ");

    if (!graphics_backend_is_ready()) {
        platform_print("blocked\n");
        return;
    }

    platform_print("ready\n");
}

void graphics_real_arm(void) {
    platform_print("Graphics real arm:\n");

    if (!graphics_is_ready()) {
        platform_print("  result: blocked\n");
        platform_print("  reason: graphics not ready\n");
        return;
    }

    graphics_real_armed = 1;

    if (graphics_framebuffer_is_graphics_mode() && framebuffer_get_address() != 0 && framebuffer_get_bpp() == 32) {
        graphics_mode = "real-framebuffer";
        graphics_backend_name = "real-framebuffer32";
    } else {
        graphics_mode = "metadata-only";
        graphics_backend_name = "dryrun-backend";
    }

    platform_print("  real armed: yes\n");
    platform_print("  backend:    ");
    platform_print(graphics_backend_name);
    platform_print("\n");
    platform_print("  result:     ready\n");
}

void graphics_real_disarm(void) {
    graphics_real_armed = 0;
    graphics_mode = "metadata-only";
    graphics_backend_name = "dryrun-backend";

    platform_print("Graphics real disarm:\n");
    platform_print("  real armed: no\n");
    platform_print("  backend:    dryrun-backend\n");
    platform_print("  result:     disarmed\n");
}

void graphics_real_gate(void) {
    platform_print("Graphics real gate:\n");

    platform_print("  armed:      ");
    platform_print(graphics_real_armed ? "yes\n" : "no\n");

    platform_print("  framebuffer:");
    platform_print(framebuffer_is_ready() ? " ready\n" : " not ready\n");

    platform_print("  address:    ");
    platform_print_hex32(framebuffer_get_address());
    platform_print("\n");

    platform_print("  fb type:    ");
    platform_print_uint(framebuffer_get_type());
    platform_print("\n");

    platform_print("  fb profile: ");
    platform_print(framebuffer_get_profile());
    platform_print("\n");

    platform_print("  graphics fb:");
    platform_print(graphics_framebuffer_is_graphics_mode() ? " yes\n" : " no\n");

    platform_print("  result:     ");

    if (!graphics_real_armed) {
        platform_print("blocked\n");
        platform_print("  reason:     real gate disarmed\n");
        return;
    }

    if (!framebuffer_is_ready()) {
        platform_print("blocked\n");
        platform_print("  reason:     framebuffer not ready\n");
        return;
    }

    if (!graphics_framebuffer_is_graphics_mode()) {
        platform_print("blocked\n");
        platform_print("  reason:     framebuffer is text mode\n");
        return;
    }

    if (framebuffer_get_address() == 0) {
        platform_print("blocked\n");
        platform_print("  reason:     framebuffer address is metadata-only\n");
        return;
    }

    platform_print("ready\n");
}

static int graphics_can_real_text_draw(void) {
    if (!graphics_text_backend_armed) {
        graphics_text_draw_blocks++;
        return 0;
    }

    if (!graphics_real_armed) {
        graphics_text_draw_blocks++;
        return 0;
    }

    if (!framebuffer_is_ready()) {
        graphics_text_draw_blocks++;
        return 0;
    }

    if (!graphics_framebuffer_is_graphics_mode()) {
        graphics_text_draw_blocks++;
        return 0;
    }

    if (framebuffer_get_address() == 0) {
        graphics_text_draw_blocks++;
        return 0;
    }

    return 1;
}

static int graphics_text_real_draw_attempt(void) {
    graphics_text_draw_attempts++;

    if (!graphics_can_real_text_draw()) {
        platform_print("  text backend: dryrun\n");
        platform_print("  text real:    blocked\n");

        if (!graphics_text_backend_armed) {
            platform_print("  text reason:  text backend gate disarmed\n");
        } else if (!graphics_real_armed) {
            platform_print("  text reason:  graphics real gate disarmed\n");
        } else if (!framebuffer_is_ready()) {
            platform_print("  text reason:  framebuffer not ready\n");
        } else if (!graphics_framebuffer_is_graphics_mode()) {
            platform_print("  text reason:  framebuffer is text mode\n");
        } else if (framebuffer_get_address() == 0) {
            platform_print("  text reason:  framebuffer address is metadata-only\n");
        } else if (framebuffer_get_bpp() != 32) {
            platform_print("  text reason:  only 32bpp framebuffer is enabled now\n");
        } else {
            platform_print("  text reason:  unavailable\n");
        }

        return 0;
    }

    if (framebuffer_get_bpp() != 32) {
        graphics_text_draw_blocks++;

        platform_print("  text backend: dryrun\n");
        platform_print("  text real:    blocked\n");
        platform_print("  text reason:  only 32bpp framebuffer is enabled now\n");

        return 0;
    }

    graphics_text_draw_staged++;

    platform_print("  text backend: bitmap-5x7\n");
    platform_print("  text real:    written\n");

    return 1;
}

static int graphics_can_real_write(void) {
    if (!graphics_real_armed) {
        graphics_real_blocks++;
        return 0;
    }

    if (!framebuffer_is_ready()) {
        graphics_real_blocks++;
        return 0;
    }

    if (!graphics_framebuffer_is_graphics_mode()) {
        graphics_real_blocks++;
        return 0;
    }

    if (framebuffer_get_address() == 0) {
        graphics_real_blocks++;
        return 0;
    }

    return 1;
}

static int graphics_real_write_attempt(const char* op) {
    graphics_real_attempts++;

    if (!graphics_can_real_write()) {
        platform_print("  backend: dryrun\n");
        platform_print("  real:    blocked\n");
        platform_print("  reason:  real framebuffer write unavailable\n");
        return 0;
    }

    if (framebuffer_get_bpp() != 32) {
        graphics_real_blocks++;
        platform_print("  backend: dryrun\n");
        platform_print("  real:    blocked\n");
        platform_print("  reason:  only 32bpp framebuffer is enabled now\n");
        return 0;
    }

    graphics_real_writes++;
    graphics_mode = "real-framebuffer";
    graphics_backend_name = "real-framebuffer32";

    platform_print("  backend: real-framebuffer32\n");
    platform_print("  real:    written\n");
    platform_print("  op:      ");
    platform_print(op);
    platform_print("\n");

    return 1;
}

void graphics_clear(void) {
    int can_write = 0;

    if (!graphics_is_ready()) {
        platform_print("Graphics clear:\n");
        platform_print("  result: blocked\n");
        platform_print("  reason: graphics not ready\n");
        return;
    }

    graphics_last_op = "clear";
    graphics_last_x = 0;
    graphics_last_y = 0;
    graphics_last_w = framebuffer_get_width();
    graphics_last_h = framebuffer_get_height();
    graphics_last_color = 0x00000000;
    graphics_draw_count++;

    platform_print("Graphics clear:\n");
    platform_print("  color:  ");
    platform_print_hex32(graphics_last_color);
    platform_print("\n");

    can_write = graphics_real_write_attempt("clear");

    if (can_write) {
        graphics_fill_rect32(0, 0, framebuffer_get_width(), framebuffer_get_height(), graphics_last_color);
        platform_print("  result: real-written\n");
        return;
    }

    platform_print("  result: staged\n");
}

void graphics_pixel(unsigned int x, unsigned int y, unsigned int color) {
    int can_write = 0;

    if (!graphics_is_ready()) {
        platform_print("Graphics pixel:\n");
        platform_print("  result: blocked\n");
        platform_print("  reason: graphics not ready\n");
        return;
    }

    graphics_last_op = "pixel";
    graphics_last_x = x;
    graphics_last_y = y;
    graphics_last_w = 1;
    graphics_last_h = 1;
    graphics_last_color = color;
    graphics_draw_count++;

    platform_print("Graphics pixel:\n");

    platform_print("  x:      ");
    platform_print_uint(x);
    platform_print("\n");

    platform_print("  y:      ");
    platform_print_uint(y);
    platform_print("\n");

    platform_print("  color:  ");
    platform_print_hex32(color);
    platform_print("\n");

    can_write = graphics_real_write_attempt("pixel");

    if (can_write) {
        graphics_write_pixel32(x, y, color);
        platform_print("  result: real-written\n");
        return;
    }

    platform_print("  result: staged\n");
}

void graphics_rect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int color) {
    int can_write = 0;

    if (!graphics_is_ready()) {
        platform_print("Graphics rect:\n");
        platform_print("  result: blocked\n");
        platform_print("  reason: graphics not ready\n");
        return;
    }

    graphics_last_op = "rect";
    graphics_last_x = x;
    graphics_last_y = y;
    graphics_last_w = w;
    graphics_last_h = h;
    graphics_last_color = color;
    graphics_draw_count++;

    platform_print("Graphics rect:\n");

    platform_print("  x:      ");
    platform_print_uint(x);
    platform_print("\n");

    platform_print("  y:      ");
    platform_print_uint(y);
    platform_print("\n");

    platform_print("  w:      ");
    platform_print_uint(w);
    platform_print("\n");

    platform_print("  h:      ");
    platform_print_uint(h);
    platform_print("\n");

    platform_print("  color:  ");
    platform_print_hex32(color);
    platform_print("\n");

    can_write = graphics_real_write_attempt("rect");

    if (can_write) {
        graphics_fill_rect32(x, y, w, h, color);
        platform_print("  result: real-written\n");
        return;
    }

    platform_print("  result: staged\n");
}

static void graphics_set_glyph5x7(
    unsigned char* rows,
    unsigned char r0,
    unsigned char r1,
    unsigned char r2,
    unsigned char r3,
    unsigned char r4,
    unsigned char r5,
    unsigned char r6
) {
    rows[0] = r0;
    rows[1] = r1;
    rows[2] = r2;
    rows[3] = r3;
    rows[4] = r4;
    rows[5] = r5;
    rows[6] = r6;
}

static void graphics_get_glyph5x7(char ch, unsigned char* rows) {
    switch (ch) {
        case 'A': graphics_set_glyph5x7(rows, 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11); return;
        case 'B': graphics_set_glyph5x7(rows, 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E); return;
        case 'C': graphics_set_glyph5x7(rows, 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E); return;
        case 'D': graphics_set_glyph5x7(rows, 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E); return;
        case 'E': graphics_set_glyph5x7(rows, 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F); return;
        case 'F': graphics_set_glyph5x7(rows, 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10); return;
        case 'G': graphics_set_glyph5x7(rows, 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E); return;
        case 'H': graphics_set_glyph5x7(rows, 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11); return;
        case 'I': graphics_set_glyph5x7(rows, 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F); return;
        case 'J': graphics_set_glyph5x7(rows, 0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E); return;
        case 'K': graphics_set_glyph5x7(rows, 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11); return;
        case 'L': graphics_set_glyph5x7(rows, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F); return;
        case 'M': graphics_set_glyph5x7(rows, 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11); return;
        case 'N': graphics_set_glyph5x7(rows, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11); return;
        case 'O': graphics_set_glyph5x7(rows, 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E); return;
        case 'P': graphics_set_glyph5x7(rows, 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10); return;
        case 'Q': graphics_set_glyph5x7(rows, 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D); return;
        case 'R': graphics_set_glyph5x7(rows, 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11); return;
        case 'S': graphics_set_glyph5x7(rows, 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E); return;
        case 'T': graphics_set_glyph5x7(rows, 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04); return;
        case 'U': graphics_set_glyph5x7(rows, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E); return;
        case 'V': graphics_set_glyph5x7(rows, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04); return;
        case 'W': graphics_set_glyph5x7(rows, 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A); return;
        case 'X': graphics_set_glyph5x7(rows, 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11); return;
        case 'Y': graphics_set_glyph5x7(rows, 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04); return;
        case 'Z': graphics_set_glyph5x7(rows, 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F); return;

        case 'a': graphics_set_glyph5x7(rows, 0x00, 0x00, 0x0E, 0x01, 0x0F, 0x11, 0x0F); return;
        case 'b': graphics_set_glyph5x7(rows, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x11, 0x1E); return;
        case 'c': graphics_set_glyph5x7(rows, 0x00, 0x00, 0x0E, 0x10, 0x10, 0x10, 0x0E); return;
        case 'd': graphics_set_glyph5x7(rows, 0x01, 0x01, 0x0F, 0x11, 0x11, 0x11, 0x0F); return;
        case 'e': graphics_set_glyph5x7(rows, 0x00, 0x00, 0x0E, 0x11, 0x1F, 0x10, 0x0E); return;
        case 'f': graphics_set_glyph5x7(rows, 0x06, 0x08, 0x08, 0x1E, 0x08, 0x08, 0x08); return;
        case 'g': graphics_set_glyph5x7(rows, 0x00, 0x00, 0x0F, 0x11, 0x0F, 0x01, 0x0E); return;
        case 'h': graphics_set_glyph5x7(rows, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x11, 0x11); return;
        case 'i': graphics_set_glyph5x7(rows, 0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x0E); return;
        case 'j': graphics_set_glyph5x7(rows, 0x02, 0x00, 0x06, 0x02, 0x02, 0x12, 0x0C); return;
        case 'k': graphics_set_glyph5x7(rows, 0x10, 0x10, 0x12, 0x14, 0x18, 0x14, 0x12); return;
        case 'l': graphics_set_glyph5x7(rows, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E); return;
        case 'm': graphics_set_glyph5x7(rows, 0x00, 0x00, 0x1A, 0x15, 0x15, 0x15, 0x15); return;
        case 'n': graphics_set_glyph5x7(rows, 0x00, 0x00, 0x1E, 0x11, 0x11, 0x11, 0x11); return;
        case 'o': graphics_set_glyph5x7(rows, 0x00, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E); return;
        case 'p': graphics_set_glyph5x7(rows, 0x00, 0x00, 0x1E, 0x11, 0x1E, 0x10, 0x10); return;
        case 'q': graphics_set_glyph5x7(rows, 0x00, 0x00, 0x0F, 0x11, 0x0F, 0x01, 0x01); return;
        case 'r': graphics_set_glyph5x7(rows, 0x00, 0x00, 0x16, 0x18, 0x10, 0x10, 0x10); return;
        case 's': graphics_set_glyph5x7(rows, 0x00, 0x00, 0x0F, 0x10, 0x0E, 0x01, 0x1E); return;
        case 't': graphics_set_glyph5x7(rows, 0x08, 0x08, 0x1E, 0x08, 0x08, 0x09, 0x06); return;
        case 'u': graphics_set_glyph5x7(rows, 0x00, 0x00, 0x11, 0x11, 0x11, 0x13, 0x0D); return;
        case 'v': graphics_set_glyph5x7(rows, 0x00, 0x00, 0x11, 0x11, 0x11, 0x0A, 0x04); return;
        case 'w': graphics_set_glyph5x7(rows, 0x00, 0x00, 0x11, 0x11, 0x15, 0x15, 0x0A); return;
        case 'x': graphics_set_glyph5x7(rows, 0x00, 0x00, 0x11, 0x0A, 0x04, 0x0A, 0x11); return;
        case 'y': graphics_set_glyph5x7(rows, 0x00, 0x00, 0x11, 0x11, 0x0F, 0x01, 0x0E); return;
        case 'z': graphics_set_glyph5x7(rows, 0x00, 0x00, 0x1F, 0x02, 0x04, 0x08, 0x1F); return;

        case '0': graphics_set_glyph5x7(rows, 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E); return;
        case '1': graphics_set_glyph5x7(rows, 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E); return;
        case '2': graphics_set_glyph5x7(rows, 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F); return;
        case '3': graphics_set_glyph5x7(rows, 0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E); return;
        case '4': graphics_set_glyph5x7(rows, 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02); return;
        case '5': graphics_set_glyph5x7(rows, 0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E); return;
        case '6': graphics_set_glyph5x7(rows, 0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E); return;
        case '7': graphics_set_glyph5x7(rows, 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08); return;
        case '8': graphics_set_glyph5x7(rows, 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E); return;
        case '9': graphics_set_glyph5x7(rows, 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E); return;

        case ' ': graphics_set_glyph5x7(rows, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00); return;
        case '.': graphics_set_glyph5x7(rows, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C); return;
        case ':': graphics_set_glyph5x7(rows, 0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00); return;
        case '-': graphics_set_glyph5x7(rows, 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00); return;
        case '/': graphics_set_glyph5x7(rows, 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10); return;
        case '_': graphics_set_glyph5x7(rows, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F); return;
        case '>': graphics_set_glyph5x7(rows, 0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10); return;
        case '<': graphics_set_glyph5x7(rows, 0x01, 0x02, 0x04, 0x08, 0x04, 0x02, 0x01); return;

        default:
            graphics_set_glyph5x7(rows, 0x1F, 0x11, 0x02, 0x04, 0x04, 0x00, 0x04);
            return;
    }
}

static void graphics_draw_char5x7(unsigned int x, unsigned int y, char ch, unsigned int color) {
    unsigned char rows[7];
    unsigned int row = 0;
    unsigned int col = 0;
    unsigned int sx = 0;
    unsigned int sy = 0;
    unsigned int scale = 2;
    unsigned int bit = 0;

    graphics_get_glyph5x7(ch, rows);

    for (row = 0; row < 7; row++) {
        for (col = 0; col < 5; col++) {
            bit = 1U << (4U - col);

            if ((rows[row] & bit) != 0) {
                for (sy = 0; sy < scale; sy++) {
                    for (sx = 0; sx < scale; sx++) {
                        graphics_write_pixel32(
                            x + (col * scale) + sx,
                            y + (row * scale) + sy,
                            color
                        );
                    }
                }
            }
        }
    }
}

static void graphics_draw_text5x7(unsigned int x, unsigned int y, const char* text, unsigned int color) {
    unsigned int index = 0;
    unsigned int cursor_x = x;

    while (text[index] != '\0') {
        graphics_draw_char5x7(cursor_x, y, text[index], color);
        cursor_x += graphics_font_width;
        index++;
    }
}

void graphics_text(unsigned int x, unsigned int y, const char* text) {
    unsigned int len = graphics_text_len(text);
    int can_write = 0;

    if (!graphics_is_ready()) {
        platform_print("Graphics text:\n");
        platform_print("  result: blocked\n");
        platform_print("  reason: graphics not ready\n");
        return;
    }

    graphics_last_op = "text";
    graphics_last_x = x;
    graphics_last_y = y;
    graphics_last_w = len * graphics_font_width;
    graphics_last_h = graphics_font_height;
    graphics_last_color = 0x00FFFFFF;
    graphics_last_text_width = graphics_last_w;
    graphics_last_text_height = graphics_last_h;
    graphics_draw_count++;

    platform_print("Graphics text:\n");

    platform_print("  x:      ");
    platform_print_uint(x);
    platform_print("\n");

    platform_print("  y:      ");
    platform_print_uint(y);
    platform_print("\n");

    platform_print("  text:   ");
    platform_print(text);
    platform_print("\n");

    platform_print("  font:   ");
    platform_print(graphics_font_name);
    platform_print("\n");

    platform_print("  cell:   ");
    platform_print_uint(graphics_font_width);
    platform_print("x");
    platform_print_uint(graphics_font_height);
    platform_print("\n");

    platform_print("  width:  ");
    platform_print_uint(graphics_last_text_width);
    platform_print("\n");

    platform_print("  height: ");
    platform_print_uint(graphics_last_text_height);
    platform_print("\n");

    can_write = graphics_text_real_draw_attempt();

    if (can_write) {
        graphics_draw_text5x7(x, y, text, graphics_last_color);
        platform_print("  result: real-written\n");
        return;
    }

    platform_print("  result: staged\n");
}

void graphics_font(void) {
    platform_print("Graphics font:\n");

    platform_print("  name:      ");
    platform_print(graphics_font_name);
    platform_print("\n");

    platform_print("  type:      bitmap metadata\n");

    platform_print("  charset:   ascii\n");

    platform_print("  width:     ");
    platform_print_uint(graphics_font_width);
    platform_print("\n");

    platform_print("  height:    ");
    platform_print_uint(graphics_font_height);
    platform_print("\n");

    platform_print("  real draw: gated\n");

    platform_print("  result:    ready\n");
}

void graphics_text_metrics(const char* text) {
    unsigned int len = graphics_text_len(text);
    unsigned int width = len * graphics_font_width;
    unsigned int height = graphics_font_height;

    platform_print("Graphics text metrics:\n");

    platform_print("  text:   ");
    platform_print(text);
    platform_print("\n");

    platform_print("  chars:  ");
    platform_print_uint(len);
    platform_print("\n");

    platform_print("  font:   ");
    platform_print(graphics_font_name);
    platform_print("\n");

    platform_print("  cell:   ");
    platform_print_uint(graphics_font_width);
    platform_print("x");
    platform_print_uint(graphics_font_height);
    platform_print("\n");

    platform_print("  width:  ");
    platform_print_uint(width);
    platform_print("\n");

    platform_print("  height: ");
    platform_print_uint(height);
    platform_print("\n");

    platform_print("  result: measured\n");
}

void graphics_text_backend(void) {
    platform_print("Graphics text backend:\n");

    platform_print("  font:        ");
    platform_print(graphics_font_name);
    platform_print("\n");

    platform_print("  cell:        ");
    platform_print_uint(graphics_font_width);
    platform_print("x");
    platform_print_uint(graphics_font_height);
    platform_print("\n");

    platform_print("  gate:        ");
    platform_print(graphics_text_backend_armed ? "armed\n" : "disarmed\n");

    platform_print("  real gate:   ");
    platform_print(graphics_real_armed ? "armed\n" : "disarmed\n");

    platform_print("  framebuffer: ");
    platform_print(framebuffer_is_ready() ? "ready\n" : "not ready\n");

    platform_print("  fb address:  ");
    platform_print_hex32(framebuffer_get_address());
    platform_print("\n");

    platform_print("  fb type:     ");
    platform_print_uint(framebuffer_get_type());
    platform_print("\n");

    platform_print("  fb profile:  ");
    platform_print(framebuffer_get_profile());
    platform_print("\n");

    platform_print("  graphics fb: ");
    platform_print(graphics_framebuffer_is_graphics_mode() ? "yes\n" : "no\n");

    platform_print("  attempts:    ");
    platform_print_uint(graphics_text_draw_attempts);
    platform_print("\n");

    platform_print("  staged:      ");
    platform_print_uint(graphics_text_draw_staged);
    platform_print("\n");

    platform_print("  blocks:      ");
    platform_print_uint(graphics_text_draw_blocks);
    platform_print("\n");

    platform_print("  result:      ready\n");
}

void graphics_text_arm(void) {
    platform_print("Graphics text arm:\n");

    if (!graphics_is_ready()) {
        platform_print("  result: blocked\n");
        platform_print("  reason: graphics not ready\n");
        return;
    }

    graphics_text_backend_armed = 1;

    platform_print("  text gate: armed\n");
    platform_print("  result:    ready\n");
}

void graphics_text_disarm(void) {
    graphics_text_backend_armed = 0;

    platform_print("Graphics text disarm:\n");
    platform_print("  text gate: disarmed\n");
    platform_print("  result:    disarmed\n");
}

void graphics_text_gate(void) {
    platform_print("Graphics text gate:\n");

    platform_print("  text armed: ");
    platform_print(graphics_text_backend_armed ? "yes\n" : "no\n");

    platform_print("  real armed: ");
    platform_print(graphics_real_armed ? "yes\n" : "no\n");

    platform_print("  framebuffer:");
    platform_print(framebuffer_is_ready() ? " ready\n" : " not ready\n");

    platform_print("  address:    ");
    platform_print_hex32(framebuffer_get_address());
    platform_print("\n");

    platform_print("  fb type:    ");
    platform_print_uint(framebuffer_get_type());
    platform_print("\n");

    platform_print("  fb profile: ");
    platform_print(framebuffer_get_profile());
    platform_print("\n");

    platform_print("  graphics fb:");
    platform_print(graphics_framebuffer_is_graphics_mode() ? " yes\n" : " no\n");

    platform_print("  result:     ");

    if (!graphics_text_backend_armed) {
        platform_print("blocked\n");
        platform_print("  reason:     text backend gate disarmed\n");
        return;
    }

    if (!graphics_real_armed) {
        platform_print("blocked\n");
        platform_print("  reason:     graphics real gate disarmed\n");
        return;
    }

    if (!framebuffer_is_ready()) {
        platform_print("blocked\n");
        platform_print("  reason:     framebuffer not ready\n");
        return;
    }

    if (!graphics_framebuffer_is_graphics_mode()) {
        platform_print("blocked\n");
        platform_print("  reason:     framebuffer is text mode\n");
        return;
    }

    if (framebuffer_get_address() == 0) {
        platform_print("blocked\n");
        platform_print("  reason:     framebuffer address is metadata-only\n");
        return;
    }

    platform_print("ready\n");
}

void graphics_boot_mode(void) {
    platform_print("Graphics boot mode:\n");

    platform_print("  current: ");
    platform_print(graphics_boot_current_mode);
    platform_print("\n");

    platform_print("  planned: ");
    platform_print(graphics_boot_planned_mode);
    platform_print("\n");

    platform_print("  changes: ");
    platform_print_uint(graphics_boot_plan_changes);
    platform_print("\n");

    platform_print("  reason:  ");
    platform_print(graphics_boot_reason);
    platform_print("\n");

    platform_print("  result:  ready\n");
}

void graphics_boot_plan(void) {
    platform_print("Graphics boot plan:\n");

    platform_print("  current boot:      ");
    platform_print(graphics_boot_current_mode);
    platform_print("\n");

    platform_print("  planned boot:      ");
    platform_print(graphics_boot_planned_mode);
    platform_print("\n");

    platform_print("  gshell output:     ");
    platform_print(gshell_get_output_plan());
    platform_print("\n");

    platform_print("  text-safe:         enabled\n");

    platform_print("  graphics-request:  ");
    if (graphics_boot_planned_mode[0] == 'g') {
        platform_print("planned\n");
    } else {
        platform_print("not planned\n");
    }

    platform_print("  gshell self-out:   ");
    if (gshell_output_graphics_planned()) {
        platform_print("planned\n");
    } else {
        platform_print("not planned\n");
    }

    platform_print("  real switch:       not executed\n");
    platform_print("  boot.asm changed:  no\n");
    platform_print("  grub changed:      no\n");

    platform_print("  required before real graphics boot:\n");
    platform_print("    gshell output graphics must be planned\n");
    platform_print("    graphics shell must self-render text\n");
    platform_print("    VGA platform_print must not be primary output\n");
    platform_print("    framebuffer type must be 0 or 1\n");
    platform_print("    real write gate must pass\n");

    platform_print("  result:            staged\n");
}

void graphics_boot_doctor(void) {
    platform_print("Graphics boot doctor:\n");

    platform_print("  current mode: ");
    platform_print(graphics_boot_current_mode);
    platform_print("\n");

    platform_print("  planned mode: ");
    platform_print(graphics_boot_planned_mode);
    platform_print("\n");

    platform_print("  gshell output:");
    platform_print(" ");
    platform_print(gshell_get_output_plan());
    platform_print("\n");

    platform_print("  framebuffer:  ");
    platform_print(framebuffer_is_ready() ? "ready\n" : "not ready\n");

    platform_print("  fb type:      ");
    platform_print_uint(framebuffer_get_type());
    platform_print("\n");

    platform_print("  fb profile:   ");
    platform_print(framebuffer_get_profile());
    platform_print("\n");

    platform_print("  graphics fb:  ");
    platform_print(graphics_framebuffer_is_graphics_mode() ? "yes\n" : "no\n");

    platform_print("  text gate:    ");
    platform_print(graphics_text_backend_armed ? "armed\n" : "disarmed\n");

    platform_print("  real gate:    ");
    platform_print(graphics_real_armed ? "armed\n" : "disarmed\n");

    platform_print("  safe now:     yes\n");

    platform_print("  can boot gfx: ");

    if (graphics_boot_planned_mode[0] != 'g') {
        platform_print("no\n");
        platform_print("  reason:       graphics boot not planned\n");
    } else if (!gshell_output_graphics_planned()) {
        platform_print("no\n");
        platform_print("  reason:       gshell graphics-self output not planned\n");
    } else if (!graphics_text_backend_armed) {
        platform_print("no\n");
        platform_print("  reason:       text backend gate disarmed\n");
    } else if (!graphics_real_armed) {
        platform_print("no\n");
        platform_print("  reason:       graphics real gate disarmed\n");
    } else if (!graphics_framebuffer_is_graphics_mode()) {
        platform_print("no\n");
        platform_print("  reason:       framebuffer is not graphics mode\n");
    } else {
        platform_print("yes\n");
        platform_print("  reason:       graphics boot can be tested later\n");
    }

    platform_print("  result:       ready\n");
}

void graphics_boot_preflight(void) {
    platform_print("Graphics boot preflight:\n");

    platform_print("  boot current: ");
    platform_print(graphics_boot_current_mode);
    platform_print("\n");

    platform_print("  boot planned: ");
    platform_print(graphics_boot_planned_mode);
    platform_print("\n");

    platform_print("  gshell output:");
    platform_print(" ");
    platform_print(gshell_get_output_plan());
    platform_print("\n");

    platform_print("  display:      ");
    platform_print(framebuffer_get_profile());
    platform_print("\n");

    platform_print("  framebuffer:  ");
    platform_print(framebuffer_is_ready() ? "ready\n" : "not ready\n");

    platform_print("  fb type:      ");
    platform_print_uint(framebuffer_get_type());
    platform_print("\n");

    platform_print("  graphics fb:  ");
    platform_print(graphics_framebuffer_is_graphics_mode() ? "yes\n" : "no\n");

    platform_print("  text gate:    ");
    platform_print(graphics_text_backend_armed ? "armed\n" : "disarmed\n");

    platform_print("  real gate:    ");
    platform_print(graphics_real_armed ? "armed\n" : "disarmed\n");

    platform_print("  checks:\n");

    platform_print("    boot graphics planned: ");
    platform_print(graphics_boot_planned_mode[0] == 'g' ? "yes\n" : "no\n");

    platform_print("    gshell self-output:    ");
    platform_print(gshell_output_graphics_planned() ? "yes\n" : "no\n");

    platform_print("    text backend armed:    ");
    platform_print(graphics_text_backend_armed ? "yes\n" : "no\n");

    platform_print("    real gate armed:       ");
    platform_print(graphics_real_armed ? "yes\n" : "no\n");

    platform_print("    framebuffer graphics:  ");
    platform_print(graphics_framebuffer_is_graphics_mode() ? "yes\n" : "no\n");

    platform_print("  result:       ");

    if (graphics_boot_planned_mode[0] != 'g') {
        platform_print("blocked\n");
        platform_print("  reason:       graphics boot not planned\n");
        return;
    }

    if (!gshell_output_graphics_planned()) {
        platform_print("blocked\n");
        platform_print("  reason:       gshell graphics-self output not planned\n");
        return;
    }

    if (!graphics_text_backend_armed) {
        platform_print("blocked\n");
        platform_print("  reason:       graphics text backend disarmed\n");
        return;
    }

    if (!graphics_real_armed) {
        platform_print("blocked\n");
        platform_print("  reason:       graphics real gate disarmed\n");
        return;
    }

    if (!framebuffer_is_ready()) {
        platform_print("blocked\n");
        platform_print("  reason:       framebuffer not ready\n");
        return;
    }

    if (!graphics_framebuffer_is_graphics_mode()) {
        platform_print("blocked\n");
        platform_print("  reason:       framebuffer is not graphics mode\n");
        return;
    }

    if (framebuffer_get_address() == 0) {
        platform_print("blocked\n");
        platform_print("  reason:       framebuffer address is metadata-only\n");
        return;
    }

    platform_print("staged\n");
    platform_print("  reason:       graphics boot preflight passed\n");
}

void graphics_boot_ready(void) {
    int boot_ready = 0;
    int gshell_ready = 0;
    int text_ready = 0;
    int real_ready = 0;
    int fb_ready = 0;
    int result_ready = 0;

    boot_ready = graphics_boot_planned_mode[0] == 'g';
    gshell_ready = gshell_output_graphics_planned();
    text_ready = graphics_text_backend_armed;
    real_ready = graphics_real_armed;
    fb_ready = graphics_framebuffer_is_graphics_mode();

    result_ready = boot_ready &&
                   gshell_ready &&
                   text_ready &&
                   real_ready &&
                   fb_ready &&
                   framebuffer_get_address() != 0;

    platform_print("BootReady: ");

    platform_print("boot=");
    platform_print(boot_ready ? "yes" : "no");

    platform_print(" gshell=");
    platform_print(gshell_ready ? "yes" : "no");

    platform_print(" text=");
    platform_print(text_ready ? "yes" : "no");

    platform_print(" real=");
    platform_print(real_ready ? "yes" : "no");

    platform_print(" fb=");
    platform_print(fb_ready ? "yes" : "no");

    platform_print(" profile=");
    platform_print(framebuffer_get_profile());

    platform_print(" result=");
    platform_print(result_ready ? "ready" : "blocked");

    platform_print("\n");
}

static void graphics_write_pixel32(unsigned int x, unsigned int y, unsigned int color) {
    unsigned int addr = 0;
    volatile unsigned int* pixel = 0;

    if (framebuffer_get_address() == 0) {
        return;
    }

    if (x >= framebuffer_get_width()) {
        return;
    }

    if (y >= framebuffer_get_height()) {
        return;
    }

    addr = framebuffer_get_address() + (y * framebuffer_get_pitch()) + (x * 4);
    pixel = (volatile unsigned int*)addr;
    *pixel = color;
}

static void graphics_fill_rect32(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int color) {
    unsigned int yy = 0;
    unsigned int xx = 0;

    for (yy = 0; yy < h; yy++) {
        for (xx = 0; xx < w; xx++) {
            graphics_write_pixel32(x + xx, y + yy, color);
        }
    }
}

static void graphics_raw_pixel32(
    unsigned int fb_addr,
    unsigned int pitch,
    unsigned int width,
    unsigned int height,
    unsigned int x,
    unsigned int y,
    unsigned int color
) {
    unsigned int addr = 0;
    volatile unsigned int* pixel = 0;

    if (fb_addr == 0) {
        return;
    }

    if (x >= width) {
        return;
    }

    if (y >= height) {
        return;
    }

    addr = fb_addr + (y * pitch) + (x * 4);
    pixel = (volatile unsigned int*)addr;
    *pixel = color;
}

static void graphics_raw_rect32(
    unsigned int fb_addr,
    unsigned int pitch,
    unsigned int width,
    unsigned int height,
    unsigned int x,
    unsigned int y,
    unsigned int w,
    unsigned int h,
    unsigned int color
) {
    unsigned int yy = 0;
    unsigned int xx = 0;

    for (yy = 0; yy < h; yy++) {
        for (xx = 0; xx < w; xx++) {
            graphics_raw_pixel32(fb_addr, pitch, width, height, x + xx, y + yy, color);
        }
    }
}

int graphics_boot_auto_mark_early(void) {
    unsigned int fb_addr = 0;
    unsigned int width = 0;
    unsigned int height = 0;
    unsigned int bpp = 0;
    unsigned int pitch = 0;
    unsigned int type = 0;
    unsigned int i = 0;

    if (!bootinfo_has_framebuffer()) {
        return 0;
    }

    fb_addr = bootinfo_get_framebuffer_addr_low();
    width = bootinfo_get_framebuffer_width();
    height = bootinfo_get_framebuffer_height();
    bpp = bootinfo_get_framebuffer_bpp();
    pitch = bootinfo_get_framebuffer_pitch();
    type = bootinfo_get_framebuffer_type();

    if (fb_addr == 0) {
        return 0;
    }

    if (width < 160 || height < 120) {
        return 0;
    }

    if (bpp != 32) {
        return 0;
    }

    if (!(type == 0 || type == 1)) {
        return 0;
    }

    graphics_raw_rect32(fb_addr, pitch, width, height, 0, 0, width, height, 0x00000000);

    graphics_raw_rect32(fb_addr, pitch, width, height, 24, 24, 360, 36, 0x0000AAFF);
    graphics_raw_rect32(fb_addr, pitch, width, height, 24, 76, 460, 20, 0x00004477);
    graphics_raw_rect32(fb_addr, pitch, width, height, 24, 112, 220, 20, 0x0000FF66);
    graphics_raw_rect32(fb_addr, pitch, width, height, 24, 148, 420, 20, 0x00FFFFFF);

    graphics_raw_rect32(fb_addr, pitch, width, height, 24, 196, 96, 96, 0x00112233);
    graphics_raw_rect32(fb_addr, pitch, width, height, 32, 204, 80, 80, 0x00000000);
    graphics_raw_rect32(fb_addr, pitch, width, height, 40, 212, 64, 64, 0x0000AAFF);
    graphics_raw_rect32(fb_addr, pitch, width, height, 52, 224, 40, 40, 0x00000000);
    graphics_raw_rect32(fb_addr, pitch, width, height, 64, 236, 16, 16, 0x0000FF66);

    graphics_raw_rect32(fb_addr, pitch, width, height, 152, 196, 220, 20, 0x00FFAA00);
    graphics_raw_rect32(fb_addr, pitch, width, height, 152, 232, 260, 12, 0x0000AAFF);
    graphics_raw_rect32(fb_addr, pitch, width, height, 152, 260, 180, 12, 0x00FFFFFF);

    for (i = 0; i < 96; i++) {
        graphics_raw_pixel32(fb_addr, pitch, width, height, 456 + i, 196 + i, 0x00FFFFFF);
        graphics_raw_pixel32(fb_addr, pitch, width, height, 456 + i, 292 - i, 0x0000FF66);
    }

    graphics_raw_rect32(fb_addr, pitch, width, height, width - 120, 24, 64, 64, 0x00FFAA00);
    graphics_raw_rect32(fb_addr, pitch, width, height, width - 108, 36, 40, 40, 0x00000000);
    graphics_raw_rect32(fb_addr, pitch, width, height, width - 96, 48, 16, 16, 0x00FFAA00);

    graphics_raw_pixel32(fb_addr, pitch, width, height, 24, 328, 0x00FFFFFF);
    graphics_raw_pixel32(fb_addr, pitch, width, height, 25, 329, 0x00FFFFFF);
    graphics_raw_pixel32(fb_addr, pitch, width, height, 26, 330, 0x00FFFFFF);
    graphics_raw_pixel32(fb_addr, pitch, width, height, 27, 331, 0x00FFFFFF);

    return 1;
}

void graphics_boot_auto_mark(void) {
    unsigned int width = framebuffer_get_width();
    unsigned int height = framebuffer_get_height();

    if (!framebuffer_is_ready()) {
        return;
    }

    if (!graphics_framebuffer_is_graphics_mode()) {
        return;
    }

    if (framebuffer_get_address() == 0) {
        return;
    }

    if (framebuffer_get_bpp() != 32) {
        return;
    }

    if (width < 160 || height < 120) {
        return;
    }

    graphics_last_op = "boot-auto-mark";
    graphics_last_x = 0;
    graphics_last_y = 0;
    graphics_last_w = width;
    graphics_last_h = height;
    graphics_last_color = 0x00000000;
    graphics_draw_count++;

    graphics_fill_rect32(0, 0, width, height, 0x00000000);

    graphics_fill_rect32(24, 24, 260, 28, 0x0000AAFF);
    graphics_fill_rect32(24, 64, 420, 18, 0x00003355);
    graphics_fill_rect32(24, 96, 180, 18, 0x0000FF55);
    graphics_fill_rect32(24, 128, 360, 18, 0x00FFFFFF);

    graphics_fill_rect32(width - 96, 24, 48, 48, 0x00FFAA00);
    graphics_fill_rect32(width - 88, 32, 32, 32, 0x00000000);
    graphics_fill_rect32(width - 80, 40, 16, 16, 0x00FFAA00);

    graphics_write_pixel32(24, 180, 0x00FFFFFF);
    graphics_write_pixel32(25, 181, 0x00FFFFFF);
    graphics_write_pixel32(26, 182, 0x00FFFFFF);
    graphics_write_pixel32(27, 183, 0x00FFFFFF);
}

void graphics_boot_text(void) {
    graphics_boot_planned_mode = "text-safe";
    graphics_boot_reason = "user selected safe VGA text boot";
    graphics_boot_plan_changes++;

    platform_print("Graphics boot text:\n");
    platform_print("  planned: text-safe\n");
    platform_print("  real switch: no\n");
    platform_print("  result:  staged\n");
}

void graphics_boot_graphics(void) {
    graphics_boot_planned_mode = "graphics-requested";
    graphics_boot_reason = "user planned graphics framebuffer boot later";
    graphics_boot_plan_changes++;

    platform_print("Graphics boot graphics:\n");
    platform_print("  planned: graphics-requested\n");
    platform_print("  real switch: no\n");

    platform_print("  gshell output: ");
    platform_print(gshell_get_output_plan());
    platform_print("\n");

    platform_print("  prerequisite: ");

    if (gshell_output_graphics_planned()) {
        platform_print("gshell graphics-self planned\n");
    } else {
        platform_print("missing gshell graphics-self\n");
    }

    platform_print("  warning: requires graphics shell self-output first\n");
    platform_print("  result:  staged\n");
}

void graphics_panel(void) {
    int can_write = 0;

    if (!graphics_is_ready()) {
        platform_print("Graphics panel:\n");
        platform_print("  result: blocked\n");
        platform_print("  reason: graphics not ready\n");
        return;
    }

    graphics_last_op = "panel";
    graphics_last_x = 16;
    graphics_last_y = 16;
    graphics_last_w = 320;
    graphics_last_h = 180;
    graphics_last_color = 0x00112233;
    graphics_draw_count++;

    platform_print("Graphics panel:\n");
    platform_print("  title:  Lingjing Runtime Panel\n");
    platform_print("  x:      16\n");
    platform_print("  y:      16\n");
    platform_print("  w:      320\n");
    platform_print("  h:      180\n");

    can_write = graphics_real_write_attempt("panel");

    if (can_write) {
        graphics_fill_rect32(16, 16, 320, 180, 0x00112233);
        graphics_fill_rect32(16, 16, 320, 4, 0x0000AAFF);
        graphics_fill_rect32(16, 16, 4, 180, 0x0000AAFF);
        graphics_fill_rect32(16, 192, 320, 4, 0x0000AAFF);
        graphics_fill_rect32(332, 16, 4, 180, 0x0000AAFF);
        graphics_fill_rect32(32, 40, 180, 12, 0x0000FF66);
        graphics_fill_rect32(32, 64, 260, 10, 0x00FFFFFF);
        graphics_fill_rect32(32, 88, 220, 10, 0x00004477);
        platform_print("  result: real-written\n");
        return;
    }

    platform_print("  result: staged\n");
}

void graphics_break(void) {
    graphics_broken = 1;

    platform_print("Graphics break:\n");
    platform_print("  result: broken\n");
}

void graphics_fix(void) {
    graphics_broken = 0;

    if (!graphics_initialized) {
        graphics_init();
    }

    platform_print("Graphics fix:\n");
    platform_print("  result: fixed\n");
}

unsigned int graphics_get_draw_count(void) {
    return graphics_draw_count;
}

unsigned int graphics_get_last_x(void) {
    return graphics_last_x;
}

unsigned int graphics_get_last_y(void) {
    return graphics_last_y;
}

unsigned int graphics_get_last_w(void) {
    return graphics_last_w;
}

unsigned int graphics_get_last_h(void) {
    return graphics_last_h;
}

unsigned int graphics_get_last_color(void) {
    return graphics_last_color;
}

const char* graphics_get_last_op(void) {
    return graphics_last_op;
}

const char* graphics_get_mode(void) {
    return graphics_mode;
}

unsigned int graphics_get_font_width(void) {
    return graphics_font_width;
}

unsigned int graphics_get_font_height(void) {
    return graphics_font_height;
}

unsigned int graphics_get_last_text_width(void) {
    return graphics_last_text_width;
}

unsigned int graphics_get_last_text_height(void) {
    return graphics_last_text_height;
}

const char* graphics_get_font_name(void) {
    return graphics_font_name;
}