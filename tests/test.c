#include "dam/dam.h"

int main() {
    void* ptr1 = dam_malloc(300);
    void* ptr2 = dam_malloc(300);
    void* ptr3 = dam_malloc(300);

    dam_realloc(ptr1, 1000);
    ptr2 = dam_realloc(ptr2, 1000);

    dam_free(ptr1);
    dam_free(ptr2);
    dam_free(ptr3);
    return 0;
}