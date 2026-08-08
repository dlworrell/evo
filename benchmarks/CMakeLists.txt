set(
    EVO_BENCHMARK_COMMIT
    "local-tree"
    CACHE STRING
    "Immutable EVO commit recorded in generated benchmark evidence"
)

find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
set(
    EVO_BENCHMARK_OUTPUT_DIR
    "${CMAKE_BINARY_DIR}/benchmark-results"
    CACHE PATH
    "Directory for generated EVO benchmark evidence"
)

add_executable(evo_core_benchmark core_benchmark.c)
target_include_directories(
    evo_core_benchmark
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(evo_core_benchmark PRIVATE catalyst_evo)
target_compile_definitions(
    evo_core_benchmark
    PRIVATE
        EVO_BENCHMARK_COMMIT="${EVO_BENCHMARK_COMMIT}"
        EVO_BENCHMARK_LINKER_ID="${CMAKE_LINKER}"
        EVO_BENCHMARK_BUILD_FRONTEND="cmake"
        EVO_BENCHMARK_BUILD_PROFILE="${CMAKE_BUILD_TYPE}"
)

add_test(
    NAME evo_core_benchmark_smoke
    COMMAND
        evo_core_benchmark
        --tier smoke
        --commit 0000000000000000000000000000000000000000
        --linker ${CMAKE_LINKER}
)

add_custom_target(
    benchmark-smoke
    COMMAND
        ${CMAKE_COMMAND} -E make_directory
        "${EVO_BENCHMARK_OUTPUT_DIR}"
    COMMAND
        ${Python3_EXECUTABLE}
        "${CMAKE_CURRENT_SOURCE_DIR}/validate_core_benchmark.py"
        --program $<TARGET_FILE:evo_core_benchmark>
        --tier smoke
        --commit "${EVO_BENCHMARK_COMMIT}"
        --linker "${CMAKE_LINKER}"
        --json "${EVO_BENCHMARK_OUTPUT_DIR}/EVO-CORE-001-smoke.json"
        --markdown "${EVO_BENCHMARK_OUTPUT_DIR}/EVO-CORE-001-smoke.md"
    DEPENDS evo_core_benchmark
    VERBATIM
)

add_custom_target(
    benchmark-extended
    COMMAND
        ${CMAKE_COMMAND} -E make_directory
        "${EVO_BENCHMARK_OUTPUT_DIR}"
    COMMAND
        ${Python3_EXECUTABLE}
        "${CMAKE_CURRENT_SOURCE_DIR}/validate_core_benchmark.py"
        --program $<TARGET_FILE:evo_core_benchmark>
        --tier extended
        --commit "${EVO_BENCHMARK_COMMIT}"
        --linker "${CMAKE_LINKER}"
        --json "${EVO_BENCHMARK_OUTPUT_DIR}/EVO-CORE-001-extended.json"
        --markdown "${EVO_BENCHMARK_OUTPUT_DIR}/EVO-CORE-001-extended.md"
    DEPENDS evo_core_benchmark
    VERBATIM
)
