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
    dam_global_lock();

    if (!verify_page_size()) {
        dam_global_unlock();
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

    dam_global_unlock();
}

void* dam_malloc(size_t size) {
    dam_global_lock();
    if (!initialized) {
        init_allocator_unlocked();
    }
    void* ptr = dam_malloc_internal(size);
    dam_global_unlock();
    return ptr;
}

void* dam_realloc(void* ptr, size_t size) {
    dam_global_lock();
    void* new_ptr = dam_realloc_internal(ptr, size);
    dam_global_unlock();
    return new_ptr;
}

void dam_free(void* ptr) {
    dam_global_lock();
    dam_free_internal(ptr);
    dam_global_unlock();
}


