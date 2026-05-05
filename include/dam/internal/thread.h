#ifndef DAM_THREAD_H
#define DAM_THREAD_H

#include <stddef.h>
#include <stdint.h>

#include "dam/internal/dam_internal.h"
#include "dam/dam_config.h"

void dam_thread_init(void);

typedef struct size_class_header size_class_header;

#define THREAD_CACHE_MAX_BLOCKS_PER_CLASS 64
#define THREAD_CACHE_REFILL_BATCH_SIZE 8

typedef struct {
    size_class_header_t* free_list;
    size_t count;
} thread_cache_bin_t;

typedef struct {
    thread_cache_bin_t bins[4];
    uint64_t allocations;
    uint64_t deallocations;
} thread_cache_t;

thread_cache_t* dam_get_thread_cache(void);
void dam_thread_cache_destroy(void);

#endif