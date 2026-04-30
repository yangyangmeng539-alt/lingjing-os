#ifndef LINGJING_MEMORY_H
#define LINGJING_MEMORY_H

void memory_init(void);

unsigned int kmalloc(unsigned int size);
unsigned int kcalloc(unsigned int size);
void kfree(unsigned int address);

unsigned int memory_get_placement_address(void);

unsigned int memory_get_alloc_count(void);
unsigned int memory_get_free_count(void);
unsigned int memory_get_reuse_count(void);
unsigned int memory_get_free_list_count(void);

int memory_doctor_ok(void);

void memory_print_status(void);
void memory_check(void);
void memory_doctor(void);
void memory_break(void);
void memory_fix(void);

#endif