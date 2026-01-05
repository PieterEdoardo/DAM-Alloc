#include <sys/mman.h>

#include "dam/dam_config.h"
#include "dam/dam_log.h"


/**********************************************************
 * allocator                                              *
 *  └── size_classes[]          ← array                   *
 *      └── pools               ← linked list             *
 *          └── blocks          ← linked list (free list) *
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
        DAM_LOG_ERR("[INIT] mmap failed for size class initialization");
        return;
    }

    block_size = DAM_SMALL_MIN;
    for (size_t j = 0; j < size_class_count; j++) {
        size_classes[j].block_size = block_size;
        size_classes[j].free_list = NULL;
        size_classes[j].pools = NULL;
        block_size *= SIZE_CLASS_MULTIPLIER;
    }

    DAM_LOG("[INIT] Small allocator initialized (%zu classes)\n", size_class_count);
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
        DAM_LOG_ERR("mmap failed for new pool");
        return NULL;
    }

    pool_header_t* new_pool = memory;
    new_pool->memory = memory;
    new_pool->size = pool_size;
    new_pool->type = DAM_POOL_SMALL;
    new_pool->next = size_classes[class_index].pools;
    size_classes[class_index].pools = new_pool;

    size_t block_stride = SIZE_CLASS_HEADER_SIZE + size_classes[class_index].block_size;
    char* cursor = (char*)memory + align_up(sizeof(pool_header_t), ALIGNMENT);
    char* pool_end = (char*)memory + pool_size;

    size_class_header_t* free_list = NULL;

    for (size_t i = 0; i < SIZE_CLASS_BLOCKS_PER_POOL; i++) {
        if (cursor + block_stride > pool_end) {
            break;
        }

        size_class_header_t* block = (size_class_header_t*)cursor;

        block->magic = FREED_MAGIC;
        block->is_free = 1;
        block->size_class_index = class_index;
        block->next = free_list;
        free_list = block;

        cursor += block_stride;
    }
    size_classes[class_index].free_list = free_list;

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

void* dam_small_malloc(size_t size) {
    uint8_t class = size_to_class(size);
    size_class_t* size_class = &size_classes[class];

    if (!size_class->free_list && !create_small_pool(class)) {
        DAM_LOG_ERR("[ALLOC] No free list and Could not create new pool.");
        return NULL;
    }

    size_class_header_t* block = size_class->free_list;
    size_class->free_list = block->next;

    stats.allocations++;
    DAM_LOG("[ALLOC] Found free size class block: class=%u (%zuB) block=%p", class, size_class->block_size, (void*)block);

    block->is_free = 0;
    block->magic = BLOCK_MAGIC;

    void* ptr = (char*)block + SIZE_CLASS_HEADER_SIZE;

    DAM_LOG("[ALLOC] Returning pointer %p", ptr);
    return ptr;
}

void dam_small_free(void* ptr) {
    size_class_header_t* header = (size_class_header_t*)ptr - 1;

    // Double free checks
    if (header->magic == FREED_MAGIC) {
        DAM_LOG_ERR("[FREE] Double free detected at %p!", ptr);
        return;
    }

    // Invalid pointer checks
    if (header->magic != BLOCK_MAGIC) {
        DAM_LOG_ERR("[FREE] Invalid pointer passed to dam_free: %p", ptr);
        return;
    }

    uint8_t class = header->size_class_index;
    header->is_free = 1;
    header->magic = FREED_MAGIC;

    header->next = size_classes[class].free_list;
    size_classes[class].free_list = header;

    DAM_LOG("[FREE] Pointer %p freed", ptr);
}

