#include "dam/dam_ptr.hpp"
#include <cstdio>

int main() {
    // basic construction and dereference
    auto p = dam::make_unique<int>(42);
    printf("value: %d\n", *p);

    // move
    auto p2 = std::move(p);
    printf("moved value: %d\n", *p2);
    printf("original is null: %d\n", !p);

    // reset
    p2.reset();
    printf("after reset is null: %d\n", !p2);

    return 0;
}