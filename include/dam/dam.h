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
int  dam_init(void);
void dam_shutdown(void);

/* ================================
 * Diagnostics API
 * ================================ */
void* dam_trace_malloc(size_t size, char* trace);
void* dam_trace_realloc(void*, size_t size, char* trace);
char* dam_get_trace(void* ptr);
void  dam_uaf_free();

/* ================================
 * Diagnostics API
 * ================================ */
void    dam_snapshot(dam_snapshot_t* snapshot);
size_t  dam_fragmentation(dam_pool_fragmentation_t* snapshot_buffer, size_t capacity);
size_t  dam_pressure(dam_pool_pressure_t* snapshot_buffer, size_t capacity);
size_t  dam_pool_count();
uint8_t dam_validate_ptr(void* ptr, uint8_t quarantine);
uint8_t dam_validate(uint8_t quarantine);
dam_layer_type_t dam_layer_for_size(size_t size);

#ifdef __cplusplus
}
#endif

#endif /* DAM_H */
