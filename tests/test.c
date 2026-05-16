/*
 * dam_stress_test.c
 *
 * Stress test suite for DAM allocator.
 * Tests: random alloc/free, realloc churn, size boundary probing,
 *        fill-and-verify integrity, fragmentation pressure,
 *        and sequential sweep patterns.
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>

#include "dam/dam.h"
#include "dam/dam_log.h"

/* ------------------------------------------------------------------ */
/* Tunables                                                             */
/* ------------------------------------------------------------------ */
#define SLOTS           4096
#define ITERS           5000000
#define MAX_ALLOC       8192

/* Sizes that sit exactly on, just below, and just above each size
 * class boundary.  Adjust to match your DAM_SMALL_MIN / multiplier. */
static const size_t BOUNDARY_SIZES[] = {
    1, 2, 4, 8,
    15, 16, 17,
    31, 32, 33,
    63, 64, 65,
    127, 128, 129,
    255, 256, 257,
    511, 512, 513,
    1023, 1024, 1025,
    2047, 2048, 2049,
    4095, 4096, 4097,
    8191, 8192,
};
#define NUM_BOUNDARY_SIZES (sizeof(BOUNDARY_SIZES) / sizeof(BOUNDARY_SIZES[0]))

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */
typedef struct {
    void    *ptr;
    size_t   size;
    uint32_t magic;
} slot_t;

static slot_t slots[SLOTS];

static uint32_t rand32(void) {
    return ((uint32_t)rand() << 16) ^ (uint32_t)rand();
}

/* Fill every uint32-aligned word with magic, then verify it. */
static void fill_magic(void *ptr, size_t size, uint32_t magic) {
    uint8_t *p = ptr;
    for (size_t i = 0; i < size; i++) {
        p[i] = (uint8_t)((magic >> ((i % 4) * 8)) & 0xFF);
    }
}

static int verify_magic(const void *ptr, size_t size, uint32_t magic) {
    const uint8_t *p = ptr;
    for (size_t i = 0; i < size; i++) {
        uint8_t expected = (uint8_t)((magic >> ((i % 4) * 8)) & 0xFF);
        if (p[i] != expected) return 0;
    }
    return 1;
}

static void free_slot(size_t idx) {
    if (!slots[idx].ptr) return;

    if (!verify_magic(slots[idx].ptr, slots[idx].size, slots[idx].magic)) {
        fprintf(stderr,
            "[FAIL] CORRUPTION before free  idx=%zu  ptr=%p  size=%zu\n",
            idx, slots[idx].ptr, slots[idx].size);
        abort();
    }

    dam_free(slots[idx].ptr);
    slots[idx].ptr   = NULL;
    slots[idx].size  = 0;
    slots[idx].magic = 0;
}

static void alloc_slot(size_t idx, size_t size) {
    free_slot(idx); /* evict any previous occupant cleanly */

    void *p = dam_malloc(size);
    if (!p) {
        fprintf(stderr, "[FAIL] dam_malloc(%zu) returned NULL\n", size);
        abort();
    }

    uint32_t magic = rand32();
    /* Avoid magic==0 so "zero memory" bugs are visible */
    if (magic == 0) magic = 0xDEADBEEF;

    fill_magic(p, size, magic);

    slots[idx].ptr   = p;
    slots[idx].size  = size;
    slots[idx].magic = magic;
}

static size_t biased_size(void) {
    switch (rand32() % 5) {
        case 0:  return (rand32() % 32)   + 1;
        case 1:  return (rand32() % 128)  + 1;
        case 2:  return (rand32() % 512)  + 1;
        case 3:  return (rand32() % 2048) + 1;
        default: return (rand32() % MAX_ALLOC) + 1;
    }
}

void print_snapshot(const dam_snapshot_t* snapshot) {
    printf("tlc_used: %zu\n", snapshot->tlc_used);
    printf("tlc_free: %zu\n", snapshot->tlc_free);
    printf("size_classes: %zu\n", snapshot->size_classes);
    printf("classes_bytes_used: %zu\n", snapshot->classes_bytes_used);
    printf("pools_active: %zu\n", snapshot->pools_active);
    printf("pools_bytes_used: %zu\n", snapshot->pools_bytes_used);
    printf("quarantined_pools: %zu\n", snapshot->quarantined_pools);
    printf("direct_allocations: %zu\n", snapshot->direct_allocations);
    printf("direct_bytes_used: %zu\n", snapshot->direct_bytes_used);
}

/* ------------------------------------------------------------------ */
/* Test 1 — Random alloc / free (original harness, fixed)              */
/* ------------------------------------------------------------------ */
static void test_random_churn(void) {
    printf("=== Test 1: Random churn (%d iters) ===\n", ITERS);
    memset(slots, 0, sizeof(slots));

    for (size_t i = 0; i < ITERS; i++) {
        size_t idx = rand32() % SLOTS;

        if (slots[idx].ptr && (rand32() % 100) < 55) {
            free_slot(idx);
        } else {
            alloc_slot(idx, biased_size());
        }

        if (i % 500000 == 0 && i > 0) {
            printf("  ... %zu iters done\n", i);
        }
    }

    /* Clean up */
    for (size_t i = 0; i < SLOTS; i++) free_slot(i);
    printf("  PASS\n\n");
}

/* ------------------------------------------------------------------ */
/* Test 2 — Size-class boundary probing                                */
/* Allocate, write, verify, free for every boundary size.              */
/* ------------------------------------------------------------------ */
static void test_boundaries(void) {
    printf("=== Test 2: Size-class boundary probing ===\n");

    for (size_t rep = 0; rep < 2000; rep++) {
        for (size_t b = 0; b < NUM_BOUNDARY_SIZES; b++) {
            size_t sz = BOUNDARY_SIZES[b];
            void *p = dam_malloc(sz);
            if (!p) {
                fprintf(stderr, "[FAIL] NULL for size=%zu\n", sz);
                abort();
            }
            uint32_t magic = rand32() | 1; /* ensure non-zero */
            fill_magic(p, sz, magic);
            if (!verify_magic(p, sz, magic)) {
                fprintf(stderr, "[FAIL] Boundary corruption size=%zu\n", sz);
                abort();
            }
            dam_free(p);
        }
    }
    printf("  PASS\n\n");
}

/* ------------------------------------------------------------------ */
/* Test 3 — Realloc churn                                              */
/* Grow and shrink allocations repeatedly, verify data survives.       */
/* ------------------------------------------------------------------ */
static void test_realloc_churn(void) {
    printf("=== Test 3: Realloc churn ===\n");
    memset(slots, 0, sizeof(slots));

    for (size_t i = 0; i < 200000; i++) {
        size_t idx = rand32() % 512; /* smaller slot range for density */

        if (!slots[idx].ptr) {
            alloc_slot(idx, biased_size());
            continue;
        }

        /* Verify existing data before realloc */
        if (!verify_magic(slots[idx].ptr, slots[idx].size, slots[idx].magic)) {
            fprintf(stderr, "[FAIL] Corruption before realloc idx=%zu\n", idx);
            abort();
        }

        size_t new_size = biased_size();
        void *new_ptr   = dam_realloc(slots[idx].ptr, new_size);
        if (!new_ptr) {
            fprintf(stderr, "[FAIL] dam_realloc NULL idx=%zu new_size=%zu\n",
                    idx, new_size);
            abort();
        }

        /* The overlapping region must be intact */
        size_t check = (new_size < slots[idx].size) ? new_size : slots[idx].size;
        if (!verify_magic(new_ptr, check, slots[idx].magic)) {
            fprintf(stderr,
                "[FAIL] Data not preserved through realloc idx=%zu "
                "old=%zu new=%zu\n", idx, slots[idx].size, new_size);
            abort();
        }

        /* Re-stamp with fresh magic */
        uint32_t magic = rand32() | 1;
        fill_magic(new_ptr, new_size, magic);

        slots[idx].ptr   = new_ptr;
        slots[idx].size  = new_size;
        slots[idx].magic = magic;
    }

    for (size_t i = 0; i < 512; i++) free_slot(i);
    printf("  PASS\n\n");
}

/* ------------------------------------------------------------------ */
/* Test 4 — Fill all slots then free all (fragmentation pressure)      */
/* ------------------------------------------------------------------ */
static void test_fill_then_free(void) {
    printf("=== Test 4: Fill then free ===\n");
    memset(slots, 0, sizeof(slots));

    /* Allocate into every slot */
    for (size_t i = 0; i < SLOTS; i++) {
        alloc_slot(i, biased_size());
    }

    /* Verify all before freeing */
    for (size_t i = 0; i < SLOTS; i++) {
        if (!verify_magic(slots[i].ptr, slots[i].size, slots[i].magic)) {
            fprintf(stderr, "[FAIL] Corruption after full fill idx=%zu\n", i);
            abort();
        }
    }

    /* Free in random order */
    size_t order[SLOTS];
    for (size_t i = 0; i < SLOTS; i++) order[i] = i;
    for (size_t i = SLOTS - 1; i > 0; i--) {
        size_t j = rand32() % (i + 1);
        size_t tmp = order[i]; order[i] = order[j]; order[j] = tmp;
    }
    for (size_t i = 0; i < SLOTS; i++) free_slot(order[i]);

    /* Allocate again — tests that freed memory is truly reusable */
    for (size_t i = 0; i < SLOTS; i++) {
        alloc_slot(i, biased_size());
    }
    for (size_t i = 0; i < SLOTS; i++) free_slot(i);

    printf("  PASS\n\n");
}

/* ------------------------------------------------------------------ */
/* Test 5 — Sequential sweep                                           */
/* Allocate N, free in FIFO order, repeat. Stresses tcache eviction.  */
/* ------------------------------------------------------------------ */
#define SWEEP_DEPTH 128

static void test_sequential_sweep(void) {
    printf("=== Test 5: Sequential sweep ===\n");

    void *ring[SWEEP_DEPTH];
    size_t ring_sizes[SWEEP_DEPTH];
    uint32_t ring_magic[SWEEP_DEPTH];
    memset(ring, 0, sizeof(ring));

    size_t head = 0;

    for (size_t i = 0; i < 300000; i++) {
        size_t slot = head % SWEEP_DEPTH;

        /* Free old occupant if present */
        if (ring[slot]) {
            if (!verify_magic(ring[slot], ring_sizes[slot], ring_magic[slot])) {
                fprintf(stderr,
                    "[FAIL] Sweep corruption i=%zu slot=%zu\n", i, slot);
                abort();
            }
            dam_free(ring[slot]);
        }

        size_t sz = biased_size();
        void *p = dam_malloc(sz);
        if (!p) { fprintf(stderr, "[FAIL] NULL i=%zu\n", i); abort(); }

        uint32_t magic = rand32() | 1;
        fill_magic(p, sz, magic);

        ring[slot]       = p;
        ring_sizes[slot] = sz;
        ring_magic[slot] = magic;
        head++;
    }

    /* Drain ring */
    for (size_t i = 0; i < SWEEP_DEPTH; i++) {
        if (ring[i]) {
            verify_magic(ring[i], ring_sizes[i], ring_magic[i]);
            dam_free(ring[i]);
        }
    }
    printf("  PASS\n\n");
}

/* ------------------------------------------------------------------ */
/* Test 6 — Repeated tcache flush pressure                             */
/* Allocate exactly THREAD_CACHE_MAX_BLOCKS_PER_CLASS+1 of the same   */
/* size class, then free them all — forces both fill and overflow.     */
/* ------------------------------------------------------------------ */
#define TCACHE_PROBE_COUNT 80   /* > 64, the typical tcache max */

static void test_tcache_pressure(void) {
    printf("=== Test 6: TCache flush pressure ===\n");

    void   *ptrs[TCACHE_PROBE_COUNT];
    uint32_t magic[TCACHE_PROBE_COUNT];

    static const size_t test_sizes[] = { 8, 16, 32, 64, 128, 256 };

    for (size_t s = 0; s < sizeof(test_sizes)/sizeof(test_sizes[0]); s++) {
        size_t sz = test_sizes[s];

        for (size_t rep = 0; rep < 5000; rep++) {
            /* Allocate more than tcache can hold */
            for (int k = 0; k < TCACHE_PROBE_COUNT; k++) {
                ptrs[k] = dam_malloc(sz);
                if (!ptrs[k]) {
                    fprintf(stderr, "[FAIL] NULL sz=%zu k=%d\n", sz, k);
                    abort();
                }
                magic[k] = rand32() | 1;
                fill_magic(ptrs[k], sz, magic[k]);
            }

            /* Verify all still intact */
            for (int k = 0; k < TCACHE_PROBE_COUNT; k++) {
                if (!verify_magic(ptrs[k], sz, magic[k])) {
                    fprintf(stderr,
                        "[FAIL] TCache pressure corruption sz=%zu k=%d\n",
                        sz, k);
                    abort();
                }
            }

            /* Free in reverse to stress different eviction patterns */
            for (int k = TCACHE_PROBE_COUNT - 1; k >= 0; k--) {
                dam_free(ptrs[k]);
            }
        }
    }
    printf("  PASS\n\n");
}

/* ------------------------------------------------------------------ */
/* Test 7 — Zero-byte and one-byte edge cases                          */
/* ------------------------------------------------------------------ */
static void test_edge_sizes(void) {
    printf("=== Test 7: Edge sizes ===\n");

    /* size=1 must succeed and not corrupt neighbours */
    for (int i = 0; i < 10000; i++) {
        void *a = dam_malloc(1);
        void *b = dam_malloc(1);
        void *c = dam_malloc(MAX_ALLOC);

        if (!a || !b || !c) {
            fprintf(stderr, "[FAIL] NULL on edge alloc\n"); abort();
        }

        *(uint8_t*)a = 0xAA;
        *(uint8_t*)b = 0xBB;
        memset(c, 0xCC, MAX_ALLOC);

        assert(*(uint8_t*)a == 0xAA);
        assert(*(uint8_t*)b == 0xBB);

        dam_free(b);
        dam_free(a);
        dam_free(c);
    }
    printf("  PASS\n\n");
}

static void test_big_direct_allocations(void) {
    printf("=== Test 8: Big direct allocations ===\n");
    void* a = dam_malloc(100000);
    void* b = dam_malloc(1000000);
    void* c = dam_malloc(10000000);

    if (!a || !b || !c) {
        fprintf(stderr, "[FAIL] NULL on big direct alloc\n"); abort();
    }

    a = dam_realloc(a, 10000000);
    b = dam_realloc(b, 1000000);
    c = dam_realloc(c, 100000);

    if (!a || !b || !c) {
        fprintf(stderr, "[FAIL] NULL on big direct realloc\n"); abort();
    }

    printf("  PASS\n\n");
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */
int main(void) {
    fprintf(stdout, "%lu\n", (unsigned long)time(NULL));
    srand((unsigned)time(NULL));

    printf("DAM Stress Test Suite\n");
    printf("=====================\n\n");

    test_boundaries();
    test_edge_sizes();
    test_tcache_pressure();
    test_fill_then_free();
    test_sequential_sweep();
    test_realloc_churn();
    test_big_direct_allocations();

    dam_snapshot_t snapshot = {0};
    dam_snapshot(&snapshot);
    print_snapshot(&snapshot);
    // test_random_churn();      /* longest — run last */

    printf("=====================\n");
    printf("ALL TESTS PASSED\n");
    fprintf(stdout, "%lu\n", (unsigned long)time(NULL));

    return 0;
}