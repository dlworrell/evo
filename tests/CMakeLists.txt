add_executable(evo_smoke_test smoke_test.c)
target_link_libraries(evo_smoke_test PRIVATE catalyst_evo)
add_test(NAME evo_smoke_test COMMAND evo_smoke_test)

add_executable(evo_lifecycle_test evo_lifecycle_test.c)
target_link_libraries(evo_lifecycle_test PRIVATE catalyst_evo)
add_test(NAME evo_lifecycle_test COMMAND evo_lifecycle_test)

add_executable(evo_population_storage_test population_storage_test.c)
target_include_directories(evo_population_storage_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(evo_population_storage_test PRIVATE catalyst_evo)
add_test(NAME evo_population_storage_test COMMAND evo_population_storage_test)

add_executable(evo_rng_test rng_test.c)
target_include_directories(evo_rng_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(evo_rng_test PRIVATE catalyst_evo)
add_test(NAME evo_rng_test COMMAND evo_rng_test)

add_executable(evo_population_initialization_test population_initialization_test.c)
target_include_directories(evo_population_initialization_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(evo_population_initialization_test PRIVATE catalyst_evo)
add_test(NAME evo_population_initialization_test COMMAND evo_population_initialization_test)

add_executable(evo_population_evaluation_test population_evaluation_test.c)
target_include_directories(evo_population_evaluation_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(evo_population_evaluation_test PRIVATE catalyst_evo)
add_test(NAME evo_population_evaluation_test COMMAND evo_population_evaluation_test)

add_executable(evo_selection_test selection_test.c)
target_include_directories(evo_selection_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(evo_selection_test PRIVATE catalyst_evo)
add_test(NAME evo_selection_test COMMAND evo_selection_test)

add_executable(evo_crossover_test crossover_test.c)
target_include_directories(evo_crossover_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(evo_crossover_test PRIVATE catalyst_evo)
add_test(NAME evo_crossover_test COMMAND evo_crossover_test)

add_executable(evo_mutation_test mutation_test.c)
target_include_directories(evo_mutation_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(evo_mutation_test PRIVATE catalyst_evo)
add_test(NAME evo_mutation_test COMMAND evo_mutation_test)

add_executable(evo_child_population_test child_population_test.c)
target_include_directories(evo_child_population_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(evo_child_population_test PRIVATE catalyst_evo)
add_test(NAME evo_child_population_test COMMAND evo_child_population_test)

add_executable(evo_parent_pair_test parent_pair_test.c)
target_include_directories(evo_parent_pair_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(evo_parent_pair_test PRIVATE catalyst_evo)
add_test(NAME evo_parent_pair_test COMMAND evo_parent_pair_test)

add_executable(evo_child_pair_test child_pair_test.c)
target_include_directories(evo_child_pair_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(evo_child_pair_test PRIVATE catalyst_evo)
add_test(NAME evo_child_pair_test COMMAND evo_child_pair_test)

add_executable(evo_child_tail_test child_tail_test.c)
target_include_directories(evo_child_tail_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(evo_child_tail_test PRIVATE catalyst_evo)
add_test(NAME evo_child_tail_test COMMAND evo_child_tail_test)

add_executable(evo_child_evaluation_test child_evaluation_test.c)
target_include_directories(evo_child_evaluation_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(evo_child_evaluation_test PRIVATE catalyst_evo)
add_test(NAME evo_child_evaluation_test COMMAND evo_child_evaluation_test)

add_executable(evo_generation_advancement_test generation_advancement_test.c)
target_include_directories(evo_generation_advancement_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(evo_generation_advancement_test PRIVATE catalyst_evo)
add_test(NAME evo_generation_advancement_test COMMAND evo_generation_advancement_test)

add_executable(evo_bounded_run_test bounded_run_test.c)
target_include_directories(evo_bounded_run_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(evo_bounded_run_test PRIVATE catalyst_evo)
add_test(NAME evo_bounded_run_test COMMAND evo_bounded_run_test)

add_executable(evo_statistics_test statistics_test.c)
target_include_directories(evo_statistics_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(evo_statistics_test PRIVATE catalyst_evo)
add_test(NAME evo_statistics_test COMMAND evo_statistics_test)

add_executable(evo_observer_test observer_test.c)
target_include_directories(evo_observer_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(evo_observer_test PRIVATE catalyst_evo)
add_test(NAME evo_observer_test COMMAND evo_observer_test)

add_executable(evo_application_stop_test application_stop_test.c)
target_link_libraries(evo_application_stop_test PRIVATE catalyst_evo)
add_test(NAME evo_application_stop_test COMMAND evo_application_stop_test)

add_library(evo_seed_schedule_research STATIC
    research/seed_schedule_research.c
)
target_include_directories(evo_seed_schedule_research
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(evo_seed_schedule_research PRIVATE catalyst_evo)

add_executable(evo_seed_schedule_research_test seed_schedule_research_test.c)
target_include_directories(evo_seed_schedule_research_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(evo_seed_schedule_research_test
    PRIVATE
        evo_seed_schedule_research
        catalyst_evo
)
add_test(NAME evo_seed_schedule_research_test COMMAND evo_seed_schedule_research_test)

add_executable(evo_seed_schedule_experiment seed_schedule_experiment.c)
target_link_libraries(evo_seed_schedule_experiment
    PRIVATE
        evo_seed_schedule_research
)
if(NOT MSVC)
    target_link_libraries(evo_seed_schedule_experiment PRIVATE m)
endif()
add_test(NAME evo_seed_schedule_experiment COMMAND evo_seed_schedule_experiment)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux"
   AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang"
   AND NOT BUILD_SHARED_LIBS)
    add_executable(evo_allocation_failure_test allocation_failure_test.c)
    target_include_directories(evo_allocation_failure_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(evo_allocation_failure_test PRIVATE catalyst_evo)
    target_link_options(
        evo_allocation_failure_test
        PRIVATE
            "-Wl,--wrap=calloc"
            "-Wl,--wrap=free"
    )
    add_test(NAME evo_allocation_failure_test COMMAND evo_allocation_failure_test)
endif()
