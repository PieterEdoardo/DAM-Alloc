#pragma once

#include <stdint.h>

#include "dam/internal/dam_internal.h"
#include "dam/internal/dam_types.h"

// Multi threading & thread local caches
void dam_thread_init(void);
thread_cache_t* dam_get_thread_cache(void);
void dam_thread_cache_destroy(void);
thread_cache_t* dam_get_current_thread_cache(void);

// Diagnostic API
void dam_snapshot_small(dam_snapshot_t* snapshot);
void dam_snapshot_general(dam_snapshot_t* snapshot);
void dam_snapshot_direct(dam_snapshot_t* snapshot);
void dam_general_fragmentation(pool_header_t* pool, dam_pool_fragmentation_t* snapshot);
void dam_general_pressure(pool_header_t* pool, dam_pool_pressure_t* snapshot);
uint8_t dam_validate_small_ptr(void* ptr, size_class_header_t* size_class_header);
uint8_t dam_validate_general_ptr(void* ptr, pool_header_t* pool_header, uint8_t quarantine, block_header_t* block_header);
uint8_t dam_validate_direct_ptr(void* ptr, const block_header_t* direct_header);

/* Helpers */
void dam_register_pool(pool_header_t* new_pool_header);
void dam_unregister_pool(pool_header_t* pool_header);
pool_header_t* create_general_pool(size_t min_size);
block_header_t* find_block_in_pools(size_t actual_size, pool_header_t** found_pool);
void split_block_if_possible(block_header_t* block_header, size_t actual_size);
void coalesce_if_possible(block_header_t* block_header, pool_header_t* pool_header);
uint32_t* dam_get_general_canary(void* ptr, block_header_t* block_header);
void general_pool_quarantine(pool_header_t* pool_header);
size_t align_up(size_t size, size_t alignment);
int verify_page_size(void);
pool_header_t* dam_pool_from_ptr(void* ptr);
size_class_header_t* get_size_class_header(void* ptr);
size_class_header_t* get_size_class_trace_header(void* ptr);
block_header_t* get_block_header(void* ptr);
block_header_t* get_block_trace_header(void* ptr);
pool_header_t* direct_pool_from_ptr(void* ptr);
block_header_t* get_direct_header(void* ptr);
block_header_t* get_direct_trace_header(void* ptr);
size_t class_to_size(uint8_t class_index);
uint8_t size_to_class(size_t size, uint8_t traced);
void add_to_free_list(pool_header_t*, block_header_t* block_header);
block_header_t* search_in_free_list(pool_header_t* pool_header, size_t actual_size);
block_header_t* find_free_block_in_pools(pool_header_t** pool_header, size_t actual_size);
void remove_from_free_list(pool_header_t* pool_header, block_header_t* block_header);
free_block_header_t* get_free_block_header(block_header_t* block_header);


/* allocator entry points */
void dam_small_init(void);
void dam_general_init(void);
void dam_direct_init(void);

void* dam_small_malloc(size_t size, const char* trace);
void* dam_general_malloc(size_t size, const char* trace);
void* dam_direct_malloc(size_t size, const char* trace);

void dam_small_free(void* ptr, size_class_header_t* size_class_header);
void dam_general_free(void* ptr, const pool_header_t* pool_header, block_header_t* block_header);
void dam_direct_free(void* ptr);

void* dam_small_realloc(void* ptr, size_t size, size_class_header_t* size_class_header, const char* trace);
void* dam_general_realloc(void* ptr, size_t size, block_header_t* block_header, const char* trace);
void* dam_direct_realloc(void* ptr, size_t size, const block_header_t* direct_header, const char* trace);

// Multi-threading
void dam_small_lock(void);
void dam_small_unlock(void);

void dam_general_lock(void);
void dam_general_unlock(void);

void dam_direct_lock(void);
void dam_direct_unlock(void);

void* dam_small_malloc_internal(size_t size, const char* trace);
void* dam_general_malloc_internal(size_t size, const char* trace);
void* dam_direct_malloc_internal(size_t size, const char* trace);

void dam_small_free_internal(void* ptr, size_class_header_t* size_class_header);
void dam_general_free_internal(void* ptr, pool_header_t* pool_header, block_header_t* block_header);
void dam_direct_free_internal(void* ptr);

#include "dam/internal/dam_invariants.h"