#define _XOPEN_SOURCE 700

#include "internal/project_provider.h"
#include "internal/project_fingerprint.h"
#include "internal/project_provider_clang_ast.h"
#include "internal/project_provider_sandbox.h"
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

static int failures = 0;

static void check(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "project Clang AST provider test failure: %s\n", message);
        failures += 1;
    }
}

static bool write_text(const char *path, const char *text)
{
    int descriptor;
    const size_t size = strlen(text);
    size_t position = 0U;

    descriptor = open(
        path,
        O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
        (mode_t)0600);
    if (descriptor < 0) {
        return false;
    }
    while (position < size) {
        const ssize_t count = write(descriptor, text + position, size - position);

        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            (void)close(descriptor);
            return false;
        }
        position += (size_t)count;
    }
    return close(descriptor) == 0;
}

static size_t find_text(const char *text, const char *needle, size_t start)
{
    const size_t text_size = strlen(text);
    const size_t needle_size = strlen(needle);
    size_t index;

    if (needle_size == 0U || needle_size > text_size || start > text_size - needle_size) {
        return SIZE_MAX;
    }
    for (index = start; index <= text_size - needle_size; index += 1U) {
        if (strncmp(text + index, needle, needle_size) == 0) {
            return index;
        }
    }
    return SIZE_MAX;
}

static bool position_from_offset(
    const char *text,
    size_t offset,
    uint32_t *line,
    uint32_t *column)
{
    const size_t size = strlen(text);
    size_t index;

    if (offset > size) {
        return false;
    }
    *line = 1U;
    *column = 1U;
    for (index = 0U; index < offset; index += 1U) {
        if (text[index] == '\n') {
            *line += 1U;
            *column = 1U;
        } else {
            *column += 1U;
        }
    }
    return true;
}

static evo_project_transformation_limits_t transformation_limits(void)
{
    return (evo_project_transformation_limits_t){
        .max_string_bytes = 512U,
        .max_path_bytes = 512U,
        .max_source_bytes = 16384U,
        .max_replacement_bytes = 2048U,
        .max_parameters = 16U,
        .max_registry_bytes = 32768U,
        .max_application_bytes = 32768U,
        .max_audit_bytes = 32768U,
        .max_total_bytes = 131072U,
    };
}

static bool prepare_target(
    const char *source,
    const char *needle,
    size_t search_start,
    const char *identity,
    evo_project_recipe_target_t *target)
{
    const size_t start = find_text(source, needle, search_start);
    const size_t end = start == SIZE_MAX ? SIZE_MAX : start + strlen(needle);

    if (start == SIZE_MAX ||
        !position_from_offset(source, start, &target->line, &target->column) ||
        !position_from_offset(source, end, &target->end_line, &target->end_column)) {
        return false;
    }
    target->location_identity = identity;
    target->file = "fixture.c";
    target->kind = EVO_PROJECT_LOCATION_SPELLING;
    target->spelling_identity = NULL;
    return true;
}

static evo_project_transformation_status_t run_provider_with_fingerprint(
    const char *source,
    const char *workspace,
    const evo_project_recipe_target_t *target,
    const char *transformation,
    const evo_project_compilation_record_t *unit,
    evo_project_clang_ast_context_t *context,
    evo_project_transformation_ast_result_t *result,
    bool stale_fingerprint)
{
    evo_project_fingerprint_t fingerprint;
    char source_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    evo_project_transformation_request_t request = {0};

    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_bytes(
        &fingerprint, (const unsigned char *)source, strlen(source));
    evo_project_fingerprint_format(fingerprint.value, source_fingerprint);
    if (stale_fingerprint) {
        source_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE - 2U] =
            source_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE - 2U] == '0'
                ? '1'
                : '0';
    }
    request = (evo_project_transformation_request_t){
        .schema_version = EVO_PROJECT_TRANSFORMATION_AST_SCHEMA_VERSION,
        .baseline_fingerprint = "fnv1a64-v1:0000000000000000",
        .analysis_fingerprint = "fnv1a64-v1:0000000000000001",
        .recipe_fingerprint = "fnv1a64-v1:0000000000000002",
        .snapshot_path = workspace,
        .record_identity = "provider-fixture-record",
        .target = target,
        .transformation_identity = transformation,
        .transformation_version = 1U,
        .parameter_count = 0U,
        .parameters = NULL,
        .source_size = strlen(source),
        .source_fingerprint = source_fingerprint,
        .limits = transformation_limits(),
        .network_access = false,
    };

    context->compilation_unit_count = 1U;
    context->compilation_units = unit;
    context->timeout_ms = 10000U;
    context->max_memory_bytes = 536870912U;
    context->max_processes = 8U;
    context->max_storage_bytes = 1048576U;
    context->max_output_bytes = 4194304U;
    return evo_project_clang_ast_provider(&request, context, result);
}

static evo_project_transformation_status_t run_provider(
    const char *source,
    const char *workspace,
    const evo_project_recipe_target_t *target,
    const char *transformation,
    const evo_project_compilation_record_t *unit,
    evo_project_clang_ast_context_t *context,
    evo_project_transformation_ast_result_t *result)
{
    return run_provider_with_fingerprint(
        source,
        workspace,
        target,
        transformation,
        unit,
        context,
        result,
        false);
}

static void exercise_source(
    const char *workspace,
    const char *source,
    bool already_satisfied,
    const evo_project_compilation_record_t *unit,
    evo_project_clang_ast_context_t *context)
{
    static const char *const identities[] = {
        "catalyst.evo.c.assignment-to-compound",
        "catalyst.evo.c.double-negation-condition",
        "catalyst.evo.c.unsigned-multiply-to-shift"};
    const char *assignment = already_satisfied ? "total += ready" : "total = total + ready";
    const char *condition = already_satisfied ? "ready" : "!!ready";
    const char *shift = already_satisfied ? "(value << 3)" : "value * 8U";
    const size_t condition_anchor = find_text(source, "if (", 0U);
    evo_project_recipe_target_t target = {0};
    evo_project_transformation_ast_result_t result = {0};
    evo_project_transformation_status_t status;

    check(
        prepare_target(source, assignment, 0U, "location-assignment", &target),
        "assignment target prepared");
    status = run_provider(source, workspace, &target, identities[0], unit, context, &result);
    check(status == EVO_PROJECT_TRANSFORMATION_SUCCESS, "assignment provider succeeds");
    check(
        result.form == (already_satisfied ? EVO_PROJECT_AST_ASSIGNMENT_COMPOUND
                                          : EVO_PROJECT_AST_ASSIGNMENT_BINARY),
        "assignment AST form");
    check(result.operator_kind == EVO_PROJECT_TRANSFORMATION_OPERATOR_ADD, "assignment operator");
    check(result.primary_plain_identifier, "assignment primary identifier");
    check(result.result_type_matches_primary, "assignment type preservation");
    check(result.primary_declaration_identity != NULL, "assignment declaration identity");
    if (!already_satisfied) {
        check(result.duplicate_declaration_identity != NULL, "assignment duplicate identity");
        if (result.primary_declaration_identity != NULL && result.duplicate_declaration_identity != NULL) {
            check(
                strcmp(result.primary_declaration_identity, result.duplicate_declaration_identity) == 0,
                "assignment declaration identity equality");
        }
    }

    target = (evo_project_recipe_target_t){0};
    result = (evo_project_transformation_ast_result_t){0};
    check(condition_anchor != SIZE_MAX, "condition anchor found");
    check(
        prepare_target(
            source,
            condition,
            condition_anchor == SIZE_MAX ? 0U : condition_anchor,
            "location-condition",
            &target),
        "condition target prepared");
    status = run_provider(source, workspace, &target, identities[1], unit, context, &result);
    check(status == EVO_PROJECT_TRANSFORMATION_SUCCESS, "condition provider succeeds");
    check(
        result.form == (already_satisfied ? EVO_PROJECT_AST_SCALAR_CONDITION
                                          : EVO_PROJECT_AST_DOUBLE_NEGATED_CONDITION),
        "condition AST form");
    check(
        result.condition_context == EVO_PROJECT_TRANSFORMATION_CONDITION_IF,
        "condition context");
    check(result.scalar_operand, "condition scalar operand");

    target = (evo_project_recipe_target_t){0};
    result = (evo_project_transformation_ast_result_t){0};
    check(
        prepare_target(source, shift, 0U, "location-shift", &target),
        "shift target prepared");
    status = run_provider(source, workspace, &target, identities[2], unit, context, &result);
    check(status == EVO_PROJECT_TRANSFORMATION_SUCCESS, "shift provider succeeds");
    check(
        result.form == (already_satisfied ? EVO_PROJECT_AST_UNSIGNED_SHIFT_POWER_OF_TWO
                                          : EVO_PROJECT_AST_UNSIGNED_MULTIPLY_POWER_OF_TWO),
        "shift AST form");
    check(
        result.operator_kind == (already_satisfied ? EVO_PROJECT_TRANSFORMATION_OPERATOR_SHIFT_LEFT
                                                   : EVO_PROJECT_TRANSFORMATION_OPERATOR_MULTIPLY),
        "shift operator");
    check(result.literal_value == (already_satisfied ? 3U : 8U), "shift literal");
    check(result.result_unsigned_integer, "shift unsigned type");
    check(result.result_type_matches_primary, "shift type preservation");
    check(result.result_width_bits == 16U, "target-local unsigned-int width");
    check(!result.contains_macro, "target macro-free despite unrelated macro");
    check(!result.volatile_access, "target nonvolatile despite unrelated volatile");
    check(!result.contains_comment, "target comment-free");
    check(!result.contains_preprocessor, "target preprocessor-free");
}

int main(void)
{
#if !defined(__linux__)
    (void)printf("real Clang AST provider fixture is Linux-only\n");
    return 77;
#else
    static const char before_source[] =
        "#define EVO_AST_NOISE(x) ((x) + 1U)\n"
        "static unsigned int transform_fixture(unsigned int value, int ready)\n"
        "{\n"
        "    volatile unsigned long long unrelated = 1ULL;\n"
        "    unsigned int macro_noise = EVO_AST_NOISE(value);\n"
        "    unsigned int scaled = value * 8U;\n"
        "    int total = 0;\n"
        "    total = total + ready;\n"
        "    if (!!ready) { total += 1; }\n"
        "    return scaled + (unsigned int)total + macro_noise + (unsigned int)unrelated;\n"
        "}\n";
    static const char after_source[] =
        "#define EVO_AST_NOISE(x) ((x) + 1U)\n"
        "static unsigned int transform_fixture(unsigned int value, int ready)\n"
        "{\n"
        "    volatile unsigned long long unrelated = 1ULL;\n"
        "    unsigned int macro_noise = EVO_AST_NOISE(value);\n"
        "    unsigned int scaled = (value << 3);\n"
        "    int total = 0;\n"
        "    total += ready;\n"
        "    if (ready) { total += 1; }\n"
        "    return scaled + (unsigned int)total + macro_noise + (unsigned int)unrelated;\n"
        "}\n";
    static const char *const compile_arguments[] = {
        "cc", "-std=c17", "-c", "fixture.c"};
    char template_path[] = "/tmp/evo-clang-ast-provider-XXXXXX";
    char source_path[512];
    char *workspace;
    evo_project_compilation_record_t unit = {
        .directory = ".",
        .file = "fixture.c",
        .output = NULL,
        .command_form = EVO_PROJECT_COMPILE_COMMAND_ARGUMENTS,
        .argument_count = sizeof(compile_arguments) / sizeof(compile_arguments[0]),
        .arguments = compile_arguments,
        .command = NULL,
    };
    evo_project_clang_ast_context_t context = {0};
    int written;

    if (!evo_project_sandbox_available() ||
        !evo_project_provider_available(
            evo_project_provider_find(EVO_PROJECT_PROVIDER_CLANG_AST_ID))) {
        (void)printf("real Clang/Bubblewrap AST provider unavailable\n");
        return 77;
    }
    workspace = mkdtemp(template_path);
    if (workspace == NULL) {
        (void)fprintf(stderr, "mkdtemp failed: %s\n", strerror(errno));
        return 1;
    }
    written = evo_project_format(source_path, sizeof(source_path), "%s/fixture.c", workspace);
    if (written <= 0 || (size_t)written >= sizeof(source_path) ||
        !write_text(source_path, before_source)) {
        (void)fprintf(stderr, "unable to create AST fixture\n");
        (void)rmdir(workspace);
        return 1;
    }
    {
        evo_project_recipe_target_t stale_target = {0};
        evo_project_transformation_ast_result_t stale_result = {0};

        check(
            prepare_target(
                before_source,
                "total = total + ready",
                0U,
                "location-stale",
                &stale_target),
            "stale fingerprint target prepared");
        check(
            run_provider_with_fingerprint(
                before_source,
                workspace,
                &stale_target,
                "catalyst.evo.c.assignment-to-compound",
                &unit,
                &context,
                &stale_result,
                true) == EVO_PROJECT_TRANSFORMATION_ERROR_BASELINE_CHANGED,
            "stale source fingerprint rejected before AST authorization");
        evo_project_clang_ast_context_destroy(&context);
    }
    exercise_source(workspace, before_source, false, &unit, &context);
    evo_project_clang_ast_context_destroy(&context);
    check(unlink(source_path) == 0, "remove before fixture");
    check(write_text(source_path, after_source), "write already-satisfied fixture");
    exercise_source(workspace, after_source, true, &unit, &context);
    evo_project_clang_ast_context_destroy(&context);
    check(unlink(source_path) == 0, "remove after fixture");
    check(rmdir(workspace) == 0, "remove AST workspace");

    if (failures != 0) {
        (void)fprintf(stderr, "%d Clang AST provider tests failed\n", failures);
        return 1;
    }
    (void)printf("real Clang transformation AST provider tests passed\n");
    return 0;
#endif
}
