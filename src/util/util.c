#include <assert.h>
#include <unistd.h>

#include "dam/dam_config.h"
#include "dam/dam_log.h"

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
        DAM_LOG_ERR("System page size (%ld) differs from assumed (%d)", actual, PAGE_SIZE);
        return 0;
    }
    return 1;
}
pool_header_t* dam_pool_from_ptr(void* address) {
    pool_header_t* p = pool_list_head;

    while (p) {
        if (address >= p->memory && (char*)address < (char*)p->memory + p->size) {
            return p;
        }
        p = p->next;
    }
    return NULL;
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