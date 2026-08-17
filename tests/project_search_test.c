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
#include "internal/project_search_orchestration.h"

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
    char source_path_a[320];
    char source_path_b[320];
    char *permitted_roots[2];
    evo_project_file_record_t files[2];
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
static const unsigned char test_helper_source_bytes[] =
    "static int helper_value(void) { return 2; }\n";

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
     "helper.c",
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
        fixture->source_path_a,
        sizeof(fixture->source_path_a),
        "%s/unit.c",
        fixture->directory);
    if (written <= 0 || (size_t)written >= sizeof(fixture->source_path_a)) {
        return false;
    }
    written = evo_project_format(
        fixture->source_path_b,
        sizeof(fixture->source_path_b),
        "%s/helper.c",
        fixture->directory);
    if (written <= 0 || (size_t)written >= sizeof(fixture->source_path_b)) {
        return false;
    }
    file_descriptor = open(
        fixture->source_path_a,
        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
        0600);
    if (file_descriptor < 0 ||
        !test_write_all(
            file_descriptor,
            test_source_bytes,
            sizeof(test_source_bytes) - 1U) ||
        fsync(file_descriptor) != 0 || close(file_descriptor) != 0) {
        if (file_descriptor >= 0) {
            (void)close(file_descriptor);
        }
        return false;
    }
    file_descriptor = open(
        fixture->source_path_b,
        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
        0600);
    if (file_descriptor < 0 ||
        !test_write_all(
            file_descriptor,
            test_helper_source_bytes,
            sizeof(test_helper_source_bytes) - 1U) ||
        fsync(file_descriptor) != 0 || close(file_descriptor) != 0) {
        if (file_descriptor >= 0) {
            (void)close(file_descriptor);
        }
        return false;
    }
    if (chmod(fixture->source_path_a, 0400) != 0 ||
        chmod(fixture->source_path_b, 0400) != 0 ||
        chmod(fixture->directory, 0500) != 0) {
        return false;
    }

    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_bytes(
        &fingerprint,
        test_helper_source_bytes,
        sizeof(test_helper_source_bytes) - 1U);
    fixture->permitted_roots[0] = "helper.c";
    fixture->files[0].path = "helper.c";
    fixture->files[0].size = sizeof(test_helper_source_bytes) - 1U;
    fixture->files[0].source_mode = 0400U;
    fixture->files[0].content_fingerprint = fingerprint.value;
    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_bytes(
        &fingerprint, test_source_bytes, sizeof(test_source_bytes) - 1U);
    fixture->permitted_roots[1] = "unit.c";
    fixture->files[1].path = "unit.c";
    fixture->files[1].size = sizeof(test_source_bytes) - 1U;
    fixture->files[1].source_mode = 0400U;
    fixture->files[1].content_fingerprint = fingerprint.value;
    fixture->baseline_owner.manifest.permitted_roots = fixture->permitted_roots;
    fixture->baseline_owner.manifest.permitted_root_count = 2U;
    fixture->baseline_owner.manifest.budget.max_files = 4U;
    fixture->baseline_owner.manifest.budget.max_file_bytes = 4096U;
    fixture->baseline_owner.manifest.budget.max_total_bytes = 4096U;
    fixture->baseline_owner.manifest.budget.max_path_bytes = 256U;
    fixture->baseline_owner.manifest.budget.max_evidence_bytes = 65536U;
    fixture->baseline_owner.snapshot_path = fixture->directory;
    fixture->baseline_owner.files = fixture->files;
    fixture->baseline_owner.file_count = 2U;
    fixture->baseline_owner.total_file_bytes =
        fixture->files[0].size + fixture->files[1].size;
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
    (void)unlink(fixture->source_path_a);
    (void)unlink(fixture->source_path_b);
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

typedef struct test_async_handle {
    size_t generation;
    size_t population_index;
    size_t polls_remaining;
    evo_project_orchestration_terminal_reason_t planned_reason;
    evo_project_search_evaluation_outcome_t outcome;
    char candidate[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char assurance[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char measurement[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    bool canceled;
    bool joined;
} test_async_handle_t;

typedef struct test_async_provider_context {
    test_provider_context_t evaluator;
    test_async_handle_t handles[8];
    size_t start_count;
    size_t cancel_count;
    size_t join_count;
    bool saw_live_recipe;
    bool force_timeout;
    size_t timeout_generation;
    size_t timeout_population_index;
} test_async_provider_context_t;

static bool test_copy_fingerprint(
    char output[EVO_PROJECT_FINGERPRINT_TEXT_SIZE],
    const char *input)
{
    size_t index = 0U;

    if (input == NULL) {
        return false;
    }
    while (index < EVO_PROJECT_FINGERPRINT_TEXT_SIZE && input[index] != '\0') {
        output[index] = input[index];
        index += 1U;
    }
    if (index == 0U || index >= EVO_PROJECT_FINGERPRINT_TEXT_SIZE) {
        return false;
    }
    output[index] = '\0';
    return true;
}

static evo_project_orchestration_status_t test_async_start(
    const evo_project_orchestration_provider_request_t *request,
    void *opaque,
    void **handle)
{
    test_async_provider_context_t *context = opaque;
    test_async_handle_t *job;
    evo_project_search_evaluation_request_t evaluation_request = {0};
    evo_project_search_evaluation_outcome_t outcome = {0};

    if (request == NULL || context == NULL || handle == NULL ||
        request->candidate.recipe == NULL ||
        request->candidate.recipe->private_owner == NULL ||
        request->candidate.population_index >= 8U ||
        strcmp(request->candidate.recipe_fingerprint,
               request->candidate.recipe->recipe_fingerprint) != 0) {
        return EVO_PROJECT_ORCHESTRATION_ERROR_PROVIDER;
    }
    context->saw_live_recipe = true;
    job = &context->handles[request->candidate.population_index];
    *job = (test_async_handle_t){0};
    job->generation = request->candidate.generation;
    job->population_index = request->candidate.population_index;
    job->polls_remaining =
        1U + ((request->candidate.population_index + request->candidate.generation) % 3U);
    job->planned_reason =
        context->force_timeout &&
                request->candidate.generation == context->timeout_generation &&
                request->candidate.population_index ==
                    context->timeout_population_index
            ? EVO_PROJECT_ORCHESTRATION_TERMINAL_TIMEOUT
            : EVO_PROJECT_ORCHESTRATION_TERMINAL_SUCCESS;
    evaluation_request.schema_version = EVO_PROJECT_SEARCH_SCHEMA_VERSION;
    evaluation_request.random_seed = request->candidate.random_seed;
    evaluation_request.generation = request->candidate.generation;
    evaluation_request.population_index = request->candidate.population_index;
    evaluation_request.provider_identity = "deterministic-provider-v1";
    evaluation_request.recipe = request->candidate.recipe;
    if (job->planned_reason == EVO_PROJECT_ORCHESTRATION_TERMINAL_SUCCESS) {
        if (test_provider(
                &evaluation_request, &context->evaluator, &outcome) !=
                EVO_PROJECT_SEARCH_SUCCESS ||
            !test_copy_fingerprint(job->candidate, outcome.candidate_fingerprint) ||
            !test_copy_fingerprint(job->assurance, outcome.assurance_fingerprint) ||
            !test_copy_fingerprint(
                job->measurement, outcome.measurement_fingerprint)) {
            return EVO_PROJECT_ORCHESTRATION_ERROR_PROVIDER;
        }
        job->outcome = outcome;
        job->outcome.candidate_fingerprint = job->candidate;
        job->outcome.assurance_fingerprint = job->assurance;
        job->outcome.measurement_fingerprint = job->measurement;
    }
    context->start_count += 1U;
    *handle = job;
    return EVO_PROJECT_ORCHESTRATION_SUCCESS;
}

static evo_project_orchestration_status_t test_async_poll(
    void *handle,
    void *opaque,
    evo_project_orchestration_provider_poll_t *poll)
{
    test_async_handle_t *job = handle;

    (void)opaque;
    if (job == NULL || poll == NULL) {
        return EVO_PROJECT_ORCHESTRATION_ERROR_PROVIDER;
    }
    *poll = (evo_project_orchestration_provider_poll_t){0};
    poll->schema_version = EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION;
    if (job->canceled) {
        poll->terminal = true;
        poll->terminal_reason = EVO_PROJECT_ORCHESTRATION_TERMINAL_CANCELED;
        return EVO_PROJECT_ORCHESTRATION_SUCCESS;
    }
    if (job->polls_remaining > 0U) {
        job->polls_remaining -= 1U;
    }
    if (job->polls_remaining == 0U) {
        poll->terminal = true;
        poll->terminal_reason = job->planned_reason;
    }
    return EVO_PROJECT_ORCHESTRATION_SUCCESS;
}

static evo_project_orchestration_status_t test_async_cancel(
    void *handle,
    void *opaque)
{
    test_async_handle_t *job = handle;
    test_async_provider_context_t *context = opaque;

    if (job == NULL || context == NULL) {
        return EVO_PROJECT_ORCHESTRATION_ERROR_PROVIDER;
    }
    if (!job->canceled) {
        job->canceled = true;
        context->cancel_count += 1U;
    }
    return EVO_PROJECT_ORCHESTRATION_SUCCESS;
}

static evo_project_orchestration_status_t test_async_join(
    void *handle,
    void *opaque,
    evo_project_orchestration_provider_join_t *join)
{
    test_async_handle_t *job = handle;
    test_async_provider_context_t *context = opaque;

    if (job == NULL || context == NULL || join == NULL || job->joined) {
        return EVO_PROJECT_ORCHESTRATION_ERROR_PROVIDER;
    }
    job->joined = true;
    context->join_count += 1U;
    *join = (evo_project_orchestration_provider_join_t){0};
    join->schema_version = EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION;
    join->terminal_reason =
        job->canceled ? EVO_PROJECT_ORCHESTRATION_TERMINAL_CANCELED
                      : job->planned_reason;
    join->capabilities.schema_version = EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION;
    join->capabilities.cpu_limit_enforced = true;
    join->capabilities.address_space_limit_enforced = true;
    join->capabilities.process_limit_enforced = true;
    join->capabilities.storage_limit_enforced = true;
    join->capabilities.output_limit_enforced = true;
    join->capabilities.timeout_enforced = true;
    join->capabilities.filesystem_isolation_enforced = true;
    join->capabilities.network_isolation_enforced = true;
    join->capabilities.descendant_cleanup_enforced = true;
    join->cleanup_complete = true;
    if (join->terminal_reason ==
        EVO_PROJECT_ORCHESTRATION_TERMINAL_SUCCESS) {
        join->evaluation = job->outcome;
    }
    return EVO_PROJECT_ORCHESTRATION_SUCCESS;
}

static evo_project_search_orchestration_policy_t test_orchestration_policy(
    size_t worker_count,
    test_async_provider_context_t *provider)
{
    return (evo_project_search_orchestration_policy_t){
        .schema_version = EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION,
        .identity = "structured-search-orchestration-v1",
        .resources = {
            .schema_version = EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION,
            .external_worker_count = worker_count,
            .cpu_time_ms = UINT64_C(1000),
            .address_space_bytes = UINT64_C(268435456),
            .descendant_process_count = 4U,
            .storage_bytes = UINT64_C(1048576),
            .output_bytes = UINT64_C(65536),
            .wall_timeout_ms = UINT64_C(1000),
            .workspace_bytes = UINT64_C(1048576),
            .require_filesystem_isolation = true,
            .require_network_isolation = true,
            .require_descendant_cleanup = true,
        },
        .provider = {
            .identity = "structured-search-async-provider-v1",
            .start = test_async_start,
            .poll = test_async_poll,
            .cancel = test_async_cancel,
            .join = test_async_join,
            .context = provider,
        },
        .limits = {
            .max_string_bytes = 256U,
            .max_candidates = 8U,
            .max_external_workers = 4U,
            .max_poll_rounds = 32U,
            .max_cpu_time_ms = UINT64_C(10000),
            .max_address_space_bytes = UINT64_C(1073741824),
            .max_descendant_process_count = 16U,
            .max_storage_bytes = UINT64_C(1073741824),
            .max_output_bytes = UINT64_C(1048576),
            .max_wall_timeout_ms = UINT64_C(10000),
            .max_workspace_bytes = UINT64_C(1073741824),
            .max_total_bytes = 1048576U,
        },
    };
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
            .max_mutations_per_event = 2U,
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
    test_check(
        parent.record_count == 2U &&
            strcmp(parent.records[0].target.file, "unit.c") == 0 &&
            strcmp(parent.records[1].target.file, "helper.c") == 0 &&
            parent.records[0].compiler_record_count == 1U &&
            parent.records[1].compiler_record_count == 1U &&
            parent.records[1].runtime_record_count == 1U,
        "structured recipe spans multiple files with provenance");
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
        first.operator_event_count >= first.lineage_count &&
            first.operator_events != NULL,
        "complete structured operator trace is retained");
    test_check(
        first.projection_complete && !first.probabilistic_authority &&
            !first.raw_source_bytes && first.canonical_json != NULL &&
            first.audit_markdown != NULL &&
            strstr(first.canonical_json, "\"operator_events\":[") != NULL &&
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

static void test_orchestrated_search_equivalence(test_fixture_t *fixture)
{
    test_provider_context_t serial_placeholder = {0};
    test_provider_context_t parallel_placeholder = {0};
    test_async_provider_context_t serial_provider = {0};
    test_async_provider_context_t parallel_provider = {0};
    evo_project_search_config_t serial_config =
        test_search_config(fixture, &serial_placeholder);
    evo_project_search_config_t parallel_config =
        test_search_config(fixture, &parallel_placeholder);
    evo_project_search_orchestration_policy_t serial_policy =
        test_orchestration_policy(1U, &serial_provider);
    evo_project_search_orchestration_policy_t parallel_policy =
        test_orchestration_policy(4U, &parallel_provider);
    evo_project_search_t serial = {0};
    evo_project_search_t parallel = {0};
    evo_project_search_orchestration_trace_t serial_trace = {0};
    evo_project_search_orchestration_trace_t parallel_trace = {0};

    test_check(
        evo_project_search_run_orchestrated_with_trace(
            &serial_config, &serial_policy, &serial, &serial_trace) ==
            EVO_PROJECT_SEARCH_SUCCESS,
        "serial orchestrated structured search succeeds");
    test_check(
        evo_project_search_run_orchestrated_with_trace(
            &parallel_config, &parallel_policy, &parallel, &parallel_trace) ==
            EVO_PROJECT_SEARCH_SUCCESS,
        "parallel orchestrated structured search succeeds");
    if (serial.private_owner == NULL || parallel.private_owner == NULL) {
        evo_project_search_orchestration_trace_destroy(&parallel_trace);
        evo_project_search_orchestration_trace_destroy(&serial_trace);
        evo_project_search_destroy(&parallel);
        evo_project_search_destroy(&serial);
        return;
    }
    test_check(
        serial_provider.saw_live_recipe && parallel_provider.saw_live_recipe &&
            serial_provider.start_count == serial_provider.join_count &&
            parallel_provider.start_count == parallel_provider.join_count &&
            serial_provider.cancel_count == 0U &&
            parallel_provider.cancel_count == 0U,
        "orchestration provider sees live recipes and joins every start");
    test_check(
        strcmp(serial.search_fingerprint, parallel.search_fingerprint) == 0 &&
            strcmp(serial.best_recipe_fingerprint,
                   parallel.best_recipe_fingerprint) == 0 &&
            serial.termination_reason == parallel.termination_reason &&
            serial.best_fitness.total == parallel.best_fitness.total &&
            serial.lineage_count == parallel.lineage_count &&
            serial.operator_event_count == parallel.operator_event_count &&
            strcmp(serial.canonical_json, parallel.canonical_json) == 0,
        "external worker count preserves complete logical search authority");
    test_check(
        serial_trace.run_complete && parallel_trace.run_complete &&
            serial_trace.projection_complete && parallel_trace.projection_complete &&
            !serial_trace.probabilistic_authority &&
            !parallel_trace.probabilistic_authority &&
            serial_trace.batch_count == parallel_trace.batch_count &&
            serial_trace.batch_count == serial.generations_completed + 1U &&
            serial_trace.job_count == parallel_trace.job_count &&
            serial_trace.job_count ==
                serial_trace.batch_count * serial_config.population_size,
        "persistent worker traces retain every generation");
    test_check(
        serial_trace.batches[0].external_worker_count == 1U &&
            parallel_trace.batches[0].external_worker_count == 4U &&
            serial_trace.batches[0].generation ==
                parallel_trace.batches[0].generation &&
            serial_trace.batches[0].generation_committed &&
            parallel_trace.batches[0].generation_committed &&
            serial_trace.batches[0].cleanup_complete &&
            parallel_trace.batches[0].cleanup_complete,
        "worker schedule remains diagnostic while generation authority matches");
    evo_project_search_orchestration_trace_destroy(&parallel_trace);
    evo_project_search_orchestration_trace_destroy(&serial_trace);
    evo_project_search_destroy(&parallel);
    evo_project_search_destroy(&serial);
}

static void test_orchestrated_failure_is_atomic(test_fixture_t *fixture)
{
    test_provider_context_t placeholder = {0};
    test_async_provider_context_t provider = {
        .force_timeout = true,
        .timeout_generation = 0U,
        .timeout_population_index = 0U,
    };
    evo_project_search_config_t config =
        test_search_config(fixture, &placeholder);
    evo_project_search_orchestration_policy_t policy =
        test_orchestration_policy(2U, &provider);
    evo_project_search_t search = {0};
    evo_project_search_orchestration_trace_t trace = {0};

    test_check(
        evo_project_search_run_orchestrated_with_trace(
            &config, &policy, &search, &trace) !=
                EVO_PROJECT_SEARCH_SUCCESS &&
            search.private_owner == NULL,
        "hard external worker failure publishes no partial search result");
    test_check(
        provider.start_count > 0U &&
            provider.start_count == provider.join_count &&
            provider.cancel_count > 0U,
        "hard external worker failure cancels and joins started siblings");
    test_check(
        !trace.run_complete && trace.projection_complete &&
            !trace.probabilistic_authority && trace.batch_count == 1U &&
            trace.job_count > 0U && trace.batches[0].has_hard_failure &&
            !trace.batches[0].generation_committed &&
            trace.batches[0].cleanup_complete,
        "failed search retains complete trustworthy worker schedule evidence");
    evo_project_search_orchestration_trace_destroy(&trace);
}

int main(void)
{
    test_fixture_t fixture = {0};

    test_check(test_fixture_prepare(&fixture), "prepare structured search fixture");
    if (test_failures == 0) {
        test_structured_operators(&fixture);
        test_search_replay_and_improvement(&fixture);
        test_exact_tie_is_stable(&fixture);
        test_orchestrated_search_equivalence(&fixture);
        test_orchestrated_failure_is_atomic(&fixture);
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
