#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include "internal/project_analysis_evidence.h"

#include "internal/project_fingerprint.h"
#include "internal/project_runtime.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct evo_project_analysis_writer {
    char *bytes;
    size_t capacity;
    size_t size;
    bool failed;
} evo_project_analysis_writer_t;

static void evo_project_analysis_writer_bytes(
    evo_project_analysis_writer_t *writer,
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

static void evo_project_analysis_writer_text(
    evo_project_analysis_writer_t *writer,
    const char *text)
{
    evo_project_analysis_writer_bytes(writer, text, strlen(text));
}

static void evo_project_analysis_writer_char(
    evo_project_analysis_writer_t *writer,
    char value)
{
    evo_project_analysis_writer_bytes(writer, &value, 1U);
}

static void evo_project_analysis_writer_u64(
    evo_project_analysis_writer_t *writer,
    uint64_t value)
{
    char text[32];
    const int written = evo_project_format(
        text, sizeof(text), "%llu", (unsigned long long)value);

    if (written <= 0 || (size_t)written >= sizeof(text)) {
        writer->failed = true;
        return;
    }
    evo_project_analysis_writer_bytes(writer, text, (size_t)written);
}

static void evo_project_analysis_writer_json_string(
    evo_project_analysis_writer_t *writer,
    const char *value)
{
    size_t index;

    evo_project_analysis_writer_char(writer, '"');
    for (index = 0U; value[index] != '\0'; index += 1U) {
        const unsigned char byte = (unsigned char)value[index];

        if (value[index] == '"') {
            evo_project_analysis_writer_text(writer, "\\\"");
        } else if (value[index] == '\\') {
            evo_project_analysis_writer_text(writer, "\\\\");
        } else if (value[index] == '\n') {
            evo_project_analysis_writer_text(writer, "\\n");
        } else if (value[index] == '\r') {
            evo_project_analysis_writer_text(writer, "\\r");
        } else if (value[index] == '\t') {
            evo_project_analysis_writer_text(writer, "\\t");
        } else if (byte < 0x20U) {
            writer->failed = true;
        } else {
            evo_project_analysis_writer_char(writer, value[index]);
        }
    }
    evo_project_analysis_writer_char(writer, '"');
}

static void evo_project_analysis_writer_json_nullable(
    evo_project_analysis_writer_t *writer,
    const char *value)
{
    if (value == NULL) {
        evo_project_analysis_writer_text(writer, "null");
    } else {
        evo_project_analysis_writer_json_string(writer, value);
    }
}

static void evo_project_analysis_writer_markdown(
    evo_project_analysis_writer_t *writer,
    const char *value)
{
    size_t index;

    if (value == NULL) {
        evo_project_analysis_writer_text(writer, "none");
        return;
    }
    for (index = 0U; value[index] != '\0'; index += 1U) {
        if (value[index] == '\\' || value[index] == '|' ||
            value[index] == '`') {
            evo_project_analysis_writer_char(writer, '\\');
        }
        if (value[index] == '\n' || value[index] == '\r') {
            evo_project_analysis_writer_char(writer, ' ');
        } else {
            evo_project_analysis_writer_char(writer, value[index]);
        }
    }
}

static const char *evo_project_analysis_location_kind_text(
    evo_project_source_location_kind_t kind)
{
    switch (kind) {
    case EVO_PROJECT_LOCATION_SPELLING:
        return "spelling";
    case EVO_PROJECT_LOCATION_MACRO_EXPANSION:
        return "macro-expansion";
    case EVO_PROJECT_LOCATION_GENERATED:
    default:
        return "generated";
    }
}

static const char *evo_project_analysis_declaration_kind_text(
    evo_project_declaration_kind_t kind)
{
    switch (kind) {
    case EVO_PROJECT_DECLARATION_FUNCTION:
        return "function";
    case EVO_PROJECT_DECLARATION_VARIABLE:
        return "variable";
    case EVO_PROJECT_DECLARATION_TYPE:
    default:
        return "type";
    }
}

static const char *evo_project_analysis_linkage_text(
    evo_project_linkage_t linkage)
{
    switch (linkage) {
    case EVO_PROJECT_LINKAGE_NONE:
        return "none";
    case EVO_PROJECT_LINKAGE_INTERNAL:
        return "internal";
    case EVO_PROJECT_LINKAGE_EXTERNAL:
    default:
        return "external";
    }
}

static const char *evo_project_analysis_call_kind_text(
    evo_project_call_kind_t kind)
{
    return kind == EVO_PROJECT_CALL_DIRECT ? "direct" : "indirect";
}

static const char *evo_project_analysis_control_kind_text(
    evo_project_control_flow_kind_t kind)
{
    switch (kind) {
    case EVO_PROJECT_CONTROL_FALLTHROUGH:
        return "fallthrough";
    case EVO_PROJECT_CONTROL_BRANCH_TRUE:
        return "branch-true";
    case EVO_PROJECT_CONTROL_BRANCH_FALSE:
        return "branch-false";
    case EVO_PROJECT_CONTROL_BACK_EDGE:
        return "back-edge";
    case EVO_PROJECT_CONTROL_RETURN:
    default:
        return "return";
    }
}

static const char *evo_project_analysis_data_kind_text(
    evo_project_data_flow_kind_t kind)
{
    switch (kind) {
    case EVO_PROJECT_DATA_READ:
        return "read";
    case EVO_PROJECT_DATA_WRITE:
        return "write";
    case EVO_PROJECT_DATA_ADDRESS:
        return "address";
    case EVO_PROJECT_DATA_ESCAPE:
    default:
        return "escape";
    }
}

static const char *evo_project_analysis_optimization_text(
    evo_project_optimization_disposition_t disposition)
{
    switch (disposition) {
    case EVO_PROJECT_OPTIMIZATION_PASSED:
        return "passed";
    case EVO_PROJECT_OPTIMIZATION_MISSED:
        return "missed";
    case EVO_PROJECT_OPTIMIZATION_ANALYSIS:
    default:
        return "analysis";
    }
}

static const char *evo_project_analysis_opportunity_kind(
    const evo_project_opportunity_record_t *record)
{
    if (record->missed_optimization_count > 0U &&
        record->runtime_evidence_present) {
        return "compiler-missed-and-runtime-hotspot";
    }
    if (record->runtime_evidence_present) {
        return "runtime-hotspot";
    }
    return "compiler-missed";
}

static void evo_project_analysis_generate_json(
    const evo_project_analysis_owner_t *owner,
    evo_project_analysis_writer_t *writer)
{
    char fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    size_t index;

    evo_project_fingerprint_format(owner->analysis_fingerprint, fingerprint);
    evo_project_analysis_writer_text(
        writer,
        "{\n\"schema\":\"catalyst.evo-project-analysis.v1\",\n"
        "\"schema_version\":1,\n\"baseline_fingerprint\":");
    evo_project_analysis_writer_json_string(
        writer, owner->baseline_fingerprint);
    evo_project_analysis_writer_text(writer, ",\n\"analysis_fingerprint\":");
    evo_project_analysis_writer_json_string(writer, fingerprint);
    evo_project_analysis_writer_text(writer, ",\n\"provider\":{\"identity\":");
    evo_project_analysis_writer_json_string(writer, owner->provider_identity);
    evo_project_analysis_writer_text(writer, ",\"clang\":");
    evo_project_analysis_writer_json_string(writer, owner->clang_identity);
    evo_project_analysis_writer_text(writer, ",\"llvm\":");
    evo_project_analysis_writer_json_string(writer, owner->llvm_identity);
    evo_project_analysis_writer_text(writer, ",\"target\":");
    evo_project_analysis_writer_json_string(writer, owner->target_identity);
    evo_project_analysis_writer_text(writer, ",\"flags\":");
    evo_project_analysis_writer_json_string(writer, owner->flags_identity);
    evo_project_analysis_writer_text(
        writer, "},\n\"runtime_profile\":{\"state\":");
    evo_project_analysis_writer_json_string(
        writer,
        evo_project_runtime_profile_state_name(
            owner->runtime_profile_state));
    evo_project_analysis_writer_text(writer, ",\"identity\":");
    evo_project_analysis_writer_json_nullable(
        writer, owner->runtime_profile_identity);
    evo_project_analysis_writer_text(writer, "},\n\"translation_units\":[");
    for (index = 0U; index < owner->translation_unit_count; index += 1U) {
        if (index > 0U) {
            evo_project_analysis_writer_char(writer, ',');
        }
        evo_project_analysis_writer_json_string(
            writer, owner->translation_units[index]);
    }
    evo_project_analysis_writer_text(writer, "],\n\"source_locations\":[");
    for (index = 0U; index < owner->source_location_count; index += 1U) {
        const evo_project_source_location_record_t *record =
            &owner->source_locations[index];

        if (index > 0U) {
            evo_project_analysis_writer_char(writer, ',');
        }
        evo_project_analysis_writer_text(writer, "{\"identity\":");
        evo_project_analysis_writer_json_string(writer, record->identity);
        evo_project_analysis_writer_text(writer, ",\"file\":");
        evo_project_analysis_writer_json_string(writer, record->file);
        evo_project_analysis_writer_text(writer, ",\"line\":");
        evo_project_analysis_writer_u64(writer, (uint64_t)record->line);
        evo_project_analysis_writer_text(writer, ",\"column\":");
        evo_project_analysis_writer_u64(writer, (uint64_t)record->column);
        evo_project_analysis_writer_text(writer, ",\"end_line\":");
        evo_project_analysis_writer_u64(writer, (uint64_t)record->end_line);
        evo_project_analysis_writer_text(writer, ",\"end_column\":");
        evo_project_analysis_writer_u64(writer, (uint64_t)record->end_column);
        evo_project_analysis_writer_text(writer, ",\"kind\":");
        evo_project_analysis_writer_json_string(
            writer, evo_project_analysis_location_kind_text(record->kind));
        evo_project_analysis_writer_text(writer, ",\"spelling_identity\":");
        evo_project_analysis_writer_json_nullable(
            writer, record->spelling_identity);
        evo_project_analysis_writer_char(writer, '}');
    }
    evo_project_analysis_writer_text(writer, "],\n\"declarations\":[");
    for (index = 0U; index < owner->declaration_count; index += 1U) {
        const evo_project_declaration_record_t *record =
            &owner->declarations[index];

        if (index > 0U) {
            evo_project_analysis_writer_char(writer, ',');
        }
        evo_project_analysis_writer_text(writer, "{\"identity\":");
        evo_project_analysis_writer_json_string(writer, record->identity);
        evo_project_analysis_writer_text(writer, ",\"name\":");
        evo_project_analysis_writer_json_string(writer, record->name);
        evo_project_analysis_writer_text(writer, ",\"translation_unit\":");
        evo_project_analysis_writer_json_string(
            writer, record->translation_unit);
        evo_project_analysis_writer_text(writer, ",\"location_identity\":");
        evo_project_analysis_writer_json_string(
            writer, record->location_identity);
        evo_project_analysis_writer_text(writer, ",\"kind\":");
        evo_project_analysis_writer_json_string(
            writer, evo_project_analysis_declaration_kind_text(record->kind));
        evo_project_analysis_writer_text(writer, ",\"linkage\":");
        evo_project_analysis_writer_json_string(
            writer, evo_project_analysis_linkage_text(record->linkage));
        evo_project_analysis_writer_text(
            writer,
            record->definition ? ",\"definition\":true}"
                               : ",\"definition\":false}");
    }
    evo_project_analysis_writer_text(writer, "],\n\"calls\":[");
    for (index = 0U; index < owner->call_count; index += 1U) {
        const evo_project_call_record_t *record = &owner->calls[index];

        if (index > 0U) {
            evo_project_analysis_writer_char(writer, ',');
        }
        evo_project_analysis_writer_text(writer, "{\"identity\":");
        evo_project_analysis_writer_json_string(writer, record->identity);
        evo_project_analysis_writer_text(writer, ",\"caller\":");
        evo_project_analysis_writer_json_string(
            writer, record->caller_identity);
        evo_project_analysis_writer_text(writer, ",\"callee\":");
        evo_project_analysis_writer_json_string(
            writer, record->callee_identity);
        evo_project_analysis_writer_text(writer, ",\"location_identity\":");
        evo_project_analysis_writer_json_string(
            writer, record->location_identity);
        evo_project_analysis_writer_text(writer, ",\"kind\":");
        evo_project_analysis_writer_json_string(
            writer, evo_project_analysis_call_kind_text(record->kind));
        evo_project_analysis_writer_char(writer, '}');
    }
    evo_project_analysis_writer_text(writer, "],\n\"control_flows\":[");
    for (index = 0U; index < owner->control_flow_count; index += 1U) {
        const evo_project_control_flow_record_t *record =
            &owner->control_flows[index];

        if (index > 0U) {
            evo_project_analysis_writer_char(writer, ',');
        }
        evo_project_analysis_writer_text(writer, "{\"identity\":");
        evo_project_analysis_writer_json_string(writer, record->identity);
        evo_project_analysis_writer_text(writer, ",\"function\":");
        evo_project_analysis_writer_json_string(
            writer, record->function_identity);
        evo_project_analysis_writer_text(writer, ",\"from\":");
        evo_project_analysis_writer_json_string(
            writer, record->from_block_identity);
        evo_project_analysis_writer_text(writer, ",\"to\":");
        evo_project_analysis_writer_json_string(
            writer, record->to_block_identity);
        evo_project_analysis_writer_text(writer, ",\"location_identity\":");
        evo_project_analysis_writer_json_string(
            writer, record->location_identity);
        evo_project_analysis_writer_text(writer, ",\"kind\":");
        evo_project_analysis_writer_json_string(
            writer, evo_project_analysis_control_kind_text(record->kind));
        evo_project_analysis_writer_char(writer, '}');
    }
    evo_project_analysis_writer_text(writer, "],\n\"data_flows\":[");
    for (index = 0U; index < owner->data_flow_count; index += 1U) {
        const evo_project_data_flow_record_t *record =
            &owner->data_flows[index];

        if (index > 0U) {
            evo_project_analysis_writer_char(writer, ',');
        }
        evo_project_analysis_writer_text(writer, "{\"identity\":");
        evo_project_analysis_writer_json_string(writer, record->identity);
        evo_project_analysis_writer_text(writer, ",\"function\":");
        evo_project_analysis_writer_json_string(
            writer, record->function_identity);
        evo_project_analysis_writer_text(writer, ",\"declaration\":");
        evo_project_analysis_writer_json_string(
            writer, record->declaration_identity);
        evo_project_analysis_writer_text(writer, ",\"location_identity\":");
        evo_project_analysis_writer_json_string(
            writer, record->location_identity);
        evo_project_analysis_writer_text(writer, ",\"kind\":");
        evo_project_analysis_writer_json_string(
            writer, evo_project_analysis_data_kind_text(record->kind));
        evo_project_analysis_writer_char(writer, '}');
    }
    evo_project_analysis_writer_text(
        writer, "],\n\"compiler_optimizations\":[");
    for (index = 0U; index < owner->optimization_record_count; index += 1U) {
        const evo_project_optimization_record_t *record =
            &owner->optimization_records[index];

        if (index > 0U) {
            evo_project_analysis_writer_char(writer, ',');
        }
        evo_project_analysis_writer_text(writer, "{\"identity\":");
        evo_project_analysis_writer_json_string(writer, record->identity);
        evo_project_analysis_writer_text(writer, ",\"pass\":");
        evo_project_analysis_writer_json_string(writer, record->pass_name);
        evo_project_analysis_writer_text(writer, ",\"function\":");
        evo_project_analysis_writer_json_string(
            writer, record->function_identity);
        evo_project_analysis_writer_text(writer, ",\"location_identity\":");
        evo_project_analysis_writer_json_string(
            writer, record->location_identity);
        evo_project_analysis_writer_text(writer, ",\"message\":");
        evo_project_analysis_writer_json_string(writer, record->message);
        evo_project_analysis_writer_text(writer, ",\"disposition\":");
        evo_project_analysis_writer_json_string(
            writer,
            evo_project_analysis_optimization_text(record->disposition));
        evo_project_analysis_writer_char(writer, '}');
    }
    evo_project_analysis_writer_text(writer, "],\n\"runtime_hotspots\":[");
    for (index = 0U; index < owner->runtime_record_count; index += 1U) {
        const evo_project_runtime_record_t *record =
            &owner->runtime_records[index];

        if (index > 0U) {
            evo_project_analysis_writer_char(writer, ',');
        }
        evo_project_analysis_writer_text(writer, "{\"identity\":");
        evo_project_analysis_writer_json_string(writer, record->identity);
        evo_project_analysis_writer_text(writer, ",\"workload\":");
        evo_project_analysis_writer_json_string(
            writer, record->workload_identity);
        evo_project_analysis_writer_text(writer, ",\"function\":");
        evo_project_analysis_writer_json_string(
            writer, record->function_identity);
        evo_project_analysis_writer_text(writer, ",\"location_identity\":");
        evo_project_analysis_writer_json_string(
            writer, record->location_identity);
        evo_project_analysis_writer_text(
            writer, ",\"metric\":\"sample-count\",\"value\":");
        evo_project_analysis_writer_u64(writer, record->value);
        evo_project_analysis_writer_char(writer, '}');
    }
    evo_project_analysis_writer_text(writer, "],\n\"opportunities\":[");
    for (index = 0U; index < owner->opportunity_count; index += 1U) {
        const evo_project_opportunity_record_t *record =
            &owner->opportunities[index];

        if (index > 0U) {
            evo_project_analysis_writer_char(writer, ',');
        }
        evo_project_analysis_writer_text(writer, "{\"rank\":");
        evo_project_analysis_writer_u64(writer, (uint64_t)record->rank);
        evo_project_analysis_writer_text(writer, ",\"kind\":");
        evo_project_analysis_writer_json_string(
            writer, evo_project_analysis_opportunity_kind(record));
        evo_project_analysis_writer_text(writer, ",\"location_identity\":");
        evo_project_analysis_writer_json_string(
            writer, record->location_identity);
        evo_project_analysis_writer_text(
            writer, ",\"missed_optimization_count\":");
        evo_project_analysis_writer_u64(
            writer, (uint64_t)record->missed_optimization_count);
        evo_project_analysis_writer_text(
            writer, ",\"runtime_sample_count\":");
        if (record->runtime_evidence_present) {
            evo_project_analysis_writer_u64(
                writer, record->runtime_sample_count);
        } else {
            evo_project_analysis_writer_text(writer, "null");
        }
        evo_project_analysis_writer_char(writer, '}');
    }
    evo_project_analysis_writer_text(
        writer,
        "],\n\"human_readable_abstraction\":{"
        "\"reference_form\":\"complete-ordered-record-arrays-and-direct-scans\","
        "\"projection\":\"analysis.md\","
        "\"complete\":true,\"probabilistic_authority\":false},\n"
        "\"source_modified\":false,\"evolutionary_operator_invoked\":false\n}\n");
}

static void evo_project_analysis_markdown_row_prefix(
    evo_project_analysis_writer_t *writer,
    const char *identity)
{
    evo_project_analysis_writer_text(writer, "| ");
    evo_project_analysis_writer_markdown(writer, identity);
    evo_project_analysis_writer_text(writer, " | ");
}

static void evo_project_analysis_generate_markdown(
    const evo_project_analysis_owner_t *owner,
    evo_project_analysis_writer_t *writer)
{
    char fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    size_t index;

    evo_project_fingerprint_format(owner->analysis_fingerprint, fingerprint);
    evo_project_analysis_writer_text(writer, "# EVO Project Analysis\n\n");
    evo_project_analysis_writer_text(writer, "Baseline: `");
    evo_project_analysis_writer_markdown(writer, owner->baseline_fingerprint);
    evo_project_analysis_writer_text(writer, "`\n\nAnalysis: `");
    evo_project_analysis_writer_markdown(writer, fingerprint);
    evo_project_analysis_writer_text(writer, "`\n\nProvider: `");
    evo_project_analysis_writer_markdown(writer, owner->provider_identity);
    evo_project_analysis_writer_text(writer, "`\n\nClang / LLVM: `");
    evo_project_analysis_writer_markdown(writer, owner->clang_identity);
    evo_project_analysis_writer_text(writer, "` / `");
    evo_project_analysis_writer_markdown(writer, owner->llvm_identity);
    evo_project_analysis_writer_text(writer, "`\n\nTarget / flags: `");
    evo_project_analysis_writer_markdown(writer, owner->target_identity);
    evo_project_analysis_writer_text(writer, "` / `");
    evo_project_analysis_writer_markdown(writer, owner->flags_identity);
    evo_project_analysis_writer_text(writer, "`\n\nRuntime profile: **");
    evo_project_analysis_writer_text(
        writer,
        evo_project_runtime_profile_state_name(
            owner->runtime_profile_state));
    evo_project_analysis_writer_text(writer, "** (`");
    evo_project_analysis_writer_markdown(
        writer, owner->runtime_profile_identity);
    evo_project_analysis_writer_text(writer, "`)\n\n");

    evo_project_analysis_writer_text(
        writer,
        "## Translation Units\n\n| Stable unit identity |\n|---|\n");
    for (index = 0U; index < owner->translation_unit_count; index += 1U) {
        evo_project_analysis_writer_text(writer, "| ");
        evo_project_analysis_writer_markdown(
            writer, owner->translation_units[index]);
        evo_project_analysis_writer_text(writer, " |\n");
    }

    evo_project_analysis_writer_text(
        writer,
        "\n## Source Locations\n\n| Identity | File | Range | Kind | Spelling |\n"
        "|---|---|---|---|---|\n");
    for (index = 0U; index < owner->source_location_count; index += 1U) {
        const evo_project_source_location_record_t *record =
            &owner->source_locations[index];
        char range[96];
        const int written = evo_project_format(
            range,
            sizeof(range),
            "%u:%u-%u:%u",
            record->line,
            record->column,
            record->end_line,
            record->end_column);

        if (written <= 0 || (size_t)written >= sizeof(range)) {
            writer->failed = true;
            return;
        }
        evo_project_analysis_markdown_row_prefix(writer, record->identity);
        evo_project_analysis_writer_markdown(writer, record->file);
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_text(writer, range);
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_text(
            writer, evo_project_analysis_location_kind_text(record->kind));
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_markdown(
            writer, record->spelling_identity);
        evo_project_analysis_writer_text(writer, " |\n");
    }

    evo_project_analysis_writer_text(
        writer,
        "\n## Declarations\n\n| Identity | Name | Unit | Location | Kind | Linkage | Definition |\n"
        "|---|---|---|---|---|---|---|\n");
    for (index = 0U; index < owner->declaration_count; index += 1U) {
        const evo_project_declaration_record_t *record =
            &owner->declarations[index];

        evo_project_analysis_markdown_row_prefix(writer, record->identity);
        evo_project_analysis_writer_markdown(writer, record->name);
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_markdown(writer, record->translation_unit);
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_markdown(
            writer, record->location_identity);
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_text(
            writer, evo_project_analysis_declaration_kind_text(record->kind));
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_text(
            writer, evo_project_analysis_linkage_text(record->linkage));
        evo_project_analysis_writer_text(
            writer, record->definition ? " | yes |\n" : " | no |\n");
    }

    evo_project_analysis_writer_text(
        writer,
        "\n## Calls\n\n| Identity | Caller | Callee | Location | Kind |\n"
        "|---|---|---|---|---|\n");
    for (index = 0U; index < owner->call_count; index += 1U) {
        const evo_project_call_record_t *record = &owner->calls[index];

        evo_project_analysis_markdown_row_prefix(writer, record->identity);
        evo_project_analysis_writer_markdown(writer, record->caller_identity);
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_markdown(writer, record->callee_identity);
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_markdown(
            writer, record->location_identity);
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_text(
            writer, evo_project_analysis_call_kind_text(record->kind));
        evo_project_analysis_writer_text(writer, " |\n");
    }

    evo_project_analysis_writer_text(
        writer,
        "\n## Control Flow\n\n| Identity | Function | From | To | Location | Kind |\n"
        "|---|---|---|---|---|---|\n");
    for (index = 0U; index < owner->control_flow_count; index += 1U) {
        const evo_project_control_flow_record_t *record =
            &owner->control_flows[index];

        evo_project_analysis_markdown_row_prefix(writer, record->identity);
        evo_project_analysis_writer_markdown(
            writer, record->function_identity);
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_markdown(
            writer, record->from_block_identity);
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_markdown(
            writer, record->to_block_identity);
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_markdown(
            writer, record->location_identity);
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_text(
            writer, evo_project_analysis_control_kind_text(record->kind));
        evo_project_analysis_writer_text(writer, " |\n");
    }

    evo_project_analysis_writer_text(
        writer,
        "\n## Data Flow\n\n| Identity | Function | Declaration | Location | Kind |\n"
        "|---|---|---|---|---|\n");
    for (index = 0U; index < owner->data_flow_count; index += 1U) {
        const evo_project_data_flow_record_t *record =
            &owner->data_flows[index];

        evo_project_analysis_markdown_row_prefix(writer, record->identity);
        evo_project_analysis_writer_markdown(
            writer, record->function_identity);
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_markdown(
            writer, record->declaration_identity);
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_markdown(
            writer, record->location_identity);
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_text(
            writer, evo_project_analysis_data_kind_text(record->kind));
        evo_project_analysis_writer_text(writer, " |\n");
    }

    evo_project_analysis_writer_text(
        writer,
        "\n## Compiler Optimization Records\n\n| Identity | Pass | Function | Location | Disposition | Message |\n"
        "|---|---|---|---|---|---|\n");
    for (index = 0U; index < owner->optimization_record_count; index += 1U) {
        const evo_project_optimization_record_t *record =
            &owner->optimization_records[index];

        evo_project_analysis_markdown_row_prefix(writer, record->identity);
        evo_project_analysis_writer_markdown(writer, record->pass_name);
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_markdown(
            writer, record->function_identity);
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_markdown(
            writer, record->location_identity);
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_text(
            writer,
            evo_project_analysis_optimization_text(record->disposition));
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_markdown(writer, record->message);
        evo_project_analysis_writer_text(writer, " |\n");
    }

    evo_project_analysis_writer_text(
        writer,
        "\n## Runtime Hotspots\n\nStatic and dynamic records are separate. An absent or unavailable profile is not zero runtime cost.\n\n"
        "| Identity | Workload | Function | Location | Metric | Value |\n"
        "|---|---|---|---|---|---:|\n");
    for (index = 0U; index < owner->runtime_record_count; index += 1U) {
        const evo_project_runtime_record_t *record =
            &owner->runtime_records[index];

        evo_project_analysis_markdown_row_prefix(writer, record->identity);
        evo_project_analysis_writer_markdown(
            writer, record->workload_identity);
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_markdown(
            writer, record->function_identity);
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_markdown(
            writer, record->location_identity);
        evo_project_analysis_writer_text(writer, " | sample-count | ");
        evo_project_analysis_writer_u64(writer, record->value);
        evo_project_analysis_writer_text(writer, " |\n");
    }

    evo_project_analysis_writer_text(
        writer,
        "\n## Ranked Opportunities\n\nRanking is exact: runtime evidence sorts before absent runtime evidence, then descending sample count, descending missed-record count, and stable source-location identity.\n\n"
        "| Rank | Kind | Location | Missed records | Runtime samples |\n"
        "|---:|---|---|---:|---:|\n");
    for (index = 0U; index < owner->opportunity_count; index += 1U) {
        const evo_project_opportunity_record_t *record =
            &owner->opportunities[index];

        evo_project_analysis_writer_text(writer, "| ");
        evo_project_analysis_writer_u64(writer, (uint64_t)record->rank);
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_text(
            writer, evo_project_analysis_opportunity_kind(record));
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_markdown(
            writer, record->location_identity);
        evo_project_analysis_writer_text(writer, " | ");
        evo_project_analysis_writer_u64(
            writer, (uint64_t)record->missed_optimization_count);
        evo_project_analysis_writer_text(writer, " | ");
        if (record->runtime_evidence_present) {
            evo_project_analysis_writer_u64(
                writer, record->runtime_sample_count);
        } else {
            evo_project_analysis_writer_text(writer, "unavailable");
        }
        evo_project_analysis_writer_text(writer, " |\n");
    }

    evo_project_analysis_writer_text(
        writer,
        "\n## Human-Readable Abstraction\n\nThe complete ordered records above and their direct deterministic scans are the exact reference form. No compressed, cached, indexed, probabilistic, or otherwise accelerated structure is present. This projection is complete, derives from the same retained owner as `analysis.json`, performs no source write, and invokes no evolutionary operator. FNV labels are replay diagnostics only and are not authority.\n");
}

static bool evo_project_analysis_path_within(
    const char *parent,
    const char *candidate)
{
    const size_t parent_size = strlen(parent);

    if (parent_size == 1U && parent[0] == '/') {
        return candidate[0] == '/';
    }
    return strcmp(parent, candidate) == 0 ||
           (strncmp(parent, candidate, parent_size) == 0 &&
            candidate[parent_size] == '/');
}

static bool evo_project_analysis_output_name_valid(const char *name)
{
    size_t index;

    if (name == NULL || name[0] == '\0' || strcmp(name, ".") == 0 ||
        strcmp(name, "..") == 0) {
        return false;
    }
    for (index = 0U; name[index] != '\0'; index += 1U) {
        const unsigned char value = (unsigned char)name[index];

        if (value < 0x20U || value == 0x7fU || name[index] == '/' ||
            name[index] == '\\') {
            return false;
        }
    }
    return true;
}

static char *evo_project_analysis_join_path(
    const char *left,
    const char *right)
{
    const size_t left_size = strlen(left);
    const size_t right_size = strlen(right);
    const bool separator = left_size > 0U && left[left_size - 1U] != '/';
    size_t size;
    size_t index;
    size_t position = 0U;
    char *path;

    if (left_size > SIZE_MAX - right_size ||
        left_size + right_size > SIZE_MAX - 2U) {
        return NULL;
    }
    size = left_size + right_size + (separator ? 1U : 0U);
    path = evo_project_allocate_zeroed(size + 1U, sizeof(*path));
    if (path == NULL) {
        return NULL;
    }
    for (index = 0U; index < left_size; index += 1U) {
        path[position] = left[index];
        position += 1U;
    }
    if (separator) {
        path[position] = '/';
        position += 1U;
    }
    for (index = 0U; index < right_size; index += 1U) {
        path[position] = right[index];
        position += 1U;
    }
    path[position] = '\0';
    return path;
}

evo_project_analysis_status_t evo_project_analysis_evidence_preflight(
    const evo_project_analysis_config_t *config,
    const evo_project_baseline_owner_t *baseline_owner)
{
    const char *last_slash;
    const char *base_name;
    char *parent_input = NULL;
    char *parent_real = NULL;
    char *output_path = NULL;
    size_t parent_size;
    size_t index;
    evo_project_analysis_status_t status = EVO_PROJECT_ANALYSIS_SUCCESS;

    if (config == NULL || baseline_owner == NULL ||
        config->output_path == NULL || config->output_path[0] != '/') {
        return EVO_PROJECT_ANALYSIS_ERROR_PATH_INVALID;
    }
    last_slash = strrchr(config->output_path, '/');
    if (last_slash == NULL || last_slash[1] == '\0') {
        return EVO_PROJECT_ANALYSIS_ERROR_PATH_INVALID;
    }
    base_name = last_slash + 1;
    if (!evo_project_analysis_output_name_valid(base_name)) {
        return EVO_PROJECT_ANALYSIS_ERROR_PATH_INVALID;
    }
    parent_size = (size_t)(last_slash - config->output_path);
    if (parent_size == 0U) {
        parent_input = evo_project_analysis_join_path("/", "");
    } else {
        parent_input = evo_project_allocate_zeroed(
            parent_size + 1U, sizeof(*parent_input));
        if (parent_input != NULL) {
            for (index = 0U; index < parent_size; index += 1U) {
                parent_input[index] = config->output_path[index];
            }
            parent_input[parent_size] = '\0';
        }
    }
    if (parent_input == NULL) {
        return EVO_PROJECT_ANALYSIS_ERROR_OUT_OF_MEMORY;
    }
    parent_real = realpath(parent_input, NULL);
    evo_project_release(parent_input);
    if (parent_real == NULL) {
        return EVO_PROJECT_ANALYSIS_ERROR_PATH_INVALID;
    }
    output_path = evo_project_analysis_join_path(parent_real, base_name);
    evo_project_release(parent_real);
    if (output_path == NULL) {
        return EVO_PROJECT_ANALYSIS_ERROR_OUT_OF_MEMORY;
    }
    if (strlen(output_path) > config->limits.max_path_bytes) {
        status = EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT;
    } else if (evo_project_analysis_path_within(
                   baseline_owner->authorized_root, output_path) ||
               evo_project_analysis_path_within(
                   baseline_owner->output_path, output_path)) {
        status = EVO_PROJECT_ANALYSIS_ERROR_PATH_INVALID;
    } else if (access(output_path, F_OK) == 0) {
        status = EVO_PROJECT_ANALYSIS_ERROR_OUTPUT_EXISTS;
    } else if (errno != ENOENT) {
        status = EVO_PROJECT_ANALYSIS_ERROR_EVIDENCE_IO;
    }
    evo_project_release(output_path);
    return status;
}

static evo_project_analysis_status_t evo_project_analysis_reserve_output(
    const evo_project_analysis_config_t *config,
    const evo_project_baseline_owner_t *baseline_owner,
    evo_project_analysis_owner_t *owner)
{
    static const char marker[] = "incomplete\n";
    const char *last_slash;
    const char *base_name;
    char *parent_input = NULL;
    char *parent_real = NULL;
    char *output_path = NULL;
    size_t parent_size;
    size_t index;
    int directory_fd = -1;
    int marker_fd = -1;
    evo_project_analysis_status_t status = EVO_PROJECT_ANALYSIS_SUCCESS;

    if (config->output_path[0] != '/') {
        return EVO_PROJECT_ANALYSIS_ERROR_PATH_INVALID;
    }
    last_slash = strrchr(config->output_path, '/');
    if (last_slash == NULL || last_slash[1] == '\0') {
        return EVO_PROJECT_ANALYSIS_ERROR_PATH_INVALID;
    }
    base_name = last_slash + 1;
    if (!evo_project_analysis_output_name_valid(base_name)) {
        return EVO_PROJECT_ANALYSIS_ERROR_PATH_INVALID;
    }
    parent_size = (size_t)(last_slash - config->output_path);
    if (parent_size == 0U) {
        parent_input = evo_project_analysis_join_path("/", "");
    } else {
        parent_input = evo_project_allocate_zeroed(
            parent_size + 1U, sizeof(*parent_input));
        if (parent_input != NULL) {
            for (index = 0U; index < parent_size; index += 1U) {
                parent_input[index] = config->output_path[index];
            }
            parent_input[parent_size] = '\0';
        }
    }
    if (parent_input == NULL) {
        return EVO_PROJECT_ANALYSIS_ERROR_OUT_OF_MEMORY;
    }
    parent_real = realpath(parent_input, NULL);
    evo_project_release(parent_input);
    if (parent_real == NULL) {
        return EVO_PROJECT_ANALYSIS_ERROR_PATH_INVALID;
    }
    output_path = evo_project_analysis_join_path(parent_real, base_name);
    evo_project_release(parent_real);
    if (output_path == NULL) {
        return EVO_PROJECT_ANALYSIS_ERROR_OUT_OF_MEMORY;
    }
    if (strlen(output_path) > config->limits.max_path_bytes) {
        evo_project_release(output_path);
        return EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT;
    }
    if (evo_project_analysis_path_within(
            baseline_owner->authorized_root, output_path) ||
        evo_project_analysis_path_within(
            baseline_owner->output_path, output_path)) {
        evo_project_release(output_path);
        return EVO_PROJECT_ANALYSIS_ERROR_PATH_INVALID;
    }
    if (mkdir(output_path, 0700) != 0) {
        const int saved_errno = errno;

        evo_project_release(output_path);
        return saved_errno == EEXIST
                   ? EVO_PROJECT_ANALYSIS_ERROR_OUTPUT_EXISTS
                   : EVO_PROJECT_ANALYSIS_ERROR_EVIDENCE_IO;
    }
    owner->output_path = output_path;
    owner->output_reserved = true;
    directory_fd = open(
        owner->output_path,
        O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (directory_fd < 0) {
        return EVO_PROJECT_ANALYSIS_ERROR_EVIDENCE_IO;
    }
    marker_fd = openat(
        directory_fd,
        ".evo-analysis-incomplete-v1",
        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
        0600);
    if (marker_fd < 0) {
        status = EVO_PROJECT_ANALYSIS_ERROR_EVIDENCE_IO;
    }
    if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
        size_t position = 0U;

        while (position < sizeof(marker) - 1U) {
            const ssize_t written = write(
                marker_fd,
                marker + position,
                (sizeof(marker) - 1U) - position);

            if (written < 0 && errno == EINTR) {
                continue;
            }
            if (written <= 0) {
                status = EVO_PROJECT_ANALYSIS_ERROR_EVIDENCE_IO;
                break;
            }
            position += (size_t)written;
        }
    }
    if (status == EVO_PROJECT_ANALYSIS_SUCCESS && fsync(marker_fd) != 0) {
        status = EVO_PROJECT_ANALYSIS_ERROR_EVIDENCE_IO;
    }
    if (marker_fd >= 0 && close(marker_fd) != 0 &&
        status == EVO_PROJECT_ANALYSIS_SUCCESS) {
        status = EVO_PROJECT_ANALYSIS_ERROR_EVIDENCE_IO;
    }
    if (fsync(directory_fd) != 0 &&
        status == EVO_PROJECT_ANALYSIS_SUCCESS) {
        status = EVO_PROJECT_ANALYSIS_ERROR_EVIDENCE_IO;
    }
    if (close(directory_fd) != 0 &&
        status == EVO_PROJECT_ANALYSIS_SUCCESS) {
        status = EVO_PROJECT_ANALYSIS_ERROR_EVIDENCE_IO;
    }
    return status;
}

static evo_project_analysis_status_t evo_project_analysis_write_file(
    int directory_fd,
    const char *name,
    const char *bytes,
    size_t byte_count)
{
    int file_fd = openat(
        directory_fd,
        name,
        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
        0600);
    evo_project_analysis_status_t status = EVO_PROJECT_ANALYSIS_SUCCESS;
    size_t position = 0U;

    if (file_fd < 0) {
        return EVO_PROJECT_ANALYSIS_ERROR_EVIDENCE_IO;
    }
    while (position < byte_count) {
        const ssize_t written = write(
            file_fd, bytes + position, byte_count - position);

        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            status = EVO_PROJECT_ANALYSIS_ERROR_EVIDENCE_IO;
            break;
        }
        position += (size_t)written;
    }
    if (status == EVO_PROJECT_ANALYSIS_SUCCESS &&
        (fchmod(file_fd, (mode_t)0400) != 0 || fsync(file_fd) != 0)) {
        status = EVO_PROJECT_ANALYSIS_ERROR_EVIDENCE_IO;
    }
    if (close(file_fd) != 0 && status == EVO_PROJECT_ANALYSIS_SUCCESS) {
        status = EVO_PROJECT_ANALYSIS_ERROR_EVIDENCE_IO;
    }
    return status;
}

static evo_project_analysis_status_t evo_project_analysis_publish_files(
    evo_project_analysis_owner_t *owner,
    const char *json_bytes,
    size_t json_size,
    const char *markdown_bytes,
    size_t markdown_size)
{
    int directory_fd = open(
        owner->output_path,
        O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    evo_project_analysis_status_t status;

    if (directory_fd < 0) {
        return EVO_PROJECT_ANALYSIS_ERROR_EVIDENCE_IO;
    }
    status = evo_project_analysis_write_file(
        directory_fd, ".analysis.json.tmp", json_bytes, json_size);
    if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
        status = evo_project_analysis_write_file(
            directory_fd,
            ".analysis.md.tmp",
            markdown_bytes,
            markdown_size);
    }
    if (status == EVO_PROJECT_ANALYSIS_SUCCESS &&
        (renameat(
             directory_fd,
             ".analysis.json.tmp",
             directory_fd,
             "analysis.json") != 0 ||
         renameat(
             directory_fd,
             ".analysis.md.tmp",
             directory_fd,
             "analysis.md") != 0 ||
         fsync(directory_fd) != 0 ||
         unlinkat(directory_fd, ".evo-analysis-incomplete-v1", 0) != 0 ||
         fchmod(directory_fd, (mode_t)0500) != 0 ||
         fsync(directory_fd) != 0)) {
        status = EVO_PROJECT_ANALYSIS_ERROR_EVIDENCE_IO;
    }
    if (close(directory_fd) != 0 && status == EVO_PROJECT_ANALYSIS_SUCCESS) {
        status = EVO_PROJECT_ANALYSIS_ERROR_EVIDENCE_IO;
    }
    if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
        owner->committed = true;
    }
    return status;
}

evo_project_analysis_status_t evo_project_analysis_evidence_commit(
    const evo_project_analysis_config_t *config,
    const evo_project_baseline_owner_t *baseline_owner,
    evo_project_analysis_owner_t *owner)
{
    const size_t capacity =
        config->limits.max_evidence_bytes <
                baseline_owner->manifest.budget.max_evidence_bytes
            ? config->limits.max_evidence_bytes
            : baseline_owner->manifest.budget.max_evidence_bytes;
    char *json_bytes;
    char *markdown_bytes;
    evo_project_analysis_writer_t json_writer;
    evo_project_analysis_writer_t markdown_writer;
    evo_project_analysis_status_t status;

    if (owner == NULL || owner->output_reserved || owner->committed) {
        return EVO_PROJECT_ANALYSIS_ERROR_STATE;
    }
    if (capacity == 0U || capacity == SIZE_MAX) {
        return EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT;
    }
    json_bytes = evo_project_allocate_zeroed(capacity + 1U, sizeof(*json_bytes));
    markdown_bytes = evo_project_allocate_zeroed(
        capacity + 1U, sizeof(*markdown_bytes));
    if (json_bytes == NULL || markdown_bytes == NULL) {
        evo_project_release(json_bytes);
        evo_project_release(markdown_bytes);
        return EVO_PROJECT_ANALYSIS_ERROR_OUT_OF_MEMORY;
    }
    json_writer = (evo_project_analysis_writer_t){
        json_bytes, capacity, 0U, false};
    markdown_writer = (evo_project_analysis_writer_t){
        markdown_bytes, capacity, 0U, false};
    evo_project_analysis_generate_json(owner, &json_writer);
    evo_project_analysis_generate_markdown(owner, &markdown_writer);
    if (json_writer.failed || markdown_writer.failed ||
        json_writer.size > capacity - markdown_writer.size) {
        status = EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT;
    } else {
        status = evo_project_analysis_reserve_output(
            config, baseline_owner, owner);
    }
    if (status == EVO_PROJECT_ANALYSIS_SUCCESS) {
        status = evo_project_analysis_publish_files(
            owner,
            json_bytes,
            json_writer.size,
            markdown_bytes,
            markdown_writer.size);
    }
    evo_project_release(json_bytes);
    evo_project_release(markdown_bytes);
    return status;
}

void evo_project_analysis_evidence_discard(
    evo_project_analysis_owner_t *owner)
{
    static const char *const names[] = {
        ".analysis.json.tmp",
        ".analysis.md.tmp",
        "analysis.json",
        "analysis.md",
        ".evo-analysis-incomplete-v1"};
    int directory_fd;
    size_t index;

    if (owner == NULL || !owner->output_reserved || owner->committed ||
        owner->output_path == NULL) {
        return;
    }
    directory_fd = open(
        owner->output_path,
        O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (directory_fd < 0) {
        return;
    }
    (void)fchmod(directory_fd, (mode_t)0700);
    for (index = 0U; index < sizeof(names) / sizeof(names[0]); index += 1U) {
        if (unlinkat(directory_fd, names[index], 0) != 0 && errno != ENOENT) {
            (void)close(directory_fd);
            return;
        }
    }
    if (close(directory_fd) == 0 && rmdir(owner->output_path) == 0) {
        owner->output_reserved = false;
    }
}
