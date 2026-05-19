#ifndef DAM_LOG_H
#define DAM_LOG_H

#include "dam_config.h"
#include <stdio.h>

#if DAM_DEBUG

#include <stdio.h>

#define DAM_LOG(fmt, ...) \
    fprintf(stderr, "[DAM] " fmt "\n", ##__VA_ARGS__)

#else

#define DAM_LOG(...)     ((void)0)

#endif

#if DAM_DEBUG_ERROR

#define DAM_LOG_ERROR(fmt, ...) \
fprintf(stderr, "[DAM][ERROR] " fmt "\n", ##__VA_ARGS__)

#else

#define DAM_LOG_ERROR(...) ((void)0)

#endif

#if DAM_ENABLE_VALIDATION

#define DAM_LOG_VALID(fmt, ...) \
fprintf(stderr, "[DAM][VALIDATE] " fmt "\n", ##__VA_ARGS__)

#define DAM_LOG_VALID_ERROR(fmt, ...) \
fprintf(stderr, "[DAM][VALIDATE][ERROR] " fmt "\n", ##__VA_ARGS__)

#else

#define DAM_LOG_VALID(...) ((void)0)
#define DAM_LOG_VALID_ERROR(...) ((void)0)

#endif

#endif /* DAM_LOG_H */
