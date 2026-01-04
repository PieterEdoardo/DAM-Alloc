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

void init_allocator(void) {
    if (initialized) return;

    if (!verify_page_size()) {
        return;
    }

    dam_small_init();
    DAM_LOG("[INIT] Initializing size class allocator...\n");
    dam_general_init();
    DAM_LOG("[INIT] Initializing growing pool allocator...\n");
    dam_direct_init();
    DAM_LOG("[INIT] Initializing direct mmap() allocator...\n");

    initialized = 1;
    DAM_LOG("[INIT] Allocator initialized\n");
}

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

    if (size <= DAM_SMALL_MAX) {
        // dam_small_realloc(ptr, size);
        DAM_LOG_ERR("SMALL_MALLOC NOT IMPLEMENTED");
    } else if (size <= DAM_GENERAL_MAX) {
        return dam_general_realloc(ptr, size);
        DAM_LOG_ERR("NOT IMPLEMENTED");
    } else {
        // dam_direct_realloc(ptr, size);
        DAM_LOG_ERR("DIRECT_MALLOC NOT IMPLEMENTED");
    }
    return NULL;
}

void dam_free(void* ptr) {
    if (!ptr) return;

    block_header_t* header = (block_header_t*)((char*)ptr - HEAD_SIZE);

    if ((uintptr_t)ptr % ALIGNMENT != 0) {
        DAM_LOG_ERR("Unaligned pointer passed to dam_free: %p", ptr);
        return;
    }

    pool_header_t* pool = dam_pool_from_ptr(header);
    if (!pool) {
        DAM_LOG_ERR("Pointer does not belong to DAM pool: %p", ptr);
        return;
    }

    switch (pool->type) {
        case DAM_POOL_SMALL:
            DAM_LOG_ERR("SMALL FREE NOT IMPLEMENTED");
            // dam_small_free(ptr);
            break;
        case DAM_POOL_GENERAL:
            dam_general_free(ptr, pool);
            break;
        case DAM_POOL_DIRECT:
            DAM_LOG_ERR("DIRECT FREE NOT IMPLEMENTED");
            // dam_direct_free(ptr);
            break;
        default:
            DAM_LOG_ERR("Unknown pool type for ptr %p", ptr);
            break;
    }
}


