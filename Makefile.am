ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/src \
	-I$(top_srcdir)/tests
AM_CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -Wconversion \
	-Wsign-conversion -Wshadow
if WARNINGS_AS_ERRORS
AM_CFLAGS += -Werror
endif
if SANITIZERS
AM_CFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer
AM_LDFLAGS = -fsanitize=address,undefined
endif

lib_LTLIBRARIES = libcatalyst_evo.la
libcatalyst_evo_la_SOURCES = \
	src/population.c \
	src/population_storage.c \
	src/population_initialization.c \
	src/population_evaluation.c \
	src/selection.c \
	src/crossover.c \
	src/mutation.c \
	src/adaptive_mutation.c \
	src/secure_erasure.c \
	src/parent_pair.c \
	src/child_pair.c \
	src/child_single.c \
	src/child_tail.c \
	src/elite.c \
	src/child_evaluation.c \
	src/generation_advancement.c \
	src/bounded_run.c \
	src/diversity.c \
	src/fitness.c \
	src/statistics.c \
	src/observer.c \
	src/stopping.c \
	src/checkpoint.c \
	src/rng.c
libcatalyst_evo_la_LDFLAGS = -version-info 0:0:0 -no-undefined
libcatalyst_evo_la_LIBADD = -lm

catalystevoincludedir = $(includedir)/catalyst/evo
catalystevoinclude_HEADERS = include/catalyst/evo/evo.h
noinst_HEADERS = \
	src/internal/population_storage.h \
	src/internal/rng.h \
	src/internal/selection.h \
	src/internal/crossover.h \
	src/internal/mutation.h \
	src/internal/adaptive_mutation.h \
	src/internal/secure_erasure.h \
	src/internal/parent_pair.h \
	src/internal/child_pair.h \
	src/internal/child_single.h \
	src/internal/child_tail.h \
	src/internal/elite.h \
	src/internal/population_evaluation.h \
	src/internal/fitness.h \
	src/internal/child_evaluation.h \
	src/internal/generation_advancement.h \
	src/internal/bounded_run.h \
	src/internal/diversity.h \
	src/internal/statistics.h \
	src/internal/observer.h \
	src/internal/stopping.h \
	tests/research/seed_schedule_research.h

noinst_LTLIBRARIES = libevo_seed_schedule_research.la
libevo_seed_schedule_research_la_SOURCES = \
	tests/research/seed_schedule_research.c
libevo_seed_schedule_research_la_LIBADD = libcatalyst_evo.la

check_PROGRAMS = \
	tests/evo_smoke_test \
	tests/evo_lifecycle_test \
	tests/evo_population_storage_test \
	tests/evo_rng_test \
	tests/evo_population_initialization_test \
	tests/evo_population_evaluation_test \
	tests/evo_fitness_test \
	tests/evo_selection_test \
	tests/evo_crossover_test \
	tests/evo_mutation_test \
	tests/evo_adaptive_mutation_test \
	tests/evo_child_population_test \
	tests/evo_parent_pair_test \
	tests/evo_child_pair_test \
	tests/evo_child_tail_test \
	tests/evo_elite_test \
	tests/evo_child_evaluation_test \
	tests/evo_generation_advancement_test \
	tests/evo_bounded_run_test \
	tests/evo_statistics_test \
	tests/evo_diversity_test \
	tests/evo_observer_test \
	tests/evo_application_stop_test \
	tests/evo_stopping_test \
	tests/evo_seed_schedule_research_test \
	tests/evo_seed_schedule_experiment \
	tests/evo_secure_erasure_test \
	tests/evo_allocation_failure_test
TESTS = $(check_PROGRAMS)

tests_evo_smoke_test_SOURCES = tests/smoke_test.c
tests_evo_smoke_test_LDADD = libcatalyst_evo.la -lm

tests_evo_lifecycle_test_SOURCES = tests/evo_lifecycle_test.c
tests_evo_lifecycle_test_LDADD = libcatalyst_evo.la -lm

tests_evo_population_storage_test_SOURCES = \
	tests/population_storage_test.c
tests_evo_population_storage_test_LDADD = libcatalyst_evo.la -lm

tests_evo_rng_test_SOURCES = tests/rng_test.c
tests_evo_rng_test_LDADD = libcatalyst_evo.la -lm

tests_evo_population_initialization_test_SOURCES = \
	tests/population_initialization_test.c
tests_evo_population_initialization_test_LDADD = libcatalyst_evo.la -lm

tests_evo_population_evaluation_test_SOURCES = \
	tests/population_evaluation_test.c
tests_evo_population_evaluation_test_LDADD = libcatalyst_evo.la -lm

tests_evo_fitness_test_SOURCES = tests/fitness_test.c
tests_evo_fitness_test_LDADD = libcatalyst_evo.la -lm

tests_evo_selection_test_SOURCES = tests/selection_test.c
tests_evo_selection_test_LDADD = libcatalyst_evo.la -lm

tests_evo_crossover_test_SOURCES = tests/crossover_test.c
tests_evo_crossover_test_LDADD = libcatalyst_evo.la -lm

tests_evo_mutation_test_SOURCES = tests/mutation_test.c
tests_evo_mutation_test_LDADD = libcatalyst_evo.la -lm

tests_evo_adaptive_mutation_test_SOURCES = \
	tests/adaptive_mutation_test.c
tests_evo_adaptive_mutation_test_LDADD = libcatalyst_evo.la -lm

tests_evo_child_population_test_SOURCES = \
	tests/child_population_test.c
tests_evo_child_population_test_LDADD = libcatalyst_evo.la -lm

tests_evo_parent_pair_test_SOURCES = tests/parent_pair_test.c
tests_evo_parent_pair_test_LDADD = libcatalyst_evo.la -lm

tests_evo_child_pair_test_SOURCES = tests/child_pair_test.c
tests_evo_child_pair_test_LDADD = libcatalyst_evo.la -lm

tests_evo_child_tail_test_SOURCES = tests/child_tail_test.c
tests_evo_child_tail_test_LDADD = libcatalyst_evo.la -lm

tests_evo_elite_test_SOURCES = tests/elite_test.c
tests_evo_elite_test_LDADD = libcatalyst_evo.la -lm

tests_evo_child_evaluation_test_SOURCES = \
	tests/child_evaluation_test.c
tests_evo_child_evaluation_test_LDADD = libcatalyst_evo.la -lm

tests_evo_generation_advancement_test_SOURCES = \
	tests/generation_advancement_test.c
tests_evo_generation_advancement_test_LDADD = libcatalyst_evo.la -lm

tests_evo_bounded_run_test_SOURCES = tests/bounded_run_test.c
tests_evo_bounded_run_test_LDADD = libcatalyst_evo.la -lm

tests_evo_statistics_test_SOURCES = tests/statistics_test.c
tests_evo_statistics_test_LDADD = libcatalyst_evo.la -lm

tests_evo_diversity_test_SOURCES = tests/diversity_test.c
tests_evo_diversity_test_LDADD = libcatalyst_evo.la -lm

tests_evo_observer_test_SOURCES = tests/observer_test.c
tests_evo_observer_test_LDADD = libcatalyst_evo.la -lm

tests_evo_application_stop_test_SOURCES = tests/application_stop_test.c
tests_evo_application_stop_test_LDADD = libcatalyst_evo.la -lm

tests_evo_stopping_test_SOURCES = tests/stopping_test.c
tests_evo_stopping_test_LDADD = libcatalyst_evo.la -lm

tests_evo_seed_schedule_research_test_SOURCES = \
	tests/seed_schedule_research_test.c
tests_evo_seed_schedule_research_test_LDADD = \
	libevo_seed_schedule_research.la libcatalyst_evo.la -lm

tests_evo_seed_schedule_experiment_SOURCES = \
	tests/seed_schedule_experiment.c
tests_evo_seed_schedule_experiment_LDADD = \
	libevo_seed_schedule_research.la libcatalyst_evo.la -lm

tests_evo_secure_erasure_test_SOURCES = \
	tests/secure_erasure_test.c
tests_evo_secure_erasure_test_LDADD = libcatalyst_evo.la -lm
tests_evo_secure_erasure_test_LDFLAGS = \
	-Wl,--wrap=calloc -Wl,--wrap=free \
	-Wl,--wrap=evo_secure_erase

tests_evo_allocation_failure_test_SOURCES = \
	tests/allocation_failure_test.c
tests_evo_allocation_failure_test_LDADD = libcatalyst_evo.la -lm
tests_evo_allocation_failure_test_LDFLAGS = \
	-Wl,--wrap=calloc -Wl,--wrap=free

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = catalyst-evo.pc

EXTRA_DIST = \
	.aems/aes-bld-001.json \
	.aems/aes-bld-001-waivers.json \
	.clang-tidy \
	.github/workflows/aes-bld-001.yml \
	CMakeLists.txt \
	CMakePresets.json \
	catalyst-evo.pc.in \
	consumer/consumer.c \
	docs/engineering/AES-BLD-001-toolchain-profile.md \
	build-aux/.gitkeep \
	m4/.gitkeep
