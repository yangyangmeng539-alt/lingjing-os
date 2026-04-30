#ifndef LINGJING_GDT_H
#define LINGJING_GDT_H

void gdt_init(void);

int gdt_user_segments_installed(void);
void gdt_install_user_segments(void);

int gdt_tss_descriptor_installed(void);
int gdt_tss_loaded(void);
void gdt_install_tss(unsigned int esp0);

#endif