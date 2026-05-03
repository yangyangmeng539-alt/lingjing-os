#ifndef LINGJING_GSHELL_H
#define LINGJING_GSHELL_H

void gshell_init(void);
void gshell_status(void);
void gshell_check(void);
void gshell_doctor(void);

void gshell_panel(void);
void gshell_runtime(void);
void gshell_intent(void);
void gshell_system(void);
void gshell_dashboard(void);
void gshell_text_panel(void);
void gshell_text_dashboard(void);
void gshell_text_compact(void);
void gshell_statusbar(void);
void gshell_graphics_dashboard(void);
void gshell_input_char(char c);
void gshell_input_status(void);
void gshell_output(void);
void gshell_output_doctor(void);
void gshell_output_text(void);
void gshell_output_graphics(void);
void gshell_self_check(void);
void gshell_self_render(void);
void gshell_runtime_check(void);

void gshell_break(void);
void gshell_fix(void);

int gshell_is_ready(void);
int gshell_is_broken(void);

int gshell_output_graphics_planned(void);
const char* gshell_get_output_mode(void);
const char* gshell_get_output_plan(void);

unsigned int gshell_get_render_count(void);
const char* gshell_get_last_view(void);
const char* gshell_get_mode(void);

#endif