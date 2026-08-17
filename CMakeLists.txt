cmake_minimum_required(VERSION 3.20)
project(catalyst_evo VERSION 0.42.0 LANGUAGES C)

set(EVO_PRODUCT_VERSION "${PROJECT_VERSION}")
set(EVO_PUBLIC_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/catalyst/evo/evo.h")
foreach(component MAJOR MINOR PATCH)
    file(
        STRINGS "${EVO_PUBLIC_HEADER}" _evo_version_line
        LIMIT_COUNT 1
        REGEX "^#define[ \t]+EVO_VERSION_${component}[ \t]+[0-9]+[ \t]*$"
    )
    if(NOT _evo_version_line)
        message(FATAL_ERROR
            "Unable to read EVO_VERSION_${component} from ${EVO_PUBLIC_HEADER}"
        )
    endif()
    string(
        REGEX REPLACE "^.*[ \t]+([0-9]+)[ \t]*$" "\\1"
        _evo_version_value "${_evo_version_line}"
    )
    set(EVO_CORE_VERSION_${component} "${_evo_version_value}")
endforeach()
set(EVO_CORE_VERSION
    "${EVO_CORE_VERSION_MAJOR}.${EVO_CORE_VERSION_MINOR}.${EVO_CORE_VERSION_PATCH}"
)
unset(_evo_version_line)
unset(_evo_version_value)

include(CTest)
include(CheckSymbolExists)
include(GNUInstallDirs)

find_package(Threads REQUIRED)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(EVO_WARNINGS_AS_ERRORS "Treat project warnings as errors" ON)
option(EVO_ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan" OFF)

set(CMAKE_REQUIRED_DEFINITIONS "-D_DEFAULT_SOURCE=1")
check_symbol_exists(
    explicit_bzero
    "string.h"
    EVO_HAVE_EXPLICIT_BZERO
)
unset(CMAKE_REQUIRED_DEFINITIONS)

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
    src/population_recycling.c
    src/population_initialization.c
    src/population_evaluation.c
    src/evaluation_workers.c
    src/selection.c
    src/crossover.c
    src/mutation.c
    src/adaptive_mutation.c
    src/secure_erasure.c
    src/parent_pair.c
    src/child_pair.c
    src/child_single.c
    src/child_tail.c
    src/elite.c
    src/child_evaluation.c
    src/generation_advancement.c
    src/bounded_run.c
    src/diversity.c
    src/fitness.c
    src/statistics.c
    src/observer.c
    src/stopping.c
    src/checkpoint.c
    src/rng.c
)
add_library(catalyst_evo::catalyst_evo ALIAS catalyst_evo)

# The source-optimizer foundation is deliberately separate from the installed
# stable core until its product-facing API is fixed by the later CLI milestone.
add_library(catalyst_evo_project_foundation STATIC
    src/project_runtime.c
    src/project_provider.c
    src/project_json.c
    src/project_fingerprint.c
    src/project_manifest.c
    src/project_compilation_database.c
    src/project_snapshot.c
    src/project_evidence.c
    src/project_ingestion.c
    src/project_analysis_model.c
    src/project_analysis_evidence.c
    src/project_analysis.c
    src/project_recipe_model.c
    src/project_recipe_encoding.c
    src/project_recipe.c
    src/project_transformation_catalogue.c
    src/project_transformation_model.c
    src/project_transformation_evidence.c
    src/project_transformation.c
    src/project_candidate_runtime.c
    src/project_candidate_model.c
    src/project_candidate.c
    src/project_assurance_model.c
    src/project_assurance_runtime.c
    src/project_assurance.c
    src/project_measurement_model.c
    src/project_measurement_runtime.c
    src/project_measurement.c
    src/project_search_model.c
    src/project_search_runtime.c
    src/project_search.c
    src/project_search_orchestration_trace.c
    src/project_orchestration_model.c
    src/project_orchestration_runtime.c
    src/project_orchestration.c
    src/project_orchestration_checkpoint.c
)
target_include_directories(catalyst_evo_project_foundation
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_features(catalyst_evo_project_foundation PRIVATE c_std_17)
target_link_libraries(catalyst_evo_project_foundation PUBLIC catalyst_evo)

target_include_directories(catalyst_evo
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_features(catalyst_evo PUBLIC c_std_17)

if(EVO_HAVE_EXPLICIT_BZERO)
    target_compile_definitions(
        catalyst_evo
        PRIVATE EVO_HAVE_EXPLICIT_BZERO=1
    )
endif()

target_link_libraries(catalyst_evo PUBLIC Threads::Threads)

if(NOT MSVC)
    target_link_libraries(catalyst_evo PUBLIC m)
endif()

if(BUILD_TESTING)
    add_subdirectory(benchmarks)
    add_subdirectory(tests)
endif()

set(prefix "${CMAKE_INSTALL_PREFIX}")
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
set(includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
set(PACKAGE_VERSION "${EVO_CORE_VERSION}")
set(EVO_THREAD_LIBS "${CMAKE_THREAD_LIBS_INIT}")
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