#include "internal/project_transformation_evidence.h"

#include "internal/project_fingerprint.h"
#include "internal/project_runtime.h"

#include <string.h>

typedef struct evo_transform_evidence_writer {
    char *bytes;
    size_t capacity;
    size_t size;
    bool failed;
} evo_transform_evidence_writer_t;

static void evo_evidence_bytes(
    evo_transform_evidence_writer_t *writer,
    const char *bytes,
    size_t count)
{
    size_t index;

    if (writer->failed) {
        return;
    }
    if (count > writer->capacity - writer->size) {
        writer->failed = true;
        return;
    }
    if (writer->bytes != NULL) {
        for (index = 0U; index < count; index += 1U) {
            writer->bytes[writer->size + index] = bytes[index];
        }
    }
    writer->size += count;
}

static void evo_evidence_text(
    evo_transform_evidence_writer_t *writer,
    const char *text)
{
    evo_evidence_bytes(writer, text, strlen(text));
}

static void evo_evidence_char(
    evo_transform_evidence_writer_t *writer,
    char value)
{
    evo_evidence_bytes(writer, &value, 1U);
}

static void evo_evidence_u64(
    evo_transform_evidence_writer_t *writer,
    uint64_t value)
{
    char text[32];
    const int written = evo_project_format(
        text, sizeof(text), "%llu", (unsigned long long)value);

    if (written <= 0 || (size_t)written >= sizeof(text)) {
        writer->failed = true;
        return;
    }
    evo_evidence_bytes(writer, text, (size_t)written);
}

static void evo_evidence_i64(
    evo_transform_evidence_writer_t *writer,
    int64_t value)
{
    char text[32];
    const int written = evo_project_format(
        text, sizeof(text), "%lld", (long long)value);

    if (written <= 0 || (size_t)written >= sizeof(text)) {
        writer->failed = true;
        return;
    }
    evo_evidence_bytes(writer, text, (size_t)written);
}

static void evo_evidence_json_string(
    evo_transform_evidence_writer_t *writer,
    const char *value)
{
    size_t index;

    evo_evidence_char(writer, '"');
    for (index = 0U; value[index] != '\0'; index += 1U) {
        const unsigned char byte = (unsigned char)value[index];

        if (value[index] == '"') {
            evo_evidence_text(writer, "\\\"");
        } else if (value[index] == '\\') {
            evo_evidence_text(writer, "\\\\");
        } else if (value[index] == '\n') {
            evo_evidence_text(writer, "\\n");
        } else if (value[index] == '\r') {
            evo_evidence_text(writer, "\\r");
        } else if (value[index] == '\t') {
            evo_evidence_text(writer, "\\t");
        } else if (byte < 0x20U) {
            writer->failed = true;
        } else {
            evo_evidence_char(writer, value[index]);
        }
    }
    evo_evidence_char(writer, '"');
}

static void evo_evidence_json_nullable(
    evo_transform_evidence_writer_t *writer,
    const char *value)
{
    if (value == NULL) {
        evo_evidence_text(writer, "null");
    } else {
        evo_evidence_json_string(writer, value);
    }
}

static void evo_evidence_json_range(
    evo_transform_evidence_writer_t *writer,
    evo_project_transformation_byte_range_t range)
{
    if (range.start == 0U && range.end == 0U) {
        evo_evidence_text(writer, "null");
        return;
    }
    evo_evidence_text(writer, "{\"start\":");
    evo_evidence_u64(writer, range.start);
    evo_evidence_text(writer, ",\"end\":");
    evo_evidence_u64(writer, range.end);
    evo_evidence_char(writer, '}');
}

static void evo_evidence_text_array(
    evo_transform_evidence_writer_t *writer,
    const char *const *values,
    size_t count)
{
    size_t index;

    evo_evidence_char(writer, '[');
    for (index = 0U; index < count; index += 1U) {
        if (index > 0U) {
            evo_evidence_char(writer, ',');
        }
        evo_evidence_json_string(writer, values[index]);
    }
    evo_evidence_char(writer, ']');
}

static const char *evo_evidence_parameter_kind_name(
    evo_project_recipe_parameter_kind_t kind)
{
    switch (kind) {
    case EVO_PROJECT_RECIPE_PARAMETER_INTEGER:
        return "integer";
    case EVO_PROJECT_RECIPE_PARAMETER_BOOLEAN:
        return "boolean";
    case EVO_PROJECT_RECIPE_PARAMETER_CHOICE:
    default:
        return "choice";
    }
}

static void evo_evidence_parameters(
    evo_transform_evidence_writer_t *writer,
    const evo_project_transformation_application_t *view)
{
    size_t index;

    evo_evidence_char(writer, '[');
    for (index = 0U; index < view->parameter_count; index += 1U) {
        const evo_project_recipe_parameter_value_t *parameter =
            &view->parameters[index];

        if (index > 0U) {
            evo_evidence_char(writer, ',');
        }
        evo_evidence_text(writer, "{\"identity\":");
        evo_evidence_json_string(writer, parameter->identity);
        evo_evidence_text(writer, ",\"kind\":");
        evo_evidence_json_string(
            writer, evo_evidence_parameter_kind_name(parameter->kind));
        evo_evidence_text(writer, ",\"integer_value\":");
        if (parameter->kind == EVO_PROJECT_RECIPE_PARAMETER_INTEGER) {
            evo_evidence_i64(writer, parameter->integer_value);
        } else {
            evo_evidence_text(writer, "null");
        }
        evo_evidence_text(writer, ",\"boolean_value\":");
        if (parameter->kind == EVO_PROJECT_RECIPE_PARAMETER_BOOLEAN) {
            evo_evidence_text(
                writer, parameter->boolean_value ? "true" : "false");
        } else {
            evo_evidence_text(writer, "null");
        }
        evo_evidence_text(writer, ",\"choice_value\":");
        if (parameter->kind == EVO_PROJECT_RECIPE_PARAMETER_CHOICE) {
            evo_evidence_json_string(writer, parameter->choice_value);
        } else {
            evo_evidence_text(writer, "null");
        }
        evo_evidence_char(writer, '}');
    }
    evo_evidence_char(writer, ']');
}

static const char *evo_disposition_name(
    evo_project_transformation_disposition_t disposition)
{
    return disposition == EVO_PROJECT_TRANSFORMATION_EDIT
               ? "edit"
               : "already-satisfied";
}

static void evo_generate_application_json(
    evo_transform_evidence_writer_t *writer,
    const evo_project_transformation_application_t *view)
{
    evo_evidence_text(
        writer,
        "{\n\"schema\":\"catalyst.evo-c-transformation-application.v1\","
        "\n\"schema_version\":1,\n\"application_fingerprint\":");
    evo_evidence_json_string(writer, view->application_fingerprint);
    evo_evidence_text(writer, ",\n\"baseline_fingerprint\":");
    evo_evidence_json_string(writer, view->baseline_fingerprint);
    evo_evidence_text(writer, ",\n\"analysis_fingerprint\":");
    evo_evidence_json_string(writer, view->analysis_fingerprint);
    evo_evidence_text(writer, ",\n\"recipe_fingerprint\":");
    evo_evidence_json_string(writer, view->recipe_fingerprint);
    evo_evidence_text(writer, ",\n\"catalogue\":{\"identity\":");
    evo_evidence_json_string(writer, view->catalogue_identity);
    evo_evidence_text(writer, ",\"version\":");
    evo_evidence_u64(writer, view->catalogue_version);
    evo_evidence_text(writer, "},\n\"record_identity\":");
    evo_evidence_json_string(writer, view->record_identity);
    evo_evidence_text(writer, ",\n\"transformation\":{\"identity\":");
    evo_evidence_json_string(writer, view->transformation_identity);
    evo_evidence_text(writer, ",\"version\":");
    evo_evidence_u64(writer, view->transformation_version);
    evo_evidence_text(writer, "},\n\"parameters\":");
    evo_evidence_parameters(writer, view);
    evo_evidence_text(writer, ",\n\"provider\":{\"identity\":");
    evo_evidence_json_string(writer, view->provider_identity);
    evo_evidence_text(writer, ",\"version\":");
    evo_evidence_u64(writer, view->provider_version);
    evo_evidence_text(writer, ",\"clang\":");
    evo_evidence_json_string(writer, view->clang_identity);
    evo_evidence_text(
        writer,
        "},\n\"target\":{\"location_identity\":");
    evo_evidence_json_string(writer, view->target.location_identity);
    evo_evidence_text(writer, ",\"file\":");
    evo_evidence_json_string(writer, view->target.file);
    evo_evidence_text(writer, ",\"line\":");
    evo_evidence_u64(writer, view->target.line);
    evo_evidence_text(writer, ",\"column\":");
    evo_evidence_u64(writer, view->target.column);
    evo_evidence_text(writer, ",\"end_line\":");
    evo_evidence_u64(writer, view->target.end_line);
    evo_evidence_text(writer, ",\"end_column\":");
    evo_evidence_u64(writer, view->target.end_column);
    evo_evidence_text(writer, ",\"kind\":\"spelling\",\"spelling_identity\":");
    evo_evidence_json_nullable(writer, view->target.spelling_identity);
    evo_evidence_text(writer, ",\"byte_range\":{\"start\":");
    evo_evidence_u64(writer, view->edit.before_start);
    evo_evidence_text(writer, ",\"end\":");
    evo_evidence_u64(writer, view->edit.before_end);
    evo_evidence_text(writer, "}},\n\"ast\":{\"form\":");
    evo_evidence_json_string(
        writer, evo_project_transformation_ast_form_name(view->ast_form));
    evo_evidence_text(writer, ",\"operator\":");
    evo_evidence_json_string(
        writer,
        evo_project_transformation_operator_name(view->operator_kind));
    evo_evidence_text(writer, ",\"condition_context\":");
    evo_evidence_json_string(
        writer,
        evo_project_transformation_condition_context_name(
            view->condition_context));
    evo_evidence_text(writer, ",\"ranges\":{\"primary\":");
    evo_evidence_json_range(writer, view->ast_primary);
    evo_evidence_text(writer, ",\"duplicate_primary\":");
    evo_evidence_json_range(writer, view->ast_duplicate_primary);
    evo_evidence_text(writer, ",\"operand\":");
    evo_evidence_json_range(writer, view->ast_operand);
    evo_evidence_text(writer, ",\"literal\":");
    evo_evidence_json_range(writer, view->ast_literal);
    evo_evidence_text(writer, "},\"primary_declaration_identity\":");
    evo_evidence_json_nullable(writer, view->primary_declaration_identity);
    evo_evidence_text(writer, ",\"duplicate_declaration_identity\":");
    evo_evidence_json_nullable(writer, view->duplicate_declaration_identity);
    evo_evidence_text(writer, ",\"literal_value\":");
    evo_evidence_u64(writer, view->literal_value);
    evo_evidence_text(writer, ",\"result_width_bits\":");
    evo_evidence_u64(writer, view->result_width_bits);
    evo_evidence_text(
        writer,
        ",\"facts\":{\"primary_plain_identifier\":");
    evo_evidence_text(writer, view->primary_plain_identifier ? "true" : "false");
    evo_evidence_text(writer, ",\"volatile_access\":");
    evo_evidence_text(writer, view->volatile_access ? "true" : "false");
    evo_evidence_text(writer, ",\"result_unsigned_integer\":");
    evo_evidence_text(writer, view->result_unsigned_integer ? "true" : "false");
    evo_evidence_text(writer, ",\"result_type_matches_primary\":");
    evo_evidence_text(
        writer, view->result_type_matches_primary ? "true" : "false");
    evo_evidence_text(writer, ",\"scalar_operand\":");
    evo_evidence_text(writer, view->scalar_operand ? "true" : "false");
    evo_evidence_text(writer, ",\"contains_macro\":");
    evo_evidence_text(writer, view->contains_macro ? "true" : "false");
    evo_evidence_text(writer, ",\"contains_comment\":");
    evo_evidence_text(writer, view->contains_comment ? "true" : "false");
    evo_evidence_text(writer, ",\"contains_preprocessor\":");
    evo_evidence_text(writer, view->contains_preprocessor ? "true" : "false");
    evo_evidence_text(writer, ",\"language_extension\":");
    evo_evidence_text(writer, view->language_extension ? "true" : "false");
    evo_evidence_text(writer, ",\"ambiguous_target\":");
    evo_evidence_text(writer, view->ambiguous_target ? "true" : "false");
    evo_evidence_text(writer, ",\"alias_assumption_required\":");
    evo_evidence_text(
        writer, view->alias_assumption_required ? "true" : "false");
    evo_evidence_text(writer, "}},\n\"disposition\":");
    evo_evidence_json_string(writer, evo_disposition_name(view->disposition));
    evo_evidence_text(writer, ",\n\"edit\":{\"before\":{\"start\":");
    evo_evidence_u64(writer, view->edit.before_start);
    evo_evidence_text(writer, ",\"end\":");
    evo_evidence_u64(writer, view->edit.before_end);
    evo_evidence_text(writer, ",\"size\":");
    evo_evidence_u64(writer, view->edit.before_size);
    evo_evidence_text(writer, ",\"fingerprint\":");
    evo_evidence_json_string(writer, view->edit.before_fingerprint);
    evo_evidence_text(writer, ",\"text\":");
    evo_evidence_json_string(writer, view->edit.before_text);
    evo_evidence_text(writer, "},\"after\":{\"start\":");
    evo_evidence_u64(writer, view->edit.after_start);
    evo_evidence_text(writer, ",\"end\":");
    evo_evidence_u64(writer, view->edit.after_end);
    evo_evidence_text(writer, ",\"replacement_size\":");
    evo_evidence_u64(writer, view->edit.replacement_size);
    evo_evidence_text(writer, ",\"replacement_fingerprint\":");
    evo_evidence_json_string(writer, view->edit.replacement_fingerprint);
    evo_evidence_text(writer, ",\"replacement_text\":");
    evo_evidence_json_nullable(writer, view->edit.replacement_text);
    evo_evidence_text(writer, "}},\n\"formatting_policy\":");
    evo_evidence_json_string(writer, view->formatting_policy);
    evo_evidence_text(writer, ",\n\"idempotence_policy\":");
    evo_evidence_json_string(writer, view->idempotence_policy);
    evo_evidence_text(writer, ",\n\"semantic_assumptions\":");
    evo_evidence_text_array(
        writer, view->semantic_assumptions, view->semantic_assumption_count);
    evo_evidence_text(writer, ",\n\"validation_obligations\":");
    evo_evidence_text_array(
        writer,
        view->validation_obligations,
        view->validation_obligation_count);
    evo_evidence_text(
        writer,
        ",\n\"human_readable_abstraction\":{"
        "\"reference_form\":\"exact-source-edit-and-direct-dispatch\","
        "\"projection\":\"complete-application-json-and-derived-markdown\","
        "\"complete\":true,\"probabilistic_authority\":false},"
        "\n\"snapshot_modified\":false,"
        "\n\"candidate_materialized\":false\n}\n");
}

static void evo_evidence_markdown_value(
    evo_transform_evidence_writer_t *writer,
    const char *value)
{
    size_t index;

    if (value == NULL) {
        evo_evidence_text(writer, "none");
        return;
    }
    for (index = 0U; value[index] != '\0'; index += 1U) {
        if (value[index] == '\\' || value[index] == '|' ||
            value[index] == '`') {
            evo_evidence_char(writer, '\\');
        }
        if (value[index] == '\n' || value[index] == '\r') {
            evo_evidence_char(writer, ' ');
        } else {
            evo_evidence_char(writer, value[index]);
        }
    }
}

static void evo_evidence_markdown_range(
    evo_transform_evidence_writer_t *writer,
    evo_project_transformation_byte_range_t range)
{
    if (range.start == 0U && range.end == 0U) {
        evo_evidence_text(writer, "none");
        return;
    }
    evo_evidence_char(writer, '[');
    evo_evidence_u64(writer, range.start);
    evo_evidence_text(writer, ", ");
    evo_evidence_u64(writer, range.end);
    evo_evidence_char(writer, ')');
}

static void evo_generate_application_markdown(
    evo_transform_evidence_writer_t *writer,
    const evo_project_transformation_application_t *view)
{
    size_t index;

    evo_evidence_text(writer, "# EVO C Transformation Application\n\n- Record: `");
    evo_evidence_markdown_value(writer, view->record_identity);
    evo_evidence_text(writer, "`\n- Transformation: `");
    evo_evidence_markdown_value(writer, view->transformation_identity);
    evo_evidence_text(writer, "` version ");
    evo_evidence_u64(writer, view->transformation_version);
    evo_evidence_text(writer, "\n- Parameters: ");
    if (view->parameter_count == 0U) {
        evo_evidence_text(writer, "none");
    }
    for (index = 0U; index < view->parameter_count; index += 1U) {
        const evo_project_recipe_parameter_value_t *parameter =
            &view->parameters[index];

        if (index > 0U) {
            evo_evidence_text(writer, ", ");
        }
        evo_evidence_char(writer, '`');
        evo_evidence_markdown_value(writer, parameter->identity);
        evo_evidence_text(writer, "=");
        if (parameter->kind == EVO_PROJECT_RECIPE_PARAMETER_INTEGER) {
            char value[32];
            const int written = evo_project_format(
                value,
                sizeof(value),
                "%lld",
                (long long)parameter->integer_value);

            if (written <= 0 || (size_t)written >= sizeof(value)) {
                writer->failed = true;
            } else {
                evo_evidence_bytes(writer, value, (size_t)written);
            }
        } else if (parameter->kind == EVO_PROJECT_RECIPE_PARAMETER_BOOLEAN) {
            evo_evidence_text(
                writer, parameter->boolean_value ? "true" : "false");
        } else {
            evo_evidence_markdown_value(writer, parameter->choice_value);
        }
        evo_evidence_char(writer, '`');
    }
    evo_evidence_text(writer, "\n- Provider: `");
    evo_evidence_markdown_value(writer, view->provider_identity);
    evo_evidence_text(writer, "` version ");
    evo_evidence_u64(writer, view->provider_version);
    evo_evidence_text(writer, " (`");
    evo_evidence_markdown_value(writer, view->clang_identity);
    evo_evidence_text(writer, "`)\n- Target: `");
    evo_evidence_markdown_value(writer, view->target.file);
    evo_evidence_text(writer, "` bytes ");
    evo_evidence_u64(writer, view->edit.before_start);
    evo_evidence_text(writer, " through ");
    evo_evidence_u64(writer, view->edit.before_end);
    evo_evidence_text(writer, " (half-open)\n- AST form: `");
    evo_evidence_markdown_value(
        writer, evo_project_transformation_ast_form_name(view->ast_form));
    evo_evidence_text(writer, "`\n- AST ranges: primary ");
    evo_evidence_markdown_range(writer, view->ast_primary);
    evo_evidence_text(writer, ", duplicate primary ");
    evo_evidence_markdown_range(writer, view->ast_duplicate_primary);
    evo_evidence_text(writer, ", operand ");
    evo_evidence_markdown_range(writer, view->ast_operand);
    evo_evidence_text(writer, ", literal ");
    evo_evidence_markdown_range(writer, view->ast_literal);
    evo_evidence_text(writer, "\n- AST declarations: primary `");
    evo_evidence_markdown_value(writer, view->primary_declaration_identity);
    evo_evidence_text(writer, "`, duplicate `");
    evo_evidence_markdown_value(writer, view->duplicate_declaration_identity);
    evo_evidence_text(writer, "`\n- AST literal/result width: ");
    evo_evidence_u64(writer, view->literal_value);
    evo_evidence_text(writer, " / ");
    evo_evidence_u64(writer, view->result_width_bits);
    evo_evidence_text(
        writer,
        " bits\n- AST facts: primary-plain-identifier=");
    evo_evidence_text(writer, view->primary_plain_identifier ? "yes" : "no");
    evo_evidence_text(writer, ", volatile-access=");
    evo_evidence_text(writer, view->volatile_access ? "yes" : "no");
    evo_evidence_text(writer, ", unsigned-result=");
    evo_evidence_text(writer, view->result_unsigned_integer ? "yes" : "no");
    evo_evidence_text(writer, ", result-type-matches-primary=");
    evo_evidence_text(
        writer, view->result_type_matches_primary ? "yes" : "no");
    evo_evidence_text(writer, ", scalar-operand=");
    evo_evidence_text(writer, view->scalar_operand ? "yes" : "no");
    evo_evidence_text(
        writer,
        ", macro=no, comment=no, preprocessor=no, extension=no, "
        "ambiguous-target=no, alias-assumption=no\n- Disposition: `");
    evo_evidence_markdown_value(writer, evo_disposition_name(view->disposition));
    evo_evidence_text(writer, "`\n- Before: `");
    evo_evidence_markdown_value(writer, view->edit.before_text);
    evo_evidence_text(writer, "`\n- Replacement: `");
    evo_evidence_markdown_value(writer, view->edit.replacement_text);
    evo_evidence_text(writer, "`\n- Formatting: `");
    evo_evidence_markdown_value(writer, view->formatting_policy);
    evo_evidence_text(writer, "`\n- Snapshot modified: no\n- Candidate materialized: no\n\n");
    evo_evidence_text(writer, "## Semantic assumptions\n\n");
    for (index = 0U; index < view->semantic_assumption_count; index += 1U) {
        evo_evidence_text(writer, "- `");
        evo_evidence_markdown_value(writer, view->semantic_assumptions[index]);
        evo_evidence_text(writer, "`\n");
    }
    evo_evidence_text(writer, "\n## Validation obligations\n\n");
    for (index = 0U; index < view->validation_obligation_count; index += 1U) {
        evo_evidence_text(writer, "- `");
        evo_evidence_markdown_value(writer, view->validation_obligations[index]);
        evo_evidence_text(writer, "`\n");
    }
    evo_evidence_text(
        writer,
        "\nThe exact before range and replacement are authority. No cache, index, "
        "filter, probabilistic structure, source write, or candidate workspace "
        "participated in this application.\n");
}

static uint64_t evo_application_fingerprint(
    const evo_project_transformation_application_t *view)
{
    evo_project_fingerprint_t fingerprint;
    size_t index;

    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_string(
        &fingerprint, "catalyst.evo-c-transformation-application.v1");
    evo_project_fingerprint_string(&fingerprint, view->baseline_fingerprint);
    evo_project_fingerprint_string(&fingerprint, view->analysis_fingerprint);
    evo_project_fingerprint_string(&fingerprint, view->recipe_fingerprint);
    evo_project_fingerprint_string(&fingerprint, view->catalogue_identity);
    evo_project_fingerprint_u64(&fingerprint, view->catalogue_version);
    evo_project_fingerprint_string(&fingerprint, view->record_identity);
    evo_project_fingerprint_string(&fingerprint, view->transformation_identity);
    evo_project_fingerprint_u64(&fingerprint, view->transformation_version);
    evo_project_fingerprint_u64(
        &fingerprint, (uint64_t)view->parameter_count);
    for (index = 0U; index < view->parameter_count; index += 1U) {
        const evo_project_recipe_parameter_value_t *parameter =
            &view->parameters[index];

        evo_project_fingerprint_string(&fingerprint, parameter->identity);
        evo_project_fingerprint_u64(&fingerprint, (uint64_t)parameter->kind);
        if (parameter->kind == EVO_PROJECT_RECIPE_PARAMETER_INTEGER) {
            evo_project_fingerprint_u64(
                &fingerprint, (uint64_t)parameter->integer_value);
        } else if (parameter->kind == EVO_PROJECT_RECIPE_PARAMETER_BOOLEAN) {
            evo_project_fingerprint_u64(
                &fingerprint, parameter->boolean_value ? 1U : 0U);
        } else {
            evo_project_fingerprint_string(
                &fingerprint, parameter->choice_value);
        }
    }
    evo_project_fingerprint_string(&fingerprint, view->provider_identity);
    evo_project_fingerprint_u64(&fingerprint, view->provider_version);
    evo_project_fingerprint_string(&fingerprint, view->clang_identity);
    evo_project_fingerprint_string(&fingerprint, view->target.location_identity);
    evo_project_fingerprint_string(&fingerprint, view->target.file);
    evo_project_fingerprint_u64(&fingerprint, view->edit.before_start);
    evo_project_fingerprint_u64(&fingerprint, view->edit.before_end);
    evo_project_fingerprint_u64(&fingerprint, (uint64_t)view->ast_form);
    evo_project_fingerprint_u64(&fingerprint, (uint64_t)view->operator_kind);
    evo_project_fingerprint_u64(
        &fingerprint, (uint64_t)view->condition_context);
    evo_project_fingerprint_u64(&fingerprint, view->ast_primary.start);
    evo_project_fingerprint_u64(&fingerprint, view->ast_primary.end);
    evo_project_fingerprint_u64(
        &fingerprint, view->ast_duplicate_primary.start);
    evo_project_fingerprint_u64(
        &fingerprint, view->ast_duplicate_primary.end);
    evo_project_fingerprint_u64(&fingerprint, view->ast_operand.start);
    evo_project_fingerprint_u64(&fingerprint, view->ast_operand.end);
    evo_project_fingerprint_u64(&fingerprint, view->ast_literal.start);
    evo_project_fingerprint_u64(&fingerprint, view->ast_literal.end);
    if (view->primary_declaration_identity == NULL) {
        evo_project_fingerprint_u64(&fingerprint, 0U);
    } else {
        evo_project_fingerprint_string(
            &fingerprint, view->primary_declaration_identity);
    }
    if (view->duplicate_declaration_identity == NULL) {
        evo_project_fingerprint_u64(&fingerprint, 0U);
    } else {
        evo_project_fingerprint_string(
            &fingerprint, view->duplicate_declaration_identity);
    }
    evo_project_fingerprint_u64(&fingerprint, view->literal_value);
    evo_project_fingerprint_u64(&fingerprint, view->result_width_bits);
    evo_project_fingerprint_u64(
        &fingerprint, view->primary_plain_identifier ? 1U : 0U);
    evo_project_fingerprint_u64(
        &fingerprint, view->volatile_access ? 1U : 0U);
    evo_project_fingerprint_u64(
        &fingerprint, view->result_unsigned_integer ? 1U : 0U);
    evo_project_fingerprint_u64(
        &fingerprint, view->result_type_matches_primary ? 1U : 0U);
    evo_project_fingerprint_u64(
        &fingerprint, view->scalar_operand ? 1U : 0U);
    evo_project_fingerprint_u64(
        &fingerprint, view->contains_macro ? 1U : 0U);
    evo_project_fingerprint_u64(
        &fingerprint, view->contains_comment ? 1U : 0U);
    evo_project_fingerprint_u64(
        &fingerprint, view->contains_preprocessor ? 1U : 0U);
    evo_project_fingerprint_u64(
        &fingerprint, view->language_extension ? 1U : 0U);
    evo_project_fingerprint_u64(
        &fingerprint, view->ambiguous_target ? 1U : 0U);
    evo_project_fingerprint_u64(
        &fingerprint, view->alias_assumption_required ? 1U : 0U);
    evo_project_fingerprint_u64(&fingerprint, (uint64_t)view->disposition);
    evo_project_fingerprint_string(&fingerprint, view->edit.before_text);
    evo_project_fingerprint_u64(&fingerprint, view->edit.replacement_size);
    if (view->edit.replacement_text != NULL) {
        evo_project_fingerprint_string(
            &fingerprint, view->edit.replacement_text);
    } else {
        evo_project_fingerprint_u64(&fingerprint, 0U);
    }
    evo_project_fingerprint_string(&fingerprint, view->formatting_policy);
    evo_project_fingerprint_string(&fingerprint, view->idempotence_policy);
    evo_project_fingerprint_u64(
        &fingerprint, (uint64_t)view->semantic_assumption_count);
    for (index = 0U; index < view->semantic_assumption_count; index += 1U) {
        evo_project_fingerprint_string(
            &fingerprint, view->semantic_assumptions[index]);
    }
    evo_project_fingerprint_u64(
        &fingerprint, (uint64_t)view->validation_obligation_count);
    for (index = 0U; index < view->validation_obligation_count; index += 1U) {
        evo_project_fingerprint_string(
            &fingerprint, view->validation_obligations[index]);
    }
    return fingerprint.value;
}

static evo_project_transformation_status_t evo_generate_owned(
    size_t capacity,
    void (*generate)(
        evo_transform_evidence_writer_t *,
        const evo_project_transformation_application_t *),
    const evo_project_transformation_application_t *view,
    char **bytes,
    size_t *size)
{
    evo_transform_evidence_writer_t measure = {NULL, capacity, 0U, false};
    evo_transform_evidence_writer_t output;

    generate(&measure, view);
    if (measure.failed || measure.size == SIZE_MAX) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT;
    }
    *bytes = evo_project_allocate_zeroed(measure.size + 1U, sizeof(**bytes));
    if (*bytes == NULL) {
        return EVO_PROJECT_TRANSFORMATION_ERROR_OUT_OF_MEMORY;
    }
    output = (evo_transform_evidence_writer_t){
        *bytes, measure.size, 0U, false};
    generate(&output, view);
    if (output.failed || output.size != measure.size) {
        evo_project_release(*bytes);
        *bytes = NULL;
        return EVO_PROJECT_TRANSFORMATION_ERROR_STATE;
    }
    (*bytes)[output.size] = '\0';
    *size = output.size;
    return EVO_PROJECT_TRANSFORMATION_SUCCESS;
}

evo_project_transformation_status_t
evo_project_transformation_application_generate_evidence(
    const evo_project_transformation_limits_t *limits,
    size_t manifest_evidence_limit,
    evo_project_transformation_application_owner_t *owner)
{
    const size_t effective_total =
        limits->max_total_bytes < manifest_evidence_limit
            ? limits->max_total_bytes
            : manifest_evidence_limit;
    evo_project_transformation_status_t status;

    owner->application_fingerprint = evo_application_fingerprint(&owner->view);
    evo_project_fingerprint_format(
        owner->application_fingerprint,
        owner->view.application_fingerprint);
    status = evo_generate_owned(
        limits->max_application_bytes,
        evo_generate_application_json,
        &owner->view,
        &owner->canonical_json,
        &owner->canonical_json_size);
    if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        return status;
    }
    status = evo_generate_owned(
        limits->max_audit_bytes,
        evo_generate_application_markdown,
        &owner->view,
        &owner->audit_markdown,
        &owner->audit_markdown_size);
    if (status != EVO_PROJECT_TRANSFORMATION_SUCCESS) {
        evo_project_release(owner->canonical_json);
        owner->canonical_json = NULL;
        owner->canonical_json_size = 0U;
        return status;
    }
    if (owner->canonical_json_size > effective_total ||
        owner->audit_markdown_size >
            effective_total - owner->canonical_json_size) {
        evo_project_release(owner->audit_markdown);
        evo_project_release(owner->canonical_json);
        owner->audit_markdown = NULL;
        owner->canonical_json = NULL;
        owner->audit_markdown_size = 0U;
        owner->canonical_json_size = 0U;
        return EVO_PROJECT_TRANSFORMATION_ERROR_RESOURCE_LIMIT;
    }
    owner->view.canonical_json_size = owner->canonical_json_size;
    owner->view.canonical_json = owner->canonical_json;
    owner->view.audit_markdown_size = owner->audit_markdown_size;
    owner->view.audit_markdown = owner->audit_markdown;
    return EVO_PROJECT_TRANSFORMATION_SUCCESS;
}
