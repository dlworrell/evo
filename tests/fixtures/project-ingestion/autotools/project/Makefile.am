AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -std=c17 -Wall -Wextra -Wpedantic

noinst_LIBRARIES = libvalue.a
libvalue_a_SOURCES = src/value.c

check_PROGRAMS = tests/value_test tests/value_benchmark
TESTS = tests/value_test
tests_value_test_SOURCES = tests/value_test.c
tests_value_test_LDADD = libvalue.a
tests_value_benchmark_SOURCES = tests/value_benchmark.c
tests_value_benchmark_LDADD = libvalue.a
