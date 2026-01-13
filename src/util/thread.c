#include <pthread.h>

#include "dam/internal/dam_internal.h"

static pthread_mutex_t dam_global_lock;
static int dam_lock_initialized = 0;

void dam_thread_init(void) {
    if (dam_lock_initialized) return;

    pthread_mutex_init(&dam_global_lock, NULL);
    dam_lock_initialized = 1;
}

void dam_lock(void) {
    pthread_mutex_lock(&dam_global_lock);
}

void dam_unlock(void) {
    pthread_mutex_unlock(&dam_global_lock);
}