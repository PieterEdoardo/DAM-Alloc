#include "dam/internal/thread.h"

#include <pthread.h>

#include "dam/internal/dam_internal.h"


static pthread_mutex_t small_lock;
static pthread_mutex_t general_lock;
static pthread_mutex_t direct_lock;

static int dam_lock_initialized = 0;

static __thread thread_cache_t* thread_cache = NULL;

static pthread_key_t dam_thread_key;
static pthread_once_t dam_thread_key_once = PTHREAD_ONCE_INIT;

extern void dam_small_free_to_central(void* ptr);

void dam_thread_init(void) {
    if (dam_lock_initialized) return;

    pthread_mutex_init(&small_lock, NULL);
    pthread_mutex_init(&general_lock, NULL);
    pthread_mutex_init(&direct_lock, NULL);
    dam_lock_initialized = 1;
}

void dam_small_lock(void) { pthread_mutex_lock(&small_lock); }
void dam_small_unlock(void) { pthread_mutex_unlock(&small_lock); }

void dam_general_lock(void) { pthread_mutex_lock(&general_lock); }
void dam_general_unlock(void) { pthread_mutex_unlock(&general_lock); }

void dam_direct_lock(void) { pthread_mutex_lock(&direct_lock); }
void dam_direct_unlock(void) { pthread_mutex_unlock(&direct_lock); }

static void thread_cache_destructor(void* cache_ptr) {

}

static void make_thread_cache_key(void) {
    pthread_key_create(&dam_thread_key, thread_cache_destructor);
}

thread_cache_t* dam_get_thread_cache(void) {

}

void dam_thread_cache_destroy(void ) {

}