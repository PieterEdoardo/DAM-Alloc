#ifndef DAM_CONFIG_H
#define DAM_CONFIG_H

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include "internal/dam_internal.h"

/* ================================
 * Build configuration
 * ================================ */

#ifndef DAM_DEBUG
#define DAM_DEBUG 1
#endif

#ifndef DAM_ENABLE_STATS
#define DAM_ENABLE_STATS 1
#endif

#ifndef DAM_ENABLE_VALIDATION
#define DAM_ENABLE_VALIDATION 1
#endif

/******************
 * Configuration *
 ******************/
#define DAM_SMALL_MAX 256
#define DAM_GENERAL_MAX KiB(64)
#define MAX_POOLS 10
#define INITIAL_POOL_SIZE MiB(1)

/********************
 * Size & alignment *
 ********************/
#define ALIGN_UP_CONST(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#define ALIGNMENT (_Alignof(max_align_t))
#define PAGE_SIZE 4096 // Assumed number


// Headers
#define HEAD_SIZE ALIGN_UP_CONST(sizeof(block_header_t), ALIGNMENT)
#define HEAD_SMALL_SIZE ALIGN_UP_CONST(sizeof(block_small_header_t), ALIGNMENT)
#define HEAD_GENERAL_SIZE ALIGN_UP_CONST(sizeof(block_general_header_t), ALIGNMENT)
#define HEAD_DIRECT_SIZE ALIGN_UP_CONST(sizeof(block_direct_header_t), ALIGNMENT)

// Blocks minimum
#define DAM_MIN_BLOCK (ALIGNMENT + MIN_BLOCK_SIZE)
#define BLOCK_SMALL_MIN (HEAD_SMALL_SIZE + ALIGNMENT)
#define MIN_BLOCK_SIZE ALIGN_UP_CONST(HEAD_SIZE + (ALIGNMENT * 2), ALIGNMENT)

#define POOL_HEADER_SIZE ALIGN_UP_CONST(sizeof(pool_header_t), ALIGNMENT)
#define POOL_SMALL_SIZE ALIGN_UP_CONST(sizeof(pool_header_t), ALIGNMENT)
#define POOL_GENERAL_SIZE ALIGN_UP_CONST(sizeof(pool_header_t), ALIGNMENT)

/******************
 * resources *
 ******************/
#define KiB(x) ((size_t)(x) * 1024)
#define MiB(x) (KiB(x) * 1024)
#define GiB(x) (MiB(x) * 1024)
#define BLOCK_MAGIC 0xDEADBEEF
#define FREED_MAGIC 0xFEEDFACE
#define CANARY_VALUE 0xDEADC0DE

/*******************
 * Data Structures *
 *******************/
typedef struct block_header {
    size_t size;
    size_t user_size;
    struct block_header* next;
    struct block_header* prev;
    uint32_t magic;
    uint8_t is_free;
} block_header_t;

typedef struct block_small_header {
    size_t size;
    size_t user_size;
    struct block_header* next;
    struct block_header* prev;
    uint32_t magic;
    uint8_t is_free;
} block_small_header_t;

typedef struct block_general_header {
    size_t size;
    size_t user_size;
    struct block_header* next;
    struct block_header* prev;
    uint32_t magic;
    uint8_t is_free;
} block_general_header_t;

typedef struct block_direct_header {
    size_t size;
    size_t user_size;
    struct block_header* next;
    struct block_header* prev;
    uint32_t magic;
    uint8_t is_free;
} block_direct_header_t;

typedef struct pool_header {
    void* memory;
    size_t size;
    block_header_t* free_list_head;
    struct pool_header* next;
} pool_header_t;

static struct {
    size_t allocations;
    size_t frees;
    size_t blocks_searched;
    size_t splits;
    size_t coalesces;
    size_t pools_created;
} stats = {0};

/* ================================
 * Platform invariants
 * ================================ */
_Static_assert(ALIGNMENT >= _Alignof(max_align_t), "ALIGNMENT must match platform max alignment");
_Static_assert(HEAD_SIZE % ALIGNMENT == 0, "HEAD_SIZE must preserve payload alignment");
_Static_assert(POOL_HEADER_SIZE % ALIGNMENT == 0, "Pool header must preserve block alignment");
_Static_assert(INITIAL_POOL_SIZE >= POOL_HEADER_SIZE + MIN_BLOCK_SIZE, "Initial pool size is too small");
_Static_assert(INITIAL_POOL_SIZE % PAGE_SIZE == 0, "Pool size must be a multiple of PAGE_SIZE");

#endif /* DAM_CONFIG_H */
