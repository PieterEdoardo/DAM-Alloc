cmake_minimum_required(VERSION 3.16)
project(DAM_Alloc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(dam
        src/core/dam_core.c
        src/core/small.c
        src/core/general.c
        src/core/direct.c
        src/util/dam_util.c
)

target_include_directories(dam PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

add_executable(dam_test
        tests/test.c
)

target_link_libraries(dam_test dam)
