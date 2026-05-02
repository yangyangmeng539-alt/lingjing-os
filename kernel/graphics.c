#include "graphics.h"
#include "platform.h"
#include "framebuffer.h"

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
static const char* graphics_font_name = "builtin-8x16-ascii";
static unsigned int graphics_font_width = 8;
static unsigned int graphics_font_height = 16;
static unsigned int graphics_last_text_width = 0;
static unsigned int graphics_last_text_height = 0;

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

    graphics_font_name = "builtin-8x16-ascii";
    graphics_font_width = 8;
    graphics_font_height = 16;
    graphics_last_text_width = 0;
    graphics_last_text_height = 0;
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

int graphics_backend_is_ready(void) {
    if (!graphics_is_ready()) {
        return 0;
    }

    return 1;
}

static int graphics_framebuffer_is_graphics_mode(void) {
    unsigned int type = framebuffer_get_type();

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

    platform_print("  font:        ");
    platform_print(graphics_font_name);
    platform_print("\n");

    platform_print("  font cell:   ");
    platform_print_uint(graphics_font_width);
    platform_print("x");
    platform_print_uint(graphics_font_height);
    platform_print("\n");

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

    platform_print("  real armed: yes\n");
    platform_print("  result:     ready\n");
}

void graphics_real_disarm(void) {
    graphics_real_armed = 0;

    platform_print("Graphics real disarm:\n");
    platform_print("  real armed: no\n");
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

static void graphics_real_write_attempt(const char* op) {
    graphics_real_attempts++;

    if (!graphics_can_real_write()) {
        platform_print("  backend: dryrun\n");
        platform_print("  real:    blocked\n");
        platform_print("  reason:  real framebuffer write unavailable\n");
        return;
    }

    graphics_real_writes++;

    platform_print("  backend: real-framebuffer\n");
    platform_print("  real:    staged\n");
    platform_print("  op:      ");
    platform_print(op);
    platform_print("\n");
}

void graphics_clear(void) {
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

    platform_print("Graphics clear dryrun:\n");
    platform_print("  mode:   metadata-only\n");
    platform_print("  color:  ");
    platform_print_hex32(graphics_last_color);
    platform_print("\n");

    graphics_real_write_attempt("clear");

    platform_print("  result: staged\n");
}

void graphics_pixel(unsigned int x, unsigned int y, unsigned int color) {
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

    platform_print("Graphics pixel dryrun:\n");

    platform_print("  x:      ");
    platform_print_uint(x);
    platform_print("\n");

    platform_print("  y:      ");
    platform_print_uint(y);
    platform_print("\n");

    platform_print("  color:  ");
    platform_print_hex32(color);
    platform_print("\n");

    graphics_real_write_attempt("pixel");

    platform_print("  result: staged\n");
}

void graphics_rect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int color) {
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

    platform_print("Graphics rect dryrun:\n");

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

    graphics_real_write_attempt("rect");

    platform_print("  result: staged\n");
}

void graphics_text(unsigned int x, unsigned int y, const char* text) {
    unsigned int len = graphics_text_len(text);

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

    platform_print("Graphics text dryrun:\n");

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

    graphics_real_write_attempt("text");

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

void graphics_panel(void) {
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

    platform_print("Graphics panel dryrun:\n");
    platform_print("  title:  Lingjing Runtime Panel\n");
    platform_print("  x:      16\n");
    platform_print("  y:      16\n");
    platform_print("  w:      320\n");
    platform_print("  h:      180\n");

    graphics_real_write_attempt("panel");

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