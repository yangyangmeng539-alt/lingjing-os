#ifndef LINGJING_FRAMEBUFFER_H
#define LINGJING_FRAMEBUFFER_H

void framebuffer_init(void);
void framebuffer_status(void);
void framebuffer_prepare(void);
void framebuffer_prepare_from_bootinfo(void);
void framebuffer_check(void);
void framebuffer_doctor(void);
void framebuffer_break(void);
void framebuffer_fix(void);

int framebuffer_is_ready(void);
int framebuffer_is_broken(void);

unsigned int framebuffer_get_width(void);
unsigned int framebuffer_get_height(void);
unsigned int framebuffer_get_bpp(void);
unsigned int framebuffer_get_pitch(void);
unsigned int framebuffer_get_address(void);
unsigned int framebuffer_get_type(void);


#endif