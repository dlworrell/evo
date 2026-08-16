#include "internal/project_recipe_encoding.h"

#include "internal/project_fingerprint.h"
#include "internal/project_json.h"
#include "internal/project_runtime.h"

#include <inttypes.h>
#include <string.h>

static const unsigned char evo_project_recipe_genome_magic[8] = {
    (unsigned char)'E',
    (unsigned char)'V',
    (unsigned char)'O',
    (unsigned char)'R',
    (unsigned char)'C',
    (unsigned char)'P',
    (unsigned char)'G',
    (unsigned char)'1'};

typedef struct evo_project_recipe_writer {
    char *bytes;
    size_t capacity;
    size_t size;
    bool failed;
} evo_project_recipe_writer_t;

typedef struct evo_project_recipe_decoded_proposals {
    evo_project_recipe_proposal_record_t *views;
    char **record_identities;
    char **target_identities;
    char **transformation_identities;
    evo_project_recipe_parameter_value_t **parameters;
    size_t count;
} evo_project_recipe_decoded_proposals_t;

static void evo_project_recipe_writer_bytes(
    evo_project_recipe_writer_t *writer,
    const char *bytes,
    size_t byte_count)
{
    size_t index;

    if (writer->failed) {
        return;
    }
    if (byte_count > writer->capacity - writer->size) {
        writer->failed = true;
        return;
    }
    for (index = 0U; index < byte_count; index += 1U) {
        writer->bytes[writer->size] = bytes[index];
        writer->size += 1U;
    }
}

static void evo_project_recipe_writer_text(
    evo_project_recipe_writer_t *writer,
    const char *text)
{
    evo_project_recipe_writer_bytes(writer, text, strlen(text));
}

static void evo_project_recipe_writer_char(
    evo_project_recipe_writer_t *writer,
    char value)
{
    evo_project_recipe_writer_bytes(writer, &value, 1U);
}

static void evo_project_recipe_writer_u64(
    evo_project_recipe_writer_t *writer,
    uint64_t value)
{
    char text[32];
    const int written = evo_project_format(
        text, sizeof(text), "%llu", (unsigned long long)value);

    if (written <= 0 || (size_t)written >= sizeof(text)) {
        writer->failed = true;
        return;
    }
    evo_project_recipe_writer_bytes(writer, text, (size_t)written);
}

static void evo_project_recipe_writer_i64(
    evo_project_recipe_writer_t *writer,
    int64_t value)
{
    char text[32];
    const int written = evo_project_format(
        text, sizeof(text), "%lld", (long long)value);

    if (written <= 0 || (size_t)written >= sizeof(text)) {
        writer->failed = true;
        return;
    }
    evo_project_recipe_writer_bytes(writer, text, (size_t)written);
}

static void evo_project_recipe_writer_json_string(
    evo_project_recipe_writer_t *writer,
    const char *value)
{
    size_t index;

    evo_project_recipe_writer_char(writer, '"');
    for (index = 0U; value[index] != '\0'; index += 1U) {
        const unsigned char byte = (unsigned char)value[index];

        if (value[index] == '"') {
            evo_project_recipe_writer_text(writer, "\\\"");
        } else if (value[index] == '\\') {
            evo_project_recipe_writer_text(writer, "\\\\");
        } else if (value[index] == '\n') {
            evo_project_recipe_writer_text(writer, "\\n");
        } else if (value[index] == '\r') {
            evo_project_recipe_writer_text(writer, "\\r");
        } else if (value[index] == '\t') {
            evo_project_recipe_writer_text(writer, "\\t");
        } else if (byte < 0x20U) {
            writer->failed = true;
        } else {
            evo_project_recipe_writer_char(writer, value[index]);
        }
    }
    evo_project_recipe_writer_char(writer, '"');
}

static void evo_project_recipe_writer_json_nullable(
    evo_project_recipe_writer_t *writer,
    const char *value)
{
    if (value == NULL) {
        evo_project_recipe_writer_text(writer, "null");
    } else {
        evo_project_recipe_writer_json_string(writer, value);
    }
}

static void evo_project_recipe_writer_markdown(
    evo_project_recipe_writer_t *writer,
    const char *value)
{
    size_t index;

    if (value == NULL) {
        evo_project_recipe_writer_text(writer, "none");
        return;
    }
    for (index = 0U; value[index] != '\0'; index += 1U) {
        if (value[index] == '\\' || value[index] == '|' ||
            value[index] == '`') {
            evo_project_recipe_writer_char(writer, '\\');
        }
        if (value[index] == '\n' || value[index] == '\r') {
            evo_project_recipe_writer_char(writer, ' ');
        } else {
            evo_project_recipe_writer_char(writer, value[index]);
        }
    }
}

static const char *evo_project_recipe_parameter_kind_name(
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

static const char *evo_project_recipe_location_kind_name(
    evo_project_source_location_kind_t kind)
{
    return kind == EVO_PROJECT_LOCATION_MACRO_EXPANSION
               ? "macro-expansion"
               : "spelling";
}

static void evo_project_recipe_json_string_array(
    evo_project_recipe_writer_t *writer,
    const char *const *values,
    size_t count)
{
    size_t index;

    evo_project_recipe_writer_char(writer, '[');
    for (index = 0U; index < count; index += 1U) {
        if (index > 0U) {
            evo_project_recipe_writer_char(writer, ',');
        }
        evo_project_recipe_writer_json_string(writer, values[index]);
    }
    evo_project_recipe_writer_char(writer, ']');
}

static void evo_project_recipe_generate_parameter_json(
    evo_project_recipe_writer_t *writer,
    const evo_project_recipe_parameter_value_t *parameter)
{
    evo_project_recipe_writer_text(writer, "{\"identity\":");
    evo_project_recipe_writer_json_string(writer, parameter->identity);
    evo_project_recipe_writer_text(writer, ",\"kind\":");
    evo_project_recipe_writer_json_string(
        writer, evo_project_recipe_parameter_kind_name(parameter->kind));
    evo_project_recipe_writer_text(writer, ",\"integer_value\":");
    if (parameter->kind == EVO_PROJECT_RECIPE_PARAMETER_INTEGER) {
        evo_project_recipe_writer_i64(writer, parameter->integer_value);
    } else {
        evo_project_recipe_writer_text(writer, "null");
    }
    evo_project_recipe_writer_text(writer, ",\"boolean_value\":");
    if (parameter->kind == EVO_PROJECT_RECIPE_PARAMETER_BOOLEAN) {
        evo_project_recipe_writer_text(
            writer, parameter->boolean_value ? "true" : "false");
    } else {
        evo_project_recipe_writer_text(writer, "null");
    }
    evo_project_recipe_writer_text(writer, ",\"choice_value\":");
    if (parameter->kind == EVO_PROJECT_RECIPE_PARAMETER_CHOICE) {
        evo_project_recipe_writer_json_string(writer, parameter->choice_value);
    } else {
        evo_project_recipe_writer_text(writer, "null");
    }
    evo_project_recipe_writer_char(writer, '}');
}

static void evo_project_recipe_generate_record_json(
    evo_project_recipe_writer_t *writer,
    const evo_project_recipe_record_t *record)
{
    size_t index;

    evo_project_recipe_writer_text(writer, "{\"identity\":");
    evo_project_recipe_writer_json_string(writer, record->identity);
    evo_project_recipe_writer_text(writer, ",\"baseline_fingerprint\":");
    evo_project_recipe_writer_json_string(writer, record->baseline_fingerprint);
    evo_project_recipe_writer_text(writer, ",\"analysis_fingerprint\":");
    evo_project_recipe_writer_json_string(writer, record->analysis_fingerprint);
    evo_project_recipe_writer_text(
        writer, ",\"catalogue\":{\"identity\":");
    evo_project_recipe_writer_json_string(writer, record->catalogue_identity);
    evo_project_recipe_writer_text(writer, ",\"version\":");
    evo_project_recipe_writer_u64(writer, record->catalogue_version);
    evo_project_recipe_writer_text(
        writer, "},\"target\":{\"location_identity\":");
    evo_project_recipe_writer_json_string(
        writer, record->target.location_identity);
    evo_project_recipe_writer_text(writer, ",\"file\":");
    evo_project_recipe_writer_json_string(writer, record->target.file);
    evo_project_recipe_writer_text(writer, ",\"line\":");
    evo_project_recipe_writer_u64(writer, record->target.line);
    evo_project_recipe_writer_text(writer, ",\"column\":");
    evo_project_recipe_writer_u64(writer, record->target.column);
    evo_project_recipe_writer_text(writer, ",\"end_line\":");
    evo_project_recipe_writer_u64(writer, record->target.end_line);
    evo_project_recipe_writer_text(writer, ",\"end_column\":");
    evo_project_recipe_writer_u64(writer, record->target.end_column);
    evo_project_recipe_writer_text(writer, ",\"kind\":");
    evo_project_recipe_writer_json_string(
        writer, evo_project_recipe_location_kind_name(record->target.kind));
    evo_project_recipe_writer_text(writer, ",\"spelling_identity\":");
    evo_project_recipe_writer_json_nullable(
        writer, record->target.spelling_identity);
    evo_project_recipe_writer_text(
        writer, "},\"transformation\":{\"identity\":");
    evo_project_recipe_writer_json_string(
        writer, record->transformation_identity);
    evo_project_recipe_writer_text(writer, ",\"version\":");
    evo_project_recipe_writer_u64(writer, record->transformation_version);
    evo_project_recipe_writer_text(writer, "},\"parameters\":[");
    for (index = 0U; index < record->parameter_count; index += 1U) {
        if (index > 0U) {
            evo_project_recipe_writer_char(writer, ',');
        }
        evo_project_recipe_generate_parameter_json(
            writer, &record->parameters[index]);
    }
    evo_project_recipe_writer_text(writer, "],\"preconditions\":");
    evo_project_recipe_json_string_array(
        writer, record->preconditions, record->precondition_count);
    evo_project_recipe_writer_text(writer, ",\"dependencies\":[");
    for (index = 0U; index < record->dependency_count; index += 1U) {
        if (index > 0U) {
            evo_project_recipe_writer_char(writer, ',');
        }
        evo_project_recipe_writer_text(writer, "{\"record_identity\":");
        evo_project_recipe_writer_json_string(
            writer, record->dependencies[index].record_identity);
        evo_project_recipe_writer_text(
            writer, ",\"transformation_identity\":");
        evo_project_recipe_writer_json_string(
            writer, record->dependencies[index].transformation_identity);
        evo_project_recipe_writer_text(writer, ",\"version\":");
        evo_project_recipe_writer_u64(
            writer, record->dependencies[index].transformation_version);
        evo_project_recipe_writer_char(writer, '}');
    }
    evo_project_recipe_writer_text(writer, "],\"conflicts\":[");
    for (index = 0U; index < record->conflict_count; index += 1U) {
        if (index > 0U) {
            evo_project_recipe_writer_char(writer, ',');
        }
        evo_project_recipe_writer_text(
            writer, "{\"transformation_identity\":");
        evo_project_recipe_writer_json_string(
            writer, record->conflicts[index].identity);
        evo_project_recipe_writer_text(writer, ",\"version\":");
        evo_project_recipe_writer_u64(
            writer, record->conflicts[index].implementation_version);
        evo_project_recipe_writer_char(writer, '}');
    }
    evo_project_recipe_writer_text(
        writer, "],\"provenance\":{\"opportunity_rank\":");
    evo_project_recipe_writer_u64(writer, record->opportunity_rank);
    evo_project_recipe_writer_text(writer, ",\"compiler_records\":");
    evo_project_recipe_json_string_array(
        writer,
        record->compiler_record_identities,
        record->compiler_record_count);
    evo_project_recipe_writer_text(writer, ",\"runtime_records\":");
    evo_project_recipe_json_string_array(
        writer,
        record->runtime_record_identities,
        record->runtime_record_count);
    evo_project_recipe_writer_text(writer, "}}");
}

static void evo_project_recipe_generate_json(
    const evo_project_recipe_owner_t *owner,
    evo_project_recipe_writer_t *writer)
{
    size_t index;

    evo_project_recipe_writer_text(
        writer,
        "{\n\"schema\":\"catalyst.evo-project-recipe.v1\",\n"
        "\"schema_version\":1,\n\"baseline_fingerprint\":");
    evo_project_recipe_writer_json_string(
        writer, owner->baseline_fingerprint);
    evo_project_recipe_writer_text(writer, ",\n\"analysis_fingerprint\":");
    evo_project_recipe_writer_json_string(
        writer, owner->analysis_fingerprint);
    evo_project_recipe_writer_text(
        writer, ",\n\"catalogue\":{\"identity\":");
    evo_project_recipe_writer_json_string(writer, owner->catalogue_identity);
    evo_project_recipe_writer_text(writer, ",\"version\":");
    evo_project_recipe_writer_u64(writer, owner->catalogue_version);
    evo_project_recipe_writer_text(writer, "},\n\"records\":[");
    for (index = 0U; index < owner->record_count; index += 1U) {
        if (index > 0U) {
            evo_project_recipe_writer_char(writer, ',');
        }
        evo_project_recipe_writer_text(writer, "\n");
        evo_project_recipe_generate_record_json(writer, &owner->records[index]);
    }
    if (owner->record_count > 0U) {
        evo_project_recipe_writer_char(writer, '\n');
    }
    evo_project_recipe_writer_text(
        writer,
        "],\n\"human_readable_abstraction\":{"
        "\"reference_form\":\"canonical-json-record-array-and-direct-scans\","
        "\"projection\":\"embedded-canonical-json-and-derived-markdown\","
        "\"complete\":true,\"probabilistic_authority\":false},\n"
        "\"raw_source_bytes\":false\n}\n");
}

static void evo_project_recipe_markdown_text_list(
    evo_project_recipe_writer_t *writer,
    const char *title,
    const char *const *values,
    size_t count)
{
    size_t index;

    evo_project_recipe_writer_text(writer, "- ");
    evo_project_recipe_writer_text(writer, title);
    evo_project_recipe_writer_text(writer, ": ");
    if (count == 0U) {
        evo_project_recipe_writer_text(writer, "none\n");
        return;
    }
    for (index = 0U; index < count; index += 1U) {
        if (index > 0U) {
            evo_project_recipe_writer_text(writer, ", ");
        }
        evo_project_recipe_writer_char(writer, '`');
        evo_project_recipe_writer_markdown(writer, values[index]);
        evo_project_recipe_writer_char(writer, '`');
    }
    evo_project_recipe_writer_char(writer, '\n');
}

static void evo_project_recipe_generate_parameter_markdown(
    evo_project_recipe_writer_t *writer,
    const evo_project_recipe_parameter_value_t *parameter)
{
    evo_project_recipe_writer_text(writer, "| `");
    evo_project_recipe_writer_markdown(writer, parameter->identity);
    evo_project_recipe_writer_text(writer, "` | ");
    evo_project_recipe_writer_text(
        writer, evo_project_recipe_parameter_kind_name(parameter->kind));
    evo_project_recipe_writer_text(writer, " | ");
    if (parameter->kind == EVO_PROJECT_RECIPE_PARAMETER_INTEGER) {
        evo_project_recipe_writer_i64(writer, parameter->integer_value);
    } else if (parameter->kind == EVO_PROJECT_RECIPE_PARAMETER_BOOLEAN) {
        evo_project_recipe_writer_text(
            writer, parameter->boolean_value ? "true" : "false");
    } else {
        evo_project_recipe_writer_char(writer, '`');
        evo_project_recipe_writer_markdown(writer, parameter->choice_value);
        evo_project_recipe_writer_char(writer, '`');
    }
    evo_project_recipe_writer_text(writer, " |\n");
}

static void evo_project_recipe_generate_record_markdown(
    evo_project_recipe_writer_t *writer,
    const evo_project_recipe_record_t *record,
    size_t composition_index)
{
    size_t index;

    evo_project_recipe_writer_text(writer, "## Record ");
    evo_project_recipe_writer_u64(writer, (uint64_t)composition_index + 1U);
    evo_project_recipe_writer_text(writer, ": `");
    evo_project_recipe_writer_markdown(writer, record->identity);
    evo_project_recipe_writer_text(writer, "`\n\n");
    evo_project_recipe_writer_text(writer, "- Baseline: `");
    evo_project_recipe_writer_markdown(writer, record->baseline_fingerprint);
    evo_project_recipe_writer_text(writer, "`\n- Analysis: `");
    evo_project_recipe_writer_markdown(writer, record->analysis_fingerprint);
    evo_project_recipe_writer_text(writer, "`\n- Catalogue: `");
    evo_project_recipe_writer_markdown(writer, record->catalogue_identity);
    evo_project_recipe_writer_text(writer, "` version ");
    evo_project_recipe_writer_u64(writer, record->catalogue_version);
    evo_project_recipe_writer_text(writer, "\n- Transformation: `");
    evo_project_recipe_writer_markdown(
        writer, record->transformation_identity);
    evo_project_recipe_writer_text(writer, "` version ");
    evo_project_recipe_writer_u64(writer, record->transformation_version);
    evo_project_recipe_writer_text(writer, "\n- Target: `");
    evo_project_recipe_writer_markdown(writer, record->target.location_identity);
    evo_project_recipe_writer_text(writer, "` at `");
    evo_project_recipe_writer_markdown(writer, record->target.file);
    evo_project_recipe_writer_char(writer, ':');
    evo_project_recipe_writer_u64(writer, record->target.line);
    evo_project_recipe_writer_char(writer, ':');
    evo_project_recipe_writer_u64(writer, record->target.column);
    evo_project_recipe_writer_char(writer, '-');
    evo_project_recipe_writer_u64(writer, record->target.end_line);
    evo_project_recipe_writer_char(writer, ':');
    evo_project_recipe_writer_u64(writer, record->target.end_column);
    evo_project_recipe_writer_text(writer, "` (");
    evo_project_recipe_writer_text(
        writer, evo_project_recipe_location_kind_name(record->target.kind));
    evo_project_recipe_writer_text(writer, ", spelling `");
    evo_project_recipe_writer_markdown(
        writer, record->target.spelling_identity);
    evo_project_recipe_writer_text(writer, "`)\n- Opportunity rank: ");
    evo_project_recipe_writer_u64(writer, record->opportunity_rank);
    evo_project_recipe_writer_text(writer, "\n\n### Parameters\n\n");
    if (record->parameter_count == 0U) {
        evo_project_recipe_writer_text(writer, "None.\n\n");
    } else {
        evo_project_recipe_writer_text(
            writer, "| Identity | Kind | Value |\n|---|---|---|\n");
        for (index = 0U; index < record->parameter_count; index += 1U) {
            evo_project_recipe_generate_parameter_markdown(
                writer, &record->parameters[index]);
        }
        evo_project_recipe_writer_char(writer, '\n');
    }
    evo_project_recipe_markdown_text_list(
        writer,
        "Preconditions",
        record->preconditions,
        record->precondition_count);
    evo_project_recipe_writer_text(writer, "- Dependencies: ");
    if (record->dependency_count == 0U) {
        evo_project_recipe_writer_text(writer, "none\n");
    }
    for (index = 0U; index < record->dependency_count; index += 1U) {
        if (index > 0U) {
            evo_project_recipe_writer_text(writer, ", ");
        }
        evo_project_recipe_writer_char(writer, '`');
        evo_project_recipe_writer_markdown(
            writer, record->dependencies[index].record_identity);
        evo_project_recipe_writer_text(writer, "` (`");
        evo_project_recipe_writer_markdown(
            writer, record->dependencies[index].transformation_identity);
        evo_project_recipe_writer_text(writer, "` v");
        evo_project_recipe_writer_u64(
            writer, record->dependencies[index].transformation_version);
        evo_project_recipe_writer_char(writer, ')');
    }
    if (record->dependency_count > 0U) {
        evo_project_recipe_writer_char(writer, '\n');
    }
    evo_project_recipe_writer_text(writer, "- Declared conflicts: ");
    if (record->conflict_count == 0U) {
        evo_project_recipe_writer_text(writer, "none\n");
    }
    for (index = 0U; index < record->conflict_count; index += 1U) {
        if (index > 0U) {
            evo_project_recipe_writer_text(writer, ", ");
        }
        evo_project_recipe_writer_char(writer, '`');
        evo_project_recipe_writer_markdown(
            writer, record->conflicts[index].identity);
        evo_project_recipe_writer_text(writer, "` v");
        evo_project_recipe_writer_u64(
            writer, record->conflicts[index].implementation_version);
    }
    if (record->conflict_count > 0U) {
        evo_project_recipe_writer_char(writer, '\n');
    }
    evo_project_recipe_markdown_text_list(
        writer,
        "Compiler evidence",
        record->compiler_record_identities,
        record->compiler_record_count);
    evo_project_recipe_markdown_text_list(
        writer,
        "Runtime evidence",
        record->runtime_record_identities,
        record->runtime_record_count);
    evo_project_recipe_writer_char(writer, '\n');
}

static void evo_project_recipe_generate_markdown(
    const evo_project_recipe_owner_t *owner,
    evo_project_recipe_writer_t *writer)
{
    char fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    size_t index;

    evo_project_fingerprint_format(owner->recipe_fingerprint, fingerprint);
    evo_project_recipe_writer_text(
        writer,
        "# EVO Transformation Recipe\n\n"
        "- Schema: `catalyst.evo-project-recipe.v1`\n"
        "- Baseline: `");
    evo_project_recipe_writer_markdown(writer, owner->baseline_fingerprint);
    evo_project_recipe_writer_text(writer, "`\n- Analysis: `");
    evo_project_recipe_writer_markdown(writer, owner->analysis_fingerprint);
    evo_project_recipe_writer_text(writer, "`\n- Catalogue: `");
    evo_project_recipe_writer_markdown(writer, owner->catalogue_identity);
    evo_project_recipe_writer_text(writer, "` version ");
    evo_project_recipe_writer_u64(writer, owner->catalogue_version);
    evo_project_recipe_writer_text(writer, "\n- Recipe fingerprint: `");
    evo_project_recipe_writer_text(writer, fingerprint);
    evo_project_recipe_writer_text(writer, "`\n- Record count: ");
    evo_project_recipe_writer_u64(writer, owner->record_count);
    evo_project_recipe_writer_text(
        writer,
        "\n- Authority: canonical JSON record array and direct scans\n"
        "- Projection: complete\n"
        "- Probabilistic authority: false\n"
        "- Raw source bytes: false\n\n");
    if (owner->record_count == 0U) {
        evo_project_recipe_writer_text(
            writer, "This is the canonical no-op recipe.\n");
    }
    for (index = 0U; index < owner->record_count; index += 1U) {
        evo_project_recipe_generate_record_markdown(
            writer, &owner->records[index], index);
    }
}

static void evo_project_recipe_write_payload_size(
    unsigned char *genome,
    size_t payload_size)
{
    uint64_t value = (uint64_t)payload_size;
    unsigned int shift;

    for (shift = 0U; shift < 64U; shift += 8U) {
        genome[8U + (shift / 8U)] =
            (unsigned char)((value >> shift) & UINT64_C(0xff));
    }
}

static size_t evo_project_recipe_effective_total(
    const evo_project_recipe_context_t *context,
    const evo_project_baseline_owner_t *baseline_owner)
{
    return context->limits.max_total_bytes <
                   baseline_owner->manifest.budget.max_evidence_bytes
               ? context->limits.max_total_bytes
               : baseline_owner->manifest.budget.max_evidence_bytes;
}

evo_project_recipe_status_t evo_project_recipe_encoding_finish(
    const evo_project_recipe_context_t *context,
    const evo_project_baseline_owner_t *baseline_owner,
    size_t genome_size,
    evo_project_recipe_owner_t *owner)
{
    const size_t effective_total =
        evo_project_recipe_effective_total(context, baseline_owner);
    size_t audit_capacity;
    evo_project_recipe_writer_t json_writer;
    evo_project_recipe_writer_t markdown_writer;
    evo_project_fingerprint_t fingerprint;
    size_t index;

    if (owner->genome != NULL || owner->audit_markdown != NULL) {
        return EVO_PROJECT_RECIPE_ERROR_STATE;
    }
    if (genome_size <= EVO_PROJECT_RECIPE_GENOME_HEADER_SIZE ||
        genome_size > context->limits.max_genome_bytes ||
        genome_size >= effective_total ||
        effective_total == 0U) {
        return EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT;
    }
    audit_capacity = effective_total - genome_size;
    if (audit_capacity > context->limits.max_audit_bytes) {
        audit_capacity = context->limits.max_audit_bytes;
    }
    if (audit_capacity == 0U || audit_capacity == SIZE_MAX) {
        return EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT;
    }
    owner->genome = evo_project_allocate_zeroed(
        genome_size, sizeof(*owner->genome));
    owner->audit_markdown = evo_project_allocate_zeroed(
        audit_capacity + 1U, sizeof(*owner->audit_markdown));
    if (owner->genome == NULL || owner->audit_markdown == NULL) {
        return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
    }
    for (index = 0U; index < sizeof(evo_project_recipe_genome_magic);
         index += 1U) {
        owner->genome[index] = evo_project_recipe_genome_magic[index];
    }
    json_writer = (evo_project_recipe_writer_t){
        (char *)owner->genome + EVO_PROJECT_RECIPE_GENOME_HEADER_SIZE,
        genome_size - EVO_PROJECT_RECIPE_GENOME_HEADER_SIZE - 1U,
        0U,
        false};
    evo_project_recipe_generate_json(owner, &json_writer);
    if (json_writer.failed || json_writer.size == 0U) {
        return EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT;
    }
    owner->genome_size = genome_size;
    owner->canonical_json_size = json_writer.size;
    evo_project_recipe_write_payload_size(owner->genome, json_writer.size);
    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_bytes(
        &fingerprint,
        owner->genome + EVO_PROJECT_RECIPE_GENOME_HEADER_SIZE,
        json_writer.size);
    owner->recipe_fingerprint = fingerprint.value;
    markdown_writer = (evo_project_recipe_writer_t){
        owner->audit_markdown, audit_capacity, 0U, false};
    evo_project_recipe_generate_markdown(owner, &markdown_writer);
    if (markdown_writer.failed || markdown_writer.size == 0U) {
        return EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT;
    }
    owner->audit_markdown_size = markdown_writer.size;
    return EVO_PROJECT_RECIPE_SUCCESS;
}

static bool evo_project_recipe_genome_magic_valid(
    const unsigned char *genome)
{
    size_t index;

    for (index = 0U; index < sizeof(evo_project_recipe_genome_magic);
         index += 1U) {
        if (genome[index] != evo_project_recipe_genome_magic[index]) {
            return false;
        }
    }
    return true;
}

static bool evo_project_recipe_read_payload_size(
    const unsigned char *genome,
    size_t *payload_size)
{
    uint64_t value = 0U;
    unsigned int shift;

    for (shift = 0U; shift < 64U; shift += 8U) {
        value |= (uint64_t)genome[8U + (shift / 8U)] << shift;
    }
    if (value > (uint64_t)SIZE_MAX) {
        return false;
    }
    *payload_size = (size_t)value;
    return true;
}

static evo_project_recipe_status_t evo_project_recipe_decode_string(
    const char *json,
    const evo_project_json_token_t *token,
    size_t maximum_bytes,
    char **value)
{
    const evo_project_json_status_t status =
        evo_project_json_decode_string(json, token, maximum_bytes, value);

    if (status == EVO_PROJECT_JSON_SUCCESS) {
        return EVO_PROJECT_RECIPE_SUCCESS;
    }
    if (status == EVO_PROJECT_JSON_OUT_OF_MEMORY) {
        return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
    }
    if (status == EVO_PROJECT_JSON_RESOURCE_LIMIT ||
        (token->type == EVO_PROJECT_JSON_STRING &&
         token->end >= token->start &&
         token->end - token->start > maximum_bytes)) {
        return EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT;
    }
    return EVO_PROJECT_RECIPE_ERROR_GENOME_CORRUPT;
}

static bool evo_project_recipe_object_member(
    const char *json,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const char *key,
    size_t *value_index)
{
    return evo_project_json_object_get(
               json,
               tokens,
               token_count,
               object_index,
               key,
               value_index) == 1;
}

static evo_project_recipe_status_t evo_project_recipe_decode_parameter_kind(
    const char *json,
    const evo_project_json_token_t *token,
    size_t maximum_bytes,
    evo_project_recipe_parameter_kind_t *kind)
{
    char *name = NULL;
    evo_project_recipe_status_t status = evo_project_recipe_decode_string(
        json, token, maximum_bytes, &name);

    if (status != EVO_PROJECT_RECIPE_SUCCESS) {
        return status;
    }
    if (strcmp(name, "integer") == 0) {
        *kind = EVO_PROJECT_RECIPE_PARAMETER_INTEGER;
    } else if (strcmp(name, "boolean") == 0) {
        *kind = EVO_PROJECT_RECIPE_PARAMETER_BOOLEAN;
    } else if (strcmp(name, "choice") == 0) {
        *kind = EVO_PROJECT_RECIPE_PARAMETER_CHOICE;
    } else {
        status = EVO_PROJECT_RECIPE_ERROR_GENOME_CORRUPT;
    }
    evo_project_release(name);
    return status;
}

static evo_project_recipe_status_t evo_project_recipe_decode_parameter(
    const char *json,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const evo_project_recipe_limits_t *limits,
    evo_project_recipe_parameter_value_t *parameter)
{
    char *identity = NULL;
    char *choice = NULL;
    size_t identity_index;
    size_t kind_index;
    size_t integer_index;
    size_t boolean_index;
    size_t choice_index;
    evo_project_recipe_status_t status;

    if (object_index >= token_count ||
        tokens[object_index].type != EVO_PROJECT_JSON_OBJECT ||
        !evo_project_recipe_object_member(
            json,
            tokens,
            token_count,
            object_index,
            "identity",
            &identity_index) ||
        !evo_project_recipe_object_member(
            json,
            tokens,
            token_count,
            object_index,
            "kind",
            &kind_index) ||
        !evo_project_recipe_object_member(
            json,
            tokens,
            token_count,
            object_index,
            "integer_value",
            &integer_index) ||
        !evo_project_recipe_object_member(
            json,
            tokens,
            token_count,
            object_index,
            "boolean_value",
            &boolean_index) ||
        !evo_project_recipe_object_member(
            json,
            tokens,
            token_count,
            object_index,
            "choice_value",
            &choice_index)) {
        return EVO_PROJECT_RECIPE_ERROR_GENOME_CORRUPT;
    }
    status = evo_project_recipe_decode_string(
        json,
        &tokens[identity_index],
        limits->max_string_bytes,
        &identity);
    parameter->identity = identity;
    if (status == EVO_PROJECT_RECIPE_SUCCESS) {
        status = evo_project_recipe_decode_parameter_kind(
            json,
            &tokens[kind_index],
            limits->max_string_bytes,
            &parameter->kind);
    }
    if (status != EVO_PROJECT_RECIPE_SUCCESS) {
        return status;
    }
    if (parameter->kind == EVO_PROJECT_RECIPE_PARAMETER_INTEGER) {
        if (!evo_project_json_parse_i64(
                json, &tokens[integer_index], &parameter->integer_value) ||
            tokens[boolean_index].type != EVO_PROJECT_JSON_NULL ||
            tokens[choice_index].type != EVO_PROJECT_JSON_NULL) {
            return EVO_PROJECT_RECIPE_ERROR_GENOME_CORRUPT;
        }
    } else if (parameter->kind == EVO_PROJECT_RECIPE_PARAMETER_BOOLEAN) {
        if (tokens[integer_index].type != EVO_PROJECT_JSON_NULL ||
            !evo_project_json_parse_bool(
                &tokens[boolean_index], &parameter->boolean_value) ||
            tokens[choice_index].type != EVO_PROJECT_JSON_NULL) {
            return EVO_PROJECT_RECIPE_ERROR_GENOME_CORRUPT;
        }
    } else {
        if (tokens[integer_index].type != EVO_PROJECT_JSON_NULL ||
            tokens[boolean_index].type != EVO_PROJECT_JSON_NULL) {
            return EVO_PROJECT_RECIPE_ERROR_GENOME_CORRUPT;
        }
        status = evo_project_recipe_decode_string(
            json,
            &tokens[choice_index],
            limits->max_string_bytes,
            &choice);
        parameter->choice_value = choice;
    }
    return status;
}

static void evo_project_recipe_decoded_proposals_destroy(
    evo_project_recipe_decoded_proposals_t *decoded)
{
    size_t record_index;

    if (decoded == NULL) {
        return;
    }
    for (record_index = 0U; record_index < decoded->count;
         record_index += 1U) {
        size_t parameter_index;

        evo_project_release(decoded->record_identities == NULL
                                ? NULL
                                : decoded->record_identities[record_index]);
        evo_project_release(decoded->target_identities == NULL
                                ? NULL
                                : decoded->target_identities[record_index]);
        evo_project_release(
            decoded->transformation_identities == NULL
                ? NULL
                : decoded->transformation_identities[record_index]);
        if (decoded->parameters == NULL ||
            decoded->parameters[record_index] == NULL) {
            continue;
        }
        for (parameter_index = 0U;
             parameter_index < decoded->views[record_index].parameter_count;
             parameter_index += 1U) {
            evo_project_release(
                (void *)decoded->parameters[record_index][parameter_index]
                    .identity);
            evo_project_release(
                (void *)decoded->parameters[record_index][parameter_index]
                    .choice_value);
        }
        evo_project_release(decoded->parameters[record_index]);
    }
    evo_project_release(decoded->views);
    evo_project_release(decoded->record_identities);
    evo_project_release(decoded->target_identities);
    evo_project_release(decoded->transformation_identities);
    evo_project_release(decoded->parameters);
    *decoded = (evo_project_recipe_decoded_proposals_t){0};
}

static evo_project_recipe_status_t evo_project_recipe_decode_parameters(
    const char *json,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t array_index,
    const evo_project_recipe_limits_t *limits,
    evo_project_recipe_proposal_record_t *proposal,
    evo_project_recipe_parameter_value_t **owned_parameters)
{
    size_t token_index;
    size_t parameter_index;

    if (array_index >= token_count ||
        tokens[array_index].type != EVO_PROJECT_JSON_ARRAY) {
        return EVO_PROJECT_RECIPE_ERROR_GENOME_CORRUPT;
    }
    if (tokens[array_index].child_count >
        limits->max_parameters_per_record) {
        return EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT;
    }
    proposal->parameter_count = tokens[array_index].child_count;
    if (proposal->parameter_count > 0U) {
        *owned_parameters = evo_project_allocate_zeroed(
            proposal->parameter_count, sizeof(**owned_parameters));
        if (*owned_parameters == NULL) {
            return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
        }
    }
    proposal->parameters = *owned_parameters;
    token_index = array_index + 1U;
    for (parameter_index = 0U;
         parameter_index < proposal->parameter_count;
         parameter_index += 1U) {
        evo_project_recipe_status_t status;

        if (token_index >= token_count ||
            tokens[token_index].parent != array_index) {
            return EVO_PROJECT_RECIPE_ERROR_GENOME_CORRUPT;
        }
        status = evo_project_recipe_decode_parameter(
            json,
            tokens,
            token_count,
            token_index,
            limits,
            &(*owned_parameters)[parameter_index]);
        if (status != EVO_PROJECT_RECIPE_SUCCESS) {
            return status;
        }
        token_index = evo_project_json_next(
            tokens, token_count, token_index);
    }
    return EVO_PROJECT_RECIPE_SUCCESS;
}

static evo_project_recipe_status_t evo_project_recipe_decode_record(
    const char *json,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const evo_project_recipe_limits_t *limits,
    evo_project_recipe_decoded_proposals_t *decoded,
    size_t record_index)
{
    evo_project_recipe_proposal_record_t *proposal =
        &decoded->views[record_index];
    size_t identity_index;
    size_t target_index;
    size_t target_identity_index;
    size_t transformation_index;
    size_t transformation_identity_index;
    size_t transformation_version_index;
    size_t parameters_index;
    uint64_t transformation_version;
    evo_project_recipe_status_t status;

    if (object_index >= token_count ||
        tokens[object_index].type != EVO_PROJECT_JSON_OBJECT ||
        !evo_project_recipe_object_member(
            json,
            tokens,
            token_count,
            object_index,
            "identity",
            &identity_index) ||
        !evo_project_recipe_object_member(
            json,
            tokens,
            token_count,
            object_index,
            "target",
            &target_index) ||
        !evo_project_recipe_object_member(
            json,
            tokens,
            token_count,
            object_index,
            "transformation",
            &transformation_index) ||
        !evo_project_recipe_object_member(
            json,
            tokens,
            token_count,
            object_index,
            "parameters",
            &parameters_index) ||
        !evo_project_recipe_object_member(
            json,
            tokens,
            token_count,
            target_index,
            "location_identity",
            &target_identity_index) ||
        !evo_project_recipe_object_member(
            json,
            tokens,
            token_count,
            transformation_index,
            "identity",
            &transformation_identity_index) ||
        !evo_project_recipe_object_member(
            json,
            tokens,
            token_count,
            transformation_index,
            "version",
            &transformation_version_index) ||
        !evo_project_json_parse_u64(
            json,
            &tokens[transformation_version_index],
            &transformation_version) ||
        transformation_version == 0U || transformation_version > UINT32_MAX) {
        return EVO_PROJECT_RECIPE_ERROR_GENOME_CORRUPT;
    }
    status = evo_project_recipe_decode_string(
        json,
        &tokens[identity_index],
        limits->max_string_bytes,
        &decoded->record_identities[record_index]);
    if (status == EVO_PROJECT_RECIPE_SUCCESS) {
        status = evo_project_recipe_decode_string(
            json,
            &tokens[target_identity_index],
            limits->max_string_bytes,
            &decoded->target_identities[record_index]);
    }
    if (status == EVO_PROJECT_RECIPE_SUCCESS) {
        status = evo_project_recipe_decode_string(
            json,
            &tokens[transformation_identity_index],
            limits->max_string_bytes,
            &decoded->transformation_identities[record_index]);
    }
    if (status != EVO_PROJECT_RECIPE_SUCCESS) {
        return status;
    }
    proposal->identity = decoded->record_identities[record_index];
    proposal->target_location_identity =
        decoded->target_identities[record_index];
    proposal->transformation_identity =
        decoded->transformation_identities[record_index];
    proposal->transformation_version = (uint32_t)transformation_version;
    return evo_project_recipe_decode_parameters(
        json,
        tokens,
        token_count,
        parameters_index,
        limits,
        proposal,
        &decoded->parameters[record_index]);
}

static evo_project_recipe_status_t evo_project_recipe_decode_records(
    const char *json,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t array_index,
    const evo_project_recipe_limits_t *limits,
    evo_project_recipe_decoded_proposals_t *decoded)
{
    size_t token_index;
    size_t record_index;

    if (array_index >= token_count ||
        tokens[array_index].type != EVO_PROJECT_JSON_ARRAY) {
        return EVO_PROJECT_RECIPE_ERROR_GENOME_CORRUPT;
    }
    decoded->count = tokens[array_index].child_count;
    if (decoded->count > limits->max_records) {
        return EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT;
    }
    if (decoded->count > 0U) {
        decoded->views = evo_project_allocate_zeroed(
            decoded->count, sizeof(*decoded->views));
        decoded->record_identities = evo_project_allocate_zeroed(
            decoded->count, sizeof(*decoded->record_identities));
        decoded->target_identities = evo_project_allocate_zeroed(
            decoded->count, sizeof(*decoded->target_identities));
        decoded->transformation_identities = evo_project_allocate_zeroed(
            decoded->count, sizeof(*decoded->transformation_identities));
        decoded->parameters = evo_project_allocate_zeroed(
            decoded->count, sizeof(*decoded->parameters));
        if (decoded->views == NULL || decoded->record_identities == NULL ||
            decoded->target_identities == NULL ||
            decoded->transformation_identities == NULL ||
            decoded->parameters == NULL) {
            return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
        }
    }
    token_index = array_index + 1U;
    for (record_index = 0U; record_index < decoded->count;
         record_index += 1U) {
        evo_project_recipe_status_t status;

        if (token_index >= token_count ||
            tokens[token_index].parent != array_index) {
            return EVO_PROJECT_RECIPE_ERROR_GENOME_CORRUPT;
        }
        status = evo_project_recipe_decode_record(
            json,
            tokens,
            token_count,
            token_index,
            limits,
            decoded,
            record_index);
        if (status != EVO_PROJECT_RECIPE_SUCCESS) {
            return status;
        }
        token_index = evo_project_json_next(
            tokens, token_count, token_index);
    }
    return EVO_PROJECT_RECIPE_SUCCESS;
}

static evo_project_recipe_status_t evo_project_recipe_decode_root(
    const evo_project_recipe_context_t *context,
    const char *json,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    evo_project_recipe_decoded_proposals_t *decoded)
{
    size_t schema_index;
    size_t schema_version_index;
    size_t baseline_index;
    size_t analysis_index;
    size_t catalogue_index;
    size_t catalogue_identity_index;
    size_t catalogue_version_index;
    size_t records_index;
    uint64_t schema_version;
    uint64_t catalogue_version;
    char *schema = NULL;
    char *baseline = NULL;
    char *analysis = NULL;
    char *catalogue = NULL;
    evo_project_recipe_status_t status;

    if (token_count == 0U || tokens[0].type != EVO_PROJECT_JSON_OBJECT ||
        !evo_project_recipe_object_member(
            json, tokens, token_count, 0U, "schema", &schema_index) ||
        !evo_project_recipe_object_member(
            json,
            tokens,
            token_count,
            0U,
            "schema_version",
            &schema_version_index) ||
        !evo_project_recipe_object_member(
            json,
            tokens,
            token_count,
            0U,
            "baseline_fingerprint",
            &baseline_index) ||
        !evo_project_recipe_object_member(
            json,
            tokens,
            token_count,
            0U,
            "analysis_fingerprint",
            &analysis_index) ||
        !evo_project_recipe_object_member(
            json,
            tokens,
            token_count,
            0U,
            "catalogue",
            &catalogue_index) ||
        !evo_project_recipe_object_member(
            json, tokens, token_count, 0U, "records", &records_index) ||
        !evo_project_recipe_object_member(
            json,
            tokens,
            token_count,
            catalogue_index,
            "identity",
            &catalogue_identity_index) ||
        !evo_project_recipe_object_member(
            json,
            tokens,
            token_count,
            catalogue_index,
            "version",
            &catalogue_version_index) ||
        !evo_project_json_parse_u64(
            json, &tokens[schema_version_index], &schema_version) ||
        !evo_project_json_parse_u64(
            json, &tokens[catalogue_version_index], &catalogue_version) ||
        schema_version != EVO_PROJECT_RECIPE_SCHEMA_VERSION ||
        catalogue_version == 0U || catalogue_version > UINT32_MAX) {
        return EVO_PROJECT_RECIPE_ERROR_GENOME_CORRUPT;
    }
    status = evo_project_recipe_decode_string(
        json, &tokens[schema_index], context->limits.max_string_bytes, &schema);
    if (status == EVO_PROJECT_RECIPE_SUCCESS) {
        status = evo_project_recipe_decode_string(
            json,
            &tokens[baseline_index],
            context->limits.max_string_bytes,
            &baseline);
    }
    if (status == EVO_PROJECT_RECIPE_SUCCESS) {
        status = evo_project_recipe_decode_string(
            json,
            &tokens[analysis_index],
            context->limits.max_string_bytes,
            &analysis);
    }
    if (status == EVO_PROJECT_RECIPE_SUCCESS) {
        status = evo_project_recipe_decode_string(
            json,
            &tokens[catalogue_identity_index],
            context->limits.max_string_bytes,
            &catalogue);
    }
    if (status == EVO_PROJECT_RECIPE_SUCCESS &&
        strcmp(schema, "catalyst.evo-project-recipe.v1") != 0) {
        status = EVO_PROJECT_RECIPE_ERROR_GENOME_CORRUPT;
    }
    if (status == EVO_PROJECT_RECIPE_SUCCESS &&
        strcmp(baseline, context->baseline->baseline_fingerprint) != 0) {
        status = EVO_PROJECT_RECIPE_ERROR_BASELINE_CHANGED;
    }
    if (status == EVO_PROJECT_RECIPE_SUCCESS &&
        strcmp(analysis, context->analysis->analysis_fingerprint) != 0) {
        status = EVO_PROJECT_RECIPE_ERROR_ANALYSIS_STALE;
    }
    if (status == EVO_PROJECT_RECIPE_SUCCESS &&
        (strcmp(catalogue, context->catalogue->identity) != 0 ||
         catalogue_version != context->catalogue->catalogue_version)) {
        status = EVO_PROJECT_RECIPE_ERROR_CATALOGUE_INVALID;
    }
    if (status == EVO_PROJECT_RECIPE_SUCCESS) {
        status = evo_project_recipe_decode_records(
            json,
            tokens,
            token_count,
            records_index,
            &context->limits,
            decoded);
    }
    evo_project_release(schema);
    evo_project_release(baseline);
    evo_project_release(analysis);
    evo_project_release(catalogue);
    return status;
}

static evo_project_recipe_status_t evo_project_recipe_parse_genome(
    const evo_project_recipe_context_t *context,
    const unsigned char *genome,
    size_t genome_size,
    size_t *payload_size,
    evo_project_json_token_t **tokens,
    size_t *token_count)
{
    size_t padding_index;
    evo_project_json_status_t json_status;

    if (genome_size > context->limits.max_genome_bytes) {
        return EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT;
    }
    if (genome_size <= EVO_PROJECT_RECIPE_GENOME_HEADER_SIZE ||
        !evo_project_recipe_genome_magic_valid(genome) ||
        !evo_project_recipe_read_payload_size(genome, payload_size) ||
        *payload_size == 0U ||
        *payload_size >
            genome_size - EVO_PROJECT_RECIPE_GENOME_HEADER_SIZE - 1U) {
        return EVO_PROJECT_RECIPE_ERROR_GENOME_CORRUPT;
    }
    for (padding_index =
             EVO_PROJECT_RECIPE_GENOME_HEADER_SIZE + *payload_size;
         padding_index < genome_size;
         padding_index += 1U) {
        if (genome[padding_index] != 0U) {
            return EVO_PROJECT_RECIPE_ERROR_GENOME_NONCANONICAL;
        }
    }
    *tokens = evo_project_allocate_zeroed(
        context->limits.max_json_tokens, sizeof(**tokens));
    if (*tokens == NULL) {
        return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
    }
    json_status = evo_project_json_parse(
        (const char *)genome + EVO_PROJECT_RECIPE_GENOME_HEADER_SIZE,
        *payload_size,
        *tokens,
        context->limits.max_json_tokens,
        context->limits.max_json_depth,
        token_count);
    if (json_status == EVO_PROJECT_JSON_OUT_OF_MEMORY) {
        return EVO_PROJECT_RECIPE_ERROR_OUT_OF_MEMORY;
    }
    if (json_status == EVO_PROJECT_JSON_RESOURCE_LIMIT) {
        return EVO_PROJECT_RECIPE_ERROR_RESOURCE_LIMIT;
    }
    if (json_status != EVO_PROJECT_JSON_SUCCESS) {
        return EVO_PROJECT_RECIPE_ERROR_GENOME_CORRUPT;
    }
    return EVO_PROJECT_RECIPE_SUCCESS;
}

evo_project_recipe_status_t evo_project_recipe_encoding_decode(
    const evo_project_recipe_context_t *context,
    const evo_project_baseline_owner_t *baseline_owner,
    const unsigned char *genome,
    size_t genome_size,
    evo_project_recipe_owner_t *owner)
{
    evo_project_recipe_decoded_proposals_t decoded = {0};
    evo_project_json_token_t *tokens = NULL;
    size_t token_count = 0U;
    size_t payload_size = 0U;
    size_t index;
    evo_project_recipe_status_t status = evo_project_recipe_parse_genome(
        context,
        genome,
        genome_size,
        &payload_size,
        &tokens,
        &token_count);

    if (status == EVO_PROJECT_RECIPE_SUCCESS) {
        status = evo_project_recipe_decode_root(
            context,
            (const char *)genome + EVO_PROJECT_RECIPE_GENOME_HEADER_SIZE,
            tokens,
            token_count,
            &decoded);
    }
    if (status == EVO_PROJECT_RECIPE_SUCCESS) {
        status = evo_project_recipe_model_build(
            context,
            baseline_owner,
            decoded.views,
            decoded.count,
            owner);
    }
    if (status == EVO_PROJECT_RECIPE_SUCCESS) {
        status = evo_project_recipe_encoding_finish(
            context, baseline_owner, genome_size, owner);
    }
    if (status == EVO_PROJECT_RECIPE_SUCCESS &&
        owner->canonical_json_size != payload_size) {
        status = EVO_PROJECT_RECIPE_ERROR_GENOME_NONCANONICAL;
    }
    if (status == EVO_PROJECT_RECIPE_SUCCESS) {
        for (index = 0U; index < genome_size; index += 1U) {
            if (owner->genome[index] != genome[index]) {
                status = EVO_PROJECT_RECIPE_ERROR_GENOME_NONCANONICAL;
                break;
            }
        }
    }
    evo_project_recipe_decoded_proposals_destroy(&decoded);
    evo_project_release(tokens);
    return status;
}
