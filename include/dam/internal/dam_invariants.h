#pragma once

#include "dam/dam_config.h"

/* ================================ *
 * Platform invariants              *
 * ================================ */
// _Static_assert(1 == 0, "Test");// invariants are live check
_Static_assert(ALIGNMENT >= _Alignof(max_align_t), "ALIGNMENT must match platform max alignment");
_Static_assert(BLOCK_HEADER_SIZE % ALIGNMENT == 0, "HEAD_SIZE must preserve payload alignment");
_Static_assert(POOL_GENERAL_SIZE % ALIGNMENT == 0, "Pool header must preserve block alignment");
_Static_assert(INITIAL_POOL_SIZE >= POOL_GENERAL_SIZE + MIN_BLOCK_SIZE, "Initial pool size is too small");
_Static_assert(INITIAL_POOL_SIZE % PAGE_SIZE == 0, "Pool size must be a multiple of PAGE_SIZE");
_Static_assert((DAM_SMALL_MIN & DAM_SMALL_MIN - 1) == 0, "DAM_SMALL_MIN must be power of two");
_Static_assert((DAM_SMALL_MAX & DAM_SMALL_MAX - 1) == 0, "DAM_SMALL_MAX must be power of two");
_Static_assert(DAM_SMALL_MIN <= DAM_SMALL_MAX, "Invalid size class range");
_Static_assert(DAM_SMALL_MAX <= DAM_GENERAL_MAX, "Invalid allocator boundaries");
_Static_assert(DAM_SIZE_CLASS_COUNT <= 255, "Bigger than 255 would overflow class header with an extra byte.");
_Static_assert(sizeof(SMALL_MAGIC) <= sizeof(uint32_t), "SMALL_MAGIC too large for size_class_header");
_Static_assert(sizeof(SMALL_FREED_MAGIC) <= sizeof(uint32_t), "SMALL_FREED_MAGIC too large for size_class_header");