#define _POSIX_C_SOURCE 200809L

#include "internal/product_cli.h"
#include "internal/project_json.h"
#include "internal/project_provider.h"
#include "internal/project_runtime.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct evo_product_document {
    char *text;
    size_t bytes;
    evo_project_json_token_t *tokens;
    size_t count;
} evo_product_document_t;

static void evo_product_document_destroy(evo_product_document_t *document)
{
    evo_project_release(document->tokens);
    evo_project_release(document->text);
}

static bool evo_product_document_read(int root, const char *name, evo_product_document_t *document)
{
    const int descriptor = evo_product_open_relative(root, name, false);
    if (descriptor < 0) {
        return false;
    }
    const bool read_ok = evo_product_read(descriptor, EVO_PRODUCT_EVIDENCE_BYTES, &document->text, &document->bytes);
    (void)close(descriptor);
    if (!read_ok) {
        return false;
    }
    document->tokens = evo_project_allocate_zeroed(1048576U, sizeof(*document->tokens));
    if (document->tokens == NULL) {
        errno = ENOMEM;
        return false;
    }
    const evo_project_json_status_t status = evo_project_json_parse(document->text, document->bytes,
                                                                    document->tokens, 1048576U, 64U, &document->count);
    if (status != EVO_PROJECT_JSON_SUCCESS) {
        errno = status == EVO_PROJECT_JSON_RESOURCE_LIMIT ? EFBIG : EINVAL;
        return false;
    }
    return document->count > 0U && document->tokens[0].type == EVO_PROJECT_JSON_OBJECT;
}

static bool evo_product_member(const evo_product_document_t *document, size_t object, const char *name, size_t *index)
{
    return evo_project_json_object_get(document->text, document->tokens, document->count, object, name, index) == 1;
}

static bool evo_product_envelope(const evo_product_document_t *document, const char *const *keys, size_t count)
{
    if (!evo_project_json_object_has_only(document->text, document->tokens, document->count, 0U, keys, count)) {
        return false;
    }
    for (size_t key = 0U; key < count; key += 1U) {
        size_t index;
        if (!evo_product_member(document, 0U, keys[key], &index)) {
            return false;
        }
    }
    return true;
}

static bool evo_product_string(const evo_product_document_t *document, size_t object,
                               const char *name, char *output, size_t capacity)
{
    size_t index;
    char *value = NULL;
    if (!evo_product_member(document, object, name, &index) ||
        evo_project_json_decode_string(document->text, &document->tokens[index], capacity - 1U, &value) != EVO_PROJECT_JSON_SUCCESS) {
        return false;
    }
    const int written = evo_project_format(output, capacity, "%s", value);
    evo_project_release(value);
    return written >= 0 && (size_t)written < capacity;
}

static bool evo_product_string_is(const evo_product_document_t *document, size_t object,
                                  const char *name, const char *expected)
{
    char value[256];
    return evo_product_string(document, object, name, value, sizeof(value)) && strcmp(value, expected) == 0;
}

static bool evo_product_number(const evo_product_document_t *document, size_t object, const char *name, uint64_t *value)
{
    size_t index;
    return evo_product_member(document, object, name, &index) &&
           evo_project_json_parse_u64(document->text, &document->tokens[index], value);
}

static bool evo_product_array_count(const evo_product_document_t *document, const char *name, uint64_t expected)
{
    size_t index;
    return evo_product_member(document, 0U, name, &index) &&
           document->tokens[index].type == EVO_PROJECT_JSON_ARRAY &&
           (uint64_t)document->tokens[index].child_count == expected;
}

static bool evo_product_false(const evo_product_document_t *document, const char *name)
{
    size_t index;
    return evo_product_member(document, 0U, name, &index) && document->tokens[index].type == EVO_PROJECT_JSON_FALSE;
}

static bool evo_product_fingerprint(const char *value)
{
    if (strlen(value) != 27U || strncmp(value, "fnv1a64-v1:", 11U) != 0) {
        return false;
    }
    for (size_t index = 11U; index < 27U; index += 1U) {
        if (!((value[index] >= '0' && value[index] <= '9') || (value[index] >= 'a' && value[index] <= 'f'))) {
            return false;
        }
    }
    return true;
}

int evo_product_execute_report(const char *evidence_argument, evo_product_stage_t *stage,
                               evo_product_summary_t *summary)
{
    static const char *const baseline_keys[] = {"schema", "schema_version", "state", "manifest", "baseline", "files", "compilation_units", "commands", "human_readable_abstraction"};
    static const char *const analysis_keys[] = {"schema", "schema_version", "baseline_fingerprint", "analysis_fingerprint", "provider", "runtime_profile", "translation_units", "source_locations", "declarations", "calls", "control_flows", "data_flows", "compiler_optimizations", "runtime_hotspots", "opportunities", "human_readable_abstraction", "source_modified", "evolutionary_operator_invoked"};
    char root_path[EVO_PRODUCT_PATH_BYTES];
    char analysis_baseline[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    evo_product_document_t baseline = {0};
    evo_product_document_t analysis = {0};
    size_t baseline_object;
    size_t provider_object;
    size_t declarations;
    uint64_t version = 0U;
    int exit_status = EVO_PROJECT_COMMAND_EXIT_CONFIGURATION;
    const int root = evo_product_open_path(evidence_argument, true, root_path);

    if (root < 0) {
        return exit_status;
    }
    if (evo_product_path_contains(root_path, stage->output_path) || evo_product_path_contains(stage->output_path, root_path)) {
        goto done;
    }
    const evo_project_command_request_t request = {
        .schema_version = EVO_PROJECT_COMMAND_SCHEMA_VERSION,
        .operation = EVO_PROJECT_COMMAND_REPORT,
        .evidence_path = root_path,
        .output_path = stage->output_path,
    };
    evo_project_command_plan_t plan = {0};
    if (evo_project_command_plan_build(&request, &plan) != EVO_PROJECT_COMMAND_SUCCESS || plan.execution_permitted) {
        goto done;
    }
    if (!evo_product_document_read(root, "baseline/baseline.json", &baseline) ||
        !evo_product_document_read(root, "analysis/analysis.json", &analysis)) {
        exit_status = errno == ENOMEM || errno == EFBIG ? EVO_PROJECT_COMMAND_EXIT_RESOURCE : EVO_PROJECT_COMMAND_EXIT_CONFIGURATION;
        goto done;
    }
    if (!evo_product_envelope(&baseline, baseline_keys, sizeof(baseline_keys) / sizeof(baseline_keys[0])) ||
        !evo_product_envelope(&analysis, analysis_keys, sizeof(analysis_keys) / sizeof(analysis_keys[0])) ||
        !evo_product_string_is(&baseline, 0U, "schema", "catalyst.evo-project-baseline.v1") ||
        !evo_product_string_is(&analysis, 0U, "schema", "catalyst.evo-project-analysis.v1") ||
        !evo_product_false(&analysis, "source_modified") || !evo_product_false(&analysis, "evolutionary_operator_invoked") ||
        !evo_product_string_is(&baseline, 0U, "state", "eligible") ||
        !evo_product_number(&baseline, 0U, "schema_version", &version) || version != 1U ||
        !evo_product_number(&analysis, 0U, "schema_version", &version) || version != 1U ||
        !evo_product_member(&baseline, 0U, "baseline", &baseline_object) ||
        !evo_product_member(&analysis, 0U, "provider", &provider_object) ||
        !evo_product_string_is(&baseline, baseline_object, "execution_provider", EVO_PROJECT_PROVIDER_LINUX_BWRAP_ID) ||
        !evo_product_string_is(&analysis, provider_object, "identity", EVO_PROJECT_PROVIDER_CLANG_ANALYSIS_ID) ||
        !evo_product_string(&baseline, baseline_object, "fingerprint", summary->baseline, sizeof(summary->baseline)) ||
        !evo_product_string(&analysis, 0U, "analysis_fingerprint", summary->analysis, sizeof(summary->analysis)) ||
        !evo_product_string(&analysis, 0U, "baseline_fingerprint", analysis_baseline, sizeof(analysis_baseline)) ||
        !evo_product_fingerprint(summary->baseline) || !evo_product_fingerprint(summary->analysis) ||
        strcmp(summary->baseline, analysis_baseline) != 0 ||
        !evo_product_number(&baseline, baseline_object, "file_count", &summary->files) ||
        !evo_product_number(&baseline, baseline_object, "total_file_bytes", &summary->bytes) ||
        !evo_product_number(&baseline, baseline_object, "compilation_unit_count", &summary->units) ||
        !evo_product_array_count(&baseline, "files", summary->files) ||
        !evo_product_array_count(&baseline, "compilation_units", summary->units) ||
        !evo_product_array_count(&analysis, "translation_units", summary->units) ||
        !evo_product_member(&analysis, 0U, "declarations", &declarations) ||
        analysis.tokens[declarations].type != EVO_PROJECT_JSON_ARRAY) {
        (void)fprintf(stderr, "evo: report: unsupported, incomplete or inconsistent native evidence\n");
        goto done;
    }
    summary->declarations = (uint64_t)analysis.tokens[declarations].child_count;
    if (!evo_product_stage_begin(stage)) {
        goto done;
    }
    exit_status = EVO_PROJECT_COMMAND_EXIT_SUCCESS;

done:
    (void)close(root);
    evo_product_document_destroy(&analysis);
    evo_product_document_destroy(&baseline);
    return exit_status;
}
