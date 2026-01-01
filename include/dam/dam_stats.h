#ifndef DAM_STATS_H
#define DAM_STATS_H

#include <stddef.h>
#include <stdint.h>

typedef struct dam_stats {
    uint64_t allocations;
    uint64_t frees;
    uint64_t reallocs;

    uint64_t splits;
    uint64_t coalesces;

    size_t bytes_allocated;
    size_t bytes_peak;

    uint32_t corruption_events;
} dam_stats_t;

/* Read-only snapshot */
const dam_stats_t* dam_get_stats(void);

#endif /* DAM_STATS_H */
