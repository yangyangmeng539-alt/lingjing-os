#ifndef LINGJING_PLATFORM_H
#define LINGJING_PLATFORM_H

void platform_init(void);

void platform_print(const char* text);
void platform_put_char(char c);
void platform_print_uint(unsigned int value);
void platform_print_hex32(unsigned int value);

unsigned int platform_ticks(void);
unsigned int platform_seconds(void);

void platform_status(void);
void platform_check(void);
int platform_doctor_ok(void);

const char* platform_get_name(void);
const char* platform_get_display(void);
const char* platform_get_timer(void);
const char* platform_get_input(void);

#endif