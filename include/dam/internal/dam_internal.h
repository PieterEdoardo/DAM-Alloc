#pragma once

#include <stdint.h>

#include "dam/internal/dam_internal.h"

/*******************
 * Data Structures *
 *******************/
typedef enum {
    DAM_POOL_SMALL,
    DAM_POOL_GENERAL,
    DAM_POOL_DIRECT,
} dam_pool_type_t;

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
    dam_pool_type_t type;
    struct pool_header* next;
    block_header_t* free_block_list;
} pool_header_t;

typedef struct size_class {
    size_t block_size;
    size_class_header_t* free_class_list;
    pool_header_t* pools;
} size_class_t;

static struct {
    size_t allocations;
    size_t frees;
    size_t blocks_searched;
    size_t splits;
    size_t coalesces;
    size_t pools_created;
} stats = {0};

typedef struct block_header block_header_t;
typedef struct pool_header pool_header_t;

extern pool_header_t* dam_pool_list;
extern int initialized;

// core internals
void init_allocator(void);
void init_allocator_unlocked(void);
void* dam_malloc_internal(size_t size);
void dam_free_internal(void* ptr);
void* dam_realloc_internal(void* ptr, size_t size);

/* helpers */
void dam_register_pool(pool_header_t* new_pool_header);
void dam_unregister_pool(pool_header_t* pool_header);
pool_header_t* create_general_pool(size_t min_size);
block_header_t* find_block_in_pools(size_t actual_size, pool_header_t** found_pool);
void split_block_if_possible(block_header_t* block_header, size_t actual_size);
void coalesce_if_possible(block_header_t* block_header, pool_header_t* pool_header_t);
size_t align_up(size_t size, size_t alignment);
int verify_page_size(void);
pool_header_t* dam_pool_from_ptr(void* address);
size_class_header_t* get_size_class_header(void* ptr);
block_header_t* get_block_header(void* ptr);
pool_header_t* direct_pool_from_ptr(void* ptr);
block_header_t* direct_block_from_ptr(void* ptr);


/* allocator entry points */
void dam_small_init(void);
void dam_general_init(void);
void dam_direct_init(void);

void* dam_small_malloc(size_t size);
void* dam_general_malloc(size_t size);
void* dam_direct_malloc(size_t size);

void dam_small_free(void* ptr);
void dam_general_free(void* ptr, pool_header_t* pool_header);
void dam_direct_free(void* ptr);

void* dam_small_realloc(void* ptr, size_t size);
void* dam_general_realloc(void* ptr, size_t size);
void* dam_direct_realloc(void* ptr, size_t size);

// Multi-threading
void dam_small_lock(void);
void dam_small_unlock(void);

void dam_general_lock(void);
void dam_general_unlock(void);

void dam_direct_lock(void);
void dam_direct_unlock(void);