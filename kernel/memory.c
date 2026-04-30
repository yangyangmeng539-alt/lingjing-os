#include "memory.h"
#include "platform.h"

extern unsigned int end;

#define HEAP_FREE_MAX 32

typedef struct heap_free_entry {
    unsigned int address;
    unsigned int size;
    unsigned int used;
} heap_free_entry_t;

static unsigned int placement_address = 0;

static heap_free_entry_t heap_free_list[HEAP_FREE_MAX];

static unsigned int heap_alloc_count = 0;
static unsigned int heap_free_count = 0;
static unsigned int heap_reuse_count = 0;
static unsigned int heap_failed_free_count = 0;

static unsigned int align_up(unsigned int value, unsigned int align) {
    return (value + align - 1) & ~(align - 1);
}

static void heap_free_list_clear(void) {
    for (int i = 0; i < HEAP_FREE_MAX; i++) {
        heap_free_list[i].address = 0;
        heap_free_list[i].size = 0;
        heap_free_list[i].used = 0;
    }
}

static int heap_find_free_slot(void) {
    for (int i = 0; i < HEAP_FREE_MAX; i++) {
        if (heap_free_list[i].used == 0) {
            return i;
        }
    }

    return -1;
}

static int heap_find_reusable_block(unsigned int size) {
    for (int i = 0; i < HEAP_FREE_MAX; i++) {
        if (heap_free_list[i].used != 0 && heap_free_list[i].size >= size) {
            return i;
        }
    }

    return -1;
}

void memory_init(void) {
    placement_address = align_up((unsigned int)&end, 0x1000);

    heap_free_list_clear();

    heap_alloc_count = 0;
    heap_free_count = 0;
    heap_reuse_count = 0;
    heap_failed_free_count = 0;
}

unsigned int kmalloc(unsigned int size) {
    unsigned int address = 0;
    int reusable_index = -1;

    if (size == 0) {
        return 0;
    }

    size = align_up(size, 4);

    reusable_index = heap_find_reusable_block(size);

    if (reusable_index >= 0) {
        address = heap_free_list[reusable_index].address;

        heap_free_list[reusable_index].address = 0;
        heap_free_list[reusable_index].size = 0;
        heap_free_list[reusable_index].used = 0;

        heap_alloc_count++;
        heap_reuse_count++;

        return address;
    }

    address = placement_address;

    placement_address += size;
    placement_address = align_up(placement_address, 4);

    heap_alloc_count++;

    return address;
}

unsigned int kcalloc(unsigned int size) {
    unsigned int address = kmalloc(size);
    volatile unsigned char* ptr = (volatile unsigned char*)address;

    if (address == 0) {
        return 0;
    }

    for (unsigned int i = 0; i < size; i++) {
        ptr[i] = 0;
    }

    return address;
}

void kfree(unsigned int address) {
    int slot = -1;

    if (address == 0) {
        heap_failed_free_count++;
        return;
    }

    slot = heap_find_free_slot();

    if (slot < 0) {
        heap_failed_free_count++;
        return;
    }

    heap_free_list[slot].address = address;
    heap_free_list[slot].size = 4;
    heap_free_list[slot].used = 1;

    heap_free_count++;
}

unsigned int memory_get_placement_address(void) {
    return placement_address;
}

unsigned int memory_get_alloc_count(void) {
    return heap_alloc_count;
}

unsigned int memory_get_free_count(void) {
    return heap_free_count;
}

unsigned int memory_get_reuse_count(void) {
    return heap_reuse_count;
}

unsigned int memory_get_free_list_count(void) {
    unsigned int count = 0;

    for (int i = 0; i < HEAP_FREE_MAX; i++) {
        if (heap_free_list[i].used != 0) {
            count++;
        }
    }

    return count;
}

int memory_doctor_ok(void) {
    if (placement_address == 0) {
        return 0;
    }

    if (heap_failed_free_count > 0) {
        return 0;
    }

    return 1;
}

void memory_print_status(void) {
    platform_print("Memory:\n");

    platform_print("  next alloc: ");
    platform_print_hex32(memory_get_placement_address());
    platform_print("\n");

    platform_print("  allocator:  free-list prototype\n");

    platform_print("  allocs:     ");
    platform_print_uint(memory_get_alloc_count());
    platform_print("\n");

    platform_print("  frees:      ");
    platform_print_uint(memory_get_free_count());
    platform_print("\n");

    platform_print("  reuses:     ");
    platform_print_uint(memory_get_reuse_count());
    platform_print("\n");

    platform_print("  free list:  ");
    platform_print_uint(memory_get_free_list_count());
    platform_print("\n");

    platform_print("  failed free:");
    platform_print_uint(heap_failed_free_count);
    platform_print("\n");
}

void memory_check(void) {
    platform_print("Heap check:\n");

    platform_print("  placement: ");
    platform_print_hex32(placement_address);
    platform_print("\n");

    platform_print("  free list: ");
    platform_print_uint(memory_get_free_list_count());
    platform_print("\n");

    platform_print("  result:    ");
    platform_print(memory_doctor_ok() ? "ok\n" : "broken\n");
}

void memory_doctor(void) {
    platform_print("Heap doctor:\n");

    platform_print("  placement:   ");
    platform_print(placement_address != 0 ? "ok\n" : "missing\n");

    platform_print("  alloc count: ");
    platform_print_uint(heap_alloc_count);
    platform_print("\n");

    platform_print("  free count:  ");
    platform_print_uint(heap_free_count);
    platform_print("\n");

    platform_print("  reuse count: ");
    platform_print_uint(heap_reuse_count);
    platform_print("\n");

    platform_print("  free list:   ");
    platform_print_uint(memory_get_free_list_count());
    platform_print("\n");

    platform_print("  failed free: ");
    platform_print_uint(heap_failed_free_count);
    platform_print("\n");

    platform_print("  result:      ");
    platform_print(memory_doctor_ok() ? "ok\n" : "broken\n");
}

void memory_break(void) {
    heap_failed_free_count++;

    platform_print("heap layer broken.\n");
}

void memory_fix(void) {
    heap_failed_free_count = 0;

    platform_print("heap layer fixed.\n");
}