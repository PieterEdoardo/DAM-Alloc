#pragma once
#include <stddef.h>
#include <stdint.h>

#include "dam/dam_config.h"

/*******************
* Data Structures *
 *******************/
typedef enum {
    DAM_LAYER_ERROR,
    DAM_LAYER_SMALL,
    DAM_LAYER_GENERAL,
    DAM_LAYER_DIRECT,
} dam_layer_type_t;

typedef struct size_class_header {
    uint32_t magic;
    uint8_t size_class_index;
    uint8_t is_free;
    uint16_t padding;
    struct size_class_header* next;
} size_class_header_t;

typedef struct block_header {
    size_t size;
    size_t user_size;
    struct block_header* next;
    struct block_header* prev;
    uint32_t magic;
    uint8_t is_free;
} block_header_t;

typedef struct pool_header {
    void* memory;
    size_t size;
    dam_layer_type_t type;
    uint8_t read_only;
    struct pool_header* next;
    block_header_t* block_list;
} pool_header_t;

typedef struct size_class {
    size_t block_size;
    size_class_header_t* free_class_list;
    pool_header_t* pools;
} size_class_t;

typedef struct {
    size_class_header_t* free_list;
    size_t count;
} thread_cache_bin_t;

typedef struct {
    thread_cache_bin_t bins[DAM_SIZE_CLASS_COUNT];
    size_t allocations;
    size_t deallocations;
} thread_cache_t;

static struct {
    size_t allocations;
    size_t frees;
    size_t blocks_searched;
    size_t splits;
    size_t coalesces;
    size_t pools_created;
} stats = {0};

typedef struct {
    size_t tlc_used;
    size_t tlc_free;
    size_t size_classes;
    size_t classes_bytes_used;
    size_t pools_active;
    size_t pools_bytes_used;
    size_t quarantined_pools;
    size_t direct_allocations;
    size_t direct_bytes_used;
} dam_snapshot_t;

typedef struct {
    size_t used;
    size_t free;
    size_t largest_free;
    float fragmentation;
} dam_pool_snapshot_t;

extern pool_header_t* dam_pool_list;
extern int initialized;
