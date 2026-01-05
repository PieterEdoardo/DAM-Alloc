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

    size_classes = mmap(NULL,
    size_class_count * sizeof(size_class_t),
    PROT_READ | PROT_WRITE,
    MAP_PRIVATE | MAP_ANONYMOUS,
    -1, 0);

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
    // create_small_pool() guarantees free_list != NULL on success
    size_t pool_size = align_up(size_classes[class_index].block_size * 1000 + sizeof(pool_header_t), ALIGNMENT);

    DAM_LOG("[POOL] Creating pool #%zu of %zu bytes...\n", sizeof(size_classes->pools), pool_size);

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
    new_pool->free_list_head = (block_header_t*)((char*)memory + POOL_SMALL_SIZE);
    size_classes[class_index].pools = new_pool;

    stats.pools_created++;
    DAM_LOG("[POOL] Created at %p with %zu bytes usable. Total pools: %zu\n", memory, new_pool->free_list_head->size, stats.pools_created);
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
        return NULL;
    }

    size_class_block_t* block = size_class->free_list;
    size_class->free_list = block->next;

    block->is_free = 0;
    block->magic = BLOCK_MAGIC;

    return block + 1;
}

void dam_small_free(void* ptr) {
    size_class_block_t* block = (size_class_block_t*)ptr - 1;

    if (block->magic != BLOCK_MAGIC) {
        DAM_LOG_ERR("Invalid small free %p", ptr);
        return;
    }

    uint8_t class = block->size_class_index;
    block->is_free = 1;
    block->magic = FREED_MAGIC;

    block->next = size_classes[class].free_list;
    size_classes[class].free_list = block;
}

