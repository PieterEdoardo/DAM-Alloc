#include <assert.h>
#include <unistd.h>

#include "dam/dam.h"
#include "dam/dam_config.h"
#include "dam/dam_log.h"
#include "dam/internal/thread.h"

typedef struct pool_header pool_header_t;
/*********************
 * Helper Functions *
 *********************/



size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

int verify_page_size(void) {
    long actual = sysconf(_SC_PAGESIZE);
    assert(actual > 0);

    if ((size_t)actual != PAGE_SIZE) {
        DAM_LOG_ERROR("System page size (%ld) differs from assumed (%d)", actual, PAGE_SIZE);
        return 0;
    }
    return 1;
}

void dam_register_pool(pool_header_t* new_pool_header) {
    new_pool_header->next = dam_pool_list;
    dam_pool_list = new_pool_header;
}

void dam_unregister_pool(pool_header_t* pool_header) {
    pool_header_t** current = &dam_pool_list;

    while (*current) {
        if (*current == pool_header) {
            *current = pool_header->next;
            return;
        }
        current = &(*current)->next;
    }
}

pool_header_t* dam_pool_from_ptr(void* address) {
    pool_header_t* p = dam_pool_list;

    while (p) {
        if (address >= p->memory && (char*)address < (char*)p->memory + p->size) {
            return p;
        }
        p = p->next;
    }
    return NULL;
}

/**********************
 * internal Functions *
 **********************/
void init_allocator_unlocked(void) {
    if (initialized) {
        return;
    }
    if (!verify_page_size()) {
        return;
    }

    DAM_LOG("[INIT] Initializing multi-threading...");
    dam_thread_init();

    DAM_LOG("[INIT] Initializing size class allocator...");
    dam_small_init();
    DAM_LOG("[INIT] Initializing growing pool allocator...");
    dam_general_init();
    DAM_LOG("[INIT] Initializing direct mmap() allocator...");
    dam_direct_init();

    initialized = 1;
    DAM_LOG("[INIT] Allocator initialized");
}

void print_allocator_stats(void) {
    DAM_LOG("\n=== Allocator Statistics ===");
    DAM_LOG("Total allocations: %zu", stats.allocations);
    DAM_LOG("Total frees: %zu", stats.frees);
    DAM_LOG("Total splits: %zu", stats.splits);
    DAM_LOG("Total coalesces: %zu", stats.coalesces);
    DAM_LOG("Pools created: %zu", stats.pools_created);

    if (stats.allocations > 0) {
        DAM_LOG("Avg blocks searched per allocation: %.2f",
               (float)stats.blocks_searched / stats.allocations);
    }

    DAM_LOG("============================\n");
}

void reset_allocator_stats(void) {
    stats.allocations = 0;
    stats.frees = 0;
    stats.blocks_searched = 0;
    stats.splits = 0;
    stats.coalesces = 0;
}