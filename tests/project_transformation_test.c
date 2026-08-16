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
#include "internal/project_transformation.h"

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

enum test_target_index {
    TEST_ASSIGNMENT = 0,
    TEST_CONDITION = 1,
    TEST_SHIFT = 2,
    TEST_TARGET_COUNT = 3
};

typedef enum test_provider_scenario {
    TEST_PROVIDER_NORMAL = 0,
    TEST_PROVIDER_NEGATIVE = 1,
    TEST_PROVIDER_MALFORMED = 2,
    TEST_PROVIDER_MACRO = 3,
    TEST_PROVIDER_COMMENT = 4,
    TEST_PROVIDER_EXTENSION = 5,
    TEST_PROVIDER_ALIAS = 6,
    TEST_PROVIDER_AMBIGUOUS = 7,
    TEST_PROVIDER_ERROR = 8,
    TEST_PROVIDER_TAMPER = 9,
    TEST_PROVIDER_PREPROCESSOR = 10,
    TEST_PROVIDER_LITERAL_MISMATCH = 11,
    TEST_PROVIDER_PARTIAL_TARGET = 12,
    TEST_PROVIDER_NON_IDENTIFIER_TOKEN = 13
} test_provider_scenario_t;

typedef struct test_fixture {
    char directory[256];
    char source_path[320];
    unsigned char *source;
    size_t source_size;
    bool already_satisfied;
    char *permitted_roots[1];
    evo_project_file_record_t file;
    evo_project_baseline_owner_t baseline_owner;
    evo_project_baseline_t baseline;
    evo_project_source_location_record_t locations[TEST_TARGET_COUNT];
    evo_project_optimization_record_t optimizations[TEST_TARGET_COUNT];
    evo_project_opportunity_record_t opportunities[TEST_TARGET_COUNT];
    evo_project_analysis_owner_t analysis_owner;
    evo_project_analysis_t analysis;
    evo_project_transformation_registry_t registry;
    evo_project_recipe_t recipe;
    size_t target_starts[TEST_TARGET_COUNT];
    size_t target_ends[TEST_TARGET_COUNT];
} test_fixture_t;

typedef struct test_provider_context {
    test_fixture_t *fixture;
    test_provider_scenario_t scenario;
    size_t calls;
    bool request_invalid;
} test_provider_context_t;

static int test_failures = 0;

static const char *const test_location_identities[TEST_TARGET_COUNT] = {
    "location-assignment", "location-condition", "location-shift"};
static const char *const test_record_identities[TEST_TARGET_COUNT] = {
    "record-assignment", "record-condition", "record-shift"};
static const char *const test_transformation_identities[TEST_TARGET_COUNT] = {
    "catalyst.evo.c.assignment-to-compound",
    "catalyst.evo.c.double-negation-condition",
    "catalyst.evo.c.unsigned-multiply-to-shift"};

static const evo_project_recipe_parameter_value_t test_assignment_parameters[] = {
    {"operator", EVO_PROJECT_RECIPE_PARAMETER_CHOICE, 0, false, "add"}};
static const evo_project_recipe_parameter_value_t test_condition_parameters[] = {
    {"context", EVO_PROJECT_RECIPE_PARAMETER_CHOICE, 0, false, "if"}};
static const evo_project_recipe_parameter_value_t test_shift_parameters[] = {
    {"maximum-shift", EVO_PROJECT_RECIPE_PARAMETER_INTEGER, 63, false, NULL}};

static const evo_project_recipe_proposal_record_t test_proposals[] = {
    {
        "record-shift",
        "location-shift",
        "catalyst.evo.c.unsigned-multiply-to-shift",
        1U,
        sizeof(test_shift_parameters) / sizeof(test_shift_parameters[0]),
        test_shift_parameters,
    },
    {
        "record-condition",
        "location-condition",
        "catalyst.evo.c.double-negation-condition",
        1U,
        sizeof(test_condition_parameters) / sizeof(test_condition_parameters[0]),
        test_condition_parameters,
    },
    {
        "record-assignment",
        "location-assignment",
        "catalyst.evo.c.assignment-to-compound",
        1U,
        sizeof(test_assignment_parameters) /
            sizeof(test_assignment_parameters[0]),
        test_assignment_parameters,
    }};

static void test_check(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(
            stderr, "project transformation test failure: %s\n", message);
        test_failures += 1;
    }
}

static bool test_json_valid(const char *text, size_t size)
{
    const size_t capacity = 4096U;
    evo_project_json_token_t *tokens = evo_project_allocate_zeroed(
        capacity, sizeof(*tokens));
    size_t count = 0U;
    bool valid = false;

    if (tokens != NULL) {
        valid = evo_project_json_parse(
                    text, size, tokens, capacity, 32U, &count) ==
                    EVO_PROJECT_JSON_SUCCESS &&
                count > 0U && tokens[0].type == EVO_PROJECT_JSON_OBJECT &&
                evo_project_json_next(tokens, count, 0U) == count;
    }
    evo_project_release(tokens);
    return valid;
}

static bool test_write_all(
    int file_descriptor,
    const unsigned char *bytes,
    size_t size)
{
    size_t position = 0U;

    while (position < size) {
        const ssize_t count = write(
            file_descriptor, bytes + position, size - position);

        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return false;
        }
        position += (size_t)count;
    }
    return true;
}

static bool test_read_file(
    const char *path,
    unsigned char **bytes,
    size_t *size)
{
    struct stat metadata;
    int file_descriptor = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    size_t position = 0U;

    if (file_descriptor < 0 || fstat(file_descriptor, &metadata) != 0 ||
        !S_ISREG(metadata.st_mode) || metadata.st_size <= 0 ||
        (uintmax_t)metadata.st_size > (uintmax_t)SIZE_MAX) {
        if (file_descriptor >= 0) {
            (void)close(file_descriptor);
        }
        return false;
    }
    *size = (size_t)metadata.st_size;
    if (*size == SIZE_MAX) {
        (void)close(file_descriptor);
        return false;
    }
    *bytes = evo_project_allocate_zeroed(*size + 1U, sizeof(**bytes));
    if (*bytes == NULL) {
        (void)close(file_descriptor);
        return false;
    }
    while (position < *size) {
        const ssize_t count = read(
            file_descriptor, *bytes + position, *size - position);

        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            evo_project_release(*bytes);
            *bytes = NULL;
            (void)close(file_descriptor);
            return false;
        }
        position += (size_t)count;
    }
    if (close(file_descriptor) != 0) {
        evo_project_release(*bytes);
        *bytes = NULL;
        return false;
    }
    (*bytes)[*size] = '\0';
    return true;
}

static bool test_load_source(
    bool already_satisfied,
    unsigned char **bytes,
    size_t *size)
{
    char path[1024];
    const int written = evo_project_format(
        path,
        sizeof(path),
        "%s/tests/fixtures/project-transformation/%s.c",
        EVO_TEST_SOURCE_DIR,
        already_satisfied ? "after" : "before");

    return written > 0 && (size_t)written < sizeof(path) &&
           test_read_file(path, bytes, size);
}

static bool test_matches_golden(
    const char *relative_path,
    const char *bytes,
    size_t size)
{
    char path[1024];
    unsigned char *golden = NULL;
    size_t golden_size = 0U;
    const int written = evo_project_format(
        path, sizeof(path), "%s/%s", EVO_TEST_SOURCE_DIR, relative_path);
    bool matches = false;

    if (written > 0 && (size_t)written < sizeof(path) &&
        test_read_file(path, &golden, &golden_size)) {
        matches = golden_size == size && memcmp(golden, bytes, size) == 0;
    }
    evo_project_release(golden);
    return matches;
}

static size_t test_find_bytes(
    const unsigned char *source,
    size_t source_size,
    const char *needle,
    size_t start)
{
    const size_t needle_size = strlen(needle);
    size_t index;

    if (needle_size == 0U || needle_size > source_size ||
        start > source_size - needle_size) {
        return SIZE_MAX;
    }
    for (index = start; index <= source_size - needle_size; index += 1U) {
        size_t offset;
        bool match = true;

        for (offset = 0U; offset < needle_size; offset += 1U) {
            if (source[index + offset] != (unsigned char)needle[offset]) {
                match = false;
                break;
            }
        }
        if (match) {
            return index;
        }
    }
    return SIZE_MAX;
}

static bool test_position_from_offset(
    const unsigned char *source,
    size_t source_size,
    size_t offset,
    uint32_t *line,
    uint32_t *column)
{
    size_t index;

    if (offset > source_size) {
        return false;
    }
    *line = 1U;
    *column = 1U;
    for (index = 0U; index < offset; index += 1U) {
        if (source[index] == (unsigned char)'\n') {
            *line += 1U;
            *column = 1U;
        } else {
            *column += 1U;
        }
    }
    return true;
}

static evo_project_transformation_limits_t test_transformation_limits(void)
{
    evo_project_transformation_limits_t limits = {0};

    limits.max_string_bytes = 256U;
    limits.max_path_bytes = 256U;
    limits.max_source_bytes = 8192U;
    limits.max_replacement_bytes = 1024U;
    limits.max_parameters = 16U;
    limits.max_registry_bytes = 32768U;
    limits.max_application_bytes = 32768U;
    limits.max_audit_bytes = 32768U;
    limits.max_total_bytes = 131072U;
    return limits;
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
    limits.max_json_tokens = 4096U;
    limits.max_json_depth = 24U;
    limits.max_genome_bytes = 32768U;
    limits.max_audit_bytes = 32768U;
    limits.max_total_bytes = 131072U;
    return limits;
}

static bool test_prepare_locations(test_fixture_t *fixture)
{
    const char *needles_before[TEST_TARGET_COUNT] = {
        "total = total + ready", "!!ready", "value * 8U"};
    const char *needles_after[TEST_TARGET_COUNT] = {
        "total += ready", "ready", "(value << 3)"};
    const char *const *needles =
        fixture->already_satisfied ? needles_after : needles_before;
    size_t index;

    for (index = 0U; index < TEST_TARGET_COUNT; index += 1U) {
        const size_t start = test_find_bytes(
            fixture->source, fixture->source_size, needles[index], 0U);
        const size_t end = start == SIZE_MAX ? SIZE_MAX : start + strlen(needles[index]);
        evo_project_source_location_record_t *location =
            &fixture->locations[index];

        if (start == SIZE_MAX || end > fixture->source_size ||
            !test_position_from_offset(
                fixture->source,
                fixture->source_size,
                start,
                &location->line,
                &location->column) ||
            !test_position_from_offset(
                fixture->source,
                fixture->source_size,
                end,
                &location->end_line,
                &location->end_column)) {
            return false;
        }
        fixture->target_starts[index] = start;
        fixture->target_ends[index] = end;
        location->identity = test_location_identities[index];
        location->file = "fixture.c";
        location->kind = EVO_PROJECT_LOCATION_SPELLING;
        location->spelling_identity = NULL;
        fixture->optimizations[index].identity =
            index == TEST_ASSIGNMENT
                ? "compiler-assignment"
            : index == TEST_CONDITION ? "compiler-condition"
                                      : "compiler-shift";
        fixture->optimizations[index].pass_name = "fixture-pass";
        fixture->optimizations[index].function_identity = "function-fixture";
        fixture->optimizations[index].location_identity = location->identity;
        fixture->optimizations[index].message = "candidate retained";
        fixture->optimizations[index].disposition =
            EVO_PROJECT_OPTIMIZATION_MISSED;
        fixture->opportunities[index].rank = index + 1U;
        fixture->opportunities[index].location_identity = location->identity;
        fixture->opportunities[index].missed_optimization_count = 1U;
        fixture->opportunities[index].runtime_evidence_present = false;
        fixture->opportunities[index].runtime_sample_count = 0U;
    }
    return true;
}

static bool test_fixture_prepare(
    test_fixture_t *fixture,
    bool already_satisfied)
{
    char temporary_template[] = "/tmp/evo-project-transform-XXXXXX";
    char *directory;
    int file_descriptor;
    int written;
    evo_project_fingerprint_t fingerprint;
    evo_project_recipe_build_config_t recipe_config = {0};
    evo_project_transformation_limits_t transformation_limits =
        test_transformation_limits();
    evo_project_transformation_status_t transformation_status;
    evo_project_recipe_status_t recipe_status;

    fixture->already_satisfied = already_satisfied;
    if (!test_load_source(
            already_satisfied, &fixture->source, &fixture->source_size) ||
        !test_prepare_locations(fixture)) {
        return false;
    }
    directory = mkdtemp(temporary_template);
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
        "%s/fixture.c",
        fixture->directory);
    if (written <= 0 || (size_t)written >= sizeof(fixture->source_path)) {
        return false;
    }
    file_descriptor = open(
        fixture->source_path,
        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
        0600);
    if (file_descriptor < 0 ||
        !test_write_all(file_descriptor, fixture->source, fixture->source_size) ||
        fsync(file_descriptor) != 0 || close(file_descriptor) != 0 ||
        chmod(fixture->source_path, 0444) != 0 ||
        chmod(fixture->directory, 0500) != 0) {
        if (file_descriptor >= 0) {
            (void)close(file_descriptor);
        }
        return false;
    }
    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_bytes(
        &fingerprint, fixture->source, fixture->source_size);
    fixture->permitted_roots[0] = "fixture.c";
    fixture->file.path = "fixture.c";
    fixture->file.size = fixture->source_size;
    fixture->file.source_mode = 0644U;
    fixture->file.content_fingerprint = fingerprint.value;
    fixture->baseline_owner.manifest.permitted_roots = fixture->permitted_roots;
    fixture->baseline_owner.manifest.permitted_root_count = 1U;
    fixture->baseline_owner.manifest.budget.max_files = 4U;
    fixture->baseline_owner.manifest.budget.max_file_bytes = 8192U;
    fixture->baseline_owner.manifest.budget.max_total_bytes = 8192U;
    fixture->baseline_owner.manifest.budget.max_path_bytes = 256U;
    fixture->baseline_owner.manifest.budget.max_evidence_bytes = 131072U;
    fixture->baseline_owner.snapshot_path = fixture->directory;
    fixture->baseline_owner.files = &fixture->file;
    fixture->baseline_owner.file_count = 1U;
    fixture->baseline_owner.total_file_bytes = fixture->source_size;
    fixture->baseline_owner.baseline_fingerprint =
        already_satisfied ? UINT64_C(0x1020304050607081)
                          : UINT64_C(0x1020304050607080);
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
    fixture->analysis_owner.source_locations = fixture->locations;
    fixture->analysis_owner.source_location_count = TEST_TARGET_COUNT;
    fixture->analysis_owner.optimization_records = fixture->optimizations;
    fixture->analysis_owner.optimization_record_count = TEST_TARGET_COUNT;
    fixture->analysis_owner.opportunities = fixture->opportunities;
    fixture->analysis_owner.opportunity_count = TEST_TARGET_COUNT;
    fixture->analysis_owner.analysis_fingerprint =
        already_satisfied ? UINT64_C(0x8877665544332212)
                          : UINT64_C(0x8877665544332211);
    fixture->analysis_owner.committed = true;
    fixture->analysis.schema_version = EVO_PROJECT_ANALYSIS_SCHEMA_VERSION;
    fixture->analysis.baseline_fingerprint =
        fixture->baseline.baseline_fingerprint;
    evo_project_fingerprint_format(
        fixture->analysis_owner.analysis_fingerprint,
        fixture->analysis.analysis_fingerprint);
    fixture->analysis.source_location_count = TEST_TARGET_COUNT;
    fixture->analysis.source_locations = fixture->locations;
    fixture->analysis.optimization_record_count = TEST_TARGET_COUNT;
    fixture->analysis.optimization_records = fixture->optimizations;
    fixture->analysis.opportunity_count = TEST_TARGET_COUNT;
    fixture->analysis.opportunities = fixture->opportunities;
    fixture->analysis.projection_complete = true;
    fixture->analysis.probabilistic_authority = false;
    fixture->analysis.private_owner = &fixture->analysis_owner;
    transformation_status = evo_project_transformation_registry_open(
        &transformation_limits, &fixture->registry);
    if (transformation_status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        return false;
    }
    recipe_config.context.baseline = &fixture->baseline;
    recipe_config.context.analysis = &fixture->analysis;
    recipe_config.context.catalogue = fixture->registry.recipe_catalogue;
    recipe_config.context.limits = test_recipe_limits();
    recipe_config.record_count =
        sizeof(test_proposals) / sizeof(test_proposals[0]);
    recipe_config.records = test_proposals;
    recipe_config.genome_size = 32768U;
    recipe_status = evo_project_recipe_build(&recipe_config, &fixture->recipe);
    return recipe_status == EVO_PROJECT_RECIPE_SUCCESS;
}

static void test_fixture_destroy(test_fixture_t *fixture)
{
    evo_project_transformation_application_t inactive = {0};

    evo_project_transformation_application_destroy(&inactive);
    evo_project_recipe_destroy(&fixture->recipe);
    evo_project_transformation_registry_destroy(&fixture->registry);
    if (fixture->directory[0] != '\0') {
        (void)chmod(fixture->directory, 0700);
    }
    if (fixture->source_path[0] != '\0') {
        (void)chmod(fixture->source_path, 0600);
        (void)unlink(fixture->source_path);
    }
    if (fixture->directory[0] != '\0') {
        (void)rmdir(fixture->directory);
    }
    evo_project_release(fixture->source);
    *fixture = (test_fixture_t){0};
}

static int test_target_for_record(const char *record_identity)
{
    size_t index;

    for (index = 0U; index < TEST_TARGET_COUNT; index += 1U) {
        if (strcmp(record_identity, test_record_identities[index]) == 0) {
            return (int)index;
        }
    }
    return -1;
}

static void test_set_range(
    evo_project_transformation_byte_range_t *range,
    size_t start,
    const char *text)
{
    range->start = start;
    range->end = start + strlen(text);
}

static bool test_fill_assignment_ast(
    test_provider_context_t *context,
    evo_project_transformation_ast_result_t *result)
{
    test_fixture_t *fixture = context->fixture;
    const size_t target_start = fixture->target_starts[TEST_ASSIGNMENT];
    const size_t target_end = fixture->target_ends[TEST_ASSIGNMENT];
    const size_t primary = test_find_bytes(
        fixture->source, fixture->source_size, "total", target_start);
    size_t duplicate;
    size_t operand;

    if (primary == SIZE_MAX) {
        return false;
    }
    duplicate = fixture->already_satisfied
                    ? SIZE_MAX
                    : test_find_bytes(
                          fixture->source,
                          fixture->source_size,
                          "total",
                          primary + strlen("total"));
    operand = test_find_bytes(
        fixture->source, fixture->source_size, "ready", primary);
    if (operand == SIZE_MAX ||
        (!fixture->already_satisfied && duplicate == SIZE_MAX)) {
        return false;
    }
    result->form = fixture->already_satisfied
                       ? EVO_PROJECT_AST_ASSIGNMENT_COMPOUND
                       : EVO_PROJECT_AST_ASSIGNMENT_BINARY;
    result->operator_kind = EVO_PROJECT_TRANSFORMATION_OPERATOR_ADD;
    result->target = (evo_project_transformation_byte_range_t){
        target_start, target_end};
    test_set_range(&result->primary, primary, "total");
    test_set_range(&result->operand, operand, "ready");
    result->primary_declaration_identity = "declaration-total";
    result->primary_plain_identifier = true;
    result->result_type_matches_primary = true;
    if (!fixture->already_satisfied) {
        test_set_range(&result->duplicate_primary, duplicate, "total");
        result->duplicate_declaration_identity = "declaration-total";
    }
    if (context->scenario == TEST_PROVIDER_NEGATIVE) {
        result->duplicate_declaration_identity = "declaration-other";
    }
    return true;
}

static bool test_fill_condition_ast(
    test_provider_context_t *context,
    evo_project_transformation_ast_result_t *result)
{
    test_fixture_t *fixture = context->fixture;
    const size_t target_start = fixture->target_starts[TEST_CONDITION];
    const size_t target_end = fixture->target_ends[TEST_CONDITION];
    const size_t operand = test_find_bytes(
        fixture->source, fixture->source_size, "ready", target_start);

    if (operand == SIZE_MAX) {
        return false;
    }
    result->form = fixture->already_satisfied
                       ? EVO_PROJECT_AST_SCALAR_CONDITION
                       : EVO_PROJECT_AST_DOUBLE_NEGATED_CONDITION;
    result->condition_context = EVO_PROJECT_TRANSFORMATION_CONDITION_IF;
    result->target = (evo_project_transformation_byte_range_t){
        target_start, target_end};
    test_set_range(&result->operand, operand, "ready");
    result->scalar_operand = context->scenario != TEST_PROVIDER_NEGATIVE;
    return true;
}

static bool test_fill_shift_ast(
    test_provider_context_t *context,
    evo_project_transformation_ast_result_t *result)
{
    test_fixture_t *fixture = context->fixture;
    const size_t target_start = fixture->target_starts[TEST_SHIFT];
    const size_t target_end = fixture->target_ends[TEST_SHIFT];
    const size_t primary = test_find_bytes(
        fixture->source, fixture->source_size, "value", target_start);
    const char *literal_text = fixture->already_satisfied ? "3" : "8U";
    const size_t literal = test_find_bytes(
        fixture->source, fixture->source_size, literal_text, primary);

    if (primary == SIZE_MAX || literal == SIZE_MAX) {
        return false;
    }
    result->form = fixture->already_satisfied
                       ? EVO_PROJECT_AST_UNSIGNED_SHIFT_POWER_OF_TWO
                       : EVO_PROJECT_AST_UNSIGNED_MULTIPLY_POWER_OF_TWO;
    result->operator_kind = fixture->already_satisfied
                                ? EVO_PROJECT_TRANSFORMATION_OPERATOR_SHIFT_LEFT
                                : EVO_PROJECT_TRANSFORMATION_OPERATOR_MULTIPLY;
    result->target = (evo_project_transformation_byte_range_t){
        target_start, target_end};
    test_set_range(&result->primary, primary, "value");
    test_set_range(&result->literal, literal, literal_text);
    result->literal_value = fixture->already_satisfied ? UINT64_C(3)
                                                       : UINT64_C(8);
    result->result_width_bits = 32U;
    result->result_unsigned_integer =
        context->scenario != TEST_PROVIDER_NEGATIVE;
    result->result_type_matches_primary = true;
    return true;
}

static bool test_mutate_snapshot(test_fixture_t *fixture)
{
    int file_descriptor;
    const unsigned char changed = (unsigned char)'X';

    if (chmod(fixture->directory, 0700) != 0 ||
        chmod(fixture->source_path, 0600) != 0) {
        return false;
    }
    file_descriptor = open(
        fixture->source_path,
        O_WRONLY | O_NOFOLLOW | O_CLOEXEC);
    if (file_descriptor < 0 || write(file_descriptor, &changed, 1U) != 1 ||
        fsync(file_descriptor) != 0 || close(file_descriptor) != 0 ||
        chmod(fixture->source_path, 0444) != 0 ||
        chmod(fixture->directory, 0500) != 0) {
        if (file_descriptor >= 0) {
            (void)close(file_descriptor);
        }
        return false;
    }
    return true;
}

static evo_project_transformation_status_t test_ast_provider(
    const evo_project_transformation_request_t *request,
    void *opaque_context,
    evo_project_transformation_ast_result_t *result)
{
    test_provider_context_t *context = opaque_context;
    const int target_index = request == NULL || request->record_identity == NULL
                                 ? -1
                                 : test_target_for_record(request->record_identity);
    bool filled = false;

    if (context == NULL || request == NULL || result == NULL ||
        context->fixture == NULL) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_PROVIDER;
    }
    context->calls += 1U;
    context->request_invalid =
        request->schema_version !=
            EVO_PROJECT_TRANSFORMATION_PROVIDER_CONTRACT_VERSION ||
        request->baseline_fingerprint == NULL ||
        request->analysis_fingerprint == NULL ||
        request->recipe_fingerprint == NULL || request->snapshot_path == NULL ||
        request->target == NULL || request->transformation_identity == NULL ||
        request->source_size != context->fixture->source_size ||
        request->source_fingerprint == NULL || request->network_access ||
        target_index < 0;
    if (context->scenario == TEST_PROVIDER_ERROR) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_PROVIDER;
    }
    *result = (evo_project_transformation_ast_result_t){0};
    result->schema_version = EVO_PROJECT_TRANSFORMATION_AST_SCHEMA_VERSION;
    result->completed = true;
    result->location_identity = request->target->location_identity;
    result->file = request->target->file;
    if (target_index == TEST_ASSIGNMENT) {
        filled = test_fill_assignment_ast(context, result);
    } else if (target_index == TEST_CONDITION) {
        filled = test_fill_condition_ast(context, result);
    } else if (target_index == TEST_SHIFT) {
        filled = test_fill_shift_ast(context, result);
    }
    if (!filled) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_PROVIDER;
    }
    if (context->scenario == TEST_PROVIDER_MALFORMED) {
        result->target.end = context->fixture->source_size + 1U;
    } else if (context->scenario == TEST_PROVIDER_MACRO) {
        result->contains_macro = true;
    } else if (context->scenario == TEST_PROVIDER_COMMENT) {
        result->contains_comment = true;
    } else if (context->scenario == TEST_PROVIDER_PREPROCESSOR) {
        result->contains_preprocessor = true;
    } else if (context->scenario == TEST_PROVIDER_EXTENSION) {
        result->language_extension = true;
    } else if (context->scenario == TEST_PROVIDER_ALIAS) {
        result->alias_assumption_required = true;
    } else if (context->scenario == TEST_PROVIDER_AMBIGUOUS) {
        result->ambiguous_target = true;
    } else if (context->scenario == TEST_PROVIDER_LITERAL_MISMATCH) {
        result->literal_value += UINT64_C(8);
    } else if (context->scenario == TEST_PROVIDER_PARTIAL_TARGET) {
        if (target_index == TEST_ASSIGNMENT || target_index == TEST_CONDITION) {
            result->operand.end -= 1U;
        } else {
            result->literal.end -= 1U;
        }
    } else if (context->scenario == TEST_PROVIDER_NON_IDENTIFIER_TOKEN &&
               target_index == TEST_ASSIGNMENT) {
        result->primary.end += 1U;
        result->duplicate_primary.end += 1U;
    } else if (context->scenario == TEST_PROVIDER_TAMPER &&
               !test_mutate_snapshot(context->fixture)) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_PROVIDER;
    }
    return EVO_PROJECT_TRANSFORMATION_SUCCESS;
}

static evo_project_transformation_apply_config_t test_apply_config(
    test_fixture_t *fixture,
    int target_index,
    evo_project_transformation_limits_t limits,
    test_provider_context_t *provider_context)
{
    evo_project_transformation_apply_config_t config = {0};

    config.baseline = &fixture->baseline;
    config.analysis = &fixture->analysis;
    config.recipe = &fixture->recipe;
    config.registry = &fixture->registry;
    config.record_identity = test_record_identities[target_index];
    config.provider_identity = "fixture-clang-ast-provider";
    config.provider_version = 1U;
    config.clang_identity = "clang-18.1.3-fixture";
    config.limits = limits;
    config.provider = test_ast_provider;
    config.provider_context = provider_context;
    return config;
}

static evo_project_transformation_status_t test_apply(
    test_fixture_t *fixture,
    int target_index,
    test_provider_scenario_t scenario,
    evo_project_transformation_limits_t limits,
    evo_project_transformation_application_t *application,
    test_provider_context_t *provider_context)
{
    evo_project_transformation_apply_config_t config;

    *provider_context = (test_provider_context_t){fixture, scenario, 0U, false};
    config = test_apply_config(
        fixture, target_index, limits, provider_context);
    return evo_project_transformation_apply(&config, application);
}

static bool test_source_unchanged(const test_fixture_t *fixture)
{
    unsigned char *bytes = NULL;
    size_t size = 0U;
    struct stat metadata;
    bool unchanged = test_read_file(fixture->source_path, &bytes, &size) &&
                     stat(fixture->source_path, &metadata) == 0 &&
                     (unsigned int)(metadata.st_mode & (mode_t)07777) == 0444U &&
                     size == fixture->source_size &&
                     memcmp(bytes, fixture->source, size) == 0;

    evo_project_release(bytes);
    return unchanged;
}

static bool test_load_after(unsigned char **bytes, size_t *size)
{
    char path[1024];
    const int written = evo_project_format(
        path,
        sizeof(path),
        "%s/tests/fixtures/project-transformation/after.c",
        EVO_TEST_SOURCE_DIR);

    return written > 0 && (size_t)written < sizeof(path) &&
           test_read_file(path, bytes, size);
}

static bool test_compose_applications(
    const test_fixture_t *fixture,
    const evo_project_transformation_application_t *applications,
    size_t application_count,
    unsigned char **output,
    size_t *output_size)
{
    size_t order[TEST_TARGET_COUNT] = {0U, 1U, 2U};
    size_t index;
    size_t source_position = 0U;
    size_t output_position = 0U;
    size_t capacity = fixture->source_size;

    for (index = 0U; index < application_count; index += 1U) {
        const evo_project_transformation_application_t *application =
            &applications[index];

        if (application->disposition != EVO_PROJECT_TRANSFORMATION_EDIT ||
            application->edit.before_size < application->edit.replacement_size) {
            const size_t growth = application->edit.replacement_size >
                                          application->edit.before_size
                                      ? application->edit.replacement_size -
                                            application->edit.before_size
                                      : 0U;
            if (growth > SIZE_MAX - capacity) {
                return false;
            }
            capacity += growth;
        }
    }
    for (index = 0U; index < application_count; index += 1U) {
        size_t right;

        for (right = index + 1U; right < application_count; right += 1U) {
            if (applications[order[right]].edit.before_start <
                applications[order[index]].edit.before_start) {
                const size_t temporary = order[index];

                order[index] = order[right];
                order[right] = temporary;
            }
        }
    }
    *output = evo_project_allocate_zeroed(capacity + 1U, sizeof(**output));
    if (*output == NULL) {
        return false;
    }
    for (index = 0U; index < application_count; index += 1U) {
        const evo_project_transformation_application_t *application =
            &applications[order[index]];
        const size_t prefix = application->edit.before_start - source_position;

        if (application->edit.before_start < source_position ||
            application->edit.before_end > fixture->source_size) {
            evo_project_release(*output);
            *output = NULL;
            return false;
        }
        (void)memcpy(
            *output + output_position,
            fixture->source + source_position,
            prefix);
        output_position += prefix;
        (void)memcpy(
            *output + output_position,
            application->edit.replacement_text,
            application->edit.replacement_size);
        output_position += application->edit.replacement_size;
        source_position = application->edit.before_end;
    }
    (void)memcpy(
        *output + output_position,
        fixture->source + source_position,
        fixture->source_size - source_position);
    output_position += fixture->source_size - source_position;
    (*output)[output_position] = '\0';
    *output_size = output_position;
    return true;
}

static void test_registry_projection(test_fixture_t *fixture)
{
    evo_project_transformation_registry_t replay = {0};
    evo_project_transformation_registry_t limited = {0};
    evo_project_transformation_limits_t limits = test_transformation_limits();
    evo_project_transformation_status_t status;
    size_t index;

    test_check(
        fixture->registry.schema_version ==
                EVO_PROJECT_TRANSFORMATION_REGISTRY_SCHEMA_VERSION &&
            fixture->registry.recipe_catalogue != NULL &&
            fixture->registry.recipe_catalogue->entry_count ==
                TEST_TARGET_COUNT &&
            fixture->registry.capability_count == TEST_TARGET_COUNT &&
            fixture->registry.canonical_json != NULL &&
            fixture->registry.canonical_json_size ==
                strlen(fixture->registry.canonical_json) &&
            test_json_valid(
                fixture->registry.canonical_json,
                fixture->registry.canonical_json_size) &&
            fixture->registry.audit_markdown != NULL &&
            fixture->registry.audit_markdown_size ==
                strlen(fixture->registry.audit_markdown) &&
            fixture->registry.projection_complete &&
            !fixture->registry.probabilistic_authority,
        "catalogue exposes a complete deterministic registry");
    test_check(
        strstr(
            fixture->registry.canonical_json,
            "\"schema\":\"catalyst.evo-c-transformation-catalogue.v1\"") !=
                NULL &&
            strstr(
                fixture->registry.canonical_json,
                "\"reference_form\":"
                "\"stable-capability-arrays-and-direct-dispatch\"") != NULL &&
            strstr(
                fixture->registry.audit_markdown,
                "No cache, index, filter, or probabilistic") != NULL,
        "catalogue projections state the exact HRA authority");
    test_check(
        test_matches_golden(
            "tests/fixtures/project-transformation/catalogue-golden-v1.json",
            fixture->registry.canonical_json,
            fixture->registry.canonical_json_size),
        "canonical catalogue JSON exactly matches the retained golden");
    for (index = 0U; index < TEST_TARGET_COUNT; index += 1U) {
        const evo_project_transformation_capability_t *capability =
            &fixture->registry.capabilities[index];

        test_check(
            strcmp(capability->identity, test_transformation_identities[index]) ==
                    0 &&
                capability->implementation_version == 1U &&
                capability->provider_contract_version ==
                    EVO_PROJECT_TRANSFORMATION_PROVIDER_CONTRACT_VERSION &&
                capability->ast_form_count == 2U &&
                capability->semantic_assumption_count > 0U &&
                capability->validation_obligation_count == 4U &&
                !capability->comments_supported &&
                !capability->macros_supported &&
                !capability->language_extensions_supported &&
                !capability->alias_assumptions_supported,
            "each built-in capability has stable policy metadata");
    }
    status = evo_project_transformation_registry_open(&limits, &replay);
    test_check(
        status == EVO_PROJECT_TRANSFORMATION_SUCCESS &&
            replay.canonical_json_size ==
                fixture->registry.canonical_json_size &&
            replay.audit_markdown_size ==
                fixture->registry.audit_markdown_size &&
            strcmp(replay.canonical_json, fixture->registry.canonical_json) ==
                0 &&
            strcmp(
                replay.audit_markdown, fixture->registry.audit_markdown) == 0,
        "catalogue serialization replays byte-for-byte");
    evo_project_transformation_registry_destroy(&replay);

    limits.max_registry_bytes = 1U;
    status = evo_project_transformation_registry_open(&limits, &limited);
    test_check(
        status == EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT &&
            limited.private_owner == NULL,
        "catalogue generation honors its exact byte budget");
    evo_project_transformation_registry_destroy(&limited);
}

static void test_application_contract(
    const test_fixture_t *fixture,
    int target_index,
    const char *before,
    const char *replacement,
    const evo_project_transformation_application_t *application)
{
    test_check(
        application->schema_version ==
                EVO_PROJECT_TRANSFORMATION_APPLICATION_SCHEMA_VERSION &&
            strcmp(
                application->baseline_fingerprint,
                fixture->baseline.baseline_fingerprint) == 0 &&
            strcmp(
                application->analysis_fingerprint,
                fixture->analysis.analysis_fingerprint) == 0 &&
            strcmp(
                application->recipe_fingerprint,
                fixture->recipe.recipe_fingerprint) == 0 &&
            strcmp(
                application->catalogue_identity,
                fixture->registry.recipe_catalogue->identity) == 0 &&
            application->catalogue_version == 1U &&
            strcmp(
                application->record_identity,
                test_record_identities[target_index]) == 0 &&
            strcmp(
                application->transformation_identity,
                test_transformation_identities[target_index]) == 0 &&
            application->transformation_version == 1U,
        "application retains catalogue, recipe, and record provenance");
    if (target_index == TEST_ASSIGNMENT) {
        test_check(
            strcmp(
                application->application_fingerprint,
                "fnv1a64-v1:038db04907776b14") == 0 &&
                test_matches_golden(
                    "tests/fixtures/project-transformation/"
                    "application-golden-v1.json",
                    application->canonical_json,
                    application->canonical_json_size),
            "canonical assignment application exactly matches the retained golden");
    }
    test_check(
        application->parameter_count == 1U &&
            application->parameters != NULL &&
            strcmp(
                application->parameters[0].identity,
                target_index == TEST_ASSIGNMENT
                    ? "operator"
                : target_index == TEST_CONDITION ? "context"
                                                 : "maximum-shift") == 0 &&
            (target_index == TEST_SHIFT
                 ? application->parameters[0].kind ==
                           EVO_PROJECT_RECIPE_PARAMETER_INTEGER &&
                       application->parameters[0].integer_value == 63
                 : application->parameters[0].kind ==
                           EVO_PROJECT_RECIPE_PARAMETER_CHOICE &&
                       strcmp(
                           application->parameters[0].choice_value,
                           target_index == TEST_ASSIGNMENT ? "add" : "if") ==
                           0),
        "application owns the selected transformation parameters");
    test_check(
        strcmp(application->provider_identity, "fixture-clang-ast-provider") ==
                0 &&
            application->provider_version == 1U &&
            strcmp(application->clang_identity, "clang-18.1.3-fixture") == 0 &&
            strcmp(application->target.file, "fixture.c") == 0 &&
            application->target.kind == EVO_PROJECT_LOCATION_SPELLING &&
            application->edit.before_start ==
                fixture->target_starts[target_index] &&
            application->edit.before_end ==
                fixture->target_ends[target_index] &&
            application->edit.before_size == strlen(before) &&
            strcmp(application->edit.before_text, before) == 0,
        "application retains the exact half-open source range");
    test_check(
        application->disposition == EVO_PROJECT_TRANSFORMATION_EDIT &&
            application->edit.after_start == application->edit.before_start &&
            application->edit.after_end ==
                application->edit.before_start + strlen(replacement) &&
            application->edit.replacement_size == strlen(replacement) &&
            application->edit.replacement_text != NULL &&
            strcmp(application->edit.replacement_text, replacement) == 0 &&
            application->formatting_policy != NULL &&
            application->idempotence_policy != NULL &&
            application->semantic_assumption_count > 0U &&
            application->validation_obligation_count == 4U,
        "application exposes the exact replacement and obligations");
    test_check(
        !application->volatile_access && !application->contains_macro &&
            !application->contains_comment &&
            !application->contains_preprocessor &&
            !application->language_extension &&
            !application->ambiguous_target &&
            !application->alias_assumption_required &&
            (target_index == TEST_ASSIGNMENT
                 ? application->ast_primary.start ==
                           application->edit.before_start &&
                       application->ast_operand.end ==
                           application->edit.before_end &&
                       application->primary_plain_identifier &&
                       application->result_type_matches_primary &&
                       application->primary_declaration_identity != NULL &&
                       application->duplicate_declaration_identity != NULL &&
                       strcmp(
                           application->primary_declaration_identity,
                           application->duplicate_declaration_identity) == 0
             : target_index == TEST_CONDITION
                 ? application->ast_operand.end ==
                           application->edit.before_end &&
                       application->scalar_operand
                 : application->ast_primary.start ==
                           application->edit.before_start &&
                       application->ast_literal.end ==
                           application->edit.before_end &&
                       application->literal_value == UINT64_C(8) &&
                       application->result_width_bits == 32U &&
                       application->result_unsigned_integer &&
                       application->result_type_matches_primary),
        "application retains complete normalized AST proof evidence");
    test_check(
        application->canonical_json != NULL &&
            application->canonical_json_size ==
                strlen(application->canonical_json) &&
            test_json_valid(
                application->canonical_json,
                application->canonical_json_size) &&
            application->audit_markdown != NULL &&
            application->audit_markdown_size ==
                strlen(application->audit_markdown) &&
            strstr(
                application->canonical_json,
                "\"reference_form\":"
                "\"exact-source-edit-and-direct-dispatch\"") != NULL &&
            strstr(
                application->canonical_json,
                "\"primary_declaration_identity\"") != NULL &&
            strstr(application->audit_markdown, "half-open") != NULL &&
            application->projection_complete &&
            !application->probabilistic_authority &&
            !application->snapshot_modified &&
            !application->candidate_materialized &&
            application->private_owner != NULL,
        "application evidence is complete and explicitly non-mutating");
}

static void test_positive_exact_ranges_and_replay(test_fixture_t *fixture)
{
    static const char *const before[TEST_TARGET_COUNT] = {
        "total = total + ready", "!!ready", "value * 8U"};
    static const char *const replacements[TEST_TARGET_COUNT] = {
        "total += ready", "ready", "(value << 3)"};
    evo_project_transformation_application_t applications[TEST_TARGET_COUNT] =
        {{0}};
    evo_project_transformation_limits_t limits = test_transformation_limits();
    unsigned char *composed = NULL;
    unsigned char *after = NULL;
    size_t composed_size = 0U;
    size_t after_size = 0U;
    size_t index;

    for (index = 0U; index < TEST_TARGET_COUNT; index += 1U) {
        evo_project_transformation_application_t replay = {0};
        test_provider_context_t provider = {0};
        test_provider_context_t replay_provider = {0};
        evo_project_transformation_status_t status = test_apply(
            fixture,
            (int)index,
            TEST_PROVIDER_NORMAL,
            limits,
            &applications[index],
            &provider);

        test_check(
            status == EVO_PROJECT_TRANSFORMATION_SUCCESS &&
                provider.calls == 1U && !provider.request_invalid,
            "positive fixture is accepted by exactly one provider call");
        if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
            continue;
        }
        test_application_contract(
            fixture,
            (int)index,
            before[index],
            replacements[index],
            &applications[index]);
        status = test_apply(
            fixture,
            (int)index,
            TEST_PROVIDER_NORMAL,
            limits,
            &replay,
            &replay_provider);
        test_check(
            status == EVO_PROJECT_TRANSFORMATION_SUCCESS &&
                replay_provider.calls == 1U &&
                strcmp(
                    replay.application_fingerprint,
                    applications[index].application_fingerprint) == 0 &&
                strcmp(
                    replay.canonical_json,
                    applications[index].canonical_json) == 0 &&
                strcmp(
                    replay.audit_markdown,
                    applications[index].audit_markdown) == 0,
            "application replay is byte-for-byte deterministic");
        evo_project_transformation_application_destroy(&replay);
    }
    test_check(
        test_compose_applications(
            fixture,
            applications,
            TEST_TARGET_COUNT,
            &composed,
            &composed_size) &&
            test_load_after(&after, &after_size) && composed_size == after_size &&
            memcmp(composed, after, after_size) == 0,
        "declared ranges alone compose to the syntax-valid after fixture");
    test_check(
        test_source_unchanged(fixture),
        "positive applications never write or materialize a candidate");
    evo_project_release(after);
    evo_project_release(composed);
    for (index = 0U; index < TEST_TARGET_COUNT; index += 1U) {
        evo_project_transformation_application_destroy(&applications[index]);
    }
}

static void test_per_transform_boundaries(test_fixture_t *fixture)
{
    static const size_t replacement_sizes[TEST_TARGET_COUNT] = {14U, 5U, 12U};
    size_t index;

    for (index = 0U; index < TEST_TARGET_COUNT; index += 1U) {
        evo_project_transformation_application_t application = {0};
        test_provider_context_t provider = {0};
        evo_project_transformation_limits_t limits =
            test_transformation_limits();
        evo_project_transformation_status_t status;

        limits.max_replacement_bytes = replacement_sizes[index];
        status = test_apply(
            fixture,
            (int)index,
            TEST_PROVIDER_NORMAL,
            limits,
            &application,
            &provider);
        test_check(
            status == EVO_PROJECT_TRANSFORMATION_SUCCESS &&
                application.edit.replacement_size == replacement_sizes[index],
            "each transform accepts its exact replacement-byte boundary");
        evo_project_transformation_application_destroy(&application);

        limits.max_replacement_bytes = replacement_sizes[index] - 1U;
        status = test_apply(
            fixture,
            (int)index,
            TEST_PROVIDER_NORMAL,
            limits,
            &application,
            &provider);
        test_check(
            status == EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT &&
                application.private_owner == NULL,
            "each transform rejects one byte beyond its declared boundary");
        evo_project_transformation_application_destroy(&application);
    }
    test_check(
        test_source_unchanged(fixture),
        "boundary outcomes leave the immutable snapshot untouched");
}

static void test_per_transform_negative_and_malformed(test_fixture_t *fixture)
{
    evo_project_transformation_limits_t limits = test_transformation_limits();
    size_t index;

    for (index = 0U; index < TEST_TARGET_COUNT; index += 1U) {
        evo_project_transformation_application_t application = {0};
        test_provider_context_t provider = {0};
        evo_project_transformation_status_t status = test_apply(
            fixture,
            (int)index,
            TEST_PROVIDER_NEGATIVE,
            limits,
            &application,
            &provider);

        test_check(
            status == EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE &&
                provider.calls == 1U && application.private_owner == NULL,
            "each transform rejects a failed semantic precondition");
        status = test_apply(
            fixture,
            (int)index,
            TEST_PROVIDER_MALFORMED,
            limits,
            &application,
            &provider);
        test_check(
            status == EVO_PROJECT_TRANSFORMATION_ERROR_AST_MALFORMED &&
                provider.calls == 1U && application.private_owner == NULL,
            "each transform rejects malformed AST range evidence");
        status = test_apply(
            fixture,
            (int)index,
            TEST_PROVIDER_PARTIAL_TARGET,
            limits,
            &application,
            &provider);
        test_check(
            status != EVO_PROJECT_TRANSFORMATION_SUCCESS &&
                provider.calls == 1U && application.private_owner == NULL,
            "each transform requires component ranges to cover its target");
        evo_project_transformation_application_destroy(&application);
    }
    {
        evo_project_transformation_application_t application = {0};
        test_provider_context_t provider = {0};
        const test_provider_scenario_t scenarios[] = {
            TEST_PROVIDER_MACRO,
            TEST_PROVIDER_COMMENT,
            TEST_PROVIDER_PREPROCESSOR,
            TEST_PROVIDER_EXTENSION,
            TEST_PROVIDER_ALIAS,
            TEST_PROVIDER_AMBIGUOUS,
            TEST_PROVIDER_ERROR};
        const evo_project_transformation_status_t expected[] = {
            EVO_PROJECT_TRANSFORMATION_ERROR_UNSUPPORTED_MACRO,
            EVO_PROJECT_TRANSFORMATION_ERROR_UNSUPPORTED_COMMENT,
            EVO_PROJECT_TRANSFORMATION_ERROR_UNSUPPORTED_PREPROCESSOR,
            EVO_PROJECT_TRANSFORMATION_ERROR_UNSUPPORTED_EXTENSION,
            EVO_PROJECT_TRANSFORMATION_ERROR_UNSUPPORTED_ALIAS_ASSUMPTION,
            EVO_PROJECT_TRANSFORMATION_ERROR_AMBIGUOUS_TARGET,
            EVO_PROJECT_TRANSFORMATION_ERROR_PROVIDER};

        for (index = 0U; index < sizeof(scenarios) / sizeof(scenarios[0]);
             index += 1U) {
            const evo_project_transformation_status_t status = test_apply(
                fixture,
                TEST_ASSIGNMENT,
                scenarios[index],
                limits,
                &application,
                &provider);

            test_check(
                status == expected[index] && provider.calls == 1U &&
                    application.private_owner == NULL,
                "unsupported AST evidence is rejected with a stable reason");
        }
        test_check(
            test_apply(
                fixture,
                TEST_SHIFT,
                TEST_PROVIDER_LITERAL_MISMATCH,
                limits,
                &application,
                &provider) ==
                EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE,
            "source literal bytes independently constrain AST numeric evidence");
        test_check(
            test_apply(
                fixture,
                TEST_ASSIGNMENT,
                TEST_PROVIDER_NON_IDENTIFIER_TOKEN,
                limits,
                &application,
                &provider) ==
                EVO_PROJECT_TRANSFORMATION_ERROR_NOT_APPLICABLE,
            "plain-identifier evidence must match an exact source token");
        evo_project_transformation_application_destroy(&application);
    }
    test_check(
        test_source_unchanged(fixture),
        "negative and malformed fixtures never alter the source");
}

static void test_idempotence(void)
{
    static const char *const before[TEST_TARGET_COUNT] = {
        "total += ready", "ready", "(value << 3)"};
    test_fixture_t fixture = {0};
    evo_project_transformation_limits_t limits = test_transformation_limits();
    size_t index;

    test_check(
        test_fixture_prepare(&fixture, true),
        "prepare already-satisfied immutable fixture");
    if (fixture.baseline.private_owner == NULL) {
        test_fixture_destroy(&fixture);
        return;
    }
    for (index = 0U; index < TEST_TARGET_COUNT; index += 1U) {
        evo_project_transformation_application_t application = {0};
        test_provider_context_t provider = {0};
        const evo_project_transformation_status_t status = test_apply(
            &fixture,
            (int)index,
            TEST_PROVIDER_NORMAL,
            limits,
            &application,
            &provider);

        test_check(
            status == EVO_PROJECT_TRANSFORMATION_SUCCESS &&
                provider.calls == 1U &&
                application.disposition ==
                    EVO_PROJECT_TRANSFORMATION_ALREADY_SATISFIED &&
                application.edit.before_start ==
                    fixture.target_starts[index] &&
                application.edit.before_end == fixture.target_ends[index] &&
                strcmp(application.edit.before_text, before[index]) == 0 &&
                application.edit.replacement_size == 0U &&
                application.edit.replacement_text == NULL &&
                application.edit.after_start ==
                    application.edit.before_start &&
                application.edit.after_end == application.edit.before_end &&
                strstr(
                    application.canonical_json,
                    "\"disposition\":\"already-satisfied\"") != NULL,
            "reapplying each transform is a deterministic no-change result");
        evo_project_transformation_application_destroy(&application);
    }
    test_check(
        test_source_unchanged(&fixture),
        "idempotence checks preserve the already-satisfied fixture");
    test_fixture_destroy(&fixture);
}

static void test_preflight_and_budgets(test_fixture_t *fixture)
{
    evo_project_transformation_application_t application = {0};
    evo_project_transformation_registry_t inactive_registry = {0};
    test_provider_context_t provider = {0};
    evo_project_transformation_limits_t limits = test_transformation_limits();
    evo_project_transformation_apply_config_t config = test_apply_config(
        fixture, TEST_ASSIGNMENT, limits, &provider);
    evo_project_transformation_status_t status;

    status = evo_project_transformation_registry_open(
        &limits, &fixture->registry);
    test_check(
        status == EVO_PROJECT_TRANSFORMATION_ERROR_RESULT_ACTIVE,
        "an active registry cannot be overwritten");

    status = evo_project_transformation_apply(NULL, &application);
    test_check(
        status == EVO_PROJECT_TRANSFORMATION_ERROR_INVALID_ARGUMENT,
        "null application config is rejected");
    config.provider_identity = NULL;
    status = evo_project_transformation_apply(&config, &application);
    test_check(
        status == EVO_PROJECT_TRANSFORMATION_ERROR_INVALID_ARGUMENT,
        "incomplete provider identity is rejected");

    provider = (test_provider_context_t){fixture, TEST_PROVIDER_NORMAL, 0U, false};
    config = test_apply_config(fixture, TEST_ASSIGNMENT, limits, &provider);
    config.record_identity = "record-absent";
    status = evo_project_transformation_apply(&config, &application);
    test_check(
        status == EVO_PROJECT_TRANSFORMATION_ERROR_RECORD_NOT_FOUND &&
            provider.calls == 0U,
        "unknown records fail before the AST provider runs");

    limits.max_source_bytes = fixture->source_size - 1U;
    status = test_apply(
        fixture,
        TEST_ASSIGNMENT,
        TEST_PROVIDER_NORMAL,
        limits,
        &application,
        &provider);
    test_check(
        status == EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT &&
            provider.calls == 0U,
        "source bytes are bounded before provider invocation");

    limits = test_transformation_limits();
    limits.max_application_bytes = 1U;
    status = test_apply(
        fixture,
        TEST_ASSIGNMENT,
        TEST_PROVIDER_NORMAL,
        limits,
        &application,
        &provider);
    test_check(
        status == EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT &&
            provider.calls == 1U && application.private_owner == NULL,
        "application evidence honors its byte budget without partial output");

    limits = test_transformation_limits();
    fixture->analysis.projection_complete = false;
    status = test_apply(
        fixture,
        TEST_ASSIGNMENT,
        TEST_PROVIDER_NORMAL,
        limits,
        &application,
        &provider);
    test_check(
        status == EVO_PROJECT_TRANSFORMATION_ERROR_RECIPE_STALE &&
            provider.calls == 0U,
        "incomplete analysis authority invalidates the recipe");
    fixture->analysis.projection_complete = true;

    fixture->registry.probabilistic_authority = true;
    status = test_apply(
        fixture,
        TEST_ASSIGNMENT,
        TEST_PROVIDER_NORMAL,
        limits,
        &application,
        &provider);
    test_check(
        status == EVO_PROJECT_TRANSFORMATION_ERROR_RECIPE_STALE &&
            provider.calls == 0U,
        "probabilistic catalogue authority is rejected");
    fixture->registry.probabilistic_authority = false;

    fixture->baseline.state = EVO_PROJECT_BASELINE_BUILD_FAILED;
    fixture->baseline_owner.state = EVO_PROJECT_BASELINE_BUILD_FAILED;
    status = test_apply(
        fixture,
        TEST_ASSIGNMENT,
        TEST_PROVIDER_NORMAL,
        limits,
        &application,
        &provider);
    test_check(
        status == EVO_PROJECT_TRANSFORMATION_ERROR_BASELINE_INELIGIBLE &&
            provider.calls == 0U,
        "ineligible baselines are rejected before source access");
    fixture->baseline.state = EVO_PROJECT_BASELINE_ELIGIBLE;
    fixture->baseline_owner.state = EVO_PROJECT_BASELINE_ELIGIBLE;

    status = test_apply(
        fixture,
        TEST_ASSIGNMENT,
        TEST_PROVIDER_NORMAL,
        limits,
        &application,
        &provider);
    test_check(
        status == EVO_PROJECT_TRANSFORMATION_SUCCESS,
        "active-result precondition fixture succeeds");
    status = test_apply(
        fixture,
        TEST_ASSIGNMENT,
        TEST_PROVIDER_NORMAL,
        limits,
        &application,
        &provider);
    test_check(
        status == EVO_PROJECT_TRANSFORMATION_ERROR_RESULT_ACTIVE &&
            provider.calls == 0U,
        "an active application cannot be overwritten");
    evo_project_transformation_application_destroy(&application);

    limits.max_total_bytes = 0U;
    status = evo_project_transformation_registry_open(
        &limits, &inactive_registry);
    test_check(
        status == EVO_PROJECT_TRANSFORMATION_ERROR_INVALID_ARGUMENT,
        "zero-valued limits are invalid");
    evo_project_transformation_registry_destroy(&inactive_registry);
    evo_project_transformation_application_destroy(&application);
    test_check(
        test_source_unchanged(fixture),
        "preflight and budget failures preserve the source snapshot");
}

static void test_snapshot_tamper_detection(void)
{
    test_fixture_t fixture = {0};
    evo_project_transformation_application_t application = {0};
    test_provider_context_t provider = {0};
    evo_project_transformation_limits_t limits = test_transformation_limits();
    evo_project_transformation_status_t status;

    test_check(
        test_fixture_prepare(&fixture, false),
        "prepare tamper-detection fixture");
    if (fixture.baseline.private_owner == NULL) {
        test_fixture_destroy(&fixture);
        return;
    }
    status = test_apply(
        &fixture,
        TEST_ASSIGNMENT,
        TEST_PROVIDER_TAMPER,
        limits,
        &application,
        &provider);
    test_check(
        status == EVO_PROJECT_TRANSFORMATION_ERROR_BASELINE_CHANGED &&
            provider.calls == 1U && application.private_owner == NULL,
        "snapshot mutation during provider execution is detected");
    evo_project_transformation_application_destroy(&application);
    test_fixture_destroy(&fixture);
}

static void test_stable_names(void)
{
    test_check(
        strcmp(
            evo_project_transformation_status_name(
                EVO_PROJECT_TRANSFORMATION_ERROR_UNSUPPORTED_MACRO),
            "unsupported-macro") == 0 &&
            strcmp(
                evo_project_transformation_status_name(
                    EVO_PROJECT_TRANSFORMATION_ERROR_AMBIGUOUS_TARGET),
                "ambiguous-target") == 0 &&
            strcmp(
                evo_project_transformation_status_name(
                    (evo_project_transformation_status_t)99),
                "state") == 0,
        "transformation rejection names are stable");
    test_check(
        strcmp(
            evo_project_transformation_ast_form_name(
                EVO_PROJECT_AST_ASSIGNMENT_BINARY),
            "assignment-binary") == 0 &&
            strcmp(
                evo_project_transformation_operator_name(
                    EVO_PROJECT_TRANSFORMATION_OPERATOR_SHIFT_LEFT),
                "shift-left") == 0 &&
            strcmp(
                evo_project_transformation_condition_context_name(
                    EVO_PROJECT_TRANSFORMATION_CONDITION_DO_WHILE),
                "do-while") == 0,
        "AST, operator, and context names are stable");
}

int main(void)
{
    test_fixture_t fixture = {0};
    const bool prepared = test_fixture_prepare(&fixture, false);

    test_check(prepared, "prepare immutable transformation fixture");
    if (!prepared) {
        test_fixture_destroy(&fixture);
        return EXIT_FAILURE;
    }
    test_registry_projection(&fixture);
    test_positive_exact_ranges_and_replay(&fixture);
    test_per_transform_boundaries(&fixture);
    test_per_transform_negative_and_malformed(&fixture);
    test_preflight_and_budgets(&fixture);
    test_fixture_destroy(&fixture);
    test_idempotence();
    test_snapshot_tamper_detection();
    test_stable_names();
    return test_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
