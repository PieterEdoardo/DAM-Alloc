#ifndef DAM_H
#define DAM_H

#include <stddef.h>

#include "internal/dam_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================
 * Core allocation API
 * ================================ */

void* dam_malloc(size_t size);
void  dam_free(void* ptr);
void* dam_realloc(void* ptr, size_t size);
void* dam_calloc(size_t nmemb, size_t size);

/* ================================
 * Lifecycle
 * ================================ */

int   dam_init(void);
void  dam_shutdown(void);

/* ================================
 * Diagnostics API
 * ================================ */
void dam_snapshot(dam_snapshot_t* snapshot);

#ifdef __cplusplus
}
#endif

#endif /* DAM_H */
