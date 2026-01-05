#ifndef DAM_CONFIG_H
#define DAM_CONFIG_H

#include <stddef.h>
#include <stdint.h>

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
#define DAM_SMALL_MIN 32
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
#define SIZE_CLASS_HEADER_SIZE align_up(sizeof(size_class_header_t), ALIGNMENT)

// Blocks minimum
#define DAM_MIN_BLOCK (ALIGNMENT + MIN_BLOCK_SIZE)
#define BLOCK_SMALL_MIN (HEAD_SMALL_SIZE + ALIGNMENT)
#define SIZE_CLASS_MULTIPLIER 2
#define SIZE_CLASS_BLOCKS_PER_POOL 1000
#define MIN_BLOCK_SIZE ALIGN_UP_CONST(HEAD_SIZE + (ALIGNMENT * 2), ALIGNMENT)


#define POOL_SMALL_SIZE ALIGN_UP_CONST(sizeof(pool_header_t), PAGE_SIZE)
#define POOL_GENERAL_SIZE ALIGN_UP_CONST(sizeof(pool_header_t), PAGE_SIZE)
/******************
 * resources *
 ******************/
#define KiB(x) ((size_t)(x) * 1024)
#define MiB(x) (KiB(x) * 1024)
#define GiB(x) (MiB(x) * 1024)
#define BLOCK_MAGIC 0xDEADBEEF
#define FREED_MAGIC 0xFEEDFACE
#define CANARY_VALUE 0xDEADC0DE

/* ================================
 * Platform invariants
 * ================================ */
_Static_assert(ALIGNMENT >= _Alignof(max_align_t), "ALIGNMENT must match platform max alignment");
_Static_assert(HEAD_SIZE % ALIGNMENT == 0, "HEAD_SIZE must preserve payload alignment");
_Static_assert(POOL_GENERAL_SIZE % ALIGNMENT == 0, "Pool header must preserve block alignment");
_Static_assert(INITIAL_POOL_SIZE >= POOL_GENERAL_SIZE + MIN_BLOCK_SIZE, "Initial pool size is too small");
_Static_assert(INITIAL_POOL_SIZE % PAGE_SIZE == 0, "Pool size must be a multiple of PAGE_SIZE");
_Static_assert((DAM_SMALL_MIN & DAM_SMALL_MIN - 1) == 0, "DAM_SMALL_MIN must be power of two");
_Static_assert((DAM_SMALL_MAX & DAM_SMALL_MAX - 1) == 0, "DAM_SMALL_MAX must be power of two");
_Static_assert(DAM_SMALL_MIN <= DAM_SMALL_MAX, "Invalid size class range");
_Static_assert(DAM_SMALL_MAX <= DAM_GENERAL_MAX, "Invalid allocator boundaries");

#endif /* DAM_CONFIG_H */
