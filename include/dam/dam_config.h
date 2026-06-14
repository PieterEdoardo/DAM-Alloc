#ifndef DAM_CONFIG_H
#define DAM_CONFIG_H

/* ================================ *
 * Build configuration              *
 * ================================ */
#ifndef DAM_DEBUG
#define DAM_DEBUG 0
#endif

#ifndef DAM_DEBUG_ERROR
#define DAM_DEBUG_ERROR 1
#endif

#ifndef DAM_ENABLE_STATS
#define DAM_ENABLE_STATS 1
#endif

#ifndef DAM_ENABLE_VALIDATION
#define DAM_ENABLE_VALIDATION 1
#endif

/*****************
 * Configuration *
 *****************/
#define DAM_SMALL_MIN 16
#define DAM_SMALL_MAX 256
#define DAM_GENERAL_MAX KiB(64)
#define MAX_POOLS 20
#define MAX_POOL_OVERFLOW_TO_DIRECT_ALLOWED 0
#define INITIAL_POOL_SIZE MiB(1)
#define DAM_DIRECT_SHRINK_PERCENTAGE 80

/********************
 * Size & alignment *
 ********************/
#define ALIGN_UP_CONST(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#define ALIGNMENT (_Alignof(max_align_t))
#define PAGE_SIZE KiB(4) // Assumed number
#define TRACE_SIZE 16 // Can be dangerous to change

// Headers
#define BLOCK_HEADER_SIZE ALIGN_UP_CONST(sizeof(block_header_t), ALIGNMENT)
#define FREE_BLOCK_HEADER_SIZE ALIGN_UP_CONST(sizeof(free_block_header_t), ALIGNMENT)
#define SIZE_CLASS_HEADER_SIZE align_up(sizeof(size_class_header_t), ALIGNMENT)

// Size classes
#define DAM_SIZE_CLASS_COUNT ((__builtin_ctzll(DAM_SMALL_MAX) - __builtin_ctzll(DAM_SMALL_MIN)) + 1)
#define SIZE_CLASS_MULTIPLIER 2
#define SIZE_CLASS_BLOCKS_PER_POOL 1000

// Pools & blocks
#define MIN_BLOCK_SIZE ALIGN_UP_CONST(BLOCK_HEADER_SIZE + DAM_SMALL_MAX, ALIGNMENT)
#define POOL_GENERAL_SIZE ALIGN_UP_CONST(sizeof(pool_header_t), PAGE_SIZE)

// Multi threading & thread local caches
#define THREAD_CACHE_MAX_BLOCKS_PER_CLASS 64
#define THREAD_CACHE_REFILL_BATCH_SIZE 8

/******************
 * resources      *
 ******************/
#define KiB(x) ((size_t)(x) * 1024)
#define MiB(x) (KiB(x) * 1024)
#define GiB(x) (MiB(x) * 1024)
#define BLOCK_MAGIC 0xDEADBEEF
#define FREED_MAGIC 0xFEEDFACE
#define SMALL_MAGIC 0xD34D
#define SMALL_FREED_MAGIC 0xF33D
#define CANARY_VALUE 0xDEADC0DE

#endif
