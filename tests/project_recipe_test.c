#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#define _XOPEN_SOURCE 700

#include "internal/project_analysis_owner.h"
#include "internal/project_baseline_owner.h"
#include "internal/project_fingerprint.h"
#include "internal/project_json.h"
#include "internal/project_recipe.h"
#include "internal/project_runtime.h"

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

#ifndef EVO_TEST_SOURCE_DIR
#define EVO_TEST_SOURCE_DIR "."
#endif

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

static const char *const test_preconditions_a[] = {
    "baseline-verified"};
static const char *const test_preconditions_b[] = {
    "baseline-verified", "target-current"};
static const char *const test_strategy_choices[] = {
    "aggressive", "balanced"};

static const evo_project_transformation_parameter_schema_t
    test_parameters_b[] = {
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
         sizeof(test_strategy_choices) / sizeof(test_strategy_choices[0]),
         test_strategy_choices}};

static const evo_project_transformation_reference_t test_dependencies_b[] = {
    {"transform-a", 1U}};
static const evo_project_transformation_reference_t test_conflicts_b[] = {
    {"transform-c", 1U}};
static const evo_project_transformation_reference_t test_dependencies_d[] = {
    {"transform-e", 1U}};
static const evo_project_transformation_reference_t test_dependencies_e[] = {
    {"transform-d", 1U}};

static const evo_project_transformation_catalogue_entry_t
    test_catalogue_entries[] = {
        {"transform-a",
         1U,
         EVO_PROJECT_RECIPE_LOCATION_ALL,
         0U,
         NULL,
         sizeof(test_preconditions_a) / sizeof(test_preconditions_a[0]),
         test_preconditions_a,
         0U,
         NULL,
         0U,
         NULL},
        {"transform-b",
         2U,
         EVO_PROJECT_RECIPE_LOCATION_ALL,
         sizeof(test_parameters_b) / sizeof(test_parameters_b[0]),
         test_parameters_b,
         sizeof(test_preconditions_b) / sizeof(test_preconditions_b[0]),
         test_preconditions_b,
         sizeof(test_dependencies_b) / sizeof(test_dependencies_b[0]),
         test_dependencies_b,
         sizeof(test_conflicts_b) / sizeof(test_conflicts_b[0]),
         test_conflicts_b},
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
         NULL},
        {"transform-d",
         1U,
         EVO_PROJECT_RECIPE_LOCATION_ALL,
         0U,
         NULL,
         0U,
         NULL,
         sizeof(test_dependencies_d) / sizeof(test_dependencies_d[0]),
         test_dependencies_d,
         0U,
         NULL},
        {"transform-e",
         1U,
         EVO_PROJECT_RECIPE_LOCATION_ALL,
         0U,
         NULL,
         0U,
         NULL,
         sizeof(test_dependencies_e) / sizeof(test_dependencies_e[0]),
         test_dependencies_e,
         0U,
         NULL}};

static const evo_project_transformation_catalogue_t test_catalogue = {
    EVO_PROJECT_TRANSFORMATION_CATALOGUE_SCHEMA_VERSION,
    "fixture-transformations",
    3U,
    sizeof(test_catalogue_entries) / sizeof(test_catalogue_entries[0]),
    test_catalogue_entries};

static const evo_project_recipe_parameter_value_t test_parameters[] = {
    {"strategy",
     EVO_PROJECT_RECIPE_PARAMETER_CHOICE,
     0,
     false,
     "balanced"},
    {"limit", EVO_PROJECT_RECIPE_PARAMETER_INTEGER, 7, false, NULL},
    {"enabled", EVO_PROJECT_RECIPE_PARAMETER_BOOLEAN, 0, true, NULL}};

static const evo_project_recipe_parameter_value_t test_parameters_ordered[] = {
    {"enabled", EVO_PROJECT_RECIPE_PARAMETER_BOOLEAN, 0, true, NULL},
    {"limit", EVO_PROJECT_RECIPE_PARAMETER_INTEGER, 7, false, NULL},
    {"strategy",
     EVO_PROJECT_RECIPE_PARAMETER_CHOICE,
     0,
     false,
     "balanced"}};

static const evo_project_recipe_proposal_record_t test_proposals[] = {
    {"record-b",
     "location-b",
     "transform-b",
     2U,
     sizeof(test_parameters) / sizeof(test_parameters[0]),
     test_parameters},
    {"record-a", "location-a", "transform-a", 1U, 0U, NULL}};

static const evo_project_recipe_proposal_record_t test_proposals_ordered[] = {
    {"record-a", "location-a", "transform-a", 1U, 0U, NULL},
    {"record-b",
     "location-b",
     "transform-b",
     2U,
     sizeof(test_parameters_ordered) / sizeof(test_parameters_ordered[0]),
     test_parameters_ordered}};

static void test_check(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "project recipe test failure: %s\n", message);
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
    evo_project_recipe_limits_t limits = {0};

    limits.max_string_bytes = 256U;
    limits.max_path_bytes = 256U;
    limits.max_catalogue_entries = 16U;
    limits.max_parameter_schemas = 32U;
    limits.max_choices = 32U;
    limits.max_records = 16U;
    limits.max_parameters_per_record = 16U;
    limits.max_preconditions_per_record = 16U;
    limits.max_dependencies_per_record = 16U;
    limits.max_conflicts_per_record = 16U;
    limits.max_provenance_records_per_record = 16U;
    limits.max_json_tokens = 2048U;
    limits.max_json_depth = 16U;
    limits.max_genome_bytes = 16384U;
    limits.max_audit_bytes = 16384U;
    limits.max_total_bytes = 65536U;
    return limits;
}

static evo_project_recipe_context_t test_recipe_context(
    const test_fixture_t *fixture)
{
    evo_project_recipe_context_t context = {0};

    context.baseline = &fixture->baseline;
    context.analysis = &fixture->analysis;
    context.catalogue = &test_catalogue;
    context.limits = test_recipe_limits();
    return context;
}

static bool test_fixture_prepare(test_fixture_t *fixture)
{
    char temporary_template[] = "/tmp/evo-project-recipe-XXXXXX";
    char *directory = mkdtemp(temporary_template);
    int file_descriptor;
    int written;
    evo_project_fingerprint_t fingerprint;

    if (directory == NULL) {
        return false;
    }
    written = evo_project_format(
        fixture->directory,
        sizeof(fixture->directory),
        "%s",
        directory);
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
    if (file_descriptor < 0) {
        return false;
    }
    if (!test_write_all(
            file_descriptor,
            test_source_bytes,
            sizeof(test_source_bytes) - 1U) ||
        fsync(file_descriptor) != 0 || close(file_descriptor) != 0) {
        (void)close(file_descriptor);
        return false;
    }
    file_descriptor = -1;
    if (chmod(fixture->source_path, 0400) != 0 ||
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
    fixture->baseline_owner.manifest.permitted_roots =
        fixture->permitted_roots;
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
    fixture->baseline_owner.baseline_fingerprint =
        UINT64_C(0x1020304050607080);
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
    fixture->analysis_owner.analysis_fingerprint =
        UINT64_C(0x8877665544332211);
    fixture->analysis_owner.committed = true;
    fixture->analysis.schema_version = EVO_PROJECT_ANALYSIS_SCHEMA_VERSION;
    fixture->analysis.baseline_fingerprint =
        fixture->baseline.baseline_fingerprint;
    evo_project_fingerprint_format(
        fixture->analysis_owner.analysis_fingerprint,
        fixture->analysis.analysis_fingerprint);
    fixture->analysis.source_location_count =
        fixture->analysis_owner.source_location_count;
    fixture->analysis.source_locations =
        fixture->analysis_owner.source_locations;
    fixture->analysis.optimization_record_count =
        fixture->analysis_owner.optimization_record_count;
    fixture->analysis.optimization_records =
        fixture->analysis_owner.optimization_records;
    fixture->analysis.runtime_record_count =
        fixture->analysis_owner.runtime_record_count;
    fixture->analysis.runtime_records = fixture->analysis_owner.runtime_records;
    fixture->analysis.opportunity_count =
        fixture->analysis_owner.opportunity_count;
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

static evo_project_recipe_build_config_t test_build_config(
    const test_fixture_t *fixture,
    const evo_project_recipe_proposal_record_t *records,
    size_t record_count,
    size_t genome_size)
{
    evo_project_recipe_build_config_t config = {0};

    config.context = test_recipe_context(fixture);
    config.record_count = record_count;
    config.records = records;
    config.genome_size = genome_size;
    return config;
}

static unsigned char *test_clone_genome(
    const evo_project_recipe_t *recipe)
{
    unsigned char *clone = evo_project_allocate_zeroed(
        recipe->genome_size, sizeof(*clone));
    size_t index;

    if (clone == NULL) {
        return NULL;
    }
    for (index = 0U; index < recipe->genome_size; index += 1U) {
        clone[index] = recipe->genome[index];
    }
    return clone;
}

static bool test_source_unchanged(const test_fixture_t *fixture)
{
    unsigned char bytes[sizeof(test_source_bytes)];
    size_t position = 0U;
    struct stat metadata;
    int file_descriptor = open(
        fixture->source_path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    bool unchanged = file_descriptor >= 0 &&
                     fstat(file_descriptor, &metadata) == 0 &&
                     (unsigned int)(metadata.st_mode & (mode_t)07777) == 0400U;

    while (unchanged && position < sizeof(test_source_bytes) - 1U) {
        const ssize_t count = read(
            file_descriptor,
            bytes + position,
            sizeof(test_source_bytes) - 1U - position);

        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            unchanged = false;
            break;
        }
        position += (size_t)count;
    }
    if (unchanged) {
        size_t index;

        for (index = 0U; index < sizeof(test_source_bytes) - 1U;
             index += 1U) {
            if (bytes[index] != test_source_bytes[index]) {
                unchanged = false;
                break;
            }
        }
    }
    if (file_descriptor >= 0 && close(file_descriptor) != 0) {
        unchanged = false;
    }
    return unchanged && position == sizeof(test_source_bytes) - 1U;
}

static bool test_recipe_matches_golden(const evo_project_recipe_t *recipe)
{
    char path[1024];
    size_t position = 0U;
    int file_descriptor;
    bool matches;
    const int written = evo_project_format(
        path,
        sizeof(path),
        "%s/tests/fixtures/project-recipe/golden-v1.json",
        EVO_TEST_SOURCE_DIR);

    if (written <= 0 || (size_t)written >= sizeof(path)) {
        return false;
    }
    file_descriptor = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    matches = file_descriptor >= 0;
    while (matches && position < recipe->canonical_json_size) {
        unsigned char bytes[256];
        size_t request = recipe->canonical_json_size - position;
        ssize_t count;
        size_t index;

        if (request > sizeof(bytes)) {
            request = sizeof(bytes);
        }
        count = read(file_descriptor, bytes, request);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            matches = false;
            break;
        }
        for (index = 0U; index < (size_t)count; index += 1U) {
            if (bytes[index] !=
                (unsigned char)recipe->canonical_json[position + index]) {
                matches = false;
                break;
            }
        }
        position += (size_t)count;
    }
    if (matches) {
        unsigned char extra;
        ssize_t count;

        do {
            count = read(file_descriptor, &extra, 1U);
        } while (count < 0 && errno == EINTR);
        matches = count == 0;
    }
    if (file_descriptor >= 0 && close(file_descriptor) != 0) {
        matches = false;
    }
    return matches;
}

static bool test_parse_i64_text(const char *text, int64_t *value)
{
    const evo_project_json_token_t token = {
        EVO_PROJECT_JSON_NUMBER, 0U, strlen(text), 0U, SIZE_MAX};

    return evo_project_json_parse_i64(text, &token, value);
}

static void test_signed_integer_boundaries(void)
{
    int64_t value = 0;

    test_check(
        test_parse_i64_text("-9223372036854775808", &value) &&
            value == INT64_MIN,
        "signed parser accepts INT64_MIN");
    test_check(
        test_parse_i64_text("9223372036854775807", &value) &&
            value == INT64_MAX,
        "signed parser accepts INT64_MAX");
    test_check(
        !test_parse_i64_text("-9223372036854775809", &value) &&
            !test_parse_i64_text("9223372036854775808", &value) &&
            !test_parse_i64_text("1.0", &value),
        "signed parser rejects overflow and non-integral tokens");
}

static void test_success_replay_and_projection(test_fixture_t *fixture)
{
    evo_project_recipe_t first = {0};
    evo_project_recipe_t reordered = {0};
    evo_project_recipe_t decoded = {0};
    evo_project_recipe_t noop = {0};
    evo_project_recipe_build_config_t first_config = test_build_config(
        fixture,
        test_proposals,
        sizeof(test_proposals) / sizeof(test_proposals[0]),
        8192U);
    evo_project_recipe_build_config_t reordered_config = test_build_config(
        fixture,
        test_proposals_ordered,
        sizeof(test_proposals_ordered) /
            sizeof(test_proposals_ordered[0]),
        8192U);
    evo_project_recipe_build_config_t noop_config = test_build_config(
        fixture, NULL, 0U, 2048U);
    evo_project_recipe_status_t status =
        evo_project_recipe_build(&first_config, &first);

    if (status != EVO_PROJECT_RECIPE_SUCCESS) {
        (void)fprintf(
            stderr,
            "first recipe status: %s (%u)\n",
            evo_project_recipe_status_name(status),
            (unsigned int)status);
    }
    test_check(status == EVO_PROJECT_RECIPE_SUCCESS, "canonical build succeeds");
    if (status != EVO_PROJECT_RECIPE_SUCCESS) {
        return;
    }
    test_check(
        first.schema_version == EVO_PROJECT_RECIPE_SCHEMA_VERSION &&
            first.record_count == 2U && first.records != NULL &&
            strcmp(first.records[0].identity, "record-a") == 0 &&
            strcmp(first.records[1].identity, "record-b") == 0,
        "stable dependency-first composition order");
    test_check(
        first.records[1].dependency_count == 1U &&
            strcmp(
                first.records[1].dependencies[0].record_identity,
                "record-a") == 0 &&
            first.records[1].parameter_count == 3U &&
            strcmp(first.records[1].parameters[0].identity, "enabled") == 0 &&
            strcmp(first.records[1].parameters[1].identity, "limit") == 0 &&
            strcmp(first.records[1].parameters[2].identity, "strategy") == 0,
        "dependencies and parameters canonicalized");
    test_check(
        first.records[0].compiler_record_count == 1U &&
            first.records[0].runtime_record_count == 0U &&
            first.records[1].compiler_record_count == 1U &&
            first.records[1].runtime_record_count == 1U &&
            strcmp(
                first.records[1].runtime_record_identities[0],
                "runtime-b") == 0,
        "complete analysis provenance retained");
    test_check(
        first.genome_size == 8192U && first.genome != NULL &&
            first.canonical_json_size > 0U &&
            first.canonical_json ==
                (const char *)first.genome +
                    EVO_PROJECT_RECIPE_GENOME_HEADER_SIZE &&
            first.audit_markdown_size > 0U &&
            first.projection_complete && !first.probabilistic_authority &&
            !first.raw_source_bytes,
        "bounded genome and explainable projection published");
    test_check(
        strstr(
            first.canonical_json,
            "\"reference_form\":\"canonical-json-record-array-and-direct-scans\"") !=
                NULL &&
            strstr(
                first.canonical_json,
                "\"probabilistic_authority\":false") != NULL &&
            strstr(first.canonical_json, "unit_value") == NULL &&
            strstr(first.audit_markdown, "# EVO Transformation Recipe") !=
                NULL &&
            strstr(first.audit_markdown, "## Record 1: `record-a`") != NULL,
        "human-readable abstraction is complete without source bytes");
    test_check(
        test_recipe_matches_golden(&first),
        "canonical JSON exactly matches retained golden");
    test_check(
        strcmp(
            first.recipe_fingerprint,
            "fnv1a64-v1:15955ec61c0c05bc") == 0,
        "recipe fingerprint golden vector");

    status = evo_project_recipe_build(&reordered_config, &reordered);
    test_check(
        status == EVO_PROJECT_RECIPE_SUCCESS &&
            evo_project_recipe_equal(&first, &reordered),
        "proposal and parameter input order do not change identity");
    if (status == EVO_PROJECT_RECIPE_SUCCESS) {
        size_t index;
        bool exact = first.genome_size == reordered.genome_size;

        for (index = 0U; exact && index < first.genome_size; index += 1U) {
            exact = first.genome[index] == reordered.genome[index];
        }
        test_check(exact, "same recipe has exact fixed-genome encoding");
    }

    status = evo_project_recipe_decode(
        &first_config.context,
        first.genome,
        first.genome_size,
        &decoded);
    test_check(
        status == EVO_PROJECT_RECIPE_SUCCESS &&
            evo_project_recipe_equal(&first, &decoded) &&
            strcmp(first.recipe_fingerprint, decoded.recipe_fingerprint) == 0,
        "canonical genome decodes and replays exactly");
    status = evo_project_recipe_decode(
        &first_config.context,
        first.genome,
        first.genome_size,
        &decoded);
    test_check(
        status == EVO_PROJECT_RECIPE_ERROR_RESULT_ACTIVE,
        "active decoded result rejected");

    status = evo_project_recipe_build(&noop_config, &noop);
    test_check(
        status == EVO_PROJECT_RECIPE_SUCCESS && noop.record_count == 0U &&
            strstr(noop.audit_markdown, "canonical no-op recipe") != NULL,
        "canonical no-op recipe supported");
    test_check(test_source_unchanged(fixture), "recipe operations never write source");

    evo_project_recipe_destroy(&noop);
    evo_project_recipe_destroy(&decoded);
    evo_project_recipe_destroy(&reordered);
    evo_project_recipe_destroy(&first);
    test_check(
        !evo_project_recipe_equal(&first, &reordered),
        "destroyed recipes are not equal authorities");
}

static void test_decode_failures(test_fixture_t *fixture)
{
    evo_project_recipe_t source = {0};
    evo_project_recipe_t decoded = {0};
    evo_project_recipe_build_config_t config = test_build_config(
        fixture,
        test_proposals,
        sizeof(test_proposals) / sizeof(test_proposals[0]),
        8192U);
    evo_project_recipe_status_t status =
        evo_project_recipe_build(&config, &source);
    unsigned char *mutated;

    test_check(status == EVO_PROJECT_RECIPE_SUCCESS, "decode fixture builds");
    if (status != EVO_PROJECT_RECIPE_SUCCESS) {
        return;
    }

    mutated = test_clone_genome(&source);
    test_check(mutated != NULL, "allocate whitespace mutation");
    if (mutated != NULL) {
        size_t index;

        for (index = EVO_PROJECT_RECIPE_GENOME_HEADER_SIZE;
             index < EVO_PROJECT_RECIPE_GENOME_HEADER_SIZE +
                         source.canonical_json_size;
             index += 1U) {
            if (mutated[index] == (unsigned char)'\n') {
                mutated[index] = (unsigned char)' ';
                break;
            }
        }
        status = evo_project_recipe_decode(
            &config.context, mutated, source.genome_size, &decoded);
        test_check(
            status == EVO_PROJECT_RECIPE_ERROR_GENOME_NONCANONICAL &&
                decoded.private_owner == NULL,
            "semantically equivalent noncanonical JSON rejected atomically");
        evo_project_release(mutated);
    }

    mutated = test_clone_genome(&source);
    test_check(mutated != NULL, "allocate padding mutation");
    if (mutated != NULL) {
        mutated[source.genome_size - 1U] = 1U;
        status = evo_project_recipe_decode(
            &config.context, mutated, source.genome_size, &decoded);
        test_check(
            status == EVO_PROJECT_RECIPE_ERROR_GENOME_NONCANONICAL &&
                decoded.private_owner == NULL,
            "nonzero padding rejected atomically");
        evo_project_release(mutated);
    }

    mutated = test_clone_genome(&source);
    test_check(mutated != NULL, "allocate header mutation");
    if (mutated != NULL) {
        mutated[0] = (unsigned char)'X';
        status = evo_project_recipe_decode(
            &config.context, mutated, source.genome_size, &decoded);
        test_check(
            status == EVO_PROJECT_RECIPE_ERROR_GENOME_CORRUPT &&
                decoded.private_owner == NULL,
            "unknown envelope rejected atomically");
        evo_project_release(mutated);
    }

    mutated = test_clone_genome(&source);
    test_check(mutated != NULL, "allocate JSON mutation");
    if (mutated != NULL) {
        mutated[EVO_PROJECT_RECIPE_GENOME_HEADER_SIZE] = (unsigned char)'[';
        status = evo_project_recipe_decode(
            &config.context, mutated, source.genome_size, &decoded);
        test_check(
            status == EVO_PROJECT_RECIPE_ERROR_GENOME_CORRUPT &&
                decoded.private_owner == NULL,
            "corrupt JSON rejected atomically");
        evo_project_release(mutated);
    }

    mutated = test_clone_genome(&source);
    test_check(mutated != NULL, "allocate derived-field mutation");
    if (mutated != NULL) {
        const char *const needle = "\"file\":\"unit.c\",\"line\":1";
        char *field = strstr(
            (char *)mutated + EVO_PROJECT_RECIPE_GENOME_HEADER_SIZE,
            needle);

        test_check(field != NULL, "find derived source range");
        if (field != NULL) {
            field[strlen(needle) - 1U] = '2';
            status = evo_project_recipe_decode(
                &config.context, mutated, source.genome_size, &decoded);
            test_check(
                status == EVO_PROJECT_RECIPE_ERROR_GENOME_NONCANONICAL &&
                    decoded.private_owner == NULL,
                "serialized derived range cannot replace live authority");
        }
        evo_project_release(mutated);
    }

    mutated = test_clone_genome(&source);
    test_check(mutated != NULL, "allocate unknown-transform mutation");
    if (mutated != NULL) {
        char *identity = strstr(
            (char *)mutated + EVO_PROJECT_RECIPE_GENOME_HEADER_SIZE,
            "\"transformation\":{\"identity\":\"transform-a\"");

        test_check(identity != NULL, "find serialized transformation");
        if (identity != NULL) {
            identity = strstr(identity, "transform-a");
            identity[10] = 'z';
            status = evo_project_recipe_decode(
                &config.context, mutated, source.genome_size, &decoded);
            test_check(
                status == EVO_PROJECT_RECIPE_ERROR_UNKNOWN_TRANSFORMATION &&
                    decoded.private_owner == NULL,
                "serialized unknown transformation rejected atomically");
        }
        evo_project_release(mutated);
    }

    mutated = test_clone_genome(&source);
    test_check(mutated != NULL, "allocate stale-target mutation");
    if (mutated != NULL) {
        char *identity = strstr(
            (char *)mutated + EVO_PROJECT_RECIPE_GENOME_HEADER_SIZE,
            "\"target\":{\"location_identity\":\"location-a\"");

        test_check(identity != NULL, "find serialized target");
        if (identity != NULL) {
            identity = strstr(identity, "location-a");
            identity[9] = 'z';
            status = evo_project_recipe_decode(
                &config.context, mutated, source.genome_size, &decoded);
            test_check(
                status == EVO_PROJECT_RECIPE_ERROR_STALE_TARGET &&
                    decoded.private_owner == NULL,
                "serialized stale target rejected atomically");
        }
        evo_project_release(mutated);
    }

    mutated = test_clone_genome(&source);
    test_check(mutated != NULL, "allocate stale-baseline mutation");
    if (mutated != NULL) {
        char *identity = strstr(
            (char *)mutated + EVO_PROJECT_RECIPE_GENOME_HEADER_SIZE,
            "fnv1a64-v1:1020304050607080");

        test_check(identity != NULL, "find serialized baseline");
        if (identity != NULL) {
            identity[strlen("fnv1a64-v1:")] = '2';
            status = evo_project_recipe_decode(
                &config.context, mutated, source.genome_size, &decoded);
            test_check(
                status == EVO_PROJECT_RECIPE_ERROR_BASELINE_CHANGED &&
                    decoded.private_owner == NULL,
                "serialized stale baseline rejected atomically");
        }
        evo_project_release(mutated);
    }

    mutated = test_clone_genome(&source);
    test_check(mutated != NULL, "allocate payload-length mutation");
    if (mutated != NULL) {
        size_t index;

        for (index = 8U; index < EVO_PROJECT_RECIPE_GENOME_HEADER_SIZE;
             index += 1U) {
            mutated[index] = 0xffU;
        }
        status = evo_project_recipe_decode(
            &config.context, mutated, source.genome_size, &decoded);
        test_check(
            status == EVO_PROJECT_RECIPE_ERROR_GENOME_CORRUPT &&
                decoded.private_owner == NULL,
            "impossible payload length rejected atomically");
        evo_project_release(mutated);
    }

    status = evo_project_recipe_decode(
        &config.context,
        source.genome,
        EVO_PROJECT_RECIPE_GENOME_HEADER_SIZE,
        &decoded);
    test_check(
        status == EVO_PROJECT_RECIPE_ERROR_GENOME_CORRUPT &&
            decoded.private_owner == NULL,
        "truncated genome rejected atomically");
    status = evo_project_recipe_decode(
        &config.context,
        source.genome,
        config.context.limits.max_genome_bytes + 1U,
        &decoded);
    test_check(
        status == EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT &&
            decoded.private_owner == NULL,
        "oversized genome rejected before byte traversal");
    status = evo_project_recipe_decode(
        &config.context,
        (const unsigned char *)&decoded,
        sizeof(decoded),
        &decoded);
    test_check(
        status == EVO_PROJECT_RECIPE_ERROR_INVALID_ARGUMENT,
        "genome/result alias rejected before work");

    evo_project_recipe_destroy(&source);
    test_check(test_source_unchanged(fixture), "decode failures never write source");
}

static void test_expect_build_status(
    const test_fixture_t *fixture,
    const evo_project_recipe_proposal_record_t *records,
    size_t record_count,
    evo_project_recipe_status_t expected,
    const char *message)
{
    evo_project_recipe_t recipe = {0};
    evo_project_recipe_build_config_t config = test_build_config(
        fixture, records, record_count, 8192U);
    const evo_project_recipe_status_t status =
        evo_project_recipe_build(&config, &recipe);

    test_check(
        status == expected && recipe.private_owner == NULL &&
            recipe.schema_version == 0U,
        message);
    evo_project_recipe_destroy(&recipe);
}

static void test_model_failures(test_fixture_t *fixture)
{
    const evo_project_recipe_proposal_record_t missing[] = {
        {"record-b",
         "location-b",
         "transform-b",
         2U,
         sizeof(test_parameters) / sizeof(test_parameters[0]),
         test_parameters}};
    const evo_project_recipe_proposal_record_t ambiguous[] = {
        {"record-a-1", "location-a", "transform-a", 1U, 0U, NULL},
        {"record-a-2", "location-a", "transform-a", 1U, 0U, NULL},
        {"record-b",
         "location-b",
         "transform-b",
         2U,
         sizeof(test_parameters) / sizeof(test_parameters[0]),
         test_parameters}};
    const evo_project_recipe_proposal_record_t cycle[] = {
        {"record-d", "location-a", "transform-d", 1U, 0U, NULL},
        {"record-e", "location-b", "transform-e", 1U, 0U, NULL}};
    const evo_project_recipe_proposal_record_t conflict[] = {
        {"record-a", "location-a", "transform-a", 1U, 0U, NULL},
        {"record-b",
         "location-b",
         "transform-b",
         2U,
         sizeof(test_parameters) / sizeof(test_parameters[0]),
         test_parameters},
        {"record-c", "location-a", "transform-c", 1U, 0U, NULL}};
    const evo_project_recipe_proposal_record_t unknown[] = {
        {"record-unknown", "location-a", "transform-unknown", 1U, 0U, NULL}};
    const evo_project_recipe_proposal_record_t stale[] = {
        {"record-a", "location-stale", "transform-a", 1U, 0U, NULL}};
    const evo_project_recipe_proposal_record_t duplicate[] = {
        {"record-same", "location-a", "transform-a", 1U, 0U, NULL},
        {"record-same", "location-b", "transform-c", 1U, 0U, NULL}};
    evo_project_recipe_parameter_value_t invalid_parameters[] = {
        {"strategy",
         EVO_PROJECT_RECIPE_PARAMETER_CHOICE,
         0,
         false,
         "balanced"},
        {"limit", EVO_PROJECT_RECIPE_PARAMETER_INTEGER, 9, false, NULL},
        {"enabled", EVO_PROJECT_RECIPE_PARAMETER_BOOLEAN, 0, true, NULL}};
    evo_project_recipe_proposal_record_t invalid_parameter = {
        "record-b",
        "location-b",
        "transform-b",
        2U,
        sizeof(invalid_parameters) / sizeof(invalid_parameters[0]),
        invalid_parameters};
    evo_project_recipe_parameter_value_t null_identity_parameters[] = {
        {NULL, EVO_PROJECT_RECIPE_PARAMETER_INTEGER, 1, false, NULL},
        {"enabled", EVO_PROJECT_RECIPE_PARAMETER_BOOLEAN, 0, true, NULL},
        {"strategy",
         EVO_PROJECT_RECIPE_PARAMETER_CHOICE,
         0,
         false,
         "balanced"}};
    evo_project_recipe_proposal_record_t null_identity = {
        "record-b",
        "location-b",
        "transform-b",
        2U,
        sizeof(null_identity_parameters) /
            sizeof(null_identity_parameters[0]),
        null_identity_parameters};

    test_expect_build_status(
        fixture,
        missing,
        sizeof(missing) / sizeof(missing[0]),
        EVO_PROJECT_RECIPE_ERROR_DEPENDENCY_MISSING,
        "missing dependency rejected atomically");
    test_expect_build_status(
        fixture,
        ambiguous,
        sizeof(ambiguous) / sizeof(ambiguous[0]),
        EVO_PROJECT_RECIPE_ERROR_DEPENDENCY_AMBIGUOUS,
        "ambiguous dependency rejected atomically");
    test_expect_build_status(
        fixture,
        cycle,
        sizeof(cycle) / sizeof(cycle[0]),
        EVO_PROJECT_RECIPE_ERROR_DEPENDENCY_CYCLE,
        "dependency cycle rejected atomically");
    test_expect_build_status(
        fixture,
        conflict,
        sizeof(conflict) / sizeof(conflict[0]),
        EVO_PROJECT_RECIPE_ERROR_CONFLICT,
        "declared conflict rejected atomically");
    test_expect_build_status(
        fixture,
        unknown,
        sizeof(unknown) / sizeof(unknown[0]),
        EVO_PROJECT_RECIPE_ERROR_UNKNOWN_TRANSFORMATION,
        "unknown transformation rejected atomically");
    test_expect_build_status(
        fixture,
        stale,
        sizeof(stale) / sizeof(stale[0]),
        EVO_PROJECT_RECIPE_ERROR_STALE_TARGET,
        "stale target rejected atomically");
    test_expect_build_status(
        fixture,
        duplicate,
        sizeof(duplicate) / sizeof(duplicate[0]),
        EVO_PROJECT_RECIPE_ERROR_RECIPE_INVALID,
        "duplicate record identity rejected atomically");
    test_expect_build_status(
        fixture,
        &invalid_parameter,
        1U,
        EVO_PROJECT_RECIPE_ERROR_INVALID_PARAMETER,
        "out-of-range parameter rejected atomically");
    test_expect_build_status(
        fixture,
        &null_identity,
        1U,
        EVO_PROJECT_RECIPE_ERROR_INVALID_PARAMETER,
        "null parameter identity rejected atomically");
    test_check(test_source_unchanged(fixture), "model failures never write source");
}

static void test_public_preflight_and_budgets(test_fixture_t *fixture)
{
    evo_project_recipe_t recipe = {0};
    evo_project_recipe_build_config_t config = test_build_config(
        fixture,
        test_proposals,
        sizeof(test_proposals) / sizeof(test_proposals[0]),
        8192U);
    evo_project_transformation_catalogue_t invalid_catalogue = test_catalogue;
    char saved_analysis_byte;
    size_t saved_source_location_count;
    evo_project_baseline_state_t saved_state;
    size_t saved_budget;
    evo_project_recipe_status_t status;

    invalid_catalogue.schema_version = 0U;
    config.context.catalogue = &invalid_catalogue;
    status = evo_project_recipe_build(&config, &recipe);
    test_check(
        status == EVO_PROJECT_RECIPE_ERROR_CATALOGUE_INVALID &&
            recipe.private_owner == NULL,
        "invalid catalogue rejected before publication");

    config = test_build_config(
        fixture,
        test_proposals,
        sizeof(test_proposals) / sizeof(test_proposals[0]),
        8192U);
    config.genome_size = EVO_PROJECT_RECIPE_GENOME_HEADER_SIZE;
    status = evo_project_recipe_build(&config, &recipe);
    test_check(
        status == EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT,
        "undersized fixed genome rejected");
    config.genome_size = config.context.limits.max_genome_bytes + 1U;
    status = evo_project_recipe_build(&config, &recipe);
    test_check(
        status == EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT,
        "oversized fixed genome rejected");
    config.genome_size = 8192U;
    config.context.limits.max_records = 1U;
    status = evo_project_recipe_build(&config, &recipe);
    test_check(
        status == EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT,
        "record budget enforced before record traversal");

    config = test_build_config(
        fixture,
        test_proposals,
        sizeof(test_proposals) / sizeof(test_proposals[0]),
        8192U);
    saved_budget = fixture->baseline_owner.manifest.budget.max_evidence_bytes;
    fixture->baseline_owner.manifest.budget.max_evidence_bytes = 8192U;
    status = evo_project_recipe_build(&config, &recipe);
    test_check(
        status == EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT,
        "immutable manifest evidence budget caps recipe and projection");
    fixture->baseline_owner.manifest.budget.max_evidence_bytes = saved_budget;

    saved_analysis_byte = fixture->analysis.analysis_fingerprint[0];
    fixture->analysis.analysis_fingerprint[0] =
        saved_analysis_byte == 'a' ? 'b' : 'a';
    status = evo_project_recipe_build(&config, &recipe);
    test_check(
        status == EVO_PROJECT_RECIPE_ERROR_ANALYSIS_STALE,
        "tampered analysis identity rejected before traversal");
    fixture->analysis.analysis_fingerprint[0] = saved_analysis_byte;

    saved_source_location_count = fixture->analysis.source_location_count;
    fixture->analysis.source_location_count = SIZE_MAX;
    status = evo_project_recipe_build(&config, &recipe);
    test_check(
        status == EVO_PROJECT_RECIPE_ERROR_ANALYSIS_STALE,
        "tampered analysis view rejected before oversized traversal");
    fixture->analysis.source_location_count = saved_source_location_count;

    saved_state = fixture->baseline_owner.state;
    fixture->baseline_owner.state = EVO_PROJECT_BASELINE_BUILD_FAILED;
    fixture->baseline.state = EVO_PROJECT_BASELINE_BUILD_FAILED;
    status = evo_project_recipe_build(&config, &recipe);
    test_check(
        status == EVO_PROJECT_RECIPE_ERROR_BASELINE_INELIGIBLE,
        "ineligible baseline rejected");
    fixture->baseline_owner.state = saved_state;
    fixture->baseline.state = saved_state;

    config.records = (const evo_project_recipe_proposal_record_t *)&recipe;
    config.record_count = 1U;
    status = evo_project_recipe_build(&config, &recipe);
    test_check(
        status == EVO_PROJECT_RECIPE_ERROR_INVALID_ARGUMENT,
        "proposal/result alias rejected before work");

    config = test_build_config(
        fixture,
        test_proposals,
        sizeof(test_proposals) / sizeof(test_proposals[0]),
        8192U);
    status = evo_project_recipe_build(&config, &recipe);
    test_check(status == EVO_PROJECT_RECIPE_SUCCESS, "active-result fixture builds");
    status = evo_project_recipe_build(&config, &recipe);
    test_check(
        status == EVO_PROJECT_RECIPE_ERROR_RESULT_ACTIVE,
        "active build result rejected");
    evo_project_recipe_destroy(&recipe);

    test_check(
        strcmp(
            evo_project_recipe_status_name(
                EVO_PROJECT_RECIPE_ERROR_GENOME_NONCANONICAL),
            "genome-noncanonical") == 0 &&
            strcmp(
                evo_project_recipe_status_name(
                    (evo_project_recipe_status_t)99),
                "state") == 0,
        "status names are stable");
    test_check(test_source_unchanged(fixture), "preflight failures never write source");
}

int main(void)
{
    test_fixture_t fixture = {0};

    test_signed_integer_boundaries();
    test_check(test_fixture_prepare(&fixture), "prepare immutable fixture");
    if (fixture.baseline.private_owner == NULL) {
        test_fixture_destroy(&fixture);
        return EXIT_FAILURE;
    }
    test_success_replay_and_projection(&fixture);
    test_decode_failures(&fixture);
    test_model_failures(&fixture);
    test_public_preflight_and_budgets(&fixture);
    test_fixture_destroy(&fixture);
    return test_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
