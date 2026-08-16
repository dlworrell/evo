#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#define _XOPEN_SOURCE 700

#include "internal/project_analysis_owner.h"
#include "internal/project_baseline_owner.h"
#include "internal/project_fingerprint.h"
#include "internal/project_recipe.h"
#include "internal/project_runtime.h"
#include "internal/project_search.h"
#include "internal/project_search_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct test_fixture {
    char directory[256];
    char source_path[320];
    char *permitted_roots[1];
    evo_project_file_record_t file;
    evo_project_baseline_owner_t baseline_owner;
    evo_project_baseline_t baseline;
    evo_project_analysis_owner_t analysis_owner;
    evo_project_analysis_t analysis;
} test_fixture_t;

typedef struct test_provider_context {
    bool constant_fitness;
    size_t calls;
    char candidate[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char assurance[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char measurement[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
} test_provider_context_t;

static int test_failures = 0;

static const unsigned char test_source_bytes[] =
    "static int unit_value(void) { return 1; }\n";

static const evo_project_source_location_record_t test_locations[] = {
    {"location-a",
     "unit.c",
     1U,
     1U,
     1U,
     18U,
     EVO_PROJECT_LOCATION_SPELLING,
     NULL},
    {"location-b",
     "unit.c",
     1U,
     19U,
     1U,
     39U,
     EVO_PROJECT_LOCATION_MACRO_EXPANSION,
     "location-a"}};

static const evo_project_optimization_record_t test_optimizations[] = {
    {"compiler-a",
     "inline",
     "function-a",
     "location-a",
     "candidate retained",
     EVO_PROJECT_OPTIMIZATION_MISSED},
    {"compiler-b",
     "vectorize",
     "function-a",
     "location-b",
     "candidate retained",
     EVO_PROJECT_OPTIMIZATION_MISSED}};

static const evo_project_runtime_record_t test_runtime[] = {
    {"runtime-b",
     "fixture-workload-v1",
     "function-a",
     "location-b",
     EVO_PROJECT_RUNTIME_SAMPLE_COUNT,
     UINT64_C(200)}};

static const evo_project_opportunity_record_t test_opportunities[] = {
    {1U, "location-b", 1U, true, UINT64_C(200)},
    {2U, "location-a", 1U, false, 0U}};

static const char *const test_choices[] = {"aggressive", "balanced"};
static const evo_project_transformation_parameter_schema_t test_parameters_b[] = {
    {"enabled",
     EVO_PROJECT_RECIPE_PARAMETER_BOOLEAN,
     true,
     0,
     0,
     0U,
     NULL},
    {"limit",
     EVO_PROJECT_RECIPE_PARAMETER_INTEGER,
     true,
     -8,
     8,
     0U,
     NULL},
    {"strategy",
     EVO_PROJECT_RECIPE_PARAMETER_CHOICE,
     true,
     0,
     0,
     sizeof(test_choices) / sizeof(test_choices[0]),
     test_choices}};
static const evo_project_transformation_reference_t test_dependencies_b[] = {
    {"transform-a", 1U}};

static const evo_project_transformation_catalogue_entry_t test_catalogue_entries[] = {
    {"transform-a",
     1U,
     EVO_PROJECT_RECIPE_LOCATION_ALL,
     0U,
     NULL,
     0U,
     NULL,
     0U,
     NULL,
     0U,
     NULL},
    {"transform-b",
     2U,
     EVO_PROJECT_RECIPE_LOCATION_ALL,
     sizeof(test_parameters_b) / sizeof(test_parameters_b[0]),
     test_parameters_b,
     0U,
     NULL,
     sizeof(test_dependencies_b) / sizeof(test_dependencies_b[0]),
     test_dependencies_b,
     0U,
     NULL},
    {"transform-c",
     1U,
     EVO_PROJECT_RECIPE_LOCATION_ALL,
     0U,
     NULL,
     0U,
     NULL,
     0U,
     NULL,
     0U,
     NULL}};

static const evo_project_transformation_catalogue_t test_catalogue = {
    EVO_PROJECT_TRANSFORMATION_CATALOGUE_SCHEMA_VERSION,
    "search-fixture-catalogue",
    1U,
    sizeof(test_catalogue_entries) / sizeof(test_catalogue_entries[0]),
    test_catalogue_entries};

static const evo_project_recipe_parameter_value_t test_b_values[] = {
    {"enabled", EVO_PROJECT_RECIPE_PARAMETER_BOOLEAN, 0, false, NULL},
    {"limit", EVO_PROJECT_RECIPE_PARAMETER_INTEGER, -8, false, NULL},
    {"strategy",
     EVO_PROJECT_RECIPE_PARAMETER_CHOICE,
     0,
     false,
     "aggressive"}};

static const evo_project_recipe_proposal_record_t test_parent_records[] = {
    {"record-a", "location-a", "transform-a", 1U, 0U, NULL},
    {"record-c", "location-b", "transform-c", 1U, 0U, NULL}};

static const evo_project_recipe_proposal_record_t test_parameter_records[] = {
    {"record-a", "location-a", "transform-a", 1U, 0U, NULL},
    {"record-b",
     "location-b",
     "transform-b",
     2U,
     sizeof(test_b_values) / sizeof(test_b_values[0]),
     test_b_values}};

static const evo_project_recipe_proposal_record_t test_parent_a_record = {
    "record-a", "location-a", "transform-a", 1U, 0U, NULL};
static const evo_project_recipe_proposal_record_t test_parent_c_record = {
    "record-c", "location-b", "transform-c", 1U, 0U, NULL};

static void test_check(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "project search test failure: %s\n", message);
        test_failures += 1;
    }
}

static bool test_write_all(
    int file_descriptor,
    const unsigned char *bytes,
    size_t byte_count)
{
    size_t position = 0U;

    while (position < byte_count) {
        const ssize_t written = write(
            file_descriptor, bytes + position, byte_count - position);

        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            return false;
        }
        position += (size_t)written;
    }
    return true;
}

static evo_project_recipe_limits_t test_recipe_limits(void)
{
    return (evo_project_recipe_limits_t){
        .max_string_bytes = 256U,
        .max_path_bytes = 256U,
        .max_catalogue_entries = 16U,
        .max_parameter_schemas = 32U,
        .max_choices = 32U,
        .max_records = 16U,
        .max_parameters_per_record = 16U,
        .max_preconditions_per_record = 16U,
        .max_dependencies_per_record = 16U,
        .max_conflicts_per_record = 16U,
        .max_provenance_records_per_record = 16U,
        .max_json_tokens = 2048U,
        .max_json_depth = 16U,
        .max_genome_bytes = 16384U,
        .max_audit_bytes = 16384U,
        .max_total_bytes = 65536U,
    };
}

static evo_project_recipe_context_t test_recipe_context(
    const test_fixture_t *fixture)
{
    return (evo_project_recipe_context_t){
        .baseline = &fixture->baseline,
        .analysis = &fixture->analysis,
        .catalogue = &test_catalogue,
        .limits = test_recipe_limits(),
    };
}

static bool test_fixture_prepare(test_fixture_t *fixture)
{
    char temporary_template[] = "/tmp/evo-project-search-XXXXXX";
    char *directory = mkdtemp(temporary_template);
    int file_descriptor;
    int written;
    evo_project_fingerprint_t fingerprint;

    if (directory == NULL) {
        return false;
    }
    written = evo_project_format(
        fixture->directory, sizeof(fixture->directory), "%s", directory);
    if (written <= 0 || (size_t)written >= sizeof(fixture->directory)) {
        return false;
    }
    written = evo_project_format(
        fixture->source_path,
        sizeof(fixture->source_path),
        "%s/unit.c",
        fixture->directory);
    if (written <= 0 || (size_t)written >= sizeof(fixture->source_path)) {
        return false;
    }
    file_descriptor = open(
        fixture->source_path,
        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
        0600);
    if (file_descriptor < 0 ||
        !test_write_all(
            file_descriptor,
            test_source_bytes,
            sizeof(test_source_bytes) - 1U) ||
        fsync(file_descriptor) != 0 || close(file_descriptor) != 0 ||
        chmod(fixture->source_path, 0400) != 0 ||
        chmod(fixture->directory, 0500) != 0) {
        if (file_descriptor >= 0) {
            (void)close(file_descriptor);
        }
        return false;
    }

    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_bytes(
        &fingerprint, test_source_bytes, sizeof(test_source_bytes) - 1U);
    fixture->permitted_roots[0] = "unit.c";
    fixture->file.path = "unit.c";
    fixture->file.size = sizeof(test_source_bytes) - 1U;
    fixture->file.source_mode = 0400U;
    fixture->file.content_fingerprint = fingerprint.value;
    fixture->baseline_owner.manifest.permitted_roots = fixture->permitted_roots;
    fixture->baseline_owner.manifest.permitted_root_count = 1U;
    fixture->baseline_owner.manifest.budget.max_files = 4U;
    fixture->baseline_owner.manifest.budget.max_file_bytes = 4096U;
    fixture->baseline_owner.manifest.budget.max_total_bytes = 4096U;
    fixture->baseline_owner.manifest.budget.max_path_bytes = 256U;
    fixture->baseline_owner.manifest.budget.max_evidence_bytes = 65536U;
    fixture->baseline_owner.snapshot_path = fixture->directory;
    fixture->baseline_owner.files = &fixture->file;
    fixture->baseline_owner.file_count = 1U;
    fixture->baseline_owner.total_file_bytes = fixture->file.size;
    fixture->baseline_owner.baseline_fingerprint = UINT64_C(0x1020304050607080);
    fixture->baseline_owner.state = EVO_PROJECT_BASELINE_ELIGIBLE;
    fixture->baseline_owner.committed = true;
    fixture->baseline.schema_version = EVO_PROJECT_BASELINE_SCHEMA_VERSION;
    fixture->baseline.state = EVO_PROJECT_BASELINE_ELIGIBLE;
    evo_project_fingerprint_format(
        fixture->baseline_owner.baseline_fingerprint,
        fixture->baseline.baseline_fingerprint);
    fixture->baseline.private_owner = &fixture->baseline_owner;

    fixture->analysis_owner.baseline_fingerprint =
        fixture->baseline.baseline_fingerprint;
    fixture->analysis_owner.source_locations =
        (evo_project_source_location_record_t *)test_locations;
    fixture->analysis_owner.source_location_count =
        sizeof(test_locations) / sizeof(test_locations[0]);
    fixture->analysis_owner.optimization_records =
        (evo_project_optimization_record_t *)test_optimizations;
    fixture->analysis_owner.optimization_record_count =
        sizeof(test_optimizations) / sizeof(test_optimizations[0]);
    fixture->analysis_owner.runtime_records =
        (evo_project_runtime_record_t *)test_runtime;
    fixture->analysis_owner.runtime_record_count =
        sizeof(test_runtime) / sizeof(test_runtime[0]);
    fixture->analysis_owner.opportunities =
        (evo_project_opportunity_record_t *)test_opportunities;
    fixture->analysis_owner.opportunity_count =
        sizeof(test_opportunities) / sizeof(test_opportunities[0]);
    fixture->analysis_owner.analysis_fingerprint = UINT64_C(0x8877665544332211);
    fixture->analysis_owner.committed = true;
    fixture->analysis.schema_version = EVO_PROJECT_ANALYSIS_SCHEMA_VERSION;
    fixture->analysis.baseline_fingerprint = fixture->baseline.baseline_fingerprint;
    evo_project_fingerprint_format(
        fixture->analysis_owner.analysis_fingerprint,
        fixture->analysis.analysis_fingerprint);
    fixture->analysis.source_location_count =
        fixture->analysis_owner.source_location_count;
    fixture->analysis.source_locations = fixture->analysis_owner.source_locations;
    fixture->analysis.optimization_record_count =
        fixture->analysis_owner.optimization_record_count;
    fixture->analysis.optimization_records =
        fixture->analysis_owner.optimization_records;
    fixture->analysis.runtime_record_count =
        fixture->analysis_owner.runtime_record_count;
    fixture->analysis.runtime_records = fixture->analysis_owner.runtime_records;
    fixture->analysis.opportunity_count = fixture->analysis_owner.opportunity_count;
    fixture->analysis.opportunities = fixture->analysis_owner.opportunities;
    fixture->analysis.projection_complete = true;
    fixture->analysis.probabilistic_authority = false;
    fixture->analysis.private_owner = &fixture->analysis_owner;
    return true;
}

static void test_fixture_destroy(test_fixture_t *fixture)
{
    (void)chmod(fixture->directory, 0700);
    (void)unlink(fixture->source_path);
    (void)rmdir(fixture->directory);
    *fixture = (test_fixture_t){0};
}

static void test_provider_identity(
    const char *domain,
    const char *recipe_fingerprint,
    char output[EVO_PROJECT_FINGERPRINT_TEXT_SIZE])
{
    evo_project_fingerprint_t fingerprint;

    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_string(&fingerprint, domain);
    evo_project_fingerprint_string(&fingerprint, recipe_fingerprint);
    evo_project_fingerprint_format(fingerprint.value, output);
}

static evo_project_search_status_t test_provider(
    const evo_project_search_evaluation_request_t *request,
    void *opaque,
    evo_project_search_evaluation_outcome_t *outcome)
{
    test_provider_context_t *context = opaque;
    double score = 0.0;
    size_t index;

    if (request == NULL || request->recipe == NULL ||
        request->recipe->private_owner == NULL || outcome == NULL) {
        return EVO_PROJECT_SEARCH_ERROR_PROVIDER;
    }
    for (index = 0U; index < request->recipe->record_count; index += 1U) {
        score += 10.0;
        if (strcmp(
                request->recipe->records[index].transformation_identity,
                "transform-c") == 0) {
            score += 2.0;
        }
    }
    if (context->constant_fitness) {
        score = 10.0;
    }
    test_provider_identity(
        "candidate", request->recipe->recipe_fingerprint, context->candidate);
    test_provider_identity(
        "assurance", request->recipe->recipe_fingerprint, context->assurance);
    test_provider_identity(
        "measurement", request->recipe->recipe_fingerprint, context->measurement);
    *outcome = (evo_project_search_evaluation_outcome_t){
        .schema_version = EVO_PROJECT_SEARCH_SCHEMA_VERSION,
        .accepted = true,
        .correctness_preserved = true,
        .performance_eligible = true,
        .fitness_available = true,
        .candidate_fingerprint = context->candidate,
        .assurance_fingerprint = context->assurance,
        .measurement_fingerprint = context->measurement,
        .fitness = {
            .correctness = 1.0,
            .performance = score,
            .memory_use = score / 10.0,
            .reliability = 1.0,
            .maintainability = 1.0,
            .constraint_penalty = 0.0,
            .total = score,
        },
    };
    context->calls += 1U;
    return EVO_PROJECT_SEARCH_SUCCESS;
}

static evo_project_search_config_t test_search_config(
    const test_fixture_t *fixture,
    test_provider_context_t *provider)
{
    return (evo_project_search_config_t){
        .recipe_context = test_recipe_context(fixture),
        .genome_size = 16384U,
        .population_size = 4U,
        .generation_limit = 1U,
        .tournament_size = 2U,
        .crossover_rate = 0.0,
        .mutation_rate = 1.0,
        .random_seed = UINT64_C(0x123456789abcdef0),
        .max_core_population_bytes = 65536U,
        .max_core_evaluation_bytes = 65536U,
        .max_core_child_population_bytes = 65536U,
        .max_core_diversity_work = SIZE_MAX,
        .policy = {
            .schema_version = EVO_PROJECT_SEARCH_SCHEMA_VERSION,
            .identity = "structured-search-test-v1",
            .mutation_policy_version = EVO_PROJECT_SEARCH_MUTATION_POLICY_VERSION,
            .crossover_policy_version = EVO_PROJECT_SEARCH_CROSSOVER_POLICY_VERSION,
            .repair_policy_version = EVO_PROJECT_SEARCH_REPAIR_POLICY_VERSION,
            .initial_record_count = 1U,
            .maximum_record_count = 4U,
            .mutation_operation_mask = EVO_PROJECT_SEARCH_MUTATION_ADD,
            .max_mutations_per_event = 1U,
            .max_repair_passes = 8U,
            .integer_parameter_wrap = false,
        },
        .evaluation_provider_identity = "deterministic-provider-v1",
        .evaluation_provider = test_provider,
        .evaluation_provider_context = provider,
        .limits = {
            .max_string_bytes = 256U,
            .max_records = 8U,
            .max_parameters_per_record = 16U,
            .max_mutations_per_event = 4U,
            .max_repair_passes = 16U,
            .max_lineage_records = 64U,
            .max_operator_events = 64U,
            .max_evidence_bytes = 262144U,
            .max_total_bytes = 524288U,
        },
    };
}

static evo_project_recipe_status_t test_build_recipe(
    const test_fixture_t *fixture,
    const evo_project_recipe_proposal_record_t *records,
    size_t record_count,
    evo_project_recipe_t *recipe)
{
    const evo_project_recipe_build_config_t config = {
        .context = test_recipe_context(fixture),
        .record_count = record_count,
        .records = records,
        .genome_size = 16384U,
    };

    return evo_project_recipe_build(&config, recipe);
}

static void test_structured_operators(test_fixture_t *fixture)
{
    test_provider_context_t provider = {0};
    evo_project_search_config_t config = test_search_config(fixture, &provider);
    evo_project_recipe_t parent = {0};
    evo_project_recipe_t parameter_parent = {0};
    evo_project_recipe_t mutated = {0};
    evo_project_recipe_t parent_a = {0};
    evo_project_recipe_t parent_c = {0};
    evo_project_recipe_t child_a = {0};
    evo_project_recipe_t child_b = {0};
    const evo_project_search_operator_kind_t operations[] = {
        EVO_PROJECT_SEARCH_OPERATOR_MUTATION_ADD,
        EVO_PROJECT_SEARCH_OPERATOR_MUTATION_REMOVE,
        EVO_PROJECT_SEARCH_OPERATOR_MUTATION_REPLACE,
        EVO_PROJECT_SEARCH_OPERATOR_MUTATION_REORDER};
    size_t index;

    test_check(
        test_build_recipe(
            fixture,
            test_parent_records,
            sizeof(test_parent_records) / sizeof(test_parent_records[0]),
            &parent) == EVO_PROJECT_RECIPE_SUCCESS,
        "operator parent builds");
    test_check(
        test_build_recipe(
            fixture,
            test_parameter_records,
            sizeof(test_parameter_records) / sizeof(test_parameter_records[0]),
            &parameter_parent) == EVO_PROJECT_RECIPE_SUCCESS,
        "parameter parent builds");
    if (parent.private_owner == NULL || parameter_parent.private_owner == NULL) {
        goto finish;
    }
    for (index = 0U; index < sizeof(operations) / sizeof(operations[0]); index += 1U) {
        const evo_project_recipe_status_t status = evo_search_mutate_recipe(
            &config,
            parent.genome,
            operations[index],
            UINT64_C(0x1000) + (uint64_t)index,
            &mutated);

        test_check(status == EVO_PROJECT_RECIPE_SUCCESS, "structured mutation succeeds");
        if (status == EVO_PROJECT_RECIPE_SUCCESS) {
            test_check(
                mutated.projection_complete && !mutated.raw_source_bytes,
                "mutated recipe remains canonical structured authority");
        }
        evo_project_recipe_destroy(&mutated);
    }
    test_check(
        evo_search_mutate_recipe(
            &config,
            parameter_parent.genome,
            EVO_PROJECT_SEARCH_OPERATOR_MUTATION_PARAMETERIZE,
            UINT64_C(0x2222),
            &mutated) == EVO_PROJECT_RECIPE_SUCCESS &&
            strcmp(
                mutated.recipe_fingerprint,
                parameter_parent.recipe_fingerprint) != 0,
        "typed parameter mutation changes canonical recipe");
    evo_project_recipe_destroy(&mutated);

    test_check(
        test_build_recipe(fixture, &test_parent_a_record, 1U, &parent_a) ==
                EVO_PROJECT_RECIPE_SUCCESS &&
            test_build_recipe(fixture, &test_parent_c_record, 1U, &parent_c) ==
                EVO_PROJECT_RECIPE_SUCCESS,
        "crossover parents build");
    if (parent_a.private_owner != NULL && parent_c.private_owner != NULL) {
        const evo_project_recipe_status_t status = evo_search_crossover_recipes(
            &config,
            parent_a.genome,
            parent_c.genome,
            UINT64_C(0x12345678abcdef00),
            &child_a,
            &child_b);

        test_check(
            status == EVO_PROJECT_RECIPE_SUCCESS &&
                child_a.projection_complete && child_b.projection_complete &&
                !child_a.raw_source_bytes && !child_b.raw_source_bytes,
            "record-level crossover produces canonical pair");
    }

finish:
    evo_project_recipe_destroy(&child_b);
    evo_project_recipe_destroy(&child_a);
    evo_project_recipe_destroy(&parent_c);
    evo_project_recipe_destroy(&parent_a);
    evo_project_recipe_destroy(&mutated);
    evo_project_recipe_destroy(&parameter_parent);
    evo_project_recipe_destroy(&parent);
}

static const evo_project_search_lineage_record_t *test_winner(
    const evo_project_search_t *search)
{
    size_t index;

    for (index = 0U; index < search->lineage_count; index += 1U) {
        if (search->lineage[index].winner) {
            return &search->lineage[index];
        }
    }
    return NULL;
}

static void test_search_replay_and_improvement(test_fixture_t *fixture)
{
    test_provider_context_t first_provider = {0};
    test_provider_context_t replay_provider = {0};
    evo_project_search_config_t first_config =
        test_search_config(fixture, &first_provider);
    evo_project_search_config_t replay_config =
        test_search_config(fixture, &replay_provider);
    evo_project_search_t first = {0};
    evo_project_search_t replay = {0};
    const evo_project_search_lineage_record_t *winner;
    double initial_best = -1.0;
    size_t index;

    test_check(
        evo_project_search_run(&first_config, &first) ==
            EVO_PROJECT_SEARCH_SUCCESS,
        "structured search succeeds");
    if (first.private_owner == NULL) {
        return;
    }
    for (index = 0U; index < first.lineage_count; index += 1U) {
        const evo_project_search_lineage_record_t *record = &first.lineage[index];

        if (record->generation == 0U && record->evaluated &&
            (initial_best < 0.0 || record->fitness.total > initial_best)) {
            initial_best = record->fitness.total;
        }
    }
    winner = test_winner(&first);
    test_check(
        winner != NULL && first.best_fitness.total > initial_best,
        "evolution finds a strict verified fitness improvement");
    test_check(
        first.projection_complete && !first.probabilistic_authority &&
            !first.raw_source_bytes && first.canonical_json != NULL &&
            first.audit_markdown != NULL &&
            strstr(first.canonical_json, "\"raw_source_bytes\":false") != NULL &&
            strstr(first.canonical_json, "unit_value") == NULL &&
            strstr(first.audit_markdown, "best verified candidate found") != NULL,
        "search publishes complete human-readable authority without source bytes");
    test_check(
        evo_project_search_run(&replay_config, &replay) ==
                EVO_PROJECT_SEARCH_SUCCESS &&
            strcmp(first.search_fingerprint, replay.search_fingerprint) == 0 &&
            strcmp(first.best_recipe_fingerprint, replay.best_recipe_fingerprint) ==
                0 &&
            first.canonical_json_size == replay.canonical_json_size &&
            strcmp(first.canonical_json, replay.canonical_json) == 0,
        "fixed seed replays population lineage and winner exactly");
    evo_project_search_destroy(&replay);
    evo_project_search_destroy(&first);
}

static void test_exact_tie_is_stable(test_fixture_t *fixture)
{
    test_provider_context_t provider = {.constant_fitness = true};
    evo_project_search_config_t config = test_search_config(fixture, &provider);
    evo_project_search_t search = {0};
    const evo_project_search_lineage_record_t *winner;

    test_check(
        evo_project_search_run(&config, &search) ==
            EVO_PROJECT_SEARCH_SUCCESS,
        "exact-tie search succeeds");
    if (search.private_owner == NULL) {
        return;
    }
    winner = test_winner(&search);
    test_check(
        winner != NULL && winner->generation == 0U &&
            winner->population_index == 0U && search.best_fitness.total == 10.0,
        "exact ties preserve earlier generation and lower stable index");
    evo_project_search_destroy(&search);
}

int main(void)
{
    test_fixture_t fixture = {0};

    test_check(test_fixture_prepare(&fixture), "prepare structured search fixture");
    if (test_failures == 0) {
        test_structured_operators(&fixture);
        test_search_replay_and_improvement(&fixture);
        test_exact_tie_is_stable(&fixture);
    }
    test_fixture_destroy(&fixture);
    if (test_failures != 0) {
        (void)fprintf(
            stderr, "project search failures: %d\n", test_failures);
        return 1;
    }
    (void)printf("project search test: PASS\n");
    return 0;
}
