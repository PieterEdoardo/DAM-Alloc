#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "dam/dam.h"
#include "dam/dam_config.h"
#include "dam/internal/dam_internal.h"
#include "dam/dam_log.h"

pool_header_t* pool_list_head = NULL;
int initialized = 0;

void* dam_malloc(size_t size) {
    if (!initialized) init_allocator();
    if (size == 0) return NULL;

    if (size <= DAM_SMALL_MAX) {
        // dam_small_malloc(size);
        DAM_LOG_ERR("SMALL_MALLOC NOT IMPLEMENTED");
    } else if (size <= DAM_GENERAL_MAX) {
        return dam_general_malloc(size);
        DAM_LOG_ERR("NOT IMPLEMENTED");
    } else {
        // dam_direct_malloc(size);
        DAM_LOG_ERR("DIRECT_MALLOC NOT IMPLEMENTED");
    }
    return NULL;
}

void* dam_realloc(void* ptr, size_t size) {
    if (!initialized) init_allocator();

    if (ptr == NULL) {
        return dam_malloc(size);
    }

    if (size == 0) {
        dam_free(ptr);
        return NULL;
    }

    block_header_t* header = (block_header_t*)((char*)ptr - HEAD_SIZE);

    if (header->magic != BLOCK_MAGIC) {
        DAM_LOG_ERR("Invalid pointer passed to dam_realloc:%p\n", ptr);
        return NULL;
    }

    size_t new_actual_size = align_up(align_up(size) + sizeof(uint32_t));
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

void dam_free(void* ptr) {
    if (!ptr) return;

    block_header_t* header = (block_header_t*)((char*)ptr - HEAD_SIZE);

    // Double free checks
    if (header->magic == FREED_MAGIC) {
        DAM_LOG_ERR("Double free detected at %p!\n", ptr);
        return;
    }

    // Alignment check
    if ((uintptr_t)ptr % ALIGNMENT != 0) {
        DAM_LOG_ERR("Invalid pointer passed to dam_free: %p\n", ptr);
        return;
    }

    // Invalid pointer checks
    if (header->magic != BLOCK_MAGIC) {
        DAM_LOG_ERR("Invalid pointer passed to dam_free: %p\n", ptr);
        return;
    }

    // Header sanity check
    if (header->size == 0) {
        DAM_LOG_ERR("Pointer passed to dam_free refers to header with invalid size: %p\n", ptr);

        return;
    }

    unsigned int* end_canary = (unsigned int*)((char*)ptr + header->user_size);
    if (*end_canary != CANARY_VALUE) {
        DAM_LOG_ERR("Buffer overflow detected at %p! Canary was 0x%X, expected 0x%X\n",ptr, *end_canary, CANARY_VALUE);
        // Continue to free, but user knows there was corruption.
    } else {
        DAM_LOG("[CANARY] Buffer overflow check passed\n");
    }

    header->magic = FREED_MAGIC;
    header->is_free = 1;



    pool_header_t* block_pool = NULL;
    pool_header_t* current_pool = pool_list_head;

    // Check for header within this pool's memory range.
    while (current_pool) {
        if ((void*)header >= current_pool->memory && (void*)header < current_pool->memory + current_pool->size) {
            block_pool = current_pool;
            break;
        }
        current_pool = current_pool->next;
    }

    if (!block_pool) {
        DAM_LOG_ERR("Could not find pool for freed block!\n");
        return;
    }

    coalesce_if_possible(header, block_pool);
    stats.frees++;
}

void print_memory_state(void) {
    DAM_LOG("\n=== Memory State ===\n");

    if (!pool_list_head) {
        DAM_LOG("No pools allocated\n");
        DAM_LOG("===================\n\n");
        return;
    }

    pool_header_t* current_pool = pool_list_head;
    int pool_num = 1;
    size_t total_free = 0;
    size_t total_used = 0;

    while (current_pool) {
        DAM_LOG("\n--- Pool #%d (size: %zu bytes, addr: %p) ---\n",
               pool_num, current_pool->size, current_pool->memory);

        block_header_t* current_block = current_pool->free_list_head;
        int block_num = 0;

        while (current_block) {
            DAM_LOG("  Block %d: size=%zu, %s, addr=%p\n",
                   block_num++,
                   current_block->size,
                   current_block->is_free ? "FREE" : "ALLOCATED",
                   (void*)current_block);

            if (current_block->is_free) {
                total_free += current_block->size;
            } else {
                total_used += current_block->size;
            }

            current_block = current_block->next;
        }

        current_pool = current_pool->next;
        pool_num++;
    }

    DAM_LOG("\n--- Summary ---\n");
    DAM_LOG("Total pools: %zu\n", stats.pools_created);
    DAM_LOG("Total free: %zu bytes\n", total_free);
    DAM_LOG("Total used: %zu bytes\n", total_used);
    DAM_LOG("===================\n\n");
}