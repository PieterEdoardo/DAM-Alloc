#include <assert.h>
#include <sys/mman.h>

#include "dam/dam_config.h"
#include "dam/dam_log.h"

typedef struct pool_header pool_header_t;
/*********************
 * Helper Functions *
 *********************/
void init_allocator(void) {
    if (initialized) return;

    DAM_LOG("[INIT] Initializing growing pool allocator...\n");

    if (!verify_page_size()) {
        return;
    }

    if (!create_pool(INITIAL_POOL_SIZE)) {
        DAM_LOG_ERR("Failed to create initial pool\n");
        return;
    }

    initialized = 1;
    DAM_LOG("[INIT] Allocator initialized\n");
}

size_t align_up(size_t size) {
    return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

int verify_page_size(void) {
    long actual = sysconf(_SC_PAGESIZE);
    assert(actual > 0);

    if ((size_t)actual != PAGE_SIZE) {
        DAM_LOG_ERR("System page size (%ld) differs from assumed (%d)", actual, PAGE_SIZE);
        return 0;
    }
    return 1;
}

static size_t calculate_next_pool_size(size_t min_required) {
    size_t next_size = INITIAL_POOL_SIZE;

    pool_header_t* current = pool_list_head;

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

pool_header_t* create_pool(size_t min_size) {
    size_t pool_count = 0;
    pool_header_t* temp = pool_list_head;
    while (temp) {
        pool_count++;
        temp = temp->next;
    }

    if (pool_count >= MAX_POOLS) {
        DAM_LOG_ERR("[ERROR] Maximum number of pools (%d) reached\n", MAX_POOLS);
        return NULL;
    }

    size_t pool_size = calculate_next_pool_size(min_size);

    DAM_LOG("[POOL] Creating pool #%zu of %zu bytes...\n", pool_count + 1, pool_size);

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
    new_pool->next = pool_list_head;

    char* usable_start = (char*)memory + POOL_HEADER_SIZE;
    size_t usable_size = pool_size - POOL_HEADER_SIZE;

    new_pool->free_list_head = (block_header_t*)usable_start;
    new_pool->free_list_head->size = usable_size - HEAD_SIZE;
    new_pool->free_list_head->is_free = 1;
    new_pool->free_list_head->next = NULL;
    new_pool->free_list_head->magic = FREED_MAGIC;

    pool_list_head = new_pool;

    stats.pools_created++;
    DAM_LOG("[POOL] Created at %p with %zu bytes usable. Total pools: %zu\n", memory, new_pool->free_list_head->size, stats.pools_created);

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
        // DAM_LOG("[SPLIT] Split block: allocated=%zu, remaining=%zu\n", user_size, new_block_header->size);
    }
}

void coalesce_if_possible(block_header_t* block_header, pool_header_t* pool_header) {
    // Coalesce with next block if it's free
    if (block_header->next && block_header->next->is_free) {
        if ((void*)block_header->next >= pool_header->memory && (char*)block_header->next < (char*)pool_header->memory + pool_header->size) {
            DAM_LOG("[COALESCE] Merging with next block: %zu + %zu\n", block_header->size, block_header->next->size);
            block_header->size += HEAD_SIZE + block_header->next->size;
            block_header->next = block_header->next->next;
            stats.coalesces++;

            if (block_header->next) block_header->next->prev = block_header;
        }
    }

    // Coalesce with previous block if it's free
    if (block_header->prev && block_header->prev->is_free) {
        DAM_LOG("[COALESCE] Merging with previous block: %zu + %zu\n", block_header->prev->size, block_header->size);
        block_header->prev->size += HEAD_SIZE + block_header->size;
        block_header->prev->next = block_header->next;
        stats.coalesces++;

        if (block_header->next) block_header->next->prev = block_header->prev;
    }
}


void cleanup_allocator(void) {
    DAM_LOG("[CLEANUP] Freeing all pools...\n");

    pool_header_t* current = pool_list_head;
    int pool_count = 0;

    while (current) {
        pool_header_t* next = current->next;

        DAM_LOG("[CLEANUP] Freeing pool at %p (size: %zu)\n",
               current->memory, current->size);

        if (munmap(current->memory, current->size) == -1) {
            DAM_LOG_ERR("munmap failed");
        }

        pool_count++;
        current = next;
    }

    DAM_LOG("[CLEANUP] Freed %d pools\n", pool_count);

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

void print_allocator_stats(void) {
    DAM_LOG("\n=== Allocator Statistics ===\n");
    DAM_LOG("Total allocations: %zu\n", stats.allocations);
    DAM_LOG("Total frees: %zu\n", stats.frees);
    DAM_LOG("Total splits: %zu\n", stats.splits);
    DAM_LOG("Total coalesces: %zu\n", stats.coalesces);
    DAM_LOG("Pools created: %zu\n", stats.pools_created);

    if (stats.allocations > 0) {
        DAM_LOG("Avg blocks searched per allocation: %.2f\n",
               (float)stats.blocks_searched / stats.allocations);
    }

    DAM_LOG("============================\n\n");
}

void reset_allocator_stats(void) {
    stats.allocations = 0;
    stats.frees = 0;
    stats.blocks_searched = 0;
    stats.splits = 0;
    stats.coalesces = 0;
    // Don't reset pools_created - that's structural info
}