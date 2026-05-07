
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "dam/dam.h"
#include "dam/dam_log.h"

#define SLOTS 4096
#define ITERS 5000000
#define MAX_ALLOC 8192

typedef struct {
    void *ptr;
    size_t size;
    uint32_t magic;
} slot_t;

static slot_t slots[SLOTS];

static uint32_t rand32(void) {
    return ((uint32_t)rand() << 16) ^ rand();
}

int main(void) {
    srand((unsigned)time(NULL));

    for (size_t i = 0; i < ITERS; i++) {
        size_t idx = rand32() % SLOTS;

        if (slots[idx].ptr && (rand32() % 100) < 55) {
            /* verify pattern before free */
            uint32_t *p = slots[idx].ptr;

            for (size_t j = 0; j < slots[idx].size / sizeof(uint32_t); j++) {
                if (p[j] != slots[idx].magic) {
                    fprintf(stderr,
                        "CORRUPTION DETECTED idx=%zu iter=%zu\n",
                        idx, i);
                    abort();
                }
            }

            dam_free(slots[idx].ptr);
            slots[idx].ptr = NULL;
        } else {
            size_t size;

            /* heavily bias toward small allocations */
            switch (rand32() % 5) {
                case 0: size = rand32() % 32 + 1; break;
                case 1: size = rand32() % 128 + 1; break;
                case 2: size = rand32() % 512 + 1; break;
                case 3: size = rand32() % 2048 + 1; break;
                default: size = rand32() % MAX_ALLOC + 1;
            }

            void *p = dam_malloc(size);

            if (!p) {
                fprintf(stderr, "malloc failed\n");
                abort();
            }

            uint32_t magic = rand32();

            for (size_t j = 0; j < size / sizeof(uint32_t); j++) {
                ((uint32_t *)p)[j] = magic;
            }

            slots[idx].ptr = p;
            slots[idx].size = size;
            slots[idx].magic = magic;
        }
    }

    printf("done\n");
    return 0;
}