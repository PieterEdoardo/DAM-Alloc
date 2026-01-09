#include <stdlib.h>

#include "dam/dam.h"
#include "dam/dam_log.h"

#define SMALL_SZ     24
#define GENERAL_SZ  1000
#define DIRECT_SZ   10000000
#define ITERATIONS  100
static void fill_pattern(unsigned char* p, size_t sz, unsigned char seed) {
    for (size_t i = 0; i < sz; i++)
        p[i] = (unsigned char)(seed + i);
}

static void verify_pattern(unsigned char* p, size_t sz, unsigned char seed) {
    for (size_t i = 0; i < sz; i++) {
        if (p[i] != (unsigned char)(seed + i)) {
            DAM_LOG_ERROR(
                "[TORTURE] Data corruption at %zu (expected %u, got %u)",
                i, (unsigned)(seed + i), (unsigned)p[i]
            );
            abort();
        }
    }
}

int main(void) {
    DAM_LOG("Testing MALLOC");
    void* ptr1 = dam_malloc(24);
    void* ptr2 = dam_malloc(24);
    dam_free(ptr1);
    void* ptr3 = dam_malloc(24);


    DAM_LOG("Testing REALLOC");
    ptr2 = dam_realloc(ptr2, 1000);
    ptr2 = dam_realloc(ptr2, 10000000);
    ptr2 = dam_realloc(ptr2, 24);
    ptr3 = dam_realloc(ptr3, 1000);


    DAM_LOG("Testing FREE");
    dam_free(ptr1);
    dam_free(ptr2);
    dam_free(ptr3);
    return 0;
}