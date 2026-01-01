#pragma once
#include "dam/dam_config.h"
#include "dam/dam_stats.h"

typedef struct block_header block_header_t;
typedef struct pool pool_t;

/* helpers */
void split_block_if_possible(...);
void coalesce_if_possible(...);

/* tier entry points */
void* dam_small_alloc(size_t);
void* dam_general_alloc(size_t);
void* dam_direct_alloc(size_t);
