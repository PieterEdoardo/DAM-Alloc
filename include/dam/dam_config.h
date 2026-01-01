#ifndef DAM_CONFIG_H
#define DAM_CONFIG_H

#include <stddef.h>
#include <stdint.h>

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

/* ================================
 * Size & alignment
 * ================================ */

#define DAM_ALIGNMENT 16
#define DAM_MIN_BLOCK 32

#define DAM_SMALL_MAX   256
#define DAM_GENERAL_MAX (64 * 1024)

/* ================================
 * Platform invariants
 * ================================ */

#define DAM_STATIC_ASSERT(cond, msg) \
    typedef char static_assert_##msg[(cond) ? 1 : -1]

DAM_STATIC_ASSERT((DAM_ALIGNMENT & (DAM_ALIGNMENT - 1)) == 0,
                  alignment_must_be_power_of_two);

#endif /* DAM_CONFIG_H */
