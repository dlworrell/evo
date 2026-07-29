add_executable(evo_smoke_test smoke_test.c)
target_link_libraries(evo_smoke_test PRIVATE catalyst_evo)
add_test(NAME evo_smoke_test COMMAND evo_smoke_test)

add_executable(evo_lifecycle_test evo_lifecycle_test.c)
target_link_libraries(evo_lifecycle_test PRIVATE catalyst_evo)
add_test(NAME evo_lifecycle_test COMMAND evo_lifecycle_test)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux"
   AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang"
   AND NOT BUILD_SHARED_LIBS)
    add_executable(evo_allocation_failure_test allocation_failure_test.c)
    target_link_libraries(evo_allocation_failure_test PRIVATE catalyst_evo)
    target_link_options(evo_allocation_failure_test PRIVATE "-Wl,--wrap=calloc")
    add_test(NAME evo_allocation_failure_test COMMAND evo_allocation_failure_test)
endif()
