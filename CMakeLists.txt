cmake_minimum_required(VERSION 3.20)
project(catalyst_evo VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(catalyst_evo
    src/population.c
    src/selection.c
    src/crossover.c
    src/mutation.c
    src/diversity.c
    src/fitness.c
    src/statistics.c
    src/checkpoint.c
    src/rng.c
)

target_include_directories(catalyst_evo
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_options(catalyst_evo PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_subdirectory(tests)
