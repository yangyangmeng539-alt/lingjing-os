#ifndef LINGJING_MEMORY_H
#define LINGJING_MEMORY_H

void memory_init(void);
unsigned int kmalloc(unsigned int size);
unsigned int kcalloc(unsigned int size);
unsigned int memory_get_placement_address(void);

#endif