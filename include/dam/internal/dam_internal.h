#pragma once

#include <stdint.h>

#include "dam/internal/dam_internal.h"
#include "dam/internal/dam_types.h"

// Core internals
void init_allocator(void);
void init_allocator_unlocked(void);
void* dam_malloc_internal(size_t size);
void dam_free_internal(void* ptr);
void* dam_realloc_internal(void* ptr, size_t size);

// Multi threading & thread local caches
void dam_thread_init(void);
thread_cache_t* dam_get_thread_cache(void);
void dam_thread_cache_destroy(void);
thread_cache_t* dam_get_current_thread_cache(void);

// Diagnostic API
void dam_snapshot_small(dam_snapshot_t* snapshot);
void dam_snapshot_general(dam_snapshot_t* snapshot);
void dam_snapshot_direct(dam_snapshot_t* snapshot);
dam_layer_type_t dam_layer_for_size(size_t size);
void dam_general_fragmentation(pool_header_t* pool, dam_pool_snapshot_t* snapshot);

/* Helpers */
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

void* dam_small_malloc_internal(size_t size);
void* dam_general_malloc_internal(size_t size);
void* dam_direct_malloc_internal(size_t size);

void dam_small_free_internal(void* ptr);
void dam_general_free_internal(void* ptr, pool_header_t* pool_header);
void dam_direct_free_internal(void* ptr);

#include "dam/internal/dam_invariants.h"