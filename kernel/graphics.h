#ifndef LINGJING_GRAPHICS_H
#define LINGJING_GRAPHICS_H

void graphics_init(void);
void graphics_status(void);
void graphics_check(void);
void graphics_doctor(void);

void graphics_backend(void);
void graphics_backend_check(void);
void graphics_backend_doctor(void);

void graphics_real_arm(void);
void graphics_real_disarm(void);
void graphics_real_gate(void);

void graphics_clear(void);
void graphics_pixel(unsigned int x, unsigned int y, unsigned int color);
void graphics_rect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int color);
void graphics_text(unsigned int x, unsigned int y, const char* text);
void graphics_font(void);
void graphics_text_metrics(const char* text);
void graphics_text_backend(void);
void graphics_text_arm(void);
void graphics_text_disarm(void);
void graphics_text_gate(void);
void graphics_boot_mode(void);
void graphics_boot_plan(void);
void graphics_boot_doctor(void);
void graphics_boot_preflight(void);
void graphics_boot_ready(void);
void graphics_boot_auto_mark(void);
int graphics_boot_auto_mark_early(void);
void graphics_boot_text(void);
void graphics_boot_graphics(void);


void graphics_panel(void);

void graphics_break(void);
void graphics_fix(void);

int graphics_is_ready(void);
int graphics_is_broken(void);
int graphics_real_is_armed(void);
int graphics_backend_is_ready(void);
int graphics_text_backend_is_armed(void);

unsigned int graphics_get_draw_count(void);
unsigned int graphics_get_last_x(void);
unsigned int graphics_get_last_y(void);
unsigned int graphics_get_last_w(void);
unsigned int graphics_get_last_h(void);
unsigned int graphics_get_last_color(void);

const char* graphics_get_last_op(void);
const char* graphics_get_mode(void);
const char* graphics_get_backend(void);
unsigned int graphics_get_font_width(void);
unsigned int graphics_get_font_height(void);
unsigned int graphics_get_last_text_width(void);
unsigned int graphics_get_last_text_height(void);
const char* graphics_get_font_name(void);

#endif