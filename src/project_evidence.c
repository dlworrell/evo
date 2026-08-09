#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L

#include "internal/project_evidence.h"

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

typedef struct evo_project_writer {
    char *bytes;
    size_t capacity;
    size_t size;
    bool failed;
} evo_project_writer_t;

static void evo_project_writer_bytes(
    evo_project_writer_t *writer,
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

static void evo_project_writer_text(
    evo_project_writer_t *writer,
    const char *text)
{
    evo_project_writer_bytes(writer, text, strlen(text));
}

static void evo_project_writer_char(evo_project_writer_t *writer, char value)
{
    evo_project_writer_bytes(writer, &value, 1U);
}

static void evo_project_writer_u64(
    evo_project_writer_t *writer,
    uint64_t value)
{
    char text[32];
    const int written = evo_project_format(
        text, sizeof(text), "%llu", (unsigned long long)value);

    if (written <= 0 || (size_t)written >= sizeof(text)) {
        writer->failed = true;
        return;
    }
    evo_project_writer_bytes(writer, text, (size_t)written);
}

static void evo_project_writer_int(evo_project_writer_t *writer, int value)
{
    char text[32];
    const int written = evo_project_format(text, sizeof(text), "%d", value);

    if (written <= 0 || (size_t)written >= sizeof(text)) {
        writer->failed = true;
        return;
    }
    evo_project_writer_bytes(writer, text, (size_t)written);
}

static void evo_project_writer_mode(
    evo_project_writer_t *writer,
    unsigned int value)
{
    char text[16];
    const int written = evo_project_format(text, sizeof(text), "%04o", value);

    if (written <= 0 || (size_t)written >= sizeof(text)) {
        writer->failed = true;
        return;
    }
    evo_project_writer_bytes(writer, text, (size_t)written);
}

static void evo_project_writer_fingerprint(
    evo_project_writer_t *writer,
    uint64_t value)
{
    char text[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];

    evo_project_fingerprint_format(value, text);
    if (text[0] == '\0') {
        writer->failed = true;
        return;
    }
    evo_project_writer_text(writer, text);
}

static void evo_project_writer_json_string(
    evo_project_writer_t *writer,
    const char *value)
{
    size_t index;

    evo_project_writer_char(writer, '"');
    for (index = 0U; value[index] != '\0'; index += 1U) {
        const unsigned char byte = (unsigned char)value[index];

        switch (value[index]) {
        case '"':
            evo_project_writer_text(writer, "\\\"");
            break;
        case '\\':
            evo_project_writer_text(writer, "\\\\");
            break;
        case '\b':
            evo_project_writer_text(writer, "\\b");
            break;
        case '\f':
            evo_project_writer_text(writer, "\\f");
            break;
        case '\n':
            evo_project_writer_text(writer, "\\n");
            break;
        case '\r':
            evo_project_writer_text(writer, "\\r");
            break;
        case '\t':
            evo_project_writer_text(writer, "\\t");
            break;
        default:
            if (byte < 0x20U) {
                writer->failed = true;
            } else {
                evo_project_writer_char(writer, value[index]);
            }
            break;
        }
    }
    evo_project_writer_char(writer, '"');
}

static void evo_project_writer_markdown_text(
    evo_project_writer_t *writer,
    const char *value)
{
    size_t index;

    for (index = 0U; value[index] != '\0'; index += 1U) {
        if (value[index] == '\\' || value[index] == '|' ||
            value[index] == '`') {
            evo_project_writer_char(writer, '\\');
        }
        evo_project_writer_char(writer, value[index]);
    }
}

static const char *evo_project_state_text(
    evo_project_baseline_state_t state)
{
    switch (state) {
    case EVO_PROJECT_BASELINE_ELIGIBLE:
        return "eligible";
    case EVO_PROJECT_BASELINE_BUILD_FAILED:
        return "build-failed";
    case EVO_PROJECT_BASELINE_CORRECTNESS_FAILED:
        return "correctness-failed";
    case EVO_PROJECT_BASELINE_BENCHMARK_INELIGIBLE:
        return "benchmark-ineligible";
    case EVO_PROJECT_BASELINE_NONE:
    default:
        return "none";
    }
}

static const char *evo_project_disposition_text(
    evo_project_command_disposition_t disposition)
{
    switch (disposition) {
    case EVO_PROJECT_COMMAND_PASSED:
        return "passed";
    case EVO_PROJECT_COMMAND_FAILED:
        return "failed";
    case EVO_PROJECT_COMMAND_TIMED_OUT:
        return "timed-out";
    case EVO_PROJECT_COMMAND_NOT_RUN:
    default:
        return "not-run";
    }
}

static void evo_project_json_string_array(
    evo_project_writer_t *writer,
    char *const *values,
    size_t count)
{
    size_t index;

    evo_project_writer_char(writer, '[');
    for (index = 0U; index < count; index += 1U) {
        if (index > 0U) {
            evo_project_writer_char(writer, ',');
        }
        evo_project_writer_json_string(writer, values[index]);
    }
    evo_project_writer_char(writer, ']');
}

static void evo_project_json_named_identities(
    evo_project_writer_t *writer,
    const evo_project_named_identity_t *values,
    size_t count)
{
    size_t index;

    evo_project_writer_char(writer, '[');
    for (index = 0U; index < count; index += 1U) {
        if (index > 0U) {
            evo_project_writer_char(writer, ',');
        }
        evo_project_writer_text(writer, "{\"name\":");
        evo_project_writer_json_string(writer, values[index].name);
        evo_project_writer_text(writer, ",\"identity\":");
        evo_project_writer_json_string(writer, values[index].identity);
        evo_project_writer_char(writer, '}');
    }
    evo_project_writer_char(writer, ']');
}

static void evo_project_generate_json(
    const evo_project_baseline_owner_t *owner,
    evo_project_writer_t *writer)
{
    const evo_project_manifest_t *manifest = &owner->manifest;
    size_t index;

    evo_project_writer_text(
        writer,
        "{\n\"schema\":\"catalyst.evo-project-baseline.v1\",\n"
        "\"schema_version\":1,\n\"state\":");
    evo_project_writer_json_string(writer, evo_project_state_text(owner->state));
    evo_project_writer_text(writer, ",\n\"manifest\":{\"schema\":");
    evo_project_writer_json_string(writer, manifest->schema);
    evo_project_writer_text(writer, ",\"id\":");
    evo_project_writer_json_string(writer, manifest->manifest_id);
    evo_project_writer_text(writer, ",\"fingerprint\":\"");
    evo_project_writer_fingerprint(writer, manifest->fingerprint);
    evo_project_writer_text(writer, "\",\"source_declared_identity\":");
    evo_project_writer_json_string(writer, manifest->source_identity);
    evo_project_writer_text(writer, ",\"permitted_roots\":");
    evo_project_json_string_array(
        writer, manifest->permitted_roots, manifest->permitted_root_count);
    evo_project_writer_text(writer, ",\"compilation_database\":");
    evo_project_writer_json_string(writer, manifest->compilation_database);
    evo_project_writer_text(writer, ",\"generated_source_policy\":");
    evo_project_writer_json_string(writer, manifest->generated_source_policy);
    evo_project_writer_text(writer, ",\"generated_sources\":[]");
    evo_project_writer_text(writer, ",\"build_frontend\":");
    evo_project_writer_json_string(writer, manifest->build_frontend);
    evo_project_writer_text(
        writer,
        manifest->benchmark_required ? ",\"benchmark_required\":true"
                                     : ",\"benchmark_required\":false");
    evo_project_writer_text(writer, ",\"language\":");
    evo_project_writer_json_string(writer, manifest->language);
    evo_project_writer_text(writer, ",\"targets\":");
    evo_project_json_string_array(
        writer, manifest->targets, manifest->target_count);
    evo_project_writer_text(writer, ",\"dependencies\":");
    evo_project_json_named_identities(
        writer, manifest->dependencies, manifest->dependency_count);
    evo_project_writer_text(writer, ",\"toolchains\":");
    evo_project_json_named_identities(
        writer, manifest->toolchains, manifest->toolchain_count);
    evo_project_writer_text(writer, ",\"environment\":[");
    for (index = 0U; index < manifest->environment_count; index += 1U) {
        if (index > 0U) {
            evo_project_writer_char(writer, ',');
        }
        evo_project_writer_text(writer, "{\"name\":");
        evo_project_writer_json_string(writer, manifest->environment[index].name);
        evo_project_writer_text(writer, ",\"value\":");
        evo_project_writer_json_string(writer, manifest->environment[index].value);
        evo_project_writer_char(writer, '}');
    }
    evo_project_writer_text(writer, "],\"workloads\":");
    evo_project_json_string_array(
        writer, manifest->workloads, manifest->workload_count);
    evo_project_writer_text(writer, ",\"constraints\":");
    evo_project_json_string_array(
        writer, manifest->constraints, manifest->constraint_count);
    evo_project_writer_text(writer, ",\"search\":{\"seed\":");
    evo_project_writer_u64(writer, manifest->search.seed);
    evo_project_writer_text(writer, ",\"population\":");
    evo_project_writer_u64(writer, (uint64_t)manifest->search.population);
    evo_project_writer_text(writer, ",\"generations\":");
    evo_project_writer_u64(writer, (uint64_t)manifest->search.generations);
    evo_project_writer_text(writer, ",\"workers\":");
    evo_project_writer_u64(writer, (uint64_t)manifest->search.workers);
    evo_project_writer_text(writer, "},\"budgets\":{\"max_files\":");
    evo_project_writer_u64(writer, (uint64_t)manifest->budget.max_files);
    evo_project_writer_text(writer, ",\"max_file_bytes\":");
    evo_project_writer_u64(writer, (uint64_t)manifest->budget.max_file_bytes);
    evo_project_writer_text(writer, ",\"max_total_bytes\":");
    evo_project_writer_u64(writer, (uint64_t)manifest->budget.max_total_bytes);
    evo_project_writer_text(writer, ",\"max_path_bytes\":");
    evo_project_writer_u64(writer, (uint64_t)manifest->budget.max_path_bytes);
    evo_project_writer_text(writer, ",\"max_compilation_database_bytes\":");
    evo_project_writer_u64(
        writer, (uint64_t)manifest->budget.max_compilation_database_bytes);
    evo_project_writer_text(writer, ",\"max_command_output_bytes\":");
    evo_project_writer_u64(
        writer, (uint64_t)manifest->budget.max_command_output_bytes);
    evo_project_writer_text(writer, ",\"max_evidence_bytes\":");
    evo_project_writer_u64(
        writer, (uint64_t)manifest->budget.max_evidence_bytes);
    evo_project_writer_text(writer, ",\"command_timeout_ms\":");
    evo_project_writer_u64(writer, manifest->budget.command_timeout_ms);
    evo_project_writer_text(writer, ",\"max_memory_bytes\":");
    evo_project_writer_u64(writer, manifest->budget.max_memory_bytes);
    evo_project_writer_text(writer, ",\"max_processes\":");
    evo_project_writer_u64(writer, (uint64_t)manifest->budget.max_processes);
    evo_project_writer_text(writer, ",\"max_storage_bytes\":");
    evo_project_writer_u64(writer, manifest->budget.max_storage_bytes);
    evo_project_writer_text(writer, ",\"network_access\":false},\"artifacts\":{\"retention\":");
    evo_project_writer_json_string(writer, manifest->artifact_retention);
    evo_project_writer_text(writer, ",\"cleanup\":");
    evo_project_writer_json_string(writer, manifest->cleanup_policy);
    evo_project_writer_text(writer, "}},\n\"baseline\":{\"fingerprint\":\"");
    evo_project_writer_fingerprint(writer, owner->baseline_fingerprint);
    evo_project_writer_text(
        writer,
        "\",\"fingerprint_authoritative\":false,"
        "\"identity_authority\":\"read-only snapshot bytes plus complete ordered registry\","
        "\"execution_provider\":");
    evo_project_writer_json_string(writer, owner->execution_provider_identity);
    evo_project_writer_text(writer, ",\"snapshot\":\"snapshot\",\"workspace_retained\":false,\"file_count\":");
    evo_project_writer_u64(writer, (uint64_t)owner->file_count);
    evo_project_writer_text(writer, ",\"total_file_bytes\":");
    evo_project_writer_u64(writer, owner->total_file_bytes);
    evo_project_writer_text(writer, ",\"normalized_build_schema\":\"catalyst.evo-project-build-description.v1\",\"normalized_build_fingerprint\":\"");
    evo_project_writer_fingerprint(
        writer, owner->normalized_build_fingerprint);
    evo_project_writer_text(writer, "\",\"compilation_unit_count\":");
    evo_project_writer_u64(writer, (uint64_t)owner->compilation_unit_count);
    evo_project_writer_text(writer, "},\n\"files\":[");
    for (index = 0U; index < owner->file_count; index += 1U) {
        if (index > 0U) {
            evo_project_writer_char(writer, ',');
        }
        evo_project_writer_text(writer, "{\"path\":");
        evo_project_writer_json_string(writer, owner->files[index].path);
        evo_project_writer_text(writer, ",\"size\":");
        evo_project_writer_u64(writer, owner->files[index].size);
        evo_project_writer_text(writer, ",\"mode\":\"");
        evo_project_writer_mode(writer, owner->files[index].source_mode);
        evo_project_writer_text(writer, "\",\"content_fingerprint\":\"");
        evo_project_writer_fingerprint(
            writer, owner->files[index].content_fingerprint);
        evo_project_writer_text(writer, "\",\"fingerprint_authoritative\":false}");
    }
    evo_project_writer_text(writer, "],\n\"compilation_units\":[");
    for (index = 0U; index < owner->compilation_unit_count; index += 1U) {
        const evo_project_compilation_record_t *record =
            &owner->compilation_units[index];

        if (index > 0U) {
            evo_project_writer_char(writer, ',');
        }
        evo_project_writer_text(writer, "{\"file\":");
        evo_project_writer_json_string(writer, record->file);
        evo_project_writer_text(writer, ",\"directory\":");
        evo_project_writer_json_string(writer, record->directory);
        evo_project_writer_text(writer, ",\"output\":");
        if (record->output == NULL) {
            evo_project_writer_text(writer, "null");
        } else {
            evo_project_writer_json_string(writer, record->output);
        }
        if (record->command_form == EVO_PROJECT_COMPILE_COMMAND_ARGUMENTS) {
            evo_project_writer_text(writer, ",\"form\":\"arguments\",\"arguments\":");
            evo_project_json_string_array(
                writer,
                (char *const *)record->arguments,
                record->argument_count);
        } else {
            evo_project_writer_text(writer, ",\"form\":\"command\",\"command\":");
            evo_project_writer_json_string(writer, record->command);
        }
        evo_project_writer_char(writer, '}');
    }
    evo_project_writer_text(writer, "],\n\"commands\":[");
    for (index = 0U; index < EVO_PROJECT_COMMAND_COUNT; index += 1U) {
        const evo_project_manifest_command_t *command = &manifest->commands[index];
        const evo_project_command_record_t *record = &owner->commands[index];

        if (index > 0U) {
            evo_project_writer_char(writer, ',');
        }
        evo_project_writer_text(writer, "{\"stage\":");
        evo_project_writer_json_string(writer, record->stage_id);
        evo_project_writer_text(writer, ",\"argv\":");
        evo_project_json_string_array(
            writer, command->arguments, command->argument_count);
        evo_project_writer_text(writer, ",\"disposition\":");
        evo_project_writer_json_string(
            writer, evo_project_disposition_text(record->disposition));
        evo_project_writer_text(writer, ",\"exit_code\":");
        evo_project_writer_int(writer, record->exit_code);
        evo_project_writer_text(writer, ",\"output_bytes\":");
        evo_project_writer_u64(writer, (uint64_t)record->output_bytes);
        evo_project_writer_text(writer, ",\"output_fingerprint\":\"");
        evo_project_writer_fingerprint(writer, record->output_fingerprint);
        evo_project_writer_text(writer, "\",\"fingerprint_authoritative\":false}");
    }
    evo_project_writer_text(
        writer,
        "],\n\"human_readable_abstraction\":{"
        "\"accelerated_structure\":null,"
        "\"reference_authority\":\"explicit snapshot, file registry, compilation-unit registry, policy registries, and gate trace\","
        "\"projection\":\"baseline.md\","
        "\"projection_complete\":true,"
        "\"stable_order\":\"UTF-8 bytewise relative path and domain identifier order\","
        "\"probabilistic_authority\":false}\n}\n");
}

static void evo_project_markdown_string_list(
    evo_project_writer_t *writer,
    const char *heading,
    char *const *values,
    size_t count)
{
    size_t index;

    evo_project_writer_text(writer, "\n### ");
    evo_project_writer_text(writer, heading);
    evo_project_writer_text(writer, "\n\n");
    if (count == 0U) {
        evo_project_writer_text(writer, "- None declared.\n");
        return;
    }
    for (index = 0U; index < count; index += 1U) {
        evo_project_writer_text(writer, "- `");
        evo_project_writer_markdown_text(writer, values[index]);
        evo_project_writer_text(writer, "`\n");
    }
}

static void evo_project_markdown_named_identities(
    evo_project_writer_t *writer,
    const char *heading,
    const evo_project_named_identity_t *values,
    size_t count)
{
    size_t index;

    evo_project_writer_text(writer, "\n### ");
    evo_project_writer_text(writer, heading);
    evo_project_writer_text(writer, "\n\n");
    if (count == 0U) {
        evo_project_writer_text(writer, "- None declared.\n");
        return;
    }
    evo_project_writer_text(
        writer, "| Name | Identity |\n|---|---|\n");
    for (index = 0U; index < count; index += 1U) {
        evo_project_writer_text(writer, "| `");
        evo_project_writer_markdown_text(writer, values[index].name);
        evo_project_writer_text(writer, "` | `");
        evo_project_writer_markdown_text(writer, values[index].identity);
        evo_project_writer_text(writer, "` |\n");
    }
}

static void evo_project_markdown_environment(
    evo_project_writer_t *writer,
    const evo_project_environment_entry_t *values,
    size_t count)
{
    size_t index;

    evo_project_writer_text(writer, "\n### Environment\n\n");
    if (count == 0U) {
        evo_project_writer_text(writer, "- None declared.\n");
        return;
    }
    evo_project_writer_text(
        writer, "| Name | Value |\n|---|---|\n");
    for (index = 0U; index < count; index += 1U) {
        evo_project_writer_text(writer, "| `");
        evo_project_writer_markdown_text(writer, values[index].name);
        evo_project_writer_text(writer, "` | `");
        evo_project_writer_markdown_text(writer, values[index].value);
        evo_project_writer_text(writer, "` |\n");
    }
}

static void evo_project_markdown_u64_row(
    evo_project_writer_t *writer,
    const char *name,
    uint64_t value)
{
    evo_project_writer_text(writer, "| `");
    evo_project_writer_text(writer, name);
    evo_project_writer_text(writer, "` | ");
    evo_project_writer_u64(writer, value);
    evo_project_writer_text(writer, " |\n");
}

static void evo_project_generate_markdown(
    const evo_project_baseline_owner_t *owner,
    evo_project_writer_t *writer)
{
    const evo_project_manifest_t *manifest = &owner->manifest;
    size_t index;

    evo_project_writer_text(writer, "# EVO Project Baseline\n\n- State: **");
    evo_project_writer_text(writer, evo_project_state_text(owner->state));
    evo_project_writer_text(writer, "**\n- Manifest: `");
    evo_project_writer_markdown_text(writer, manifest->manifest_id);
    evo_project_writer_text(writer, "`\n- Declared source: `");
    evo_project_writer_markdown_text(writer, manifest->source_identity);
    evo_project_writer_text(writer, "`\n- Compilation database: `");
    evo_project_writer_markdown_text(
        writer, manifest->compilation_database);
    evo_project_writer_text(writer, "`\n- Frontend: `");
    evo_project_writer_markdown_text(writer, manifest->build_frontend);
    evo_project_writer_text(
        writer,
        manifest->benchmark_required
            ? "`\n- Benchmark required: yes\n- Language: `"
            : "`\n- Benchmark required: no\n- Language: `");
    evo_project_writer_markdown_text(writer, manifest->language);
    evo_project_writer_text(
        writer,
        "`\n- Generated-source policy: `reject`\n"
        "- Generated sources: none\n"
        "- Execution provider: `");
    evo_project_writer_markdown_text(writer, owner->execution_provider_identity);
    evo_project_writer_text(writer, "`\n- Manifest fingerprint: `");
    evo_project_writer_fingerprint(writer, manifest->fingerprint);
    evo_project_writer_text(writer, "`\n- Baseline fingerprint: `");
    evo_project_writer_fingerprint(writer, owner->baseline_fingerprint);
    evo_project_writer_text(writer, "`\n- Normalized build fingerprint: `");
    evo_project_writer_fingerprint(
        writer, owner->normalized_build_fingerprint);
    evo_project_writer_text(
        writer,
        "`\n\nThe FNV-1a labels are deterministic diagnostics, not authentication or sole identity authority. The read-only snapshot bytes and the complete ordered registry below are canonical.\n\n"
        "## File Registry\n\n| Relative path | Bytes | Source mode | Diagnostic fingerprint |\n"
        "|---|---:|---:|---|\n");
    for (index = 0U; index < owner->file_count; index += 1U) {
        evo_project_writer_text(writer, "| `");
        evo_project_writer_markdown_text(writer, owner->files[index].path);
        evo_project_writer_text(writer, "` | ");
        evo_project_writer_u64(writer, owner->files[index].size);
        evo_project_writer_text(writer, " | `");
        evo_project_writer_mode(writer, owner->files[index].source_mode);
        evo_project_writer_text(writer, "` | `");
        evo_project_writer_fingerprint(
            writer, owner->files[index].content_fingerprint);
        evo_project_writer_text(writer, "` |\n");
    }
    evo_project_writer_text(
        writer,
        "\n## Normalized Compilation Units\n\nThe ordered registry below is the complete normalized projection of the retained compilation database. `arguments` preserves an explicit argv vector; `command` preserves an opaque shell command without interpreting it.\n\n| Source file | Directory | Output | Form | Invocation |\n"
        "|---|---|---|---|---|\n");
    for (index = 0U; index < owner->compilation_unit_count; index += 1U) {
        const evo_project_compilation_record_t *record =
            &owner->compilation_units[index];
        size_t argument;

        evo_project_writer_text(writer, "| `");
        evo_project_writer_markdown_text(writer, record->file);
        evo_project_writer_text(writer, "` | `");
        evo_project_writer_markdown_text(writer, record->directory);
        evo_project_writer_text(writer, "` | ");
        if (record->output == NULL) {
            evo_project_writer_text(writer, "—");
        } else {
            evo_project_writer_text(writer, "`");
            evo_project_writer_markdown_text(writer, record->output);
            evo_project_writer_text(writer, "`");
        }
        if (record->command_form == EVO_PROJECT_COMPILE_COMMAND_ARGUMENTS) {
            evo_project_writer_text(writer, " | arguments | ");
            for (argument = 0U; argument < record->argument_count;
                 argument += 1U) {
                if (argument > 0U) {
                    evo_project_writer_text(writer, " ");
                }
                evo_project_writer_text(writer, "`");
                evo_project_writer_markdown_text(
                    writer, record->arguments[argument]);
                evo_project_writer_text(writer, "`");
            }
        } else {
            evo_project_writer_text(writer, " | command | `");
            evo_project_writer_markdown_text(writer, record->command);
            evo_project_writer_text(writer, "`");
        }
        evo_project_writer_text(writer, " |\n");
    }
    evo_project_writer_text(
        writer,
        "\n## Baseline Gate Trace\n\n| Stage | Invocation | Disposition | Exit | Output bytes | Diagnostic fingerprint |\n"
        "|---|---|---|---:|---:|---|\n");
    for (index = 0U; index < EVO_PROJECT_COMMAND_COUNT; index += 1U) {
        const evo_project_manifest_command_t *command =
            &manifest->commands[index];
        size_t argument;

        evo_project_writer_text(writer, "| `");
        evo_project_writer_text(writer, owner->commands[index].stage_id);
        evo_project_writer_text(writer, "` | ");
        for (argument = 0U; argument < command->argument_count;
             argument += 1U) {
            if (argument > 0U) {
                evo_project_writer_text(writer, " ");
            }
            evo_project_writer_text(writer, "`");
            evo_project_writer_markdown_text(
                writer, command->arguments[argument]);
            evo_project_writer_text(writer, "`");
        }
        evo_project_writer_text(writer, " | ");
        evo_project_writer_text(
            writer,
            evo_project_disposition_text(owner->commands[index].disposition));
        evo_project_writer_text(writer, " | ");
        evo_project_writer_int(writer, owner->commands[index].exit_code);
        evo_project_writer_text(writer, " | ");
        evo_project_writer_u64(
            writer, (uint64_t)owner->commands[index].output_bytes);
        evo_project_writer_text(writer, " | `");
        evo_project_writer_fingerprint(
            writer, owner->commands[index].output_fingerprint);
        evo_project_writer_text(writer, "` |\n");
    }
    evo_project_writer_text(writer, "\n## Manifest Policy\n");
    evo_project_markdown_string_list(
        writer,
        "Permitted Roots",
        manifest->permitted_roots,
        manifest->permitted_root_count);
    evo_project_writer_text(
        writer,
        "\n### Generated Sources\n\n- None. Manifest policy `reject` permits no generated source in the immutable baseline.\n");
    evo_project_markdown_string_list(
        writer, "Targets", manifest->targets, manifest->target_count);
    evo_project_markdown_string_list(
        writer, "Workloads", manifest->workloads, manifest->workload_count);
    evo_project_markdown_string_list(
        writer,
        "Constraints",
        manifest->constraints,
        manifest->constraint_count);
    evo_project_markdown_named_identities(
        writer,
        "Dependencies",
        manifest->dependencies,
        manifest->dependency_count);
    evo_project_markdown_named_identities(
        writer,
        "Toolchains",
        manifest->toolchains,
        manifest->toolchain_count);
    evo_project_markdown_environment(
        writer, manifest->environment, manifest->environment_count);
    evo_project_writer_text(
        writer,
        "\n### Search\n\n| Field | Value |\n|---|---:|\n");
    evo_project_markdown_u64_row(writer, "seed", manifest->search.seed);
    evo_project_markdown_u64_row(
        writer, "population", (uint64_t)manifest->search.population);
    evo_project_markdown_u64_row(
        writer, "generations", (uint64_t)manifest->search.generations);
    evo_project_markdown_u64_row(
        writer, "workers", (uint64_t)manifest->search.workers);
    evo_project_writer_text(
        writer,
        "\n### Resource Budgets\n\n| Budget | Value |\n|---|---:|\n");
    evo_project_markdown_u64_row(
        writer, "max_files", (uint64_t)manifest->budget.max_files);
    evo_project_markdown_u64_row(
        writer,
        "max_file_bytes",
        (uint64_t)manifest->budget.max_file_bytes);
    evo_project_markdown_u64_row(
        writer,
        "max_total_bytes",
        (uint64_t)manifest->budget.max_total_bytes);
    evo_project_markdown_u64_row(
        writer,
        "max_path_bytes",
        (uint64_t)manifest->budget.max_path_bytes);
    evo_project_markdown_u64_row(
        writer,
        "max_compilation_database_bytes",
        (uint64_t)manifest->budget.max_compilation_database_bytes);
    evo_project_markdown_u64_row(
        writer,
        "max_command_output_bytes",
        (uint64_t)manifest->budget.max_command_output_bytes);
    evo_project_markdown_u64_row(
        writer,
        "max_evidence_bytes",
        (uint64_t)manifest->budget.max_evidence_bytes);
    evo_project_markdown_u64_row(
        writer,
        "command_timeout_ms",
        manifest->budget.command_timeout_ms);
    evo_project_markdown_u64_row(
        writer, "max_memory_bytes", manifest->budget.max_memory_bytes);
    evo_project_markdown_u64_row(
        writer,
        "max_processes",
        (uint64_t)manifest->budget.max_processes);
    evo_project_markdown_u64_row(
        writer, "max_storage_bytes", manifest->budget.max_storage_bytes);
    evo_project_writer_text(writer, "| `network_access` | false |\n");
    evo_project_writer_text(
        writer, "\n### Artifact Policy\n\n- Retention: `");
    evo_project_writer_markdown_text(writer, manifest->artifact_retention);
    evo_project_writer_text(writer, "`\n- Cleanup: `");
    evo_project_writer_markdown_text(writer, manifest->cleanup_policy);
    evo_project_writer_text(writer, "`\n");
    evo_project_writer_text(
        writer,
        "\n## Human-Readable Abstraction\n\nNo compressed, cached, indexed, probabilistic, or otherwise accelerated structure is introduced. The snapshot, ordered file and compilation-unit registries, ordered policy registries, and gate trace are the exact reference form. This projection is complete and generated from the same retained owner immediately before commit.\n");
}

static evo_project_status_t evo_project_write_file(
    const char *directory,
    const char *name,
    const char *bytes,
    size_t byte_count)
{
    int directory_fd = open(
        directory, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    int file_fd;
    size_t position = 0U;
    evo_project_status_t status = EVO_PROJECT_SUCCESS;

    if (directory_fd < 0) {
        return EVO_PROJECT_ERROR_EVIDENCE_IO;
    }
    file_fd = openat(
        directory_fd,
        name,
        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
        0400);
    if (file_fd < 0) {
        (void)close(directory_fd);
        return EVO_PROJECT_ERROR_EVIDENCE_IO;
    }
    while (position < byte_count) {
        const ssize_t written = write(file_fd, bytes + position, byte_count - position);

        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            status = EVO_PROJECT_ERROR_EVIDENCE_IO;
            break;
        }
        position += (size_t)written;
    }
    if (status == EVO_PROJECT_SUCCESS && fsync(file_fd) != 0) {
        status = EVO_PROJECT_ERROR_EVIDENCE_IO;
    }
    if (close(file_fd) != 0 && status == EVO_PROJECT_SUCCESS) {
        status = EVO_PROJECT_ERROR_EVIDENCE_IO;
    }
    if (status == EVO_PROJECT_SUCCESS && fsync(directory_fd) != 0) {
        status = EVO_PROJECT_ERROR_EVIDENCE_IO;
    }
    if (close(directory_fd) != 0 && status == EVO_PROJECT_SUCCESS) {
        status = EVO_PROJECT_ERROR_EVIDENCE_IO;
    }
    return status;
}

evo_project_status_t evo_project_write_baseline_evidence(
    const evo_project_baseline_owner_t *owner)
{
    const size_t capacity = owner->manifest.budget.max_evidence_bytes;
    char *json_bytes;
    char *markdown_bytes;
    evo_project_writer_t json_writer;
    evo_project_writer_t markdown_writer;
    evo_project_status_t status;

    if (capacity == 0U || capacity == SIZE_MAX) {
        return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
    }
    json_bytes = evo_project_allocate_zeroed(capacity + 1U, sizeof(*json_bytes));
    markdown_bytes = evo_project_allocate_zeroed(capacity + 1U, sizeof(*markdown_bytes));
    if (json_bytes == NULL || markdown_bytes == NULL) {
        evo_project_release(json_bytes);
        evo_project_release(markdown_bytes);
        return EVO_PROJECT_ERROR_OUT_OF_MEMORY;
    }
    json_writer.bytes = json_bytes;
    json_writer.capacity = capacity;
    json_writer.size = 0U;
    json_writer.failed = false;
    markdown_writer.bytes = markdown_bytes;
    markdown_writer.capacity = capacity;
    markdown_writer.size = 0U;
    markdown_writer.failed = false;
    evo_project_generate_json(owner, &json_writer);
    evo_project_generate_markdown(owner, &markdown_writer);
    if (json_writer.failed || markdown_writer.failed ||
        json_writer.size > capacity - markdown_writer.size) {
        evo_project_release(json_bytes);
        evo_project_release(markdown_bytes);
        return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
    }
    status = evo_project_write_file(
        owner->stage_path, "baseline.json", json_bytes, json_writer.size);
    if (status == EVO_PROJECT_SUCCESS) {
        status = evo_project_write_file(
            owner->stage_path,
            "baseline.md",
            markdown_bytes,
            markdown_writer.size);
    }
    evo_project_release(json_bytes);
    evo_project_release(markdown_bytes);
    return status;
}
