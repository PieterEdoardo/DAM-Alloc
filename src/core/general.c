
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

#include "dam/dam.h"
#include "dam/dam_config.h"
#include "dam/dam_log.h"
#include "dam/internal/dam_internal.h"

typedef struct pool_header pool_header_t;

void  dam_general_init() {
    if (!create_general_pool(INITIAL_POOL_SIZE)) {
        DAM_LOG_ERR("Failed to create initial pool");
    }
}

void* dam_general_malloc(size_t size) {

    size_t aligned_size = align_up(size, ALIGNMENT);
    size_t actual_size = aligned_size + sizeof(uint32_t);
    actual_size = align_up(actual_size, ALIGNMENT);

    pool_header_t* found_pool = NULL;
    block_header_t* found_block = NULL;

    found_block = find_block_in_pools(actual_size, &found_pool);

    if (!found_block) {
        size_t min_pool_size = POOL_GENERAL_SIZE + HEAD_SIZE +  actual_size + MIN_BLOCK_SIZE;
        pool_header_t* new_pool = create_general_pool(min_pool_size);

        if (!new_pool) {
            DAM_LOG_ERR("[ALLOC] FAILED: Could not create new pool");
            return NULL;
        }

        found_block = find_block_in_pools(actual_size, &found_pool);

        if (!found_block) {
            DAM_LOG_ERR("[ALLOC] FAILED: Still no space after creating pool!");
            return NULL;
        }
    }

    stats.allocations++;

    DAM_LOG("[ALLOC] Found free block: size=%zu at %p", found_block->size, (void*)found_block);

    found_block->is_free = 0;
    found_block->magic = BLOCK_MAGIC;

    split_block_if_possible(found_block, actual_size);

    found_block->user_size = size;

    void* ptr = (char*)found_block + HEAD_SIZE;

    uint32_t* end_canary = (uint32_t*)((char*)ptr + found_block->user_size);
    *end_canary = CANARY_VALUE;

    DAM_LOG("[ALLOC] Returning pointer %p", ptr);
    return ptr;
}

void dam_general_free(void* ptr, pool_header_t* pool_header) {

    block_header_t* header = (block_header_t*)((char*)ptr - HEAD_SIZE);


    // Double free checks
    if (header->magic == FREED_MAGIC) {
        DAM_LOG_ERR("Double free detected at %p!", ptr);
        return;
    }

    // Alignment check
    if ((uintptr_t)ptr % ALIGNMENT != 0) {
        DAM_LOG_ERR("Invalid pointer passed to dam_free: %p", ptr);
        return;
    }

    // Invalid pointer checks
    if (header->magic != BLOCK_MAGIC) {
        DAM_LOG_ERR("Invalid pointer passed to dam_free: %p", ptr);
        return;
    }

    // Header sanity check
    if (header->size == 0) {
        DAM_LOG_ERR("Pointer passed to dam_free refers to header with invalid size: %p", ptr);

        return;
    }

    unsigned int* end_canary = (unsigned int*)((char*)ptr + header->user_size);
    if (*end_canary != CANARY_VALUE) {
        DAM_LOG_ERR("Buffer overflow detected at %p! Canary was 0x%X, expected 0x%X",ptr, *end_canary, CANARY_VALUE);
        // Continue to free, but user knows there was corruption.
    } else {
        DAM_LOG("[CANARY] Buffer overflow check passed");
    }

    header->magic = FREED_MAGIC;
    header->is_free = 1;

    coalesce_if_possible(header, pool_header);
    stats.frees++;
}

void* dam_general_realloc(void* ptr, size_t size) {
    block_header_t* header = (block_header_t*)((char*)ptr - HEAD_SIZE);
    size_t new_actual_size = align_up(size + sizeof(uint32_t), ALIGNMENT);
    // Case 1 Shrink in place
    if (header->size >= new_actual_size) {
        header->user_size = size;

        uint32_t* end_canary = (uint32_t*)((char*)ptr + size);
        *end_canary = CANARY_VALUE;

        // Split if shrinking leaves enough headspace
        split_block_if_possible(header, new_actual_size);

        return ptr;
    }

    // Case 2 grow in-place if next block is free
    if (header->next && header->next->is_free) {
        size_t available_space = header->size + HEAD_SIZE + header->next->size;

        if (header->magic != BLOCK_MAGIC) {
            DAM_LOG_ERR("Invalid pointer passed to dam_realloc:%p", ptr);
            return NULL;
        }

        if (available_space >= new_actual_size) {

            block_header_t* old_next = header->next;
            header->size = available_space;
            header->next = old_next->next;

            old_next->magic = 0;
            old_next->next = NULL;
            old_next->prev = NULL;

            if (header->next) header->next->prev = header;

            header->user_size = size;
            uint32_t* end_canary = (uint32_t*)((char*)ptr + size);
            *end_canary = CANARY_VALUE;

            // Split if there is room after coalescing
            split_block_if_possible(header, new_actual_size);
            stats.coalesces++;
        }

        return ptr;
    }

    // Case 3 Grow in-place but no next block is not free, so copy and free
    void* new_ptr = dam_malloc(size);
    if (!new_ptr) return NULL;

    // Copy smaller of old and new sizes
    size_t copy_size = (header->user_size < size) ? header->user_size : size;
    memcpy(new_ptr, ptr, copy_size);
    dam_free(ptr);

    return new_ptr;
}

static size_t calculate_next_pool_size(size_t min_required) {
    size_t next_size = INITIAL_POOL_SIZE;

    struct pool_header* current = pool_list_head;

    while (current) {
        if (current->size > next_size) {
            next_size = current->size;
        }
        current = current->next;
    }

    next_size *= 2;

    if (next_size < min_required) {
        next_size = min_required;
    }

    return next_size;
}

pool_header_t* create_general_pool(size_t min_size) {
    size_t pool_count = 0;
    pool_header_t* temp = pool_list_head;
    while (temp) {
        pool_count++;
        temp = temp->next;
    }

    if (pool_count >= MAX_POOLS) {
        DAM_LOG_ERR("[ERROR] Maximum number of pools (%d) reached", MAX_POOLS);
        return NULL;
    }

    size_t pool_size = calculate_next_pool_size(min_size);

    DAM_LOG("[POOL] Creating pool #%zu of %zu bytes...", pool_count + 1, pool_size);

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
    new_pool->type = DAM_POOL_GENERAL;
    new_pool->next = pool_list_head;
    new_pool->free_list_head = (block_header_t*)((char*)memory + POOL_GENERAL_SIZE);

    char* usable_start = (char*)memory + POOL_GENERAL_SIZE;
    size_t usable_size = pool_size - POOL_GENERAL_SIZE;

    new_pool->free_list_head = (block_header_t*)usable_start;
    new_pool->free_list_head->size = usable_size - HEAD_SIZE;
    new_pool->free_list_head->is_free = 1;
    new_pool->free_list_head->next = NULL;
    new_pool->free_list_head->magic = FREED_MAGIC;

    pool_list_head = new_pool;

    stats.pools_created++;
    DAM_LOG("[POOL] Created at %p with %zu bytes usable. Total pools: %zu", memory, new_pool->free_list_head->size, stats.pools_created);

    return new_pool;
}

block_header_t* find_block_in_pools(size_t actual_size, pool_header_t** found_pool) {
    if (!pool_list_head) return NULL;

    size_t blocks_checked = 0;
    pool_header_t* current_pool = pool_list_head;
    while (current_pool) {
        block_header_t* current_block = current_pool->free_list_head;
        while (current_block) {
            blocks_checked++;
            if (current_block->is_free && current_block->size >= actual_size) {
                *found_pool = current_pool;
                stats.blocks_searched += blocks_checked;
                return current_block;
            }
            current_block = current_block->next;
        }
        current_pool = current_pool->next;
    }

    stats.blocks_searched += blocks_checked;
    return NULL;
}

void split_block_if_possible(block_header_t* block_header, size_t actual_size) {
    if (block_header->size >= actual_size + HEAD_SIZE + MIN_BLOCK_SIZE) {
        block_header_t* new_block_header = (block_header_t*)((char*)block_header + HEAD_SIZE + actual_size);

        new_block_header->size = block_header->size - actual_size - HEAD_SIZE;
        new_block_header->user_size = 0;
        new_block_header->is_free = 1;
        new_block_header->next = block_header->next;
        new_block_header->prev = block_header;
        new_block_header->magic = FREED_MAGIC;

        if (block_header->next) block_header->next->prev = new_block_header;

        block_header->size = actual_size;
        block_header->next = new_block_header;

        stats.splits++;
        // DAM_LOG("[SPLIT] Split block: allocated=%zu, remaining=%zu", user_size, new_block_header->size);
    }
}

void coalesce_if_possible(block_header_t* block_header, pool_header_t* pool_header) {
    // Coalesce with previous block if it's free
    if (block_header->prev && block_header->prev->is_free) {
        DAM_LOG("[COALESCE] Merging with previous block: %zu + %zu", block_header->prev->size, block_header->size);
        block_header->prev->size += HEAD_SIZE + block_header->size;
        block_header->prev->next = block_header->next;
        stats.coalesces++;

        if (block_header->next) block_header->next->prev = block_header->prev;
    }

    // Coalesce with next block if it's free
    if (block_header->next && block_header->next->is_free) {
        if ((void*)block_header->next >= pool_header->memory && (char*)block_header->next < (char*)pool_header->memory + pool_header->size) {
            DAM_LOG("[COALESCE] Merging with next block: %zu + %zu", block_header->size, block_header->next->size);
            block_header->size += HEAD_SIZE + block_header->next->size;
            block_header->next = block_header->next->next;
            stats.coalesces++;

            if (block_header->next) block_header->next->prev = block_header;
        }
    }
}

void cleanup_allocator(void) {
    DAM_LOG("[CLEANUP] Freeing all pools...");

    pool_header_t* current = pool_list_head;
    int pool_count = 0;

    while (current) {
        pool_header_t* next = current->next;

        DAM_LOG("[CLEANUP] Freeing pool at %p (size: %zu)",
               current->memory, current->size);

        if (munmap(current->memory, current->size) == -1) {
            DAM_LOG_ERR("munmap failed");
        }

        pool_count++;
        current = next;
    }

    DAM_LOG("[CLEANUP] Freed %d pools", pool_count);

    pool_list_head = NULL;
    initialized = 0;

    // Reset stats
    stats.allocations = 0;
    stats.frees = 0;
    stats.blocks_searched = 0;
    stats.splits = 0;
    stats.coalesces = 0;
    stats.pools_created = 0;
}