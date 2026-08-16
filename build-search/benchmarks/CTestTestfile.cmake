# CMake generated Testfile for 
# Source directory: /home/runner/work/evo/evo/benchmarks
# Build directory: /home/runner/work/evo/evo/build-search/benchmarks
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[evo_core_benchmark_smoke]=] "/home/runner/work/evo/evo/build-search/benchmarks/evo_core_benchmark" "--tier" "smoke" "--commit" "0000000000000000000000000000000000000000" "--linker" "/usr/bin/ld")
set_tests_properties([=[evo_core_benchmark_smoke]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/runner/work/evo/evo/benchmarks/CMakeLists.txt;32;add_test;/home/runner/work/evo/evo/benchmarks/CMakeLists.txt;0;")
