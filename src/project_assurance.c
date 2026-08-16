#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L

#include "internal/project_assurance.h"
#include "internal/project_assurance_internal.h"
#include "internal/project_assurance_owner.h"

#include "internal/project_fingerprint.h"
#include "internal/project_runtime.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static bool evo_assurance_candidate_valid(
    const evo_project_assurance_config_t *config,
    const evo_project_assurance_t *assurance)
{
    const evo_project_candidate_t *candidate;

    if (config == NULL || assurance == NULL || config->candidate == NULL) {
        return false;
    }
    candidate = config->candidate;
    return assurance->private_owner == NULL &&
           candidate->private_owner != NULL &&
           candidate->schema_version == EVO_PROJECT_CANDIDATE_SCHEMA_VERSION &&
           candidate->workspace_policy == EVO_PROJECT_CANDIDATE_WORKSPACE_RETAIN &&
           candidate->workspace_path != NULL && candidate->output_path != NULL &&
           candidate->baseline_fingerprint != NULL &&
           candidate->recipe_fingerprint != NULL &&
           evo_assurance_text_valid(
               candidate->candidate_fingerprint, config->limits.max_string_bytes) &&
           candidate->projection_complete && !candidate->probabilistic_authority &&
           !candidate->source_modified && !candidate->snapshot_modified;
}

static bool evo_assurance_outcome_consistent(
    const evo_project_assurance_config_t *config,
    const evo_project_assurance_gate_t *gate,
    const evo_project_assurance_gate_outcome_t *outcome)
{
    size_t combined_output;
    unsigned int terminal_flags = 0U;

    if (outcome->schema_version != EVO_PROJECT_ASSURANCE_SCHEMA_VERSION ||
        !outcome->completed || outcome->stdout_bytes > gate->max_output_bytes ||
        outcome->stderr_bytes > gate->max_output_bytes) {
        return false;
    }
    if (outcome->stdout_bytes > SIZE_MAX - outcome->stderr_bytes) {
        return false;
    }
    combined_output = outcome->stdout_bytes + outcome->stderr_bytes;
    if (combined_output > gate->max_output_bytes ||
        combined_output > config->limits.max_output_bytes ||
        !evo_assurance_diagnostic_valid(
            outcome->diagnostic_excerpt,
            outcome->diagnostic_excerpt_bytes,
            config->limits.max_diagnostic_bytes)) {
        return false;
    }
    terminal_flags += outcome->timed_out ? 1U : 0U;
    terminal_flags += outcome->signaled ? 1U : 0U;
    terminal_flags += outcome->resource_exhausted ? 1U : 0U;
    if (terminal_flags > 1U) {
        return false;
    }
    if (!outcome->available) {
        return terminal_flags == 0U && outcome->exit_code == -1 &&
               outcome->signal_number == 0 && outcome->stdout_bytes == 0U &&
               outcome->stderr_bytes == 0U;
    }
    if (outcome->signaled) {
        return outcome->signal_number > 0 && outcome->signal_number <= 255 &&
               outcome->exit_code >= -1 && outcome->exit_code <= 255;
    }
    return outcome->signal_number == 0 && outcome->exit_code >= -1 &&
           outcome->exit_code <= 255 &&
           (outcome->timed_out || outcome->resource_exhausted ||
            outcome->exit_code >= 0);
}

static bool evo_assurance_copy_gate_identity(
    evo_project_assurance_owner_t *owner,
    const evo_project_assurance_gate_t *gate,
    size_t index)
{
    evo_project_assurance_gate_result_t *result = &owner->gates[index];

    owner->gate_ids[index] = evo_assurance_duplicate(gate->gate_id);
    owner->profile_ids[index] = evo_assurance_duplicate(gate->profile_id);
    if (owner->gate_ids[index] == NULL || owner->profile_ids[index] == NULL) {
        return false;
    }
    result->gate_id = owner->gate_ids[index];
    result->profile_id = owner->profile_ids[index];
    result->stage = gate->stage;
    result->required = gate->required;
    result->disposition = EVO_PROJECT_ASSURANCE_GATE_NOT_RUN;
    result->exit_code = -1;
    result->signal_number = 0;
    evo_project_fingerprint_format(0U, result->stdout_fingerprint);
    evo_project_fingerprint_format(0U, result->stderr_fingerprint);
    evo_project_fingerprint_format(0U, result->toolchain_fingerprint);
    return true;
}

static void evo_assurance_record_outcome(
    evo_project_assurance_owner_t *owner,
    const evo_project_assurance_gate_outcome_t *outcome,
    size_t index)
{
    evo_project_assurance_gate_result_t *result = &owner->gates[index];

    result->disposition = evo_assurance_outcome_disposition(outcome);
    result->exit_code = outcome->exit_code;
    result->signal_number = outcome->signal_number;
    result->stdout_bytes = outcome->stdout_bytes;
    evo_project_fingerprint_format(
        outcome->stdout_fingerprint, result->stdout_fingerprint);
    result->stderr_bytes = outcome->stderr_bytes;
    evo_project_fingerprint_format(
        outcome->stderr_fingerprint, result->stderr_fingerprint);
    evo_project_fingerprint_format(
        outcome->toolchain_fingerprint, result->toolchain_fingerprint);
    result->filesystem_policy_enforced = outcome->filesystem_policy_enforced;
    result->network_policy_enforced = outcome->network_policy_enforced;
    result->process_group_clean = outcome->process_group_clean;
    result->source_modified = outcome->source_modified;
    result->snapshot_modified = outcome->snapshot_modified;
    if (outcome->diagnostic_excerpt_bytes > 0U) {
        owner->diagnostic_excerpts[index] = evo_assurance_duplicate_n(
            outcome->diagnostic_excerpt, outcome->diagnostic_excerpt_bytes);
        result->diagnostic_excerpt = owner->diagnostic_excerpts[index];
    }
}

static evo_project_assurance_status_t evo_assurance_run_gates(
    const evo_project_assurance_config_t *config,
    evo_project_assurance_owner_t *owner)
{
    bool required_failed = false;
    size_t index;

    for (index = 0U; index < config->gate_count; index += 1U) {
        const evo_project_assurance_gate_t *gate = &config->gates[index];
        evo_project_assurance_gate_outcome_t outcome = {0};
        evo_project_assurance_gate_view_t view;
        evo_project_assurance_status_t status;

        if (!evo_assurance_gate_selected(config->stage, gate->stage) ||
            required_failed) {
            continue;
        }
        view.gate_id = gate->gate_id;
        view.profile_id = gate->profile_id;
        view.stage = gate->stage;
        view.required = gate->required;
        view.argument_count = gate->argument_count;
        view.arguments = gate->arguments;
        view.environment_count = gate->environment_count;
        view.environment = gate->environment;
        view.working_directory = gate->working_directory;
        view.timeout_ms = gate->timeout_ms;
        view.max_memory_bytes = gate->max_memory_bytes;
        view.max_processes = gate->max_processes;
        view.max_storage_bytes = gate->max_storage_bytes;
        view.max_output_bytes = gate->max_output_bytes;
        view.network_access = gate->network_access;
        view.workspace_only_filesystem = true;
        view.shell_interpretation = false;
        outcome.schema_version = EVO_PROJECT_ASSURANCE_SCHEMA_VERSION;
        outcome.exit_code = -1;
        status = config->runner(
            &view,
            config->candidate->workspace_path,
            config->runner_context,
            &outcome);
        if (status != EVO_PROJECT_ASSURANCE_SUCCESS ||
            !evo_assurance_outcome_consistent(config, gate, &outcome)) {
            return status == EVO_PROJECT_ASSURANCE_ERROR_OUT_OF_MEMORY
                       ? status
                       : EVO_PROJECT_ASSURANCE_ERROR_EXECUTION_PROVIDER;
        }
        evo_assurance_record_outcome(owner, &outcome, index);
        if (outcome.diagnostic_excerpt_bytes > 0U &&
            owner->diagnostic_excerpts[index] == NULL) {
            return EVO_PROJECT_ASSURANCE_ERROR_OUT_OF_MEMORY;
        }
        owner->view.source_modified =
            owner->view.source_modified || outcome.source_modified;
        owner->view.snapshot_modified =
            owner->view.snapshot_modified || outcome.snapshot_modified;
        if (gate->required &&
            owner->gates[index].disposition != EVO_PROJECT_ASSURANCE_GATE_PASSED) {
            required_failed = true;
        }
    }
    return EVO_PROJECT_ASSURANCE_SUCCESS;
}

static bool evo_assurance_required_gates_passed(
    const evo_project_assurance_config_t *config,
    const evo_project_assurance_owner_t *owner)
{
    size_t index;

    for (index = 0U; index < config->gate_count; index += 1U) {
        const evo_project_assurance_gate_t *gate = &config->gates[index];

        if (gate->required &&
            evo_assurance_gate_selected(config->stage, gate->stage) &&
            owner->gates[index].disposition != EVO_PROJECT_ASSURANCE_GATE_PASSED) {
            return false;
        }
    }
    return !owner->view.source_modified && !owner->view.snapshot_modified;
}

static uint64_t evo_assurance_fingerprint_result(
    const evo_project_assurance_config_t *config,
    const evo_project_assurance_owner_t *owner)
{
    evo_project_fingerprint_t fingerprint;
    size_t index;

    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_string(&fingerprint, "evo-project-assurance-result-v1");
    evo_project_fingerprint_string(&fingerprint, owner->candidate_fingerprint);
    evo_project_fingerprint_u64(&fingerprint, owner->policy_fingerprint);
    evo_project_fingerprint_string(
        &fingerprint, owner->execution_provider_identity);
    evo_project_fingerprint_u64(&fingerprint, (uint64_t)config->stage);
    evo_project_fingerprint_u64(&fingerprint, (uint64_t)owner->gate_count);
    for (index = 0U; index < owner->gate_count; index += 1U) {
        const evo_project_assurance_gate_result_t *result = &owner->gates[index];

        evo_project_fingerprint_string(&fingerprint, result->gate_id);
        evo_project_fingerprint_string(&fingerprint, result->profile_id);
        evo_project_fingerprint_u64(&fingerprint, (uint64_t)result->stage);
        evo_project_fingerprint_u64(&fingerprint, result->required ? 1U : 0U);
        evo_project_fingerprint_u64(
            &fingerprint, (uint64_t)result->disposition);
        evo_project_fingerprint_u64(
            &fingerprint, (uint64_t)(result->exit_code + 1));
        evo_project_fingerprint_u64(
            &fingerprint, (uint64_t)result->signal_number);
        evo_project_fingerprint_u64(
            &fingerprint, (uint64_t)result->stdout_bytes);
        evo_project_fingerprint_string(&fingerprint, result->stdout_fingerprint);
        evo_project_fingerprint_u64(
            &fingerprint, (uint64_t)result->stderr_bytes);
        evo_project_fingerprint_string(&fingerprint, result->stderr_fingerprint);
        evo_project_fingerprint_string(&fingerprint, result->toolchain_fingerprint);
        evo_project_fingerprint_string(
            &fingerprint,
            result->diagnostic_excerpt == NULL ? "" : result->diagnostic_excerpt);
        evo_project_fingerprint_u64(
            &fingerprint, result->filesystem_policy_enforced ? 1U : 0U);
        evo_project_fingerprint_u64(
            &fingerprint, result->network_policy_enforced ? 1U : 0U);
        evo_project_fingerprint_u64(
            &fingerprint, result->process_group_clean ? 1U : 0U);
        evo_project_fingerprint_u64(
            &fingerprint, result->source_modified ? 1U : 0U);
        evo_project_fingerprint_u64(
            &fingerprint, result->snapshot_modified ? 1U : 0U);
    }
    evo_project_fingerprint_u64(
        &fingerprint, owner->view.performance_eligible ? 1U : 0U);
    evo_project_fingerprint_u64(
        &fingerprint, owner->view.champion_eligible ? 1U : 0U);
    return fingerprint.value;
}

static evo_project_assurance_status_t evo_assurance_write_file(
    int output_fd,
    const char *name,
    const char *bytes,
    size_t byte_count)
{
    int file_fd = openat(
        output_fd,
        name,
        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
        0600);
    evo_project_candidate_status_t write_status;

    if (file_fd < 0) {
        return EVO_PROJECT_ASSURANCE_ERROR_EVIDENCE;
    }
    write_status = evo_candidate_write_all(
        file_fd, (const unsigned char *)bytes, byte_count);
    if (write_status != EVO_PROJECT_CANDIDATE_SUCCESS || fsync(file_fd) != 0) {
        (void)close(file_fd);
        return EVO_PROJECT_ASSURANCE_ERROR_EVIDENCE;
    }
    if (close(file_fd) != 0) {
        return EVO_PROJECT_ASSURANCE_ERROR_EVIDENCE;
    }
    return EVO_PROJECT_ASSURANCE_SUCCESS;
}

static evo_project_assurance_status_t evo_assurance_publish_evidence(
    const evo_project_assurance_owner_t *owner)
{
    const char marker[] = "incomplete\n";
    int output_fd = -1;
    int marker_fd = -1;
    evo_project_assurance_status_t status = EVO_PROJECT_ASSURANCE_SUCCESS;

    if (mkdir(owner->output_path, 0700) != 0) {
        return errno == EEXIST ? EVO_PROJECT_ASSURANCE_ERROR_OUTPUT_EXISTS
                               : EVO_PROJECT_ASSURANCE_ERROR_EVIDENCE;
    }
    output_fd = open(
        owner->output_path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (output_fd < 0) {
        status = EVO_PROJECT_ASSURANCE_ERROR_EVIDENCE;
        goto finish;
    }
    marker_fd = openat(
        output_fd,
        ".evo-assurance-incomplete-v1",
        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
        0600);
    if (marker_fd < 0 ||
        evo_candidate_write_all(
            marker_fd,
            (const unsigned char *)marker,
            sizeof(marker) - 1U) != EVO_PROJECT_CANDIDATE_SUCCESS ||
        fsync(marker_fd) != 0) {
        status = EVO_PROJECT_ASSURANCE_ERROR_EVIDENCE;
        goto finish;
    }
    if (close(marker_fd) != 0) {
        marker_fd = -1;
        status = EVO_PROJECT_ASSURANCE_ERROR_EVIDENCE;
        goto finish;
    }
    marker_fd = -1;
    status = evo_assurance_write_file(
        output_fd,
        "assurance.json",
        owner->canonical_json,
        owner->canonical_json_size);
    if (status == EVO_PROJECT_ASSURANCE_SUCCESS) {
        status = evo_assurance_write_file(
            output_fd,
            "assurance.md",
            owner->audit_markdown,
            owner->audit_markdown_size);
    }
    if (status == EVO_PROJECT_ASSURANCE_SUCCESS &&
        (fsync(output_fd) != 0 ||
         unlinkat(output_fd, ".evo-assurance-incomplete-v1", 0) != 0 ||
         fsync(output_fd) != 0)) {
        status = EVO_PROJECT_ASSURANCE_ERROR_EVIDENCE;
    }

finish:
    if (marker_fd >= 0) {
        (void)close(marker_fd);
    }
    if (output_fd >= 0) {
        (void)close(output_fd);
    }
    if (status != EVO_PROJECT_ASSURANCE_SUCCESS) {
        (void)evo_candidate_remove_tree(owner->output_path);
    }
    return status;
}

static void evo_assurance_owner_destroy(evo_project_assurance_owner_t *owner)
{
    size_t index;

    if (owner == NULL) {
        return;
    }
    for (index = 0U; index < owner->gate_count; index += 1U) {
        if (owner->diagnostic_excerpts != NULL) {
            evo_project_release(owner->diagnostic_excerpts[index]);
        }
        if (owner->profile_ids != NULL) {
            evo_project_release(owner->profile_ids[index]);
        }
        if (owner->gate_ids != NULL) {
            evo_project_release(owner->gate_ids[index]);
        }
    }
    evo_project_release(owner->diagnostic_excerpts);
    evo_project_release(owner->profile_ids);
    evo_project_release(owner->gate_ids);
    evo_project_release(owner->gates);
    evo_project_release(owner->audit_markdown);
    evo_project_release(owner->canonical_json);
    evo_project_release(owner->output_path);
    evo_project_release(owner->execution_provider_identity);
    evo_project_release(owner->policy_id);
    evo_project_release(owner->candidate_fingerprint);
}

evo_project_assurance_status_t evo_project_candidate_assure(
    const evo_project_assurance_config_t *config,
    evo_project_assurance_t *assurance)
{
    evo_project_assurance_owner_t *owner = NULL;
    evo_candidate_buffer_t json = {0};
    evo_candidate_buffer_t markdown = {0};
    char *normalized_output = NULL;
    char policy_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char assurance_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    uint64_t policy_value = 0U;
    evo_project_assurance_status_t status;
    size_t index;
    bool owner_published = false;

    if (config == NULL || assurance == NULL) {
        return EVO_PROJECT_ASSURANCE_ERROR_INVALID_ARGUMENT;
    }
    if (assurance->private_owner != NULL) {
        return EVO_PROJECT_ASSURANCE_ERROR_RESULT_ACTIVE;
    }
    if (!evo_assurance_candidate_valid(config, assurance)) {
        return EVO_PROJECT_ASSURANCE_ERROR_CANDIDATE_INELIGIBLE;
    }
    status = evo_assurance_validate_policy(config, &policy_value);
    if (status != EVO_PROJECT_ASSURANCE_SUCCESS) {
        return status;
    }
    status = evo_assurance_validate_output_path(config, &normalized_output);
    if (status != EVO_PROJECT_ASSURANCE_SUCCESS) {
        return status;
    }
    owner = evo_project_allocate_zeroed(1U, sizeof(*owner));
    if (owner == NULL) {
        evo_project_release(normalized_output);
        return EVO_PROJECT_ASSURANCE_ERROR_OUT_OF_MEMORY;
    }
    owner->candidate_fingerprint =
        evo_assurance_duplicate(config->candidate->candidate_fingerprint);
    owner->policy_id = evo_assurance_duplicate(config->policy_id);
    owner->execution_provider_identity =
        evo_assurance_duplicate(config->execution_provider_identity);
    owner->output_path = normalized_output;
    normalized_output = NULL;
    owner->gate_count = config->gate_count;
    owner->gates = evo_project_allocate_zeroed(
        owner->gate_count, sizeof(*owner->gates));
    owner->gate_ids = evo_project_allocate_zeroed(
        owner->gate_count, sizeof(*owner->gate_ids));
    owner->profile_ids = evo_project_allocate_zeroed(
        owner->gate_count, sizeof(*owner->profile_ids));
    owner->diagnostic_excerpts = evo_project_allocate_zeroed(
        owner->gate_count, sizeof(*owner->diagnostic_excerpts));
    if (owner->candidate_fingerprint == NULL || owner->policy_id == NULL ||
        owner->execution_provider_identity == NULL || owner->gates == NULL ||
        owner->gate_ids == NULL || owner->profile_ids == NULL ||
        owner->diagnostic_excerpts == NULL ||
        !evo_candidate_buffer_open(&json, config->limits.max_evidence_bytes) ||
        !evo_candidate_buffer_open(&markdown, config->limits.max_evidence_bytes)) {
        status = EVO_PROJECT_ASSURANCE_ERROR_OUT_OF_MEMORY;
        goto finish;
    }
    for (index = 0U; index < owner->gate_count; index += 1U) {
        if (!evo_assurance_copy_gate_identity(owner, &config->gates[index], index)) {
            status = EVO_PROJECT_ASSURANCE_ERROR_OUT_OF_MEMORY;
            goto finish;
        }
    }
    owner->policy_fingerprint = policy_value;
    owner->view.schema_version = EVO_PROJECT_ASSURANCE_SCHEMA_VERSION;
    owner->view.candidate_fingerprint = owner->candidate_fingerprint;
    owner->view.policy_id = owner->policy_id;
    owner->view.execution_provider_identity = owner->execution_provider_identity;
    owner->view.stage = config->stage;
    owner->view.gate_count = owner->gate_count;
    owner->view.gates = owner->gates;
    owner->view.output_path = owner->output_path;
    owner->view.projection_complete = true;
    owner->view.probabilistic_authority = false;
    status = evo_assurance_run_gates(config, owner);
    if (status != EVO_PROJECT_ASSURANCE_SUCCESS) {
        goto finish;
    }
    owner->view.performance_eligible =
        evo_assurance_required_gates_passed(config, owner);
    owner->view.champion_eligible =
        config->stage == EVO_PROJECT_ASSURANCE_STAGE_FINALIST &&
        owner->view.performance_eligible;
    owner->assurance_fingerprint = evo_assurance_fingerprint_result(config, owner);
    evo_project_fingerprint_format(owner->policy_fingerprint, policy_fingerprint);
    evo_project_fingerprint_format(
        owner->assurance_fingerprint, assurance_fingerprint);
    if (!evo_assurance_build_json(
            config,
            owner,
            policy_fingerprint,
            assurance_fingerprint,
            &json) ||
        !evo_assurance_build_markdown(
            config,
            owner,
            policy_fingerprint,
            assurance_fingerprint,
            &markdown)) {
        status = EVO_PROJECT_ASSURANCE_ERROR_RESOURCE_LIMIT;
        goto finish;
    }
    owner->canonical_json = json.bytes;
    owner->canonical_json_size = json.size;
    json.bytes = NULL;
    json.size = 0U;
    json.capacity = 0U;
    owner->audit_markdown = markdown.bytes;
    owner->audit_markdown_size = markdown.size;
    markdown.bytes = NULL;
    markdown.size = 0U;
    markdown.capacity = 0U;
    owner->view.canonical_json = owner->canonical_json;
    owner->view.canonical_json_size = owner->canonical_json_size;
    owner->view.audit_markdown = owner->audit_markdown;
    owner->view.audit_markdown_size = owner->audit_markdown_size;
    evo_project_fingerprint_format(
        owner->policy_fingerprint, owner->view.policy_fingerprint);
    evo_project_fingerprint_format(
        owner->assurance_fingerprint, owner->view.assurance_fingerprint);
    status = evo_assurance_publish_evidence(owner);
    if (status != EVO_PROJECT_ASSURANCE_SUCCESS) {
        goto finish;
    }
    owner->view.private_owner = owner;
    *assurance = owner->view;
    owner_published = true;

finish:
    evo_candidate_buffer_close(&markdown);
    evo_candidate_buffer_close(&json);
    evo_project_release(normalized_output);
    if (!owner_published) {
        evo_assurance_owner_destroy(owner);
        evo_project_release(owner);
    }
    return status;
}

void evo_project_assurance_destroy(evo_project_assurance_t *assurance)
{
    evo_project_assurance_owner_t *owner;

    if (assurance == NULL || assurance->private_owner == NULL) {
        return;
    }
    owner = (evo_project_assurance_owner_t *)assurance->private_owner;
    evo_assurance_owner_destroy(owner);
    evo_project_release(owner);
    *assurance = (evo_project_assurance_t){0};
}
