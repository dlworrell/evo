#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include "internal/project_assurance_internal.h"

#include "internal/project_runtime.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static bool evo_assurance_path_contains(
    const char *root,
    const char *path)
{
    const size_t root_size = strlen(root);

    if (strncmp(root, path, root_size) != 0) {
        return false;
    }
    if (root_size == 1U && root[0] == '/') {
        return true;
    }
    return path[root_size] == '\0' || path[root_size] == '/';
}

static char *evo_assurance_parent_path(const char *path, const char **basename)
{
    const char *slash = strrchr(path, '/');
    char *parent;
    size_t parent_size;

    if (slash == NULL || slash[1] == '\0') {
        return NULL;
    }
    *basename = slash + 1;
    parent_size = slash == path ? 1U : (size_t)(slash - path);
    parent = evo_assurance_duplicate_n(path, parent_size);
    return parent;
}

evo_project_assurance_status_t evo_assurance_validate_output_path(
    const evo_project_assurance_config_t *config,
    char **normalized_output)
{
    const char *basename = NULL;
    char *parent = NULL;
    char *canonical_parent = NULL;
    char *canonical_workspace = NULL;
    char *candidate_output = NULL;
    struct stat path_stat;
    evo_project_assurance_status_t status = EVO_PROJECT_ASSURANCE_ERROR_PATH_INVALID;

    if (config == NULL || normalized_output == NULL || config->candidate == NULL ||
        config->output_path == NULL || config->output_path[0] != '/' ||
        strlen(config->output_path) >= config->limits.max_path_bytes) {
        return EVO_PROJECT_ASSURANCE_ERROR_PATH_INVALID;
    }
    parent = evo_assurance_parent_path(config->output_path, &basename);
    if (parent == NULL || basename == NULL || basename[0] == '\0' ||
        strcmp(basename, ".") == 0 || strcmp(basename, "..") == 0 ||
        strchr(basename, '\\') != NULL ||
        !evo_assurance_text_valid(basename, config->limits.max_path_bytes)) {
        goto finish;
    }
    canonical_parent = realpath(parent, NULL);
    canonical_workspace = realpath(config->candidate->workspace_path, NULL);
    if (canonical_parent == NULL || canonical_workspace == NULL ||
        stat(canonical_parent, &path_stat) != 0 || !S_ISDIR(path_stat.st_mode) ||
        stat(canonical_workspace, &path_stat) != 0 || !S_ISDIR(path_stat.st_mode)) {
        goto finish;
    }
    candidate_output = evo_candidate_join_path(canonical_parent, basename);
    if (candidate_output == NULL) {
        status = EVO_PROJECT_ASSURANCE_ERROR_OUT_OF_MEMORY;
        goto finish;
    }
    if (strlen(candidate_output) >= config->limits.max_path_bytes ||
        evo_assurance_path_contains(canonical_workspace, candidate_output)) {
        goto finish;
    }
    if (lstat(candidate_output, &path_stat) == 0) {
        status = EVO_PROJECT_ASSURANCE_ERROR_OUTPUT_EXISTS;
        goto finish;
    }
    if (errno != ENOENT) {
        goto finish;
    }
    *normalized_output = candidate_output;
    candidate_output = NULL;
    status = EVO_PROJECT_ASSURANCE_SUCCESS;

finish:
    evo_project_release(candidate_output);
    evo_project_release(canonical_workspace);
    evo_project_release(canonical_parent);
    evo_project_release(parent);
    return status;
}

static bool evo_assurance_append_bool(
    evo_candidate_buffer_t *buffer,
    bool value)
{
    return evo_candidate_buffer_append_text(buffer, value ? "true" : "false");
}

static bool evo_assurance_append_int(
    evo_candidate_buffer_t *buffer,
    int value)
{
    char text[32];
    const int written = evo_project_format(text, sizeof(text), "%d", value);

    return written > 0 && (size_t)written < sizeof(text) &&
           evo_candidate_buffer_append_text(buffer, text);
}

static bool evo_assurance_append_environment_keys(
    evo_candidate_buffer_t *buffer,
    const evo_project_assurance_gate_t *gate)
{
    size_t index;

    if (!evo_candidate_buffer_append_text(buffer, "[")) {
        return false;
    }
    for (index = 0U; index < gate->environment_count; index += 1U) {
        const char *equals = strchr(gate->environment[index], '=');
        const size_t key_size = (size_t)(equals - gate->environment[index]);
        char *key = evo_assurance_duplicate_n(gate->environment[index], key_size);
        bool appended;

        if (key == NULL) {
            return false;
        }
        appended = (index == 0U || evo_candidate_buffer_append_text(buffer, ",")) &&
                   evo_candidate_buffer_append_json_string(buffer, key);
        evo_project_release(key);
        if (!appended) {
            return false;
        }
    }
    return evo_candidate_buffer_append_text(buffer, "]");
}

static bool evo_assurance_append_arguments(
    evo_candidate_buffer_t *buffer,
    const evo_project_assurance_gate_t *gate)
{
    size_t index;

    if (!evo_candidate_buffer_append_text(buffer, "[")) {
        return false;
    }
    for (index = 0U; index < gate->argument_count; index += 1U) {
        if ((index > 0U && !evo_candidate_buffer_append_text(buffer, ",")) ||
            !evo_candidate_buffer_append_json_string(buffer, gate->arguments[index])) {
            return false;
        }
    }
    return evo_candidate_buffer_append_text(buffer, "]");
}

static bool evo_assurance_append_gate_json(
    evo_candidate_buffer_t *json,
    const evo_project_assurance_gate_t *gate,
    const evo_project_assurance_gate_result_t *result)
{
    if (!evo_candidate_buffer_append_text(json, "{\"gate_id\":") ||
        !evo_candidate_buffer_append_json_string(json, gate->gate_id) ||
        !evo_candidate_buffer_append_text(json, ",\"profile_id\":") ||
        !evo_candidate_buffer_append_json_string(json, gate->profile_id) ||
        !evo_candidate_buffer_append_text(json, ",\"stage\":") ||
        !evo_candidate_buffer_append_json_string(
            json, evo_project_assurance_stage_name(gate->stage)) ||
        !evo_candidate_buffer_append_text(json, ",\"required\":") ||
        !evo_assurance_append_bool(json, gate->required) ||
        !evo_candidate_buffer_append_text(json, ",\"arguments\":") ||
        !evo_assurance_append_arguments(json, gate) ||
        !evo_candidate_buffer_append_text(json, ",\"environment_keys\":") ||
        !evo_assurance_append_environment_keys(json, gate) ||
        !evo_candidate_buffer_append_text(json, ",\"working_directory\":") ||
        !evo_candidate_buffer_append_json_string(json, gate->working_directory) ||
        !evo_candidate_buffer_append_text(json, ",\"timeout_ms\":") ||
        !evo_candidate_buffer_append_u64(json, gate->timeout_ms) ||
        !evo_candidate_buffer_append_text(json, ",\"max_memory_bytes\":") ||
        !evo_candidate_buffer_append_u64(json, gate->max_memory_bytes) ||
        !evo_candidate_buffer_append_text(json, ",\"max_processes\":") ||
        !evo_candidate_buffer_append_size(json, gate->max_processes) ||
        !evo_candidate_buffer_append_text(json, ",\"max_storage_bytes\":") ||
        !evo_candidate_buffer_append_u64(json, gate->max_storage_bytes) ||
        !evo_candidate_buffer_append_text(json, ",\"max_output_bytes\":") ||
        !evo_candidate_buffer_append_size(json, gate->max_output_bytes) ||
        !evo_candidate_buffer_append_text(json, ",\"network_access\":") ||
        !evo_assurance_append_bool(json, gate->network_access) ||
        !evo_candidate_buffer_append_text(
            json, ",\"workspace_only_filesystem\":true,\"shell_interpretation\":false") ||
        !evo_candidate_buffer_append_text(json, ",\"disposition\":") ||
        !evo_candidate_buffer_append_json_string(
            json, evo_project_assurance_disposition_name(result->disposition)) ||
        !evo_candidate_buffer_append_text(json, ",\"exit_code\":") ||
        !evo_assurance_append_int(json, result->exit_code) ||
        !evo_candidate_buffer_append_text(json, ",\"signal_number\":") ||
        !evo_assurance_append_int(json, result->signal_number) ||
        !evo_candidate_buffer_append_text(json, ",\"stdout_bytes\":") ||
        !evo_candidate_buffer_append_size(json, result->stdout_bytes) ||
        !evo_candidate_buffer_append_text(json, ",\"stdout_fingerprint\":") ||
        !evo_candidate_buffer_append_json_string(json, result->stdout_fingerprint) ||
        !evo_candidate_buffer_append_text(json, ",\"stderr_bytes\":") ||
        !evo_candidate_buffer_append_size(json, result->stderr_bytes) ||
        !evo_candidate_buffer_append_text(json, ",\"stderr_fingerprint\":") ||
        !evo_candidate_buffer_append_json_string(json, result->stderr_fingerprint) ||
        !evo_candidate_buffer_append_text(json, ",\"toolchain_fingerprint\":") ||
        !evo_candidate_buffer_append_json_string(json, result->toolchain_fingerprint) ||
        !evo_candidate_buffer_append_text(json, ",\"diagnostic_excerpt\":") ||
        !evo_candidate_buffer_append_json_string(
            json,
            result->diagnostic_excerpt == NULL ? "" : result->diagnostic_excerpt) ||
        !evo_candidate_buffer_append_text(json, ",\"filesystem_policy_enforced\":") ||
        !evo_assurance_append_bool(json, result->filesystem_policy_enforced) ||
        !evo_candidate_buffer_append_text(json, ",\"network_policy_enforced\":") ||
        !evo_assurance_append_bool(json, result->network_policy_enforced) ||
        !evo_candidate_buffer_append_text(json, ",\"process_group_clean\":") ||
        !evo_assurance_append_bool(json, result->process_group_clean) ||
        !evo_candidate_buffer_append_text(json, ",\"source_modified\":") ||
        !evo_assurance_append_bool(json, result->source_modified) ||
        !evo_candidate_buffer_append_text(json, ",\"snapshot_modified\":") ||
        !evo_assurance_append_bool(json, result->snapshot_modified)) {
        return false;
    }
    return evo_candidate_buffer_append_text(json, "}");
}

bool evo_assurance_build_json(
    const evo_project_assurance_config_t *config,
    const evo_project_assurance_owner_t *owner,
    const char *policy_fingerprint,
    const char *assurance_fingerprint,
    evo_candidate_buffer_t *json)
{
    size_t index;

    if (!evo_candidate_buffer_append_text(json, "{\"schema_version\":1,\"candidate_fingerprint\":") ||
        !evo_candidate_buffer_append_json_string(json, owner->candidate_fingerprint) ||
        !evo_candidate_buffer_append_text(json, ",\"policy_id\":") ||
        !evo_candidate_buffer_append_json_string(json, owner->policy_id) ||
        !evo_candidate_buffer_append_text(json, ",\"policy_fingerprint\":") ||
        !evo_candidate_buffer_append_json_string(json, policy_fingerprint) ||
        !evo_candidate_buffer_append_text(json, ",\"execution_provider_identity\":") ||
        !evo_candidate_buffer_append_json_string(json, owner->execution_provider_identity) ||
        !evo_candidate_buffer_append_text(json, ",\"stage\":") ||
        !evo_candidate_buffer_append_json_string(
            json, evo_project_assurance_stage_name(config->stage)) ||
        !evo_candidate_buffer_append_text(json, ",\"gate_count\":") ||
        !evo_candidate_buffer_append_size(json, owner->gate_count) ||
        !evo_candidate_buffer_append_text(json, ",\"gates\":[")) {
        return false;
    }
    for (index = 0U; index < owner->gate_count; index += 1U) {
        if ((index > 0U && !evo_candidate_buffer_append_text(json, ",")) ||
            !evo_assurance_append_gate_json(
                json, &config->gates[index], &owner->gates[index])) {
            return false;
        }
    }
    return evo_candidate_buffer_append_text(json, "],\"performance_eligible\":") &&
           evo_assurance_append_bool(json, owner->view.performance_eligible) &&
           evo_candidate_buffer_append_text(json, ",\"champion_eligible\":") &&
           evo_assurance_append_bool(json, owner->view.champion_eligible) &&
           evo_candidate_buffer_append_text(json, ",\"projection_complete\":true,\"probabilistic_authority\":false,\"source_modified\":") &&
           evo_assurance_append_bool(json, owner->view.source_modified) &&
           evo_candidate_buffer_append_text(json, ",\"snapshot_modified\":") &&
           evo_assurance_append_bool(json, owner->view.snapshot_modified) &&
           evo_candidate_buffer_append_text(json, ",\"assurance_fingerprint\":") &&
           evo_candidate_buffer_append_json_string(json, assurance_fingerprint) &&
           evo_candidate_buffer_append_text(json, "}");
}

bool evo_assurance_build_markdown(
    const evo_project_assurance_config_t *config,
    const evo_project_assurance_owner_t *owner,
    const char *policy_fingerprint,
    const char *assurance_fingerprint,
    evo_candidate_buffer_t *markdown)
{
    size_t index;

    if (!evo_candidate_buffer_append_text(markdown, "# EVO candidate assurance\n\n- Candidate: `") ||
        !evo_candidate_buffer_append_text(markdown, owner->candidate_fingerprint) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Policy: `") ||
        !evo_candidate_buffer_append_text(markdown, owner->policy_id) ||
        !evo_candidate_buffer_append_text(markdown, "` (`") ||
        !evo_candidate_buffer_append_text(markdown, policy_fingerprint) ||
        !evo_candidate_buffer_append_text(markdown, "`)\n- Execution provider: `") ||
        !evo_candidate_buffer_append_text(markdown, owner->execution_provider_identity) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Stage: `") ||
        !evo_candidate_buffer_append_text(
            markdown, evo_project_assurance_stage_name(config->stage)) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Assurance: `") ||
        !evo_candidate_buffer_append_text(markdown, assurance_fingerprint) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Performance eligible: `") ||
        !evo_candidate_buffer_append_text(
            markdown, owner->view.performance_eligible ? "true" : "false") ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Champion eligible: `") ||
        !evo_candidate_buffer_append_text(
            markdown, owner->view.champion_eligible ? "true" : "false") ||
        !evo_candidate_buffer_append_text(
            markdown,
            "`\n- Projection complete: `true`\n- Probabilistic authority: `false`\n\n## Ordered gate trace\n\n| # | gate | profile | stage | required | result | exit | signal | filesystem | network | cleanup |\n|---:|---|---|---|---|---|---:|---:|---|---|---|\n")) {
        return false;
    }
    for (index = 0U; index < owner->gate_count; index += 1U) {
        const evo_project_assurance_gate_result_t *result = &owner->gates[index];
        char row_prefix[32];
        char row_status[64];
        const int prefix_written = evo_project_format(
            row_prefix, sizeof(row_prefix), "%zu | ", index + 1U);
        const int status_written = evo_project_format(
            row_status,
            sizeof(row_status),
            "%d | %d | ",
            result->exit_code,
            result->signal_number);

        if (prefix_written <= 0 || (size_t)prefix_written >= sizeof(row_prefix) ||
            status_written <= 0 || (size_t)status_written >= sizeof(row_status) ||
            !evo_candidate_buffer_append_text(markdown, "| ") ||
            !evo_candidate_buffer_append_text(markdown, row_prefix) ||
            !evo_candidate_buffer_append_text(markdown, result->gate_id) ||
            !evo_candidate_buffer_append_text(markdown, " | ") ||
            !evo_candidate_buffer_append_text(markdown, result->profile_id) ||
            !evo_candidate_buffer_append_text(markdown, " | ") ||
            !evo_candidate_buffer_append_text(
                markdown, evo_project_assurance_stage_name(result->stage)) ||
            !evo_candidate_buffer_append_text(markdown, " | ") ||
            !evo_candidate_buffer_append_text(
                markdown, result->required ? "yes" : "no") ||
            !evo_candidate_buffer_append_text(markdown, " | ") ||
            !evo_candidate_buffer_append_text(
                markdown,
                evo_project_assurance_disposition_name(result->disposition)) ||
            !evo_candidate_buffer_append_text(markdown, " | ") ||
            !evo_candidate_buffer_append_text(markdown, row_status) ||
            !evo_candidate_buffer_append_text(
                markdown, result->filesystem_policy_enforced ? "yes" : "no") ||
            !evo_candidate_buffer_append_text(markdown, " | ") ||
            !evo_candidate_buffer_append_text(
                markdown, result->network_policy_enforced ? "yes" : "no") ||
            !evo_candidate_buffer_append_text(markdown, " | ") ||
            !evo_candidate_buffer_append_text(
                markdown, result->process_group_clean ? "yes" : "no") ||
            !evo_candidate_buffer_append_text(markdown, " |\n")) {
            return false;
        }
    }
    return evo_candidate_buffer_append_text(
        markdown,
        "\nThe command contract is argv-only (`shell_interpretation:false`). Environment evidence exposes keys only; configured values remain policy inputs. Required gate rejection carries no performance/champion authority.\n");
}
