
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "dam/dam.h"
#include "dam/dam_config.h"
#include "dam/dam_log.h"
#include "dam/internal/dam_internal.h"
#include "dam/internal/dam_types.h"

/**********************************************************
 * General allocator
 *
 * dam_pool_list           ← linked list (global)
 * └─ pool_header_t        ← DAM_POOL_GENERAL
 *     └─ free_block_list  ← linked list (blocks)
 *
 * Each pool manages its own blocks.
 **********************************************************/

void  dam_general_init() {
    if (!create_general_pool(INITIAL_POOL_SIZE)) {
        DAM_LOG_ERROR("Failed to create initial pool");
    }
}

void* dam_general_malloc_internal(size_t size, const char* trace) {
    const size_t aligned_size = align_up(size, ALIGNMENT);
    size_t actual_size = aligned_size + sizeof(uint32_t);
    actual_size = align_up(actual_size, ALIGNMENT);

    pool_header_t* found_pool = NULL;
    block_header_t* found_block = NULL;

    found_block = find_block_in_pools(actual_size, &found_pool);

    if (!found_block) {
        const size_t min_pool_size = POOL_GENERAL_SIZE + BLOCK_HEADER_SIZE +  actual_size + MIN_BLOCK_SIZE;
        pool_header_t* new_pool = create_general_pool(min_pool_size);

        if (!new_pool) {
            DAM_LOG_ERROR("[ALLOC] FAILED: Could not create new pool");
            return NULL;
        }

        found_block = find_block_in_pools(actual_size, &found_pool);

        if (!found_block) {
            DAM_LOG_ERROR("[ALLOC] FAILED: Still no space after creating pool!");
            return NULL;
        }
    }

    DAM_LOG("[ALLOC] Found free block: size=%zu at %p", found_block->size, (void*)found_block);

    found_block->is_free = 0;
    found_block->magic = BLOCK_MAGIC;
    split_block_if_possible(found_block, actual_size);
    found_block->user_size = size;

    void* ptr;

    // Trace allocation!
    if (trace != NULL) {
    // printf("*dam_trace_malloc() trace: %s*\n" , trace);

        found_block->is_traced = 1;
        char* trace_ptr = (char*)found_block + BLOCK_HEADER_SIZE;
        strncpy(trace_ptr, trace, TRACE_SIZE - 1);
        trace_ptr[TRACE_SIZE - 1] = '\0';

        ptr = (char*)found_block + BLOCK_HEADER_SIZE + TRACE_SIZE;
    } else {
        ptr = (char*)found_block + BLOCK_HEADER_SIZE;
    }

    uint32_t* end_canary = (uint32_t*)((char*)ptr + found_block->user_size);
    *end_canary = CANARY_VALUE;

    DAM_LOG("[ALLOC] Returning pointer %p", ptr);
    return ptr;
}

void dam_general_free_internal(void* ptr, pool_header_t* pool_header, block_header_t* block_header) {
    // Double free checks
    if (block_header->magic == FREED_MAGIC) {
        DAM_LOG_ERROR("[FREE] Double free detected at %p!", ptr);
        return;
    }

    // Alignment check
    if ((uintptr_t)ptr % ALIGNMENT != 0) {
        DAM_LOG_ERROR("[FREE] Invalid pointer passed to dam_free: %p", ptr);
        return;
    }

    // Invalid pointer checks
    if (block_header->magic != BLOCK_MAGIC) {
        DAM_LOG_ERROR("[FREE] Invalid pointer passed to dam_free: %p", ptr);
        return;
    }

    // Header sanity check
    if (block_header->size == 0) {
        DAM_LOG_ERROR("[FREE] Pointer passed to dam_free refers to header with invalid size: %p", ptr);
        return;
    }

    const unsigned int* end_canary = (unsigned int*)((char*)ptr + block_header->user_size);
    if (*end_canary != CANARY_VALUE) {
        DAM_LOG_ERROR("[FREE][CANARY] Buffer overflow detected at %p! Canary was 0x%X, expected 0x%X",ptr, *end_canary, CANARY_VALUE);
        // Continue to free, but user knows there was corruption.
    } else {
        DAM_LOG("[FREE][CANARY] Buffer overflow check passed");
    }

    block_header->magic = FREED_MAGIC;
    block_header->is_free = 1;

    coalesce_if_possible(block_header, pool_header);

    // general_add_to_free_list(pool_header, block_header);

    DAM_LOG("[FREE] Pointer %p freed", ptr);
}

void add_to_free_list(pool_header_t* pool_header, block_header_t* block_header) {
    block_header->next_ptr = pool_header->free_list;
    free_block_header_t* free_block_header = get_free_block_header(block_header);

    free_block_header->next_ptr = pool_header->free_list;
    free_block_header->prev_ptr = NULL;

    if (pool_header->free_list) {
        free_block_header_t* old_header = get_free_block_header(pool_header->free_list);
        old_header->prev_ptr = block_header;
    }

    pool_header->free_list = block_header;
}

inline free_block_header_t* get_free_block_header(block_header_t* block_header) {
    return (free_block_header_t*)((char*)block_header + BLOCK_HEADER_SIZE);
}

block_header_t* search_free_block_in_free_list(pool_header_t* pool_header, size_t actual_size) {
    block_header_t* current = pool_header->free_list;
    block_header_t* previous = NULL;

    while (current) {
        if (current->size >= actual_size) {
            if (previous) {
                previous->next_ptr = current->next_ptr;
            } else {
                pool_header->free_list = current->next_ptr;
            }
            return current;
        }
        previous = current;
        current = current->next_ptr;
    }
    return NULL;
}

void* dam_general_malloc(size_t size, const char* trace) {
    dam_general_lock();
    void* ptr = dam_general_malloc_internal(size, trace);
    dam_general_unlock();

    return ptr;
}

void dam_general_free(void* ptr, const pool_header_t* pool_header, block_header_t* block_header) {
    dam_general_lock();
    dam_general_free_internal(ptr, pool_header, block_header);
    dam_general_unlock();
}

void* dam_general_realloc(void* ptr, size_t size, block_header_t* block_header, const char* trace) {
    size_t new_actual_size = align_up(size + sizeof(uint32_t), ALIGNMENT);
    uint8_t quarantine = 0;

    if (block_header->pool->read_only) {
        DAM_LOG_ERROR("[REALLOC] Pointer: %p from quarantined pool detected: %p. Forced copy/free to new pool", ptr, block_header->pool);
        quarantine = 1;
    }

    dam_general_lock();

    if (block_header->magic != BLOCK_MAGIC) {
        DAM_LOG_ERROR("[REALLOC] Invalid pointer passed to dam_general_realloc: %p", ptr);
        return NULL;
    }

    // Case 1 Shrink in place
    if (block_header->size >= new_actual_size && !quarantine) {

        block_header->user_size = size;
        uint32_t* end_canary = (uint32_t*)((char*)ptr + size);
        *end_canary = CANARY_VALUE;

        split_block_if_possible(block_header, new_actual_size);

        dam_general_unlock();
        return ptr;
    }

    // Case 2 grow in-place if next block is free
    if (block_header->next_ptr && block_header->next_ptr->is_free && !quarantine) {
        size_t available_space = block_header->size + BLOCK_HEADER_SIZE + block_header->next_ptr->size;

        if (available_space >= new_actual_size) {

            block_header_t* old_next = block_header->next_ptr;
            block_header->size = available_space;
            block_header->next_ptr = old_next->next_ptr;

            old_next->magic = 0;
            old_next->next_ptr = NULL;
            old_next->prev.ptr = NULL;

            if (block_header->next_ptr) block_header->next_ptr->prev.ptr = block_header;

            block_header->user_size = size;
            uint32_t* end_canary = (uint32_t*)((char*)ptr + size);
            *end_canary = CANARY_VALUE;

            split_block_if_possible(block_header, new_actual_size);

            dam_general_unlock();

            return ptr;
        }
    }

    // Case 3 Grow in-place but no next block is not free, so copy and free
    dam_general_unlock();
    void* new_ptr = dam_trace_malloc(size, trace); // always traced call, even if trace is NULL.
    if (new_ptr) {
        size_t copy_size = (block_header->user_size < size) ? block_header->user_size : size;
        memcpy(new_ptr, ptr, copy_size);
        dam_free(ptr);
    }

    return new_ptr;
}

static size_t calculate_next_pool_size(size_t min_required) {
    size_t next_size = INITIAL_POOL_SIZE;

    struct pool_header* current = dam_pool_list;

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
    pool_header_t* temp = dam_pool_list;
    while (temp) {
        pool_count++;
        temp = temp->next;
    }

    if (pool_count >= MAX_POOLS) {
        DAM_LOG_ERROR("[ERROR] Maximum number of pools (%d) reached", MAX_POOLS);
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
        DAM_LOG_ERROR("mmap failed for new pool");
        return NULL;
    }

    pool_header_t* new_pool = memory;
    new_pool->memory = memory;
    new_pool->size = pool_size;
    new_pool->type = DAM_LAYER_GENERAL;
    new_pool->block_list = (block_header_t*)((char*)memory + POOL_GENERAL_SIZE);

    char* usable_start = (char*)memory + POOL_GENERAL_SIZE;
    size_t usable_size = pool_size - POOL_GENERAL_SIZE;

    new_pool->block_list = (block_header_t*)usable_start;
    new_pool->block_list->size = usable_size - BLOCK_HEADER_SIZE;
    new_pool->block_list->is_free = 1;
    new_pool->block_list->next_ptr = NULL;
    new_pool->block_list->magic = FREED_MAGIC;
    new_pool->block_list->pool = new_pool;

    dam_register_pool(new_pool);

    DAM_LOG("[POOL] Created at %p with %zu bytes usable. Total pools: %zu", memory, new_pool->free_block_list->size, stats.pools_created);

    return new_pool;
}

block_header_t* find_block_in_pools(size_t actual_size, pool_header_t** found_pool) {
    if (!dam_pool_list) return NULL;

    pool_header_t* current_pool = dam_pool_list;
    while (current_pool) {
        if (current_pool->read_only == 1) {
            DAM_LOG_VALID("[ALLOC] Pool %p skipped due to quarantine.", current_pool);
            current_pool = current_pool->next;
            continue;
        }
        block_header_t* current_block = current_pool->block_list;
        while (current_block) {
            if (current_block->is_free && current_block->size >= actual_size) {
                *found_pool = current_pool;

                return current_block;
            }
            current_block = current_block->next_ptr;
        }
        current_pool = current_pool->next;
    }

    return NULL;
}

void split_block_if_possible(block_header_t* block_header, size_t actual_size) {
    if (block_header->size >= actual_size + BLOCK_HEADER_SIZE + MIN_BLOCK_SIZE) {
        block_header_t* new_block_header = (block_header_t*)((char*)block_header + BLOCK_HEADER_SIZE + actual_size);

        new_block_header->size = block_header->size - actual_size - BLOCK_HEADER_SIZE;
        new_block_header->user_size = 0;
        new_block_header->is_free = 1;
        new_block_header->next_ptr = block_header->next_ptr;
        new_block_header->prev.ptr = block_header;
        new_block_header->magic = FREED_MAGIC;
        new_block_header->pool = block_header->pool;

        if (block_header->next_ptr) block_header->next_ptr->prev.ptr = new_block_header;

        block_header->size = actual_size;
        block_header->next_ptr = new_block_header;
        DAM_LOG("[SPLIT] Split block: allocated=%zu, remaining=%zu", user_size, new_block_header->size);
    }
}

void coalesce_if_possible(block_header_t* block_header, const pool_header_t* pool_header) {
    // Coalesce with previous block if it's free
    if (block_header->prev.ptr && block_header->prev.ptr->is_free && block_header->prev.ptr->magic == FREED_MAGIC) {
        DAM_LOG("[COALESCE] Merging with previous block: %zu + %zu", block_header->prev->size, block_header->size);
        block_header->prev.ptr->size += BLOCK_HEADER_SIZE + block_header->size;
        block_header->prev.ptr->next_ptr = block_header->next_ptr;

        if (block_header->next_ptr) block_header->next_ptr->prev.ptr = block_header->prev.ptr;

        block_header = block_header->prev.ptr;
    }

    // Coalesce with next block if it's free
    if (block_header->next_ptr && block_header->next_ptr->is_free && block_header->next_ptr->magic == FREED_MAGIC) {
        if ((void*)block_header->next_ptr >= pool_header->memory && (char*)block_header->next_ptr < (char*)pool_header->memory + pool_header->size) {
            DAM_LOG("[COALESCE] Merging with next block: %zu + %zu", block_header->size, block_header->next->size);
            block_header->size += BLOCK_HEADER_SIZE + block_header->next_ptr->size;
            block_header->next_ptr = block_header->next_ptr->next_ptr;

            if (block_header->next_ptr) block_header->next_ptr->prev.ptr = block_header;
        }
    }
}

inline block_header_t* get_block_header(void* ptr) {
    return (block_header_t*)((char*)ptr - BLOCK_HEADER_SIZE);
}

inline block_header_t* get_block_trace_header(void* ptr) {
    return (block_header_t*)((char*)ptr - BLOCK_HEADER_SIZE - TRACE_SIZE);
}


void dam_snapshot_general(dam_snapshot_t* snapshot) {
    pool_header_t* current = dam_pool_list;
    dam_general_lock();

    while (current) {
        if (current->type == DAM_LAYER_GENERAL) {
            snapshot->pools_bytes_used += current->size;
            snapshot->pools_active++;
            if (current->read_only) snapshot->quarantined_pools++;
        }
        current = current->next;
    }

    dam_general_unlock();
}

// 1.0 - (largest_free_block / total_free_bytes)
// Get the largest free block by iteration and saving the latest biggest one.
// While doing that, addition all the bytes of free blocks.
void dam_general_fragmentation(pool_header_t* pool, dam_pool_fragmentation_t* snapshot) {
    dam_general_lock();
    block_header_t* current = pool->block_list;
    while (current) {
        if (current->is_free && current->magic == FREED_MAGIC) {
            if (current->size > snapshot->largest_free) snapshot->largest_free = current->size;
            snapshot->free += current->size;
        }
        current = current->next_ptr;
    }
    snapshot->fragmentation = (float)snapshot->largest_free / (float)snapshot->free;
    dam_general_unlock();
}

void dam_general_pressure(pool_header_t* pool, dam_pool_pressure_t* snapshot) {
    dam_general_lock();
    block_header_t* current = pool->block_list;
    while (current) {
        if (!current->is_free && current->magic == BLOCK_MAGIC) {
            if (current->size > snapshot->largest_used) snapshot->largest_used = current->size;
            snapshot->used += current->size;
        }
        current = current->next_ptr;
    }
    snapshot->pressure = (float)snapshot->largest_used / (float)snapshot->used;
    dam_general_unlock();
}

uint8_t dam_validate_general_ptr(void* ptr, pool_header_t* pool_header, uint8_t quarantine, block_header_t* block_header) {
    if (!block_header->is_free) {
        if (block_header->magic != BLOCK_MAGIC) {
            DAM_LOG_VALID_ERROR("Pointer magic does not match: %p, magic %d", ptr, block_header->magic);
            if (quarantine) general_pool_quarantine(pool_header);

            return 0;
        }

        uint32_t* canary = dam_get_general_canary(ptr, block_header);

        if (*canary != CANARY_VALUE) {
            DAM_LOG_VALID("Possible overflow detected at: %p", canary);
            DAM_LOG_VALID_ERROR("Pointer canary value is invalid: %p, canary %d", ptr, *canary);
            if (quarantine) general_pool_quarantine(pool_header);

            return 0;
        }
    } else {
        DAM_LOG("Pointer size class is free: %p", ptr);
        if (block_header->magic != FREED_MAGIC) {
            DAM_LOG_VALID_ERROR("Pointer size class magic does not match: %p, magic %d", ptr, block_header->magic);
            if (quarantine) general_pool_quarantine(pool_header);

            return 0;
        }
    }

    return 1;
}

inline void general_pool_quarantine(pool_header_t* pool_header) {
    DAM_LOG_VALID("Pool %p has been put in quarantine!", pool_header);
    pool_header->read_only = 1;
}

inline uint32_t* dam_get_general_canary(void* ptr, block_header_t* block_header) {
    return (uint32_t*)((char*)ptr + block_header->user_size);
}