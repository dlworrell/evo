cmake_minimum_required(VERSION 3.16)

project(catalyst_evo VERSION 0.40.0 LANGUAGES C)

include(GNUInstallDirs)
include(CheckSymbolExists)
find_package(Threads REQUIRED)
find_package(Python3 3.8 COMPONENTS Interpreter REQUIRED)

option(EVO_BUILD_EXAMPLES "Build catalyst_evo examples" ON)
option(EVO_BUILD_BENCHMARKS "Build catalyst_evo benchmarks" ON)
option(EVO_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(EVO_ENABLE_COVERAGE "Enable GCC/Clang coverage instrumentation" OFF)
option(EVO_ENABLE_ABI_COMPLIANCE "Enable ABI compliance dump target" OFF)
option(EVO_ENABLE_WERROR "Treat compiler warnings as errors" ON)
option(EVO_BUILD_RESEARCH "Build non-production research helpers" ON)

set(EVO_WARNING_FLAGS
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
)
if(EVO_ENABLE_WERROR)
    list(APPEND EVO_WARNING_FLAGS -Werror)
endif()

if(EVO_ENABLE_SANITIZERS)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    else()
        message(FATAL_ERROR "EVO_ENABLE_SANITIZERS requires Clang or GCC")
    endif()
endif()

if(EVO_ENABLE_COVERAGE)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        add_compile_options(--coverage -O0 -g)
        add_link_options(--coverage)
    else()
        message(FATAL_ERROR "EVO_ENABLE_COVERAGE requires Clang or GCC")
    endif()
endif()

check_symbol_exists(explicit_bzero "string.h" EVO_HAVE_EXPLICIT_BZERO)

set(EVO_SOURCES
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

add_library(catalyst_evo ${EVO_SOURCES})
add_library(Catalyst::evo ALIAS catalyst_evo)
set_target_properties(catalyst_evo PROPERTIES
    C_STANDARD 17
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)

target_compile_options(catalyst_evo PRIVATE ${EVO_WARNING_FLAGS})

target_include_directories(catalyst_evo
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_features(catalyst_evo PUBLIC c_std_17)

add_library(catalyst_evo_project_foundation STATIC
    src/project_runtime.c
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
)
target_include_directories(catalyst_evo_project_foundation
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_features(catalyst_evo_project_foundation PRIVATE c_std_17)
target_compile_options(catalyst_evo_project_foundation PRIVATE ${EVO_WARNING_FLAGS})

if(EVO_HAVE_EXPLICIT_BZERO)
    target_compile_definitions(
        catalyst_evo
        PRIVATE EVO_HAVE_EXPLICIT_BZERO=1
    )
endif()

target_link_libraries(catalyst_evo PUBLIC Threads::Threads)
if(UNIX)
    target_link_libraries(catalyst_evo PUBLIC m)
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(EVO_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(EVO_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(EVO_ENABLE_ABI_COMPLIANCE)
    find_program(ABI_DUMPER_EXECUTABLE abi-dumper)
    if(NOT ABI_DUMPER_EXECUTABLE)
        message(FATAL_ERROR "EVO_ENABLE_ABI_COMPLIANCE requires abi-dumper")
    endif()
    add_custom_target(abi-dump
        COMMAND ${ABI_DUMPER_EXECUTABLE}
                $<TARGET_FILE:catalyst_evo>
                -o ${CMAKE_BINARY_DIR}/catalyst_evo.abi.dump
                -lver ${PROJECT_VERSION}
        DEPENDS catalyst_evo
        VERBATIM
    )
endif()

install(TARGETS catalyst_evo
    EXPORT CatalystEvoTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT CatalystEvoTargets
    FILE CatalystEvoTargets.cmake
    NAMESPACE Catalyst::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CatalystEvo
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    cmake/CatalystEvoConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/CatalystEvoConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CatalystEvo
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/CatalystEvoConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/CatalystEvoConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/CatalystEvoConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CatalystEvo
)

configure_file(
    catalyst-evo.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/catalyst-evo.pc
    @ONLY
)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/catalyst-evo.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
