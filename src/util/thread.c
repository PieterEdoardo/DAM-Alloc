#include <pthread.h>

#include "dam/internal/dam_internal.h"


static pthread_mutex_t global_lock;
static pthread_mutex_t small_lock;
static pthread_mutex_t general_lock;
static pthread_mutex_t direct_lock;

static int dam_lock_initialized = 0;

void dam_thread_init(void) {
    if (dam_lock_initialized) return;

    pthread_mutex_init(&global_lock, NULL);
    pthread_mutex_init(&small_lock, NULL);
    pthread_mutex_init(&general_lock, NULL);
    pthread_mutex_init(&direct_lock, NULL);
    dam_lock_initialized = 1;
}

void dam_global_lock(void) { pthread_mutex_lock(&global_lock); }
void dam_global_unlock(void) { pthread_mutex_unlock(&global_lock); }

void dam_small_lock(void) { pthread_mutex_lock(&small_lock); }
void dam_small_unlock(void) { pthread_mutex_unlock(&small_lock); }

void dam_general_lock(void) { pthread_mutex_lock(&general_lock); }
void dam_general_unlock(void) { pthread_mutex_unlock(&general_lock); }

void dam_direct_lock(void) { pthread_mutex_lock(&direct_lock); }
void dam_direct_unlock(void) { pthread_mutex_unlock(&direct_lock); }