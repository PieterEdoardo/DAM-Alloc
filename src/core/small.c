#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "dam/dam.h"
#include "dam/dam_config.h"
#include "dam/dam_log.h"


/**********************************************************
 * Small allocator
 *
 * size_classes[]          ← array (per size class)
 * ├─ size_classes[i]
 * │   ├─ free_class_list  ← linked list (blocks)
 * │   └─ pools            ← linked list (backing pools)
 * │
 * └─ size_classes[N]
 *
 * Blocks belong to size classes.
 * Pools only provide memory.
 **********************************************************/
static size_class_t* size_classes;
static size_t size_class_count = 0;

void dam_small_init(void) {
    size_t block_size = DAM_SMALL_MIN;
    size_class_count = 0;

    while (block_size <= DAM_SMALL_MAX) {
        size_class_count++;
        block_size *= SIZE_CLASS_MULTIPLIER;
    }

    size_classes = mmap(
        NULL,
        size_class_count * sizeof(size_class_t),
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );

    if (size_classes == MAP_FAILED) {
        DAM_LOG_ERROR("[INIT] mmap failed for size class initialization");
        return;
    }

    block_size = DAM_SMALL_MIN;
    for (size_t j = 0; j < size_class_count; j++) {
        size_classes[j].block_size = block_size;
        size_classes[j].free_class_list = NULL;
        size_classes[j].pools = NULL;
        block_size *= SIZE_CLASS_MULTIPLIER;
    }

    DAM_LOG("[INIT] Small allocator initialized (%zu classes)", size_class_count);
}

static pool_header_t* create_small_pool(uint8_t class_index) {
    size_t usable_bytes = (sizeof(size_class_header_t) + size_classes[class_index].block_size) * SIZE_CLASS_BLOCKS_PER_POOL;
    size_t pool_size = align_up( sizeof(pool_header_t) + usable_bytes, ALIGNMENT);

    DAM_LOG("[POOL] Creating size class pool for class %zuB with total size of %zuB...", size_classes[class_index].block_size, pool_size);

    void* memory = mmap(
        NULL,
        pool_size,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );

    if (memory == MAP_FAILED) {
        DAM_LOG_ERROR("mmap failed for new pool");
        return NULL;
    }

    pool_header_t* new_pool = memory;
    new_pool->memory = memory;
    new_pool->size = pool_size;
    new_pool->type = DAM_POOL_SMALL;

    dam_register_pool(new_pool);

    size_t block_stride = SIZE_CLASS_HEADER_SIZE + size_classes[class_index].block_size;
    char* cursor = (char*)memory + align_up(sizeof(pool_header_t), ALIGNMENT);
    char* pool_end = (char*)memory + pool_size;

    size_class_header_t* free_class_list = NULL;

    for (size_t i = 0; i < SIZE_CLASS_BLOCKS_PER_POOL; i++) {
        if (cursor + block_stride > pool_end) {
            break;
        }

        size_class_header_t* block = (size_class_header_t*)cursor;

        block->magic = FREED_MAGIC;
        block->is_free = 1;
        block->size_class_index = class_index;
        block->next = free_class_list;
        free_class_list = block;

        cursor += block_stride;
    }
    size_classes[class_index].free_class_list = free_class_list;

    stats.pools_created++;
    DAM_LOG("[POOL] Created at %p with %zu bytes usable. Total pools: %zu", memory, pool_size, stats.pools_created);
    return new_pool;
}

uint8_t size_to_class(size_t size) {
    for (size_t i = 0; i < size_class_count; i++) {
        if (size <= size_classes[i].block_size) {
            return i;
        }
    }

    // Should never happen if caller checks DAM_SMALL_MAX
    return size_class_count - 1;
}

void* small_alloc_class(size_class_t*) {

}

void* dam_small_malloc(size_t size) {
    uint8_t class = size_to_class(size);
    size_class_t* size_class = &size_classes[class];

    if (!size_class->free_class_list && !create_small_pool(class)) {
        DAM_LOG_ERROR("[ALLOC] No free list and Could not create new pool.");
        return NULL;
    }

    size_class_header_t* block = size_class->free_class_list;
    size_class->free_class_list = block->next;

    stats.allocations++;
    DAM_LOG("[ALLOC] Found free size class block: class=%u (%zuB) block=%p", class, size_class->block_size, (void*)block);

    block->is_free = 0;
    block->magic = BLOCK_MAGIC;

    void* ptr = (char*)block + SIZE_CLASS_HEADER_SIZE;

    DAM_LOG("[ALLOC] Returning pointer %p", ptr);
    return ptr;
}

void* dam_small_realloc(void* ptr, size_t size) {
    size_class_header_t* header = get_size_class_header(ptr);
    uint8_t current_index = header->size_class_index;

    // Stays within small
    void * new_ptr;
    if (size <= DAM_SMALL_MAX) {
        //case A requested class aligns to same as current
        uint8_t requested_index = size_to_class(size);
        if (requested_index <= current_index) {
            return ptr;
        }

        new_ptr = dam_small_malloc(size);
        if (!new_ptr) return NULL;

        size_t copy_size = size_classes[current_index].block_size;
        memcpy(new_ptr, ptr, copy_size);
        dam_small_free(ptr);
        return new_ptr;
    }

    // case 2: Grows beyond small
    new_ptr = dam_malloc_internal(size);
    if (!new_ptr) return NULL;

    size_t copy_size = size_classes[current_index].block_size;
    memcpy(new_ptr, ptr, copy_size);
    dam_free_internal(ptr);

    return new_ptr;
}

void dam_small_free(void* ptr) {
    size_class_header_t* header = (size_class_header_t*)ptr - 1;

    // Double free checks
    if (header->magic == FREED_MAGIC) {
        DAM_LOG_ERROR("[FREE] Double free detected at %p!", ptr);
        return;
    }

    // Invalid pointer checks
    if (header->magic != BLOCK_MAGIC) {
        DAM_LOG_ERROR("[FREE] Invalid pointer passed to dam_free: %p", ptr);
        return;
    }

    uint8_t class = header->size_class_index;
    header->is_free = 1;
    header->magic = FREED_MAGIC;

    header->next = size_classes[class].free_class_list;
    size_classes[class].free_class_list = header;

    DAM_LOG("[FREE] Pointer %p freed", ptr);
}

size_class_header_t* get_size_class_header(void* ptr) {
    return (size_class_header_t*)ptr - 1;
}