#include "dam/dam_ptr.hpp"

int main() {
    auto ptr = dam::make_unique<int>(42);

    return 0;
}