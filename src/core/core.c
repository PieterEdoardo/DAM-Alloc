#include <stdio.h>
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
    if (size <= DAM_SMALL_MAX) return dam_small_malloc(size, NULL);
    if (size <= DAM_GENERAL_MAX) return dam_general_malloc(size, NULL);
    return dam_direct_malloc(size, NULL);
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
            return dam_small_realloc(ptr, size, NULL);

        case DAM_LAYER_GENERAL:
            return dam_general_realloc(ptr, size, NULL);

        case DAM_LAYER_DIRECT:
            return dam_direct_realloc(ptr, size, NULL);

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

void* dam_trace_malloc(size_t size, const char* trace) {

    if (!initialized) dam_init();

    if (size == 0) return NULL;
    if (size <= DAM_SMALL_MAX) return dam_small_malloc(size, trace);
    if (size <= DAM_GENERAL_MAX) return dam_general_malloc(size, trace);
    return dam_direct_malloc(size, trace);
}

void* dam_trace_realloc(void* ptr, size_t size, const char* trace) {
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

    // none of this works yet
    switch (pool->type) {
        case DAM_LAYER_SMALL:
            return dam_small_realloc(ptr, size, trace);

        case DAM_LAYER_GENERAL:
            return dam_general_realloc(ptr, size, trace);

        case DAM_LAYER_DIRECT:
            return dam_direct_realloc(ptr, size, trace);

        default:
            DAM_LOG_ERROR("[REALLOC] Unknown pool type for ptr %p", ptr);
            return NULL;
    }
}
char* dam_get_trace(void* ptr) {
     pool_header_t* pool = dam_pool_from_ptr(ptr);

    if (!pool) {
        DAM_LOG_ERROR("[TRACE] Pointer does not belong to DAM: %p", ptr);
        return NULL;
    }

    switch (pool->type) {
        case DAM_LAYER_SMALL:
            return get_small_trace(ptr);

        case DAM_LAYER_GENERAL:
            return get_general_trace(ptr);

        case DAM_LAYER_DIRECT:
            return get_direct_trace(ptr);

        default:
            DAM_LOG_ERROR("[REALLOC] Unknown pool type for ptr %p", ptr);
            return NULL;
    }
}

void dam_uaf_free(void* ptr) {
    pool_header_t* pool_header = dam_pool_from_ptr(ptr);

    if (!pool_header) {
        DAM_LOG_ERROR("[REALLOC] Pointer does not belong to DAM: %p", ptr);
    } else {
        switch (pool_header->type) {
            case DAM_LAYER_SMALL:
                memset(ptr, 0, class_to_size(get_size_class_header(ptr)->size_class_index));
                dam_small_free(ptr);
                break;

            case DAM_LAYER_GENERAL:
                memset(ptr, 0, get_block_header(ptr)->size);
                dam_general_free(ptr, pool_header);
                break;

            case DAM_LAYER_DIRECT:
                memset(ptr, 0, get_block_header(ptr)->size);
                dam_direct_free(ptr);
                break;

            default:
                DAM_LOG_ERROR("[UAF][FREE] Unknown layer type for ptr %p", ptr);
                break;
        }
    }
}

/*
 * Creates systemwide snapshot of each layer and their usage statistics. Expensive, and slow.
 */
void dam_snapshot(dam_snapshot_t* snapshot) {
    dam_snapshot_small(snapshot);
    dam_snapshot_general(snapshot);
    dam_snapshot_direct(snapshot);
}

dam_layer_type_t dam_layer_for_size(size_t size) {
    if (size == 0) return DAM_LAYER_ERROR;
    if (size <= DAM_SMALL_MAX) return DAM_LAYER_SMALL;
    if (size <= DAM_GENERAL_MAX) return DAM_LAYER_GENERAL;
    return DAM_LAYER_DIRECT;
}

/*
 * 1.0 is no fragmentation and 0.0 is maximum.
 * 0.0 would also happen if all pools are perfectly filled up.
 * Fragmentation only pertains to general pools.
 * n > DAM_SMALL_MAX && n <= DAM_GENERAL_MAX
 * Other layers don't experience fragmentation in the traditional sense.
 * Usage example:
 * size_t pool_count = dam_pool_count();
 * dam_pool_snapshot_t buffer[pool_count];
 * size_t count = dam_general_pool_snapshots(buffer, pool_count);
 */
size_t dam_fragmentation(dam_pool_fragmentation_t* snapshot_buffer, size_t capacity) {
    pool_header_t* current = dam_pool_list;
    size_t count = 0;
    while (current) {
        if (current->type == DAM_LAYER_GENERAL && count < capacity) {
            dam_general_fragmentation(current, &snapshot_buffer[count]);
            count++;
        }
        current = current->next;
    }

    return count;
}

// This function is kinda useless for the public API as it only counts general pools.
size_t dam_pool_count() {
    pool_header_t* current = dam_pool_list;
    size_t count = 0;
    while (current) {
        if (current->type == DAM_LAYER_GENERAL) count++;
        current = current->next;
    }

    return count;
}
/*
 * Expensive function that checks metadata integrity of pointer. Does some required pool metadata testing as well.
 * If second argument is set to a non 0 number it will place the associated pool in quarantine in case of event.
 */
uint8_t dam_validate_ptr(void* ptr, uint8_t quarantine, uint8_t is_traced) {
    if (!ptr) {
        DAM_LOG_VALID_ERROR("Pointer invalid: %p", ptr);
        return 0;
    }
    pool_header_t* pool_header = dam_pool_from_ptr(ptr);

    if (!pool_header) {
        DAM_LOG_VALID_ERROR("Pointer does not belong to DAM pool: %p", ptr);
        return 0;
    }

    if (pool_header->read_only) DAM_LOG_VALID("Pointer belongs to quarantined pool: %p", ptr);
    if (!pool_header->size) DAM_LOG_VALID("Pointer pool has no size: %p", ptr);

    uint8_t result = 0;
    switch (pool_header->type) {
        case DAM_LAYER_ERROR:
            DAM_LOG_VALID_ERROR("Header layer type invalid: %p, given type: %d", ptr, pool_header->type);
            break;

        case DAM_LAYER_SMALL:
            dam_small_lock();
            result = dam_validate_small_ptr(ptr, is_traced);
            dam_small_unlock();
            break;

        case DAM_LAYER_GENERAL:
            dam_general_lock();
            result = dam_validate_general_ptr(ptr, pool_header, quarantine, is_traced);
            dam_general_unlock();
            break;

        case DAM_LAYER_DIRECT:
            dam_direct_lock();
            result = dam_validate_direct_ptr(ptr, is_traced);
            dam_direct_unlock();
            break;

        default:
            DAM_LOG_VALID_ERROR("Header layer type invalid: %p, given type: %d", ptr, pool_header->type);
            break;
    }
    if (result) DAM_LOG_VALID("Pointer: %p is validated to be in a safe state", ptr);
    return result;
}

/*
 * Very expensive sequence that loops through all pools of all layers and memory segments to validate metadata.
 * expected complexity for this function is O(n * x) where n is amount of pools and x is the complexity of dam_validate_ptr()
 */
uint8_t dam_validate(uint8_t quarantine) {
    // pool_header_t* pool_header = dam_pool_list;
    //
    // while (pool_header) {
    //
    //     uint8_t result;
    //
    //     switch (pool_header->type) {
    //         case DAM_LAYER_ERROR:
    //             DAM_LOG_VALID_ERROR("Header layer type invalid: %p, given type: %d", pool_header, pool_header->type);
    //             break;
    //
    //         case DAM_LAYER_SMALL:
    //             dam_small_lock();
    //
    //             size_class_header_t* size_class_header = dam_size_class_header_from_ptr(pool_header->block_list, quarantine);
    //
    //             result = dam_validate_small_ptr(ptr, is_traced);
    //             dam_small_unlock();
    //             break;
    //
    //         case DAM_LAYER_GENERAL:
    //             dam_general_lock();
    //             result = dam_validate_general_ptr(ptr, pool_header, quarantine, is_traced);
    //             dam_general_unlock();
    //             break;
    //
    //         case DAM_LAYER_DIRECT:
    //             dam_direct_lock();
    //             result = dam_validate_direct_ptr(ptr, is_traced);
    //             dam_direct_unlock();
    //             break;
    //
    //         default:
    //             DAM_LOG_VALID_ERROR("Header layer type invalid: %p, given type: %d", pool_header, pool_header->type);
    //             break;
    //     }
    //
    //     dam_validate_ptr(pool_header, quarantine, pool_header->is_traced);
    //
    //     pool_header = pool_header->next;
    // }
    return 1;
}