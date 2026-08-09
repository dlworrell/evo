cmake_minimum_required(VERSION 3.20)
project(evo_reference_adapters VERSION 1.0.0 LANGUAGES C)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(CATALYST_EVO REQUIRED IMPORTED_TARGET catalyst-evo)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Werror
    )
endif()

function(add_evo_reference_adapter target source)
    add_executable(${target} ${source})
    target_link_libraries(
        ${target}
        PRIVATE
            PkgConfig::CATALYST_EVO
            Threads::Threads
    )
endfunction()

add_evo_reference_adapter(
    evo_repository_scoring_adapter
    repository-scoring/repository_scoring.c
)
add_evo_reference_adapter(
    evo_compiler_options_adapter
    compiler/compiler_options.c
)
add_evo_reference_adapter(
    evo_scheduler_tuning_adapter
    scheduler/scheduler_tuning.c
)
add_evo_reference_adapter(
    evo_fpga_placement_adapter
    fpga-placement/fpga_placement.c
)
