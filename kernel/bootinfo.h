#ifndef LINGJING_BOOTINFO_H
#define LINGJING_BOOTINFO_H

void bootinfo_init(unsigned int magic, unsigned int info_addr);
void bootinfo_status(void);
void bootinfo_framebuffer(void);
void bootinfo_raw(void);
void bootinfo_check(void);
void bootinfo_doctor(void);
void bootinfo_probe(void);

int bootinfo_is_multiboot_valid(void);
int bootinfo_has_framebuffer(void);

unsigned int bootinfo_get_magic(void);
unsigned int bootinfo_get_info_addr(void);
unsigned int bootinfo_get_flags(void);

unsigned int bootinfo_get_framebuffer_addr_low(void);
unsigned int bootinfo_get_framebuffer_addr_high(void);
unsigned int bootinfo_get_framebuffer_pitch(void);
unsigned int bootinfo_get_framebuffer_width(void);
unsigned int bootinfo_get_framebuffer_height(void);
unsigned int bootinfo_get_framebuffer_bpp(void);
unsigned int bootinfo_get_framebuffer_type(void);
unsigned int bootinfo_get_requested_width(void);
unsigned int bootinfo_get_requested_height(void);
unsigned int bootinfo_get_requested_bpp(void);

#endif