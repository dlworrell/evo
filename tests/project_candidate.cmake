add_executable(evo_project_candidate_test tests/project_candidate_test.c)
target_include_directories(
    evo_project_candidate_test
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(
    evo_project_candidate_test
    PRIVATE
        catalyst_evo_project_foundation
)
add_test(
    NAME evo_project_candidate_test
    COMMAND evo_project_candidate_test
)
