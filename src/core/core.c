#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "dam/dam.h"
#include "dam/dam_config.h"
#include "dam/internal/dam_internal.h"
#include "dam/dam_log.h"

/**********************************************************
 * DAM allocator (core)
 *
 * dam_pool_list           ← linked list (ALL pools)
 * ├─ pool_header_t        ← DAM_POOL_SMALL
 * ├─ pool_header_t        ← DAM_POOL_GENERAL
 * └─ pool_header_t        ← DAM_POOL_DIRECT
 *
 * Used for:
 *  - ownership checks
 *  - routing free() / realloc()
 **********************************************************/

pool_header_t* dam_pool_list = NULL;
int initialized = 0;

void init_allocator(void) {
    if (initialized) return;

    if (!verify_page_size()) {
        return;
    }

    dam_small_init();
    DAM_LOG("[INIT] Initialized size class allocator...");
    dam_general_init();
    DAM_LOG("[INIT] Initialized growing pool allocator...");
    dam_direct_init();
    DAM_LOG("[INIT] Initialized direct mmap() allocator...");

    initialized = 1;
    DAM_LOG("[INIT] Allocator initialized");
}

void dam_register_pool(pool_header_t* new_pool_header) {
    new_pool_header->next = dam_pool_list;
    dam_pool_list = new_pool_header;
}

void* dam_malloc(size_t size) {
    if (!initialized) init_allocator();
    if (size == 0) return NULL;

    if (size <= DAM_SMALL_MAX) {
        return dam_small_malloc(size);
        // DAM_LOG_ERROR("SMALL_MALLOC NOT IMPLEMENTED");
    } else if (size <= DAM_GENERAL_MAX) {
        return dam_general_malloc(size);
        DAM_LOG_ERROR("NOT IMPLEMENTED");
    } else {
        // return dam_direct_malloc(size);
        DAM_LOG_ERROR("DIRECT_MALLOC NOT IMPLEMENTED");
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

    if (size <= DAM_SMALL_MAX) {
        return dam_small_realloc(ptr, size);
    } else if (size <= DAM_GENERAL_MAX) {
        return dam_general_realloc(ptr, size);
    } else {
        // return dam_direct_realloc(ptr, size);
        DAM_LOG_ERROR("DIRECT_MALLOC NOT IMPLEMENTED");
    }
    return NULL;
}

void dam_free(void* ptr) {
    if (!ptr) {
        DAM_LOG_ERROR("[FREE] NULL pointer");
        return;
    }

    pool_header_t* pool = dam_pool_from_ptr(ptr);

    if (!pool) {
        DAM_LOG_ERROR("[FREE] Pointer does not belong to DAM pool: %p", ptr);
        return;
    }

    DAM_LOG("[FREE] Pool type to be freed: %d", pool->type);
    switch (pool->type) {
        case DAM_POOL_SMALL:
            dam_small_free(ptr);
            break;
        case DAM_POOL_GENERAL:
            dam_general_free(ptr, pool);
            break;
        case DAM_POOL_DIRECT:
            DAM_LOG_ERROR("DIRECT FREE NOT IMPLEMENTED");
            // dam_direct_free(ptr);
            break;
        default:
            DAM_LOG_ERROR("Unknown pool type for ptr %p", ptr);
            break;
    }
}


