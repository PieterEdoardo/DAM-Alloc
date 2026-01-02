#pragma once
#include <stddef.h>

typedef struct block_header block_header_t;
typedef struct pool_header pool_header_t;

extern pool_header_t* pool_list_head;
extern int initialized;

/* helpers */
void init_allocator(void);
pool_header_t* create_pool(size_t min_size);
block_header_t* find_block_in_pools(size_t actual_size, pool_header_t** found_pool);
void split_block_if_possible(block_header_t* block_header, size_t actual_size);
void coalesce_if_possible(block_header_t* block_header, pool_header_t* pool_header);
size_t align_up(size_t size);
int verify_page_size(void);


/* allocator entry points */
void* dam_small_malloc(size_t size);
void* dam_general_malloc(size_t size);
void* dam_direct_malloc(size_t size);

void dam_small_free(void* ptr);
void dam_general_free(void* ptr);
void dam_direct_free(void* ptr);

void* dam_small_realloc(void* ptr, size_t size);
void* dam_general_realloc(void* ptr, size_t size);
void* dam_direct_realloc(void* ptr, size_t size);