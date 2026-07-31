cmake_minimum_required(VERSION 3.20)
project(catalyst_evo VERSION 0.11.0 LANGUAGES C)

include(CTest)
include(GNUInstallDirs)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(EVO_WARNINGS_AS_ERRORS "Treat project warnings as errors" ON)
option(EVO_ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan" OFF)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wshadow
    )
    if(EVO_WARNINGS_AS_ERRORS)
        add_compile_options(-Werror)
    endif()
    if(EVO_ENABLE_SANITIZERS)
        add_compile_options(
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

add_library(catalyst_evo STATIC
    src/population.c
    src/population_storage.c
    src/population_initialization.c
    src/population_evaluation.c
    src/selection.c
    src/crossover.c
    src/mutation.c
    src/parent_pair.c
    src/diversity.c
    src/fitness.c
    src/statistics.c
    src/checkpoint.c
    src/rng.c
)
add_library(catalyst_evo::catalyst_evo ALIAS catalyst_evo)

target_include_directories(catalyst_evo
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_features(catalyst_evo PUBLIC c_std_17)

if(NOT MSVC)
    target_link_libraries(catalyst_evo PUBLIC m)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

set(prefix "${CMAKE_INSTALL_PREFIX}")
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
set(includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
set(PACKAGE_VERSION "${PROJECT_VERSION}")
configure_file(
    catalyst-evo.pc.in
    "${CMAKE_CURRENT_BINARY_DIR}/catalyst-evo.pc"
    @ONLY
)

install(
    TARGETS catalyst_evo
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)
install(
    FILES include/catalyst/evo/evo.h
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/catalyst/evo"
)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/catalyst-evo.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)
