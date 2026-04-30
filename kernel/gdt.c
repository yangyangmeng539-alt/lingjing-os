#include "gdt.h"

#define GDT_KERNEL_CODE_SELECTOR 0x08
#define GDT_KERNEL_DATA_SELECTOR 0x10
#define GDT_USER_CODE_SELECTOR   0x1B
#define GDT_USER_DATA_SELECTOR   0x23
#define GDT_TSS_SELECTOR         0x28

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

struct tss_entry {
    unsigned int prev_tss;
    unsigned int esp0;
    unsigned int ss0;
    unsigned int esp1;
    unsigned int ss1;
    unsigned int esp2;
    unsigned int ss2;
    unsigned int cr3;
    unsigned int eip;
    unsigned int eflags;
    unsigned int eax;
    unsigned int ecx;
    unsigned int edx;
    unsigned int ebx;
    unsigned int esp;
    unsigned int ebp;
    unsigned int esi;
    unsigned int edi;
    unsigned int es;
    unsigned int cs;
    unsigned int ss;
    unsigned int ds;
    unsigned int fs;
    unsigned int gs;
    unsigned int ldt;
    unsigned short trap;
    unsigned short iomap_base;
} __attribute__((packed));

static struct gdt_entry gdt_entries[6];
static struct gdt_ptr gdt_pointer;
static struct tss_entry tss_entry;

static int gdt_user_segments_ready = 0;
static int gdt_tss_ready = 0;
static int gdt_tss_loaded_state = 0;

extern void gdt_flush(unsigned int gdt_ptr_address);
extern void gdt_load_tss(unsigned int selector);

static void memory_zero(void* ptr, unsigned int size) {
    unsigned char* bytes = (unsigned char*)ptr;

    for (unsigned int i = 0; i < size; i++) {
        bytes[i] = 0;
    }
}

static void gdt_set_gate(int index, unsigned int base, unsigned int limit, unsigned char access, unsigned char granularity) {
    gdt_entries[index].base_low = (unsigned short)(base & 0xFFFF);
    gdt_entries[index].base_middle = (unsigned char)((base >> 16) & 0xFF);
    gdt_entries[index].base_high = (unsigned char)((base >> 24) & 0xFF);

    gdt_entries[index].limit_low = (unsigned short)(limit & 0xFFFF);
    gdt_entries[index].granularity = (unsigned char)((limit >> 16) & 0x0F);

    gdt_entries[index].granularity |= (unsigned char)(granularity & 0xF0);
    gdt_entries[index].access = access;
}

static void gdt_write_tss(unsigned int esp0) {
    unsigned int base = (unsigned int)&tss_entry;
    unsigned int limit = sizeof(struct tss_entry) - 1;

    memory_zero(&tss_entry, sizeof(struct tss_entry));

    tss_entry.ss0 = GDT_KERNEL_DATA_SELECTOR;
    tss_entry.esp0 = esp0;

    tss_entry.cs = GDT_USER_CODE_SELECTOR;
    tss_entry.ss = GDT_USER_DATA_SELECTOR;
    tss_entry.ds = GDT_USER_DATA_SELECTOR;
    tss_entry.es = GDT_USER_DATA_SELECTOR;
    tss_entry.fs = GDT_USER_DATA_SELECTOR;
    tss_entry.gs = GDT_USER_DATA_SELECTOR;

    tss_entry.iomap_base = sizeof(struct tss_entry);

    gdt_set_gate(5, base, limit, 0x89, 0x00);
}

void gdt_init(void) {
    gdt_pointer.limit = (unsigned short)(sizeof(struct gdt_entry) * 6 - 1);
    gdt_pointer.base = (unsigned int)&gdt_entries;

    gdt_set_gate(0, 0, 0, 0, 0);

    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    gdt_set_gate(5, 0, 0, 0, 0);

    gdt_user_segments_ready = 1;
    gdt_tss_ready = 0;
    gdt_tss_loaded_state = 0;

    gdt_flush((unsigned int)&gdt_pointer);
}

int gdt_user_segments_installed(void) {
    if (!gdt_user_segments_ready) {
        return 0;
    }

    if (gdt_entries[3].access != 0xFA) {
        return 0;
    }

    if (gdt_entries[4].access != 0xF2) {
        return 0;
    }

    if ((gdt_entries[3].granularity & 0xF0) != 0xC0) {
        return 0;
    }

    if ((gdt_entries[4].granularity & 0xF0) != 0xC0) {
        return 0;
    }

    return 1;
}

void gdt_install_user_segments(void) {
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    gdt_user_segments_ready = 1;

    gdt_flush((unsigned int)&gdt_pointer);
}

int gdt_tss_descriptor_installed(void) {
    if (!gdt_tss_ready) {
        return 0;
    }

    if (gdt_entries[5].base_low == 0 && gdt_entries[5].base_middle == 0 && gdt_entries[5].base_high == 0) {
        return 0;
    }

    if (gdt_entries[5].access != 0x89 && gdt_entries[5].access != 0x8B) {
        return 0;
    }

    if (tss_entry.ss0 != GDT_KERNEL_DATA_SELECTOR) {
        return 0;
    }

    if (tss_entry.esp0 == 0) {
        return 0;
    }

    return 1;
}

int gdt_tss_loaded(void) {
    if (!gdt_tss_loaded_state) {
        return 0;
    }

    if (!gdt_tss_descriptor_installed()) {
        return 0;
    }

    return 1;
}

void gdt_install_tss(unsigned int esp0) {
    gdt_write_tss(esp0);

    gdt_tss_ready = 1;

    gdt_flush((unsigned int)&gdt_pointer);
    gdt_load_tss(GDT_TSS_SELECTOR);

    gdt_tss_loaded_state = 1;
}