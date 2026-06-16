#include "dam/dam_ptr.hpp"
#include <cstdio>

int main() {
    // single object
    auto p = dam::make_unique<int>(42);
    printf("single: %d\n", *p);

    // array
    auto arr = dam::make_unique<int[]>(4);
    arr[0] = 10;
    arr[1] = 20;
    arr[2] = 30;
    arr[3] = 40;
    printf("array: %d %d %d %d\n", arr[0], arr[1], arr[2], arr[3]);

    // resize
    arr.resize(8);
    arr[7] = 99;
    printf("after resize: %d\n", arr[7]);

    // move
    auto arr2 = std::move(arr);
    printf("moved: %d\n", arr2[0]);
    printf("original null: %d\n", !arr);

    return 0;
}