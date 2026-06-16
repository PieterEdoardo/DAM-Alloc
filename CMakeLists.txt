cmake_minimum_required(VERSION 3.16)
project(DAM_Alloc C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(dam
        src/core/core.c
        src/core/small.c
        src/core/general.c
        src/core/direct.c
        src/util/util.c
        src/util/thread.c
)

target_include_directories(dam PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

add_executable(dam_test
        tests/test.c
)

add_executable(dam_test_cpp
        tests/test.cpp
)

target_link_libraries(dam_test dam)
target_link_libraries(dam_test_cpp dam)