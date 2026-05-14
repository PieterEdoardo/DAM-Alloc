#include <stdio.h>

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

// Returns 0 on success, 1 on failure.
int dam_init(void) {
    if (initialized) return 0;

    if (!verify_page_size()) {
        return 1;
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

    return 0;
}


/**********************************************************
* DAM allocator (core)
*
* Memory Allocation suite
***********************************************************/
void* dam_malloc(size_t size) {

    if (!initialized) dam_init();

    if (size == 0) return NULL;
    if (size <= DAM_SMALL_MAX) return dam_small_malloc(size);
    if (size <= DAM_GENERAL_MAX) return dam_general_malloc(size);
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
        case DAM_LAYER_SMALL:
            return dam_small_realloc(ptr, size);

        case DAM_LAYER_GENERAL:
            return dam_general_realloc(ptr, size);

        case DAM_LAYER_DIRECT:
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
        case DAM_LAYER_SMALL:
            dam_small_free(ptr);
            break;
        case DAM_LAYER_GENERAL:
            dam_general_free(ptr, pool);
            break;
        case DAM_LAYER_DIRECT:
            dam_direct_free(ptr);
            break;
        default:
            DAM_LOG_ERROR("Unknown pool type for ptr %p", ptr);
            break;
    }
}

/**********************************************************
* DAM allocator (core)
*
* Memory diagnostics and security suite
***********************************************************/
dam_layer_type_t dam_layer_for_size(size_t size) {
    if (size == 0) return DAM_LAYER_ERROR;
    if (size <= DAM_SMALL_MAX) return DAM_LAYER_SMALL;
    if (size <= DAM_GENERAL_MAX) return DAM_LAYER_GENERAL;
    return DAM_LAYER_DIRECT;
}