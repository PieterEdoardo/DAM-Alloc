#pragma once
#include <stddef.h>
#include "dam/internal/dam_internal.h"

/*******************
 * Data Structures *
 *******************/
typedef enum {
    DAM_POOL_SMALL,
    DAM_POOL_GENERAL,
    DAM_POOL_DIRECT,
} dam_pool_type_t;

typedef struct block_header {
    size_t size;
    size_t user_size;
    struct block_header* next;
    struct block_header* prev;
    uint32_t magic;
    uint8_t is_free;
} block_header_t;

typedef struct size_class_header {
    uint32_t magic;
    uint8_t size_class_index;
    uint8_t is_free;
    uint16_t padding;
    struct size_class_header* next;
} size_class_header_t;

typedef struct pool_header {
    void* memory;
    size_t size;
    dam_pool_type_t type;
    struct pool_header* next;
    block_header_t* free_list_head;
} pool_header_t;

typedef struct size_class {
    size_t block_size;
    size_class_header_t* free_list;
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

extern pool_header_t* pool_list_head;
extern int initialized;

/* helpers */
void init_allocator(void);
pool_header_t* create_general_pool(size_t min_size);
block_header_t* find_block_in_pools(size_t actual_size, pool_header_t** found_pool);
void split_block_if_possible(block_header_t* block_header, size_t actual_size);
void coalesce_if_possible(block_header_t* block_header, pool_header_t* pool_header_t);
size_t align_up(size_t size, size_t alignment);
int verify_page_size(void);
pool_header_t* dam_pool_from_ptr(void* address);


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