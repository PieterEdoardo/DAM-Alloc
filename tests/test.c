#include <stdlib.h>

#include "dam/dam.h"

int main() {
    void* ptr1 = dam_malloc(24);
    void* ptr2 = dam_malloc(24);
    dam_free(ptr1);
    void* ptr3 = dam_malloc(24);

    dam_free(ptr1);
    dam_free(ptr2);
    dam_free(ptr3);
    return 0;
}