
#include <stddef.h>
#include <stdint.h>

#include "dam/dam_config.h"
#include "dam/dam_log.h"
#include "dam/internal/dam_internal.h"

void* dam_general_malloc(size_t size) {

    size_t aligned_size = align_up(size);
    size_t actual_size = aligned_size + sizeof(uint32_t);
    actual_size = align_up(actual_size);

    pool_header_t* found_pool = NULL;
    block_header_t* found_block = NULL;

    found_block = find_block_in_pools(actual_size, &found_pool);

    if (!found_block) {
        size_t min_pool_size = POOL_HEADER_SIZE + HEAD_SIZE +  actual_size + MIN_BLOCK_SIZE;
        pool_header_t* new_pool = create_pool(min_pool_size);

        if (!new_pool) {
            DAM_LOG_ERR("[ALLOC] FAILED: Could not create new pool\n");
            return NULL;
        }

        found_block = find_block_in_pools(actual_size, &found_pool);

        if (!found_block) {
            DAM_LOG_ERR("[ALLOC] FAILED: Still no space after creating pool!\n");
            return NULL;
        }
    }

    stats.allocations++;

    DAM_LOG("[ALLOC] Found free block: size=%zu at %p\n", found_block->size, (void*)found_block);

    found_block->is_free = 0;
    found_block->magic = BLOCK_MAGIC;

    split_block_if_possible(found_block, actual_size);

    found_block->user_size = size;

    void* ptr = (char*)found_block + HEAD_SIZE;

    uint32_t* end_canary = (uint32_t*)((char*)ptr + found_block->user_size);
    *end_canary = CANARY_VALUE;

    DAM_LOG("[ALLOC] Returning pointer %p\n", ptr);
    return ptr;
}

void dam_general_free(void* ptr) {}

void* dam_general_realloc(void* ptr, size_t size) {
    return NULL;
}