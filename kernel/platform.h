#ifndef LINGJING_PLATFORM_H
#define LINGJING_PLATFORM_H

void platform_init(void);

void platform_print(const char* text);
void platform_put_char(char c);
void platform_print_uint(unsigned int value);
void platform_print_hex32(unsigned int value);
void platform_clear(void);

char platform_read_char(void);

unsigned int platform_ticks(void);
unsigned int platform_seconds(void);
void platform_sleep(unsigned int seconds);

void platform_reboot(void);
void platform_halt(void);

void platform_status(void);
void platform_check(void);
void platform_deps(void);
void platform_boot_info(void);
void platform_summary(void);
int platform_doctor_ok(void);

const char* platform_get_name(void);
const char* platform_get_display(void);
const char* platform_get_timer(void);
const char* platform_get_input(void);

#endif