#include "paging.h"
#include "platform.h"

#define PAGE_SIZE 4096
#define PAGE_ENTRIES 1024

static unsigned int page_directory[PAGE_ENTRIES] __attribute__((aligned(PAGE_SIZE)));
static unsigned int first_page_table[PAGE_ENTRIES] __attribute__((aligned(PAGE_SIZE)));

static int paging_init_done = 0;
static int paging_ready = 0;
static int paging_broken = 0;
static int paging_enabled = 0;

static unsigned int paging_mapped_pages = 0;

static void paging_clear_tables(void) {
    int i = 0;

    for (i = 0; i < PAGE_ENTRIES; i++) {
        page_directory[i] = 0;
        first_page_table[i] = 0;
    }
}

static void paging_build_identity_map(void) {
    int i = 0;

    for (i = 0; i < PAGE_ENTRIES; i++) {
        first_page_table[i] = (i * PAGE_SIZE) | 0x3;
    }

    page_directory[0] = ((unsigned int)first_page_table) | 0x3;
    paging_mapped_pages = PAGE_ENTRIES;
}

static unsigned int paging_read_cr0(void) {
    unsigned int value = 0;

    __asm__ volatile ("mov %%cr0, %0" : "=r"(value));

    return value;
}

static void paging_write_cr0(unsigned int value) {
    __asm__ volatile ("mov %0, %%cr0" : : "r"(value) : "memory");
}

static void paging_write_cr3(unsigned int value) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(value) : "memory");
}

static int paging_cpu_enabled(void) {
    unsigned int cr0 = paging_read_cr0();

    if ((cr0 & 0x80000000) != 0) {
        return 1;
    }

    return 0;
}

void paging_init(void) {
    paging_clear_tables();
    paging_build_identity_map();

    paging_init_done = 1;
    paging_ready = 1;
    paging_broken = 0;
    paging_enabled = 0;
}

int paging_is_ready(void) {
    if (!paging_init_done) {
        return 0;
    }

    if (!paging_ready) {
        return 0;
    }

    if (paging_broken) {
        return 0;
    }

    if (page_directory[0] == 0) {
        return 0;
    }

    if (paging_mapped_pages == 0) {
        return 0;
    }

    return 1;
}

int paging_doctor_ok(void) {
    return paging_is_ready();
}

const char* paging_get_state(void) {
    return paging_is_ready() ? "ready" : "broken";
}

const char* paging_get_mode(void) {
    if (paging_enabled) {
        return "enabled";
    }

    return "identity-map-prototype";
}

unsigned int paging_get_directory_address(void) {
    return (unsigned int)page_directory;
}

unsigned int paging_get_table_address(void) {
    return (unsigned int)first_page_table;
}

unsigned int paging_get_mapped_pages(void) {
    return paging_mapped_pages;
}

int paging_is_identity_mapped(unsigned int address) {
    unsigned int page_index = address / PAGE_SIZE;
    unsigned int expected_base = 0;
    unsigned int actual_entry = 0;
    unsigned int actual_base = 0;

    if (page_index >= PAGE_ENTRIES) {
        return 0;
    }

    if (page_directory[0] == 0) {
        return 0;
    }

    actual_entry = first_page_table[page_index];

    if ((actual_entry & 0x1) == 0) {
        return 0;
    }

    if ((actual_entry & 0x2) == 0) {
        return 0;
    }

    expected_base = page_index * PAGE_SIZE;
    actual_base = actual_entry & 0xFFFFF000;

    if (actual_base != expected_base) {
        return 0;
    }

    return 1;
}
void paging_map_check(unsigned int address) {
    unsigned int page_index = address / PAGE_SIZE;
    unsigned int page_base = page_index * PAGE_SIZE;

    platform_print("Paging map check:\n");

    platform_print("  virtual:  ");
    platform_print_hex32(address);
    platform_print("\n");

    platform_print("  page:     ");
    platform_print_uint(page_index);
    platform_print("\n");

    platform_print("  page base:");
    platform_print_hex32(page_base);
    platform_print("\n");

    if (page_index < PAGE_ENTRIES) {
        platform_print("  entry:    ");
        platform_print_hex32(first_page_table[page_index]);
        platform_print("\n");
    } else {
        platform_print("  entry:    out of first table\n");
    }

    platform_print("  result:   ");

    if (paging_is_identity_mapped(address)) {
        platform_print("identity mapped\n");
    } else {
        platform_print("unmapped\n");
    }
}

void paging_enable(void) {
    unsigned int cr0 = 0;
    unsigned int directory_address = (unsigned int)page_directory;
    unsigned int table_address = (unsigned int)first_page_table;

    platform_print("Paging enable:\n");

    if (!paging_is_ready()) {
        platform_print("  result: blocked\n");
        platform_print("  reason: paging layer not ready\n");
        return;
    }

    if (paging_enabled || paging_cpu_enabled()) {
        paging_enabled = 1;
        platform_print("  result: already enabled\n");
        return;
    }

    if (!paging_is_identity_mapped(directory_address)) {
        platform_print("  result: blocked\n");
        platform_print("  reason: page directory not identity mapped\n");
        return;
    }

    if (!paging_is_identity_mapped(table_address)) {
        platform_print("  result: blocked\n");
        platform_print("  reason: first page table not identity mapped\n");
        return;
    }

    platform_print("  directory: ");
    platform_print_hex32(directory_address);
    platform_print("\n");

    platform_print("  first table:");
    platform_print_hex32(table_address);
    platform_print("\n");

    paging_write_cr3(directory_address);

    cr0 = paging_read_cr0();
    cr0 = cr0 | 0x80000000;
    paging_write_cr0(cr0);

    paging_enabled = paging_cpu_enabled();

    platform_print("  enabled:   ");
    platform_print(paging_enabled ? "yes\n" : "no\n");

    platform_print("  result:    ");
    platform_print(paging_enabled ? "ok\n" : "failed\n");
}

void paging_status(void) {
    platform_print("Paging layer:\n");

    platform_print("  state:       ");
    platform_print(paging_get_state());
    platform_print("\n");

    platform_print("  mode:        ");
    platform_print(paging_get_mode());
    platform_print("\n");

    paging_enabled = paging_cpu_enabled();

    platform_print("  enabled:     ");
    platform_print(paging_enabled ? "yes\n" : "no\n");

    platform_print("  directory:   ");
    platform_print_hex32(paging_get_directory_address());
    platform_print("\n");

    platform_print("  first table: ");
    platform_print_hex32(paging_get_table_address());
    platform_print("\n");

    platform_print("  pages:       ");
    platform_print_uint(paging_get_mapped_pages());
    platform_print("\n");
}

void paging_check(void) {
    platform_print("Paging check:\n");

    platform_print("  init:      ");
    platform_print(paging_init_done ? "ok\n" : "missing\n");

    platform_print("  directory: ");
    platform_print(page_directory[0] != 0 ? "ok\n" : "missing\n");

    platform_print("  identity:  ");
    platform_print(paging_mapped_pages > 0 ? "ok\n" : "missing\n");

    paging_enabled = paging_cpu_enabled();

    platform_print("  enabled:     ");
    platform_print(paging_enabled ? "yes\n" : "no\n");

    platform_print("  result:    ");
    platform_print(paging_doctor_ok() ? "ok\n" : "broken\n");
}

void paging_doctor(void) {
    platform_print("Paging doctor:\n");

    platform_print("  init:        ");
    platform_print(paging_init_done ? "ok\n" : "missing\n");

    platform_print("  ready:       ");
    platform_print(paging_ready ? "ok\n" : "bad\n");

    platform_print("  broken:      ");
    platform_print(paging_broken ? "yes\n" : "no\n");

    paging_enabled = paging_cpu_enabled();

    platform_print("  enabled:   ");
    platform_print(paging_enabled ? "yes\n" : "no\n");
    platform_print("  directory:   ");
    platform_print(page_directory[0] != 0 ? "ok\n" : "missing\n");

    platform_print("  mapped pages:");
    platform_print_uint(paging_mapped_pages);
    platform_print("\n");

    platform_print("  result:      ");
    platform_print(paging_doctor_ok() ? "ok\n" : "broken\n");
}

void paging_break(void) {
    paging_broken = 1;
    paging_ready = 0;

    platform_print("paging layer broken.\n");
}

void paging_fix(void) {
    paging_clear_tables();
    paging_build_identity_map();

    paging_init_done = 1;
    paging_ready = 1;
    paging_broken = 0;
    paging_enabled = paging_cpu_enabled();

    platform_print("paging layer fixed.\n");
}