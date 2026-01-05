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
        size_classes[size_class_count].block_size = block_size;
        size_classes[size_class_count].free_list = NULL;
        size_classes[size_class_count].pools = NULL;

        size_class_count++;
        block_size *= SIZE_CLASS_MULTIPLIER;
    }

    DAM_LOG("[INIT] Small allocator initialized (%zu classes)\n", size_class_count);
}

static pool_header_t* create_small_pool(uint8_t class_index) {}

uint8_t size_to_class(size_t size) {
    for (uint8_t i = 0; i < size_class_count; i++) {
        if (size_classes[i].block_size == size) {
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

