// test_tcache.c
#include <pthread.h>
#include <stdio.h>
#include <assert.h>
#include "dam/dam.h"
#include "dam/dam_log.h"

// Test 1: Single-threaded cache reuse
void test_tcache_reuse(void) {
    DAM_LOG("\n=== Test 1: TCache Reuse ===");

    void* p1 = dam_malloc(32);
    printf("Allocated p1 = %p\n", p1);

    dam_free(p1);
    printf("Freed p1\n");

    void* p2 = dam_malloc(32);
    printf("Allocated p2 = %p\n", p2);

    if (p1 == p2) {
        DAM_LOG("[PASS] TCache reused block: %p == %p", p1, p2);
    } else {
        DAM_LOG("[FAIL] TCache didn't reuse: %p != %p", p1, p2);
    }

    dam_free(p2);
}

// Test 2: Cache fills up
void test_tcache_overflow(void) {
    DAM_LOG("\n=== Test 2: TCache Overflow ===");

    void* ptrs[100];

    // Allocate 100 blocks
    for (int i = 0; i < 100; i++) {
        ptrs[i] = dam_malloc(32);
    }

    // Free all - first 64 go to tcache, rest to central
    for (int i = 0; i < 100; i++) {
        dam_free(ptrs[i]);
    }

    DAM_LOG("[PASS] Freed 100 blocks without crash");
}

// Test 3: Different size classes
void test_tcache_size_classes(void) {
    DAM_LOG("\n=== Test 3: Multiple Size Classes ===");

    void* p32_1 = dam_malloc(32);
    void* p64_1 = dam_malloc(64);
    void* p128_1 = dam_malloc(128);

    dam_free(p32_1);
    dam_free(p64_1);
    dam_free(p128_1);

    void* p32_2 = dam_malloc(32);
    void* p64_2 = dam_malloc(64);
    void* p128_2 = dam_malloc(128);

    assert(p32_1 == p32_2);
    assert(p64_1 == p64_2);
    assert(p128_1 == p128_2);

    DAM_LOG("[PASS] All size classes reused correctly");

    dam_free(p32_2);
    dam_free(p64_2);
    dam_free(p128_2);
}

// Test 4: Multi-threaded
void* thread_worker(void* arg) {
    int thread_id = *(int*)arg;
    DAM_LOG("[Thread %d] Starting", thread_id);

    void* ptrs[50];

    // Allocate and free many times
    for (int round = 0; round < 10; round++) {
        for (int i = 0; i < 50; i++) {
            ptrs[i] = dam_malloc(32);
        }
        for (int i = 0; i < 50; i++) {
            dam_free(ptrs[i]);
        }
    }

    DAM_LOG("[Thread %d] Completed 500 alloc/free cycles", thread_id);
    return NULL;
}

void test_tcache_multithread(void) {
    DAM_LOG("\n=== Test 4: Multi-threaded ===");

    pthread_t threads[4];
    int thread_ids[4] = {1, 2, 3, 4};

    for (int i = 0; i < 4; i++) {
        pthread_create(&threads[i], NULL, thread_worker, &thread_ids[i]);
    }

    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }

    DAM_LOG("[PASS] All threads completed without deadlock");
}

int main(void) {
    DAM_LOG("Starting TCache Tests\n");

    test_tcache_reuse();
    test_tcache_overflow();
    test_tcache_size_classes();
    test_tcache_multithread();

    DAM_LOG("\n=== All Tests Passed ===");
    return 0;
}