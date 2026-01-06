#include "dam/dam.h"
#include "dam/dam_log.h"

int main() {
    DAM_LOG("Testing MALLOC");
    void* ptr1 = dam_malloc(24);
    void* ptr2 = dam_malloc(24);
    dam_free(ptr1);
    void* ptr3 = dam_malloc(24);


    DAM_LOG("Testing REALLOC");
    ptr2 = dam_realloc(ptr2, 1000);
    ptr2 = dam_realloc(ptr2, 24);

    DAM_LOG("Testing FREE");
    dam_free(ptr1);
    dam_free(ptr2);
    dam_free(ptr3);
    return 0;
}