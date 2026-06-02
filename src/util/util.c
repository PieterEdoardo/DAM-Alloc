#include <assert.h>
#include <unistd.h>

#include "dam/dam_config.h"
#include "dam/dam_log.h"
#include "dam/internal/dam_internal.h"

typedef struct pool_header pool_header_t;
/*********************
 * Helper Functions *
 *********************/

inline size_t align_up(size_t size, size_t alignment) {
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

pool_header_t* dam_pool_from_ptr(void* ptr) {
    pool_header_t* pool_header = dam_pool_list;

    while (pool_header) {
        if (ptr >= pool_header->memory && (char*)ptr < (char*)pool_header->memory + pool_header->size) {
            return pool_header;
        }
        pool_header = pool_header->next;
    }
    return NULL;
}