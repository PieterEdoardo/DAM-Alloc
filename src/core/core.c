#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "dam/dam.h"
#include "dam/dam_config.h"
#include "dam/internal/dam_internal.h"
#include "dam/internal/thread.h"
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

void* dam_malloc(size_t size) {
    if (!initialized) {
        init_allocator_unlocked();
    }
    if (size == 0) return NULL;

    if (size <= DAM_SMALL_MAX) {
        return dam_small_malloc(size);
    }
    if (size <= DAM_GENERAL_MAX) {
        return dam_general_malloc(size);
    }
    return dam_direct_malloc(size);
}

void* dam_realloc(void* ptr, size_t size) {
    if (!ptr) return dam_malloc(size);

    if (size == 0) {
        dam_free(ptr);
        return NULL;
    }

    pool_header_t* pool = dam_pool_from_ptr(ptr);

    if (!pool) {
        DAM_LOG_ERROR("[REALLOC] Pointer does not belong to DAM: %p", ptr);
        return NULL;
    }

    switch (pool->type) {
        case DAM_POOL_SMALL:
            return dam_small_realloc(ptr, size);

        case DAM_POOL_GENERAL:
            return dam_general_realloc(ptr, size);

        case DAM_POOL_DIRECT:
            return dam_direct_realloc(ptr, size);

        default:
            DAM_LOG_ERROR("[REALLOC] Unknown pool type for ptr %p", ptr);
            return NULL;
    }
}

void dam_free(void* ptr) {
    if (!ptr)
        return;

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
            dam_direct_free(ptr);
            break;
        default:
            DAM_LOG_ERROR("Unknown pool type for ptr %p", ptr);
            break;
    }
}


