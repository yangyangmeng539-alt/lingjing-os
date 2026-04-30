#include "memory.h"
#include "platform.h"

extern unsigned int end;

#define HEAP_FREE_MAX 32
#define HEAP_MAGIC_ALLOC 0xC0FFEE01
#define HEAP_MAGIC_FREE  0xDEADFA11

typedef struct heap_block_header {
    unsigned int magic;
    unsigned int size;
    unsigned int freed;
} heap_block_header_t;

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
static unsigned int heap_double_free_count = 0;
static unsigned int heap_allocated_bytes = 0;
static unsigned int heap_freed_bytes = 0;

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

static heap_block_header_t* heap_header_from_payload(unsigned int address) {
    return (heap_block_header_t*)(address - sizeof(heap_block_header_t));
}

static unsigned int heap_payload_from_header(heap_block_header_t* header) {
    return ((unsigned int)header) + sizeof(heap_block_header_t);
}

void memory_init(void) {
    placement_address = align_up((unsigned int)&end, 0x1000);

    heap_free_list_clear();

    heap_alloc_count = 0;
    heap_free_count = 0;
    heap_reuse_count = 0;
    heap_failed_free_count = 0;
    heap_double_free_count = 0;
    heap_allocated_bytes = 0;
    heap_freed_bytes = 0;
}

unsigned int kmalloc(unsigned int size) {
    unsigned int payload_address = 0;
    unsigned int total_size = 0;
    int reusable_index = -1;
    heap_block_header_t* header = 0;

    if (size == 0) {
        return 0;
    }

    size = align_up(size, 4);
    total_size = align_up(size + sizeof(heap_block_header_t), 4);

    reusable_index = heap_find_reusable_block(total_size);

    if (reusable_index >= 0) {
        header = (heap_block_header_t*)heap_free_list[reusable_index].address;

        heap_free_list[reusable_index].address = 0;
        heap_free_list[reusable_index].size = 0;
        heap_free_list[reusable_index].used = 0;

        header->magic = HEAP_MAGIC_ALLOC;
        header->size = size;
        header->freed = 0;

        heap_alloc_count++;
        heap_reuse_count++;
        heap_allocated_bytes += size;

        return heap_payload_from_header(header);
    }

    header = (heap_block_header_t*)placement_address;
    header->magic = HEAP_MAGIC_ALLOC;
    header->size = size;
    header->freed = 0;

    payload_address = heap_payload_from_header(header);

    placement_address += total_size;
    placement_address = align_up(placement_address, 4);

    heap_alloc_count++;
    heap_allocated_bytes += size;

    return payload_address;
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
    heap_block_header_t* header = 0;
    unsigned int total_size = 0;

    if (address == 0) {
        heap_failed_free_count++;
        return;
    }

    header = heap_header_from_payload(address);

    if (header->magic == HEAP_MAGIC_FREE || header->freed != 0) {
        heap_double_free_count++;
        heap_failed_free_count++;
        return;
    }

    if (header->magic != HEAP_MAGIC_ALLOC) {
        heap_failed_free_count++;
        return;
    }

    slot = heap_find_free_slot();

    if (slot < 0) {
        heap_failed_free_count++;
        return;
    }

    total_size = align_up(header->size + sizeof(heap_block_header_t), 4);

    header->magic = HEAP_MAGIC_FREE;
    header->freed = 1;

    heap_free_list[slot].address = (unsigned int)header;
    heap_free_list[slot].size = total_size;
    heap_free_list[slot].used = 1;

    heap_free_count++;
    heap_freed_bytes += header->size;
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

unsigned int memory_get_failed_free_count(void) {
    return heap_failed_free_count;
}

unsigned int memory_get_double_free_count(void) {
    return heap_double_free_count;
}

unsigned int memory_get_allocated_bytes(void) {
    return heap_allocated_bytes;
}

unsigned int memory_get_freed_bytes(void) {
    return heap_freed_bytes;
}

unsigned int memory_get_live_alloc_count(void) {
    if (heap_alloc_count < heap_free_count) {
        return 0;
    }

    return heap_alloc_count - heap_free_count;
}

unsigned int memory_get_live_bytes(void) {
    if (heap_allocated_bytes < heap_freed_bytes) {
        return 0;
    }

    return heap_allocated_bytes - heap_freed_bytes;
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

    if (heap_double_free_count > 0) {
        return 0;
    }

    return 1;
}

void memory_print_status(void) {
    platform_print("Memory:\n");

    platform_print("  next alloc: ");
    platform_print_hex32(memory_get_placement_address());
    platform_print("\n");

    platform_print("  allocator:  header + free-list prototype\n");

    platform_print("  allocs:     ");
    platform_print_uint(memory_get_alloc_count());
    platform_print("\n");

    platform_print("  frees:      ");
    platform_print_uint(memory_get_free_count());
    platform_print("\n");

    platform_print("  live:       ");
    platform_print_uint(memory_get_live_alloc_count());
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

    platform_print("  double free:");
    platform_print_uint(heap_double_free_count);
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

    platform_print("  live:      ");
    platform_print_uint(memory_get_live_alloc_count());
    platform_print("\n");

    platform_print("  failed:    ");
    platform_print_uint(heap_failed_free_count);
    platform_print("\n");

    platform_print("  double:    ");
    platform_print_uint(heap_double_free_count);
    platform_print("\n");

    platform_print("  result:    ");
    platform_print(memory_doctor_ok() ? "ok\n" : "broken\n");
}

void memory_stats(void) {
    platform_print("Heap stats:\n");

    platform_print("  allocated bytes: ");
    platform_print_uint(memory_get_allocated_bytes());
    platform_print("\n");

    platform_print("  freed bytes:     ");
    platform_print_uint(memory_get_freed_bytes());
    platform_print("\n");

    platform_print("  live bytes:      ");
    platform_print_uint(memory_get_live_bytes());
    platform_print("\n");

    platform_print("  alloc count:     ");
    platform_print_uint(heap_alloc_count);
    platform_print("\n");

    platform_print("  free count:      ");
    platform_print_uint(heap_free_count);
    platform_print("\n");

    platform_print("  reuse count:     ");
    platform_print_uint(heap_reuse_count);
    platform_print("\n");

    platform_print("  failed free:     ");
    platform_print_uint(heap_failed_free_count);
    platform_print("\n");

    platform_print("  double free:     ");
    platform_print_uint(heap_double_free_count);
    platform_print("\n");
}

void memory_doctor(void) {
    platform_print("Heap doctor:\n");

    platform_print("  placement:   ");
    platform_print(placement_address != 0 ? "ok\n" : "missing\n");

    platform_print("  allocator:   header + free-list\n");

    platform_print("  alloc count: ");
    platform_print_uint(heap_alloc_count);
    platform_print("\n");

    platform_print("  free count:  ");
    platform_print_uint(heap_free_count);
    platform_print("\n");

    platform_print("  live count:  ");
    platform_print_uint(memory_get_live_alloc_count());
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

    platform_print("  double free: ");
    platform_print_uint(heap_double_free_count);
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
    heap_double_free_count = 0;

    platform_print("heap layer fixed.\n");
}