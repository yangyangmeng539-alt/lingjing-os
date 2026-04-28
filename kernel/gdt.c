#include "gdt.h"

struct gdt_entry {
    unsigned short limit_low;
    unsigned short base_low;
    unsigned char base_middle;
    unsigned char access;
    unsigned char granularity;
    unsigned char base_high;
} __attribute__((packed));

struct gdt_ptr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

static struct gdt_entry gdt_entries[5];
static struct gdt_ptr gdt_pointer;

extern void gdt_flush(unsigned int gdt_ptr_address);

static void gdt_set_gate(int index, unsigned int base, unsigned int limit, unsigned char access, unsigned char granularity) {
    gdt_entries[index].base_low = (unsigned short)(base & 0xFFFF);
    gdt_entries[index].base_middle = (unsigned char)((base >> 16) & 0xFF);
    gdt_entries[index].base_high = (unsigned char)((base >> 24) & 0xFF);

    gdt_entries[index].limit_low = (unsigned short)(limit & 0xFFFF);
    gdt_entries[index].granularity = (unsigned char)((limit >> 16) & 0x0F);

    gdt_entries[index].granularity |= (unsigned char)(granularity & 0xF0);
    gdt_entries[index].access = access;
}

void gdt_init(void) {
    gdt_pointer.limit = (unsigned short)(sizeof(struct gdt_entry) * 5 - 1);
    gdt_pointer.base = (unsigned int)&gdt_entries;

    gdt_set_gate(0, 0, 0, 0, 0);

    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    gdt_flush((unsigned int)&gdt_pointer);
}
