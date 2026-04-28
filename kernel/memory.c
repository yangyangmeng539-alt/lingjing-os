#include "memory.h"

extern unsigned int end;

static unsigned int placement_address = 0;

static unsigned int align_up(unsigned int value, unsigned int align) {
    return (value + align - 1) & ~(align - 1);
}

void memory_init(void) {
    placement_address = align_up((unsigned int)&end, 0x1000);
}

unsigned int kmalloc(unsigned int size) {
    unsigned int address = placement_address;

    placement_address += size;
    placement_address = align_up(placement_address, 4);

    return address;
}

unsigned int kcalloc(unsigned int size) {
    unsigned int address = kmalloc(size);
    volatile unsigned char* ptr = (volatile unsigned char*)address;

    for (unsigned int i = 0; i < size; i++) {
        ptr[i] = 0;
    }

    return address;
}

unsigned int memory_get_placement_address(void) {
    return placement_address;
}