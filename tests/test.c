#include "dam/dam.h"

int main() {
    void* ptr = dam_malloc(100);
    dam_realloc(ptr, 1000);
    dam_free(ptr);
    return 0;
}