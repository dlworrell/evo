cmake_minimum_required(VERSION 3.20)
project(evo_ingestion_cmake_fixture LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(value STATIC src/value.c)
target_include_directories(value PUBLIC include)

add_executable(value_test tests/value_test.c)
target_link_libraries(value_test PRIVATE value)

enable_testing()
add_test(NAME value_test COMMAND value_test)

add_executable(value_benchmark tests/value_benchmark.c)
