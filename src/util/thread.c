#include "dam/internal/thread.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "dam/dam_log.h"
#include "dam/dam_config.h"
#include "dam/internal/dam_internal.h"


static pthread_mutex_t small_lock;
static pthread_mutex_t general_lock;
static pthread_mutex_t direct_lock;

static int dam_lock_initialized = 0;

static __thread thread_cache_t* thread_cache = NULL;

static pthread_key_t dam_thread_cache_key;
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
    if (!cache_ptr) return;
    thread_cache_t* tc = (thread_cache_t*)cache_ptr;

    DAM_LOG("[TCACHE] Thread %lu exiting, flushing %lu allocations back to central",
        pthread_self(), tc->allocations);

    for (size_t class_idx = 0; class_idx < DAM_SIZE_CLASS_COUNT; class_idx++) {
        size_class_header_t* block = tc->bins[class_idx].free_list;

        while (block) {
            size_class_header_t* next = block->next;
            dam_small_free_to_central((char*)block + SIZE_CLASS_HEADER_SIZE);
            block = next;
        }
    }

    munmap(tc, sizeof(thread_cache_t));
}

static void make_thread_cache_key(void) {
    pthread_key_create(&dam_thread_cache_key, thread_cache_destructor);
}

thread_cache_t* dam_get_thread_cache(void) {
    if (thread_cache) {
        return thread_cache;
    }

    thread_cache = mmap(
        NULL,
        sizeof(thread_cache_t),
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );

    if (thread_cache == MAP_FAILED) {
        DAM_LOG_ERROR("[TCACHE] Failed to allocate thread cache");
        return NULL;
    }

    memset(thread_cache, 0, sizeof(thread_cache_t));

    thread_cache = calloc(1, sizeof(thread_cache_t));
    if (!thread_cache) {
        DAM_LOG_ERROR("[TCACHE] Failed to allocate thread cache");
        return NULL;
    }

    pthread_setspecific(dam_thread_cache_key, thread_cache);

    DAM_LOG("[TCACHE] Initialized cache for thread %lu", pthread_self());

    return thread_cache;
}

void dam_thread_cache_destroy(void ) {
    if (thread_cache) {
        thread_cache_destructor(thread_cache);
        thread_cache = NULL;
    }
}