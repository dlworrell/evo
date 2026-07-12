add_executable(evo_smoke_test smoke_test.c)
target_link_libraries(evo_smoke_test PRIVATE catalyst_evo)
add_test(NAME evo_smoke_test COMMAND evo_smoke_test)
