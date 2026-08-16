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
	src/population_recycling.c \
	src/population_initialization.c \
	src/population_evaluation.c \
	src/evaluation_workers.c \
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
libcatalyst_evo_la_LIBADD = -lm $(EVO_THREAD_LIBS)

catalystevoincludedir = $(includedir)/catalyst/evo
catalystevoinclude_HEADERS = include/catalyst/evo/evo.h
noinst_HEADERS = \
	src/internal/population_storage.h \
	src/internal/population_recycling.h \
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
	src/internal/evaluation_workers.h \
	src/internal/fitness.h \
	src/internal/child_evaluation.h \
	src/internal/generation_advancement.h \
	src/internal/bounded_run.h \
	src/internal/diversity.h \
	src/internal/statistics.h \
	src/internal/observer.h \
	src/internal/stopping.h \
	src/internal/run_state.h \
	src/internal/checkpoint.h \
	src/internal/result_storage.h \
	src/internal/project_runtime.h \
	src/internal/project_json.h \
	src/internal/project_fingerprint.h \
	src/internal/project_manifest.h \
	src/internal/project_compilation_database.h \
	src/internal/project_ingestion.h \
	src/internal/project_baseline_owner.h \
	src/internal/project_snapshot.h \
	src/internal/project_evidence.h \
	src/internal/project_analysis.h \
	src/internal/project_analysis_owner.h \
	src/internal/project_analysis_model.h \
	src/internal/project_analysis_evidence.h \
	src/internal/project_recipe.h \
	src/internal/project_recipe_owner.h \
	src/internal/project_recipe_model.h \
	src/internal/project_recipe_encoding.h \
	src/internal/project_transformation.h \
	src/internal/project_transformation_owner.h \
	src/internal/project_transformation_catalogue.h \
	src/internal/project_transformation_model.h \
	src/internal/project_transformation_evidence.h \
	src/internal/project_candidate.h \
	src/internal/project_candidate_owner.h \
	src/internal/project_candidate_internal.h \
	src/internal/project_assurance.h \
	src/internal/project_assurance_owner.h \
	src/internal/project_assurance_internal.h \
	src/internal/project_measurement.h \
	src/internal/project_measurement_owner.h \
	src/internal/project_measurement_internal.h \
	src/internal/project_search.h \
	src/internal/project_search_owner.h \
	src/internal/project_search_internal.h \
	src/internal/project_orchestration.h \
	src/internal/project_orchestration_owner.h \
	src/internal/project_orchestration_internal.h \
	src/internal/project_orchestration_checkpoint.h \
	tests/research/seed_schedule_research.h

noinst_LTLIBRARIES = \
	libevo_seed_schedule_research.la \
	libevo_project_foundation.la
libevo_seed_schedule_research_la_SOURCES = \
	tests/research/seed_schedule_research.c
libevo_seed_schedule_research_la_LIBADD = libcatalyst_evo.la

libevo_project_foundation_la_SOURCES = \
	src/project_runtime.c \
	src/project_json.c \
	src/project_fingerprint.c \
	src/project_manifest.c \
	src/project_compilation_database.c \
	src/project_snapshot.c \
	src/project_evidence.c \
	src/project_ingestion.c \
	src/project_analysis_model.c \
	src/project_analysis_evidence.c \
	src/project_analysis.c \
	src/project_recipe_model.c \
	src/project_recipe_encoding.c \
	src/project_recipe.c \
	src/project_transformation_catalogue.c \
	src/project_transformation_model.c \
	src/project_transformation_evidence.c \
	src/project_transformation.c \
	src/project_candidate_runtime.c \
	src/project_candidate_model.c \
	src/project_candidate.c \
	src/project_assurance_model.c \
	src/project_assurance_runtime.c \
	src/project_assurance.c \
	src/project_measurement_model.c \
	src/project_measurement_runtime.c \
	src/project_measurement.c \
	src/project_search_model.c \
	src/project_search_runtime.c \
	src/project_search.c \
	src/project_orchestration_model.c \
	src/project_orchestration_runtime.c \
	src/project_orchestration.c \
	src/project_orchestration_checkpoint.c
libevo_project_foundation_la_LIBADD = libcatalyst_evo.la -lm

check_PROGRAMS = \
	tests/evo_smoke_test \
	tests/evo_lifecycle_test \
	tests/evo_population_storage_test \
	tests/evo_population_recycling_test \
	tests/evo_rng_test \
	tests/evo_population_initialization_test \
	tests/evo_population_evaluation_test \
	tests/evo_evaluation_workers_test \
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
	tests/evo_checkpoint_test \
	tests/evo_checkpoint_fuzz_test \
	tests/evo_seed_schedule_research_test \
	tests/evo_seed_schedule_experiment \
	tests/evo_project_ingestion_test \
	tests/evo_project_analysis_test \
	tests/evo_project_recipe_test \
	tests/evo_project_transformation_test \
	tests/evo_project_candidate_test \
	tests/evo_project_assurance_test \
	tests/evo_project_measurement_test \
	tests/evo_project_search_test \
	tests/evo_project_orchestration_test \
	tests/evo_project_orchestration_checkpoint_test \
	benchmarks/evo_core_benchmark \
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

tests_evo_population_recycling_test_SOURCES = \
	tests/population_recycling_test.c
tests_evo_population_recycling_test_LDADD = libcatalyst_evo.la -lm

tests_evo_rng_test_SOURCES = tests/rng_test.c
tests_evo_rng_test_LDADD = libcatalyst_evo.la -lm

tests_evo_population_initialization_test_SOURCES = \
	tests/population_initialization_test.c
tests_evo_population_initialization_test_LDADD = libcatalyst_evo.la -lm

tests_evo_population_evaluation_test_SOURCES = \
	tests/population_evaluation_test.c
tests_evo_population_evaluation_test_LDADD = libcatalyst_evo.la -lm

tests_evo_evaluation_workers_test_SOURCES = \
	tests/evaluation_workers_test.c
tests_evo_evaluation_workers_test_LDADD = libcatalyst_evo.la -lm

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

tests_evo_checkpoint_test_SOURCES = tests/checkpoint_test.c
tests_evo_checkpoint_test_LDADD = libcatalyst_evo.la -lm

tests_evo_checkpoint_fuzz_test_SOURCES = tests/checkpoint_fuzz_test.c
tests_evo_checkpoint_fuzz_test_LDADD = libcatalyst_evo.la -lm

tests_evo_seed_schedule_research_test_SOURCES = \
	tests/seed_schedule_research_test.c
tests_evo_seed_schedule_research_test_LDADD = \
	libevo_seed_schedule_research.la libcatalyst_evo.la -lm

tests_evo_seed_schedule_experiment_SOURCES = \
	tests/seed_schedule_experiment.c
tests_evo_seed_schedule_experiment_LDADD = \
	libevo_seed_schedule_research.la libcatalyst_evo.la -lm

tests_evo_project_ingestion_test_SOURCES = \
	tests/project_ingestion_test.c
tests_evo_project_ingestion_test_CPPFLAGS = \
	$(AM_CPPFLAGS) -DEVO_TEST_SOURCE_DIR=\"$(abs_srcdir)\"
tests_evo_project_ingestion_test_LDADD = \
	libevo_project_foundation.la

tests_evo_project_analysis_test_SOURCES = \
	tests/project_analysis_test.c
tests_evo_project_analysis_test_CPPFLAGS = \
	$(AM_CPPFLAGS) -DEVO_TEST_SOURCE_DIR=\"$(abs_srcdir)\"
tests_evo_project_analysis_test_LDADD = \
	libevo_project_foundation.la

tests_evo_project_recipe_test_SOURCES = \
	tests/project_recipe_test.c
tests_evo_project_recipe_test_CPPFLAGS = \
	$(AM_CPPFLAGS) -DEVO_TEST_SOURCE_DIR=\"$(abs_srcdir)\"
tests_evo_project_recipe_test_LDADD = \
	libevo_project_foundation.la

tests_evo_project_transformation_test_SOURCES = \
	tests/project_transformation_test.c
tests_evo_project_transformation_test_CPPFLAGS = \
	$(AM_CPPFLAGS) -DEVO_TEST_SOURCE_DIR=\"$(abs_srcdir)\"
tests_evo_project_transformation_test_LDADD = \
	libevo_project_foundation.la

tests_evo_project_candidate_test_SOURCES = \
	tests/project_candidate_test.c
tests_evo_project_candidate_test_LDADD = \
	libevo_project_foundation.la

tests_evo_project_assurance_test_SOURCES = \
	tests/project_assurance_test.c
tests_evo_project_assurance_test_LDADD = \
	libevo_project_foundation.la

tests_evo_project_measurement_test_SOURCES = \
	tests/project_measurement_test.c
tests_evo_project_measurement_test_LDADD = \
	libevo_project_foundation.la

tests_evo_project_search_test_SOURCES = \
	tests/project_search_test.c
tests_evo_project_search_test_LDADD = \
	libevo_project_foundation.la

tests_evo_project_orchestration_test_SOURCES = \
	tests/project_orchestration_test.c
tests_evo_project_orchestration_test_LDADD = \
	libevo_project_foundation.la

tests_evo_project_orchestration_checkpoint_test_SOURCES = \
	tests/project_orchestration_checkpoint_test.c
tests_evo_project_orchestration_checkpoint_test_LDADD = \
	libevo_project_foundation.la

benchmarks_evo_core_benchmark_SOURCES = benchmarks/core_benchmark.c
benchmarks_evo_core_benchmark_LDADD = libcatalyst_evo.la -lm
benchmarks_evo_core_benchmark_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DEVO_BENCHMARK_COMMIT=\"0000000000000000000000000000000000000000\" \
	-DEVO_BENCHMARK_LINKER_ID=\"autotools-default\" \
	-DEVO_BENCHMARK_BUILD_FRONTEND=\"autotools\" \
	-DEVO_BENCHMARK_BUILD_PROFILE=\"c17\"

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

BENCHMARK_OUTPUT_DIR = $(abs_builddir)/benchmark-results
BENCHMARK_SMOKE_JSON = $(BENCHMARK_OUTPUT_DIR)/EVO-CORE-001-smoke.json
BENCHMARK_SMOKE_MARKDOWN = $(BENCHMARK_OUTPUT_DIR)/EVO-CORE-001-smoke.md
BENCHMARK_EXTENDED_JSON = \
	$(BENCHMARK_OUTPUT_DIR)/EVO-CORE-001-extended.json
BENCHMARK_EXTENDED_MARKDOWN = \
	$(BENCHMARK_OUTPUT_DIR)/EVO-CORE-001-extended.md

.PHONY: benchmark-smoke benchmark-extended
benchmark-smoke: benchmarks/evo_core_benchmark
	$(MKDIR_P) "$(BENCHMARK_OUTPUT_DIR)"
	python3 "$(abs_srcdir)/benchmarks/validate_core_benchmark.py" \
		--program "$(abs_builddir)/benchmarks/evo_core_benchmark" \
		--tier smoke \
		--commit "$${EVO_BENCHMARK_COMMIT:-local-tree}" \
		--linker "$(LD)" \
		--json "$(BENCHMARK_SMOKE_JSON)" \
		--markdown "$(BENCHMARK_SMOKE_MARKDOWN)"

benchmark-extended: benchmarks/evo_core_benchmark
	$(MKDIR_P) "$(BENCHMARK_OUTPUT_DIR)"
	python3 "$(abs_srcdir)/benchmarks/validate_core_benchmark.py" \
		--program "$(abs_builddir)/benchmarks/evo_core_benchmark" \
		--tier extended \
		--commit "$${EVO_BENCHMARK_COMMIT:-local-tree}" \
		--linker "$(LD)" \
		--json "$(BENCHMARK_EXTENDED_JSON)" \
		--markdown "$(BENCHMARK_EXTENDED_MARKDOWN)"

CLEANFILES = \
	benchmark-results/EVO-CORE-001-smoke.json \
	benchmark-results/EVO-CORE-001-smoke.md \
	benchmark-results/EVO-CORE-001-extended.json \
	benchmark-results/EVO-CORE-001-extended.md

EXTRA_DIST = \
	.aems/aes-bld-001.json \
	.aems/aes-bld-001-waivers.json \
	.clang-tidy \
	.github/workflows/aes-bld-001.yml \
	.github/workflows/reference-adapters.yml \
	.github/workflows/project-ingestion.yml \
	.github/workflows/project-analysis.yml \
	.github/workflows/project-recipe.yml \
	.github/workflows/project-transformation.yml \
	.github/workflows/project-candidate.yml \
	.github/workflows/project-assurance.yml \
	.github/workflows/project-measurement.yml \
	CMakeLists.txt \
	CMakePresets.json \
	benchmarks/CMakeLists.txt \
	benchmarks/README.md \
	benchmarks/evo-core-benchmark-v1.schema.json \
	benchmarks/validate_core_benchmark.py \
	catalyst-evo.pc.in \
	consumer/consumer.c \
	docs/adr/ADR-0034-reference-consumer-adapters.md \
	docs/adr/ADR-0035-immutable-project-ingestion-and-baselines.md \
	docs/adr/ADR-0036-clang-llvm-analysis-and-hotspot-model.md \
	docs/adr/ADR-0037-versioned-source-transformation-recipes.md \
	docs/adr/ADR-0038-ast-aware-c-transformation-catalogue.md \
	docs/adr/ADR-0039-isolated-candidate-materialization.md \
	docs/adr/ADR-0040-isolated-candidate-correctness-gates.md \
	docs/adr/ADR-0041-reproducible-candidate-performance-fitness.md \
	docs/schemas/evo-project-manifest-v1.schema.json \
	docs/schemas/evo-project-baseline-v1.schema.json \
	docs/schemas/evo-project-analysis-v1.schema.json \
	docs/schemas/evo-project-recipe-v1.schema.json \
	docs/schemas/evo-c-transformation-catalogue-v1.schema.json \
	docs/schemas/evo-c-transformation-application-v1.schema.json \
	docs/schemas/evo-project-candidate-v1.schema.json \
	docs/schemas/evo-project-assurance-v1.schema.json \
	docs/schemas/evo-project-measurement-v1.schema.json \
	examples/CMakeLists.txt \
	examples/Makefile \
	examples/README.md \
	examples/common/reference_adapter.h \
	examples/compiler/README.md \
	examples/compiler/compiler_options.c \
	examples/fpga-placement/README.md \
	examples/fpga-placement/fpga_placement.c \
	examples/reference-adapters-v1.golden.json \
	examples/reference-adapters-v1.schema.json \
	examples/repository-scoring/README.md \
	examples/repository-scoring/repository_scoring.c \
	examples/scheduler/README.md \
	examples/scheduler/scheduler_tuning.c \
	examples/validate_reference_adapters.py \
	docs/engineering/AES-BLD-001-toolchain-profile.md \
	docs/engineering/reports/EVO-HRA-006-reference-adapter-audit.md \
	docs/engineering/reports/EVO-HRA-007-project-ingestion-audit.md \
	docs/engineering/reports/EVO-HRA-008-project-analysis-audit.md \
	docs/engineering/reports/EVO-HRA-009-project-recipe-audit.md \
	docs/engineering/reports/EVO-HRA-010-c-transformation-catalogue-audit.md \
	docs/engineering/reports/EVO-HRA-011-candidate-materialization-audit.md \
	docs/engineering/reports/EVO-HRA-012-candidate-correctness-gates-audit.md \
	docs/engineering/reports/EVO-HRA-013-candidate-performance-fitness-audit.md \
	tests/project_candidate.cmake \
	tests/validate_project_ingestion.py \
	tests/validate_project_analysis.py \
	tests/validate_project_recipe.py \
	tests/validate_project_transformation.py \
	tests/validate_project_candidate.py \
	tests/validate_project_assurance.py \
	tests/validate_project_measurement.py \
	tests/fixtures/project-analysis/golden-v1.json \
	tests/fixtures/project-recipe/golden-v1.json \
	tests/fixtures/project-transformation/catalogue-golden-v1.json \
	tests/fixtures/project-transformation/application-golden-v1.json \
	tests/fixtures/project-transformation/before.c \
	tests/fixtures/project-transformation/after.c \
	tests/fixtures/project-ingestion/golden-v1.json \
	tests/fixtures/project-ingestion/cmake/manifest.json \
	tests/fixtures/project-ingestion/cmake/manifest-reordered.json \
	tests/fixtures/project-ingestion/cmake/project/CMakeLists.txt \
	tests/fixtures/project-ingestion/cmake/project/compile_commands.json \
	tests/fixtures/project-ingestion/cmake/project/bad_compile_commands.json \
	tests/fixtures/project-ingestion/cmake/project/include/value.h \
	tests/fixtures/project-ingestion/cmake/project/src/value.c \
	tests/fixtures/project-ingestion/cmake/project/tests/value_test.c \
	tests/fixtures/project-ingestion/cmake/project/tests/value_benchmark.c \
	tests/fixtures/project-ingestion/autotools/manifest.json \
	tests/fixtures/project-ingestion/autotools/project/Makefile.am \
	tests/fixtures/project-ingestion/autotools/project/configure.ac \
	tests/fixtures/project-ingestion/autotools/project/compile_commands.json \
	tests/fixtures/project-ingestion/autotools/project/include/value.h \
	tests/fixtures/project-ingestion/autotools/project/src/value.c \
	tests/fixtures/project-ingestion/autotools/project/tests/value_test.c \
	tests/fixtures/project-ingestion/autotools/project/tests/value_benchmark.c \
	tests/fuzz/checkpoint_fuzz.c \
	build-aux/.gitkeep \
	m4/.gitkeep
