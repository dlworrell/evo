#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L

#include "internal/project_ingestion.h"

#include "internal/project_baseline_owner.h"
#include "internal/project_evidence.h"
#include "internal/project_fingerprint.h"
#include "internal/project_manifest.h"
#include "internal/project_runtime.h"
#include "internal/project_snapshot.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static bool evo_project_capture_limits_valid(
    const evo_project_ingest_limits_t *limits)
{
    return limits->max_manifest_bytes > 0U &&
           limits->max_json_tokens > 0U && limits->max_json_depth > 0U &&
           limits->max_string_bytes > 0U && limits->max_path_bytes > 0U &&
           limits->max_files > 0U && limits->max_file_bytes > 0U &&
           limits->max_total_bytes > 0U &&
           limits->max_compilation_database_bytes > 0U &&
           limits->max_permitted_roots > 0U &&
           limits->max_dependencies > 0U &&
           limits->max_toolchains > 0U &&
           limits->max_environment_entries > 0U &&
           limits->max_targets > 0U && limits->max_workloads > 0U &&
           limits->max_constraints > 0U &&
           limits->max_command_args > 0U &&
           limits->max_command_bytes > 0U &&
           limits->max_command_output_bytes > 0U &&
           limits->max_evidence_bytes > 0U &&
           limits->max_command_timeout_ms > 0U &&
           limits->max_memory_bytes > 0U &&
           limits->max_processes > 0U && limits->max_storage_bytes > 0U;
}

static bool evo_project_capture_text_valid(
    const char *value,
    size_t maximum_bytes)
{
    size_t index;

    if (value == NULL || value[0] == '\0') {
        return false;
    }
    for (index = 0U; value[index] != '\0'; index += 1U) {
        const unsigned char byte = (unsigned char)value[index];

        if (index >= maximum_bytes || byte < 0x20U || byte == 0x7fU) {
            return false;
        }
    }
    return true;
}

static bool evo_project_capture_config_valid(
    const evo_project_capture_config_t *config,
    const evo_project_baseline_t *baseline)
{
    return config != NULL && baseline != NULL &&
           config->manifest_path != NULL &&
           config->authorized_project_root != NULL &&
           config->output_path != NULL &&
           config->command_runner != NULL &&
           evo_project_capture_limits_valid(&config->limits) &&
           evo_project_capture_text_valid(
               config->execution_provider_identity,
               config->limits.max_string_bytes);
}

static evo_project_status_t evo_project_read_manifest(
    const evo_project_capture_config_t *config,
    char **text,
    size_t *text_size)
{
    struct stat before;
    struct stat after;
    char *bytes;
    int file_fd;
    size_t size;
    size_t position = 0U;

    file_fd = open(
        config->manifest_path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (file_fd < 0 || fstat(file_fd, &before) != 0 ||
        !S_ISREG(before.st_mode) || before.st_size <= 0) {
        if (file_fd >= 0) {
            (void)close(file_fd);
        }
        return EVO_PROJECT_ERROR_MANIFEST_IO;
    }
    if ((uintmax_t)before.st_size >
        (uintmax_t)config->limits.max_manifest_bytes) {
        (void)close(file_fd);
        return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
    }
    size = (size_t)before.st_size;
    if (size == SIZE_MAX) {
        (void)close(file_fd);
        return EVO_PROJECT_ERROR_RESOURCE_LIMIT;
    }
    bytes = evo_project_allocate_zeroed(size + 1U, sizeof(*bytes));
    if (bytes == NULL) {
        (void)close(file_fd);
        return EVO_PROJECT_ERROR_OUT_OF_MEMORY;
    }
    while (position < size) {
        const ssize_t read_count = read(file_fd, bytes + position, size - position);

        if (read_count < 0 && errno == EINTR) {
            continue;
        }
        if (read_count <= 0) {
            evo_project_release(bytes);
            (void)close(file_fd);
            return EVO_PROJECT_ERROR_MANIFEST_IO;
        }
        position += (size_t)read_count;
    }
    if (fstat(file_fd, &after) != 0 || after.st_size != before.st_size ||
        after.st_mtime != before.st_mtime || after.st_ctime != before.st_ctime) {
        evo_project_release(bytes);
        (void)close(file_fd);
        return EVO_PROJECT_ERROR_MANIFEST_IO;
    }
    if (close(file_fd) != 0) {
        evo_project_release(bytes);
        return EVO_PROJECT_ERROR_MANIFEST_IO;
    }
    bytes[size] = '\0';
    *text = bytes;
    *text_size = size;
    return EVO_PROJECT_SUCCESS;
}

static evo_project_command_disposition_t evo_project_outcome_disposition(
    const evo_project_command_outcome_t *outcome)
{
    if (outcome->timed_out) {
        return EVO_PROJECT_COMMAND_TIMED_OUT;
    }
    return outcome->exit_code == 0 ? EVO_PROJECT_COMMAND_PASSED
                                   : EVO_PROJECT_COMMAND_FAILED;
}

static evo_project_status_t evo_project_run_command(
    const evo_project_capture_config_t *config,
    evo_project_baseline_owner_t *owner,
    size_t command_index)
{
    const evo_project_manifest_command_t *command =
        &owner->manifest.commands[command_index];
    evo_project_command_outcome_t outcome = {0};
    evo_project_command_view_t view;
    evo_project_status_t status;

    view.stage = command->stage;
    view.stage_id = command->stage_id;
    view.argument_count = command->argument_count;
    view.arguments = (const char *const *)command->arguments;
    view.timeout_ms = owner->manifest.budget.command_timeout_ms;
    view.max_memory_bytes = owner->manifest.budget.max_memory_bytes;
    view.max_processes = owner->manifest.budget.max_processes;
    view.max_storage_bytes = owner->manifest.budget.max_storage_bytes;
    view.max_output_bytes = owner->manifest.budget.max_command_output_bytes;
    view.network_access = owner->manifest.budget.network_access;
    outcome.schema_version = EVO_PROJECT_BASELINE_SCHEMA_VERSION;
    status = config->command_runner(
        &view,
        owner->workspace_path,
        config->command_runner_context,
        &outcome);
    if (status != EVO_PROJECT_SUCCESS ||
        outcome.schema_version != EVO_PROJECT_BASELINE_SCHEMA_VERSION ||
        !outcome.completed || outcome.exit_code < 0 || outcome.exit_code > 255 ||
        outcome.output_bytes > owner->manifest.budget.max_command_output_bytes ||
        (outcome.timed_out && outcome.exit_code == 0)) {
        return status == EVO_PROJECT_ERROR_OUT_OF_MEMORY
                   ? status
                   : EVO_PROJECT_ERROR_EXECUTION_PROVIDER;
    }
    owner->commands[command_index].stage = command->stage;
    owner->commands[command_index].stage_id = command->stage_id;
    owner->commands[command_index].disposition =
        evo_project_outcome_disposition(&outcome);
    owner->commands[command_index].exit_code = outcome.exit_code;
    owner->commands[command_index].output_bytes = outcome.output_bytes;
    owner->commands[command_index].output_fingerprint =
        outcome.output_fingerprint;
    return EVO_PROJECT_SUCCESS;
}

static evo_project_status_t evo_project_run_baseline_gates(
    const evo_project_capture_config_t *config,
    evo_project_baseline_owner_t *owner)
{
    size_t index;
    evo_project_status_t status;

    for (index = 0U; index < EVO_PROJECT_COMMAND_COUNT; index += 1U) {
        owner->commands[index].stage = owner->manifest.commands[index].stage;
        owner->commands[index].stage_id =
            owner->manifest.commands[index].stage_id;
        owner->commands[index].disposition = EVO_PROJECT_COMMAND_NOT_RUN;
        owner->commands[index].exit_code = -1;
        owner->commands[index].output_bytes = 0U;
        owner->commands[index].output_fingerprint = 0U;
    }

    status = evo_project_run_command(config, owner, 0U);
    if (status != EVO_PROJECT_SUCCESS) {
        return status;
    }
    if (owner->commands[0].disposition != EVO_PROJECT_COMMAND_PASSED) {
        owner->state = EVO_PROJECT_BASELINE_BUILD_FAILED;
        return EVO_PROJECT_SUCCESS;
    }
    status = evo_project_run_command(config, owner, 1U);
    if (status != EVO_PROJECT_SUCCESS) {
        return status;
    }
    if (owner->commands[1].disposition != EVO_PROJECT_COMMAND_PASSED) {
        owner->state = EVO_PROJECT_BASELINE_BUILD_FAILED;
        return EVO_PROJECT_SUCCESS;
    }
    status = evo_project_run_command(config, owner, 2U);
    if (status != EVO_PROJECT_SUCCESS) {
        return status;
    }
    if (owner->commands[2].disposition != EVO_PROJECT_COMMAND_PASSED) {
        owner->state = EVO_PROJECT_BASELINE_CORRECTNESS_FAILED;
        return EVO_PROJECT_SUCCESS;
    }
    if (!owner->manifest.benchmark_required) {
        owner->state = EVO_PROJECT_BASELINE_ELIGIBLE;
        return EVO_PROJECT_SUCCESS;
    }
    status = evo_project_run_command(config, owner, 3U);
    if (status != EVO_PROJECT_SUCCESS) {
        return status;
    }
    owner->state =
        owner->commands[3].disposition == EVO_PROJECT_COMMAND_PASSED
            ? EVO_PROJECT_BASELINE_ELIGIBLE
            : EVO_PROJECT_BASELINE_BENCHMARK_INELIGIBLE;
    return EVO_PROJECT_SUCCESS;
}

static void evo_project_publish_baseline(
    evo_project_baseline_owner_t *owner,
    evo_project_baseline_t *baseline)
{
    baseline->schema_version = EVO_PROJECT_BASELINE_SCHEMA_VERSION;
    baseline->state = owner->state;
    baseline->manifest_id = owner->manifest.manifest_id;
    baseline->source_declared_identity = owner->manifest.source_identity;
    baseline->build_frontend = owner->manifest.build_frontend;
    baseline->language = owner->manifest.language;
    baseline->compilation_database = owner->manifest.compilation_database;
    baseline->execution_provider_identity = owner->execution_provider_identity;
    baseline->output_path = owner->output_path;
    evo_project_fingerprint_format(
        owner->manifest.fingerprint, baseline->manifest_fingerprint);
    evo_project_fingerprint_format(
        owner->baseline_fingerprint, baseline->baseline_fingerprint);
    baseline->file_count = owner->file_count;
    baseline->total_file_bytes = owner->total_file_bytes;
    baseline->files = owner->files;
    baseline->compilation_unit_count = owner->compilation_unit_count;
    baseline->compilation_units = owner->compilation_units;
    evo_project_fingerprint_format(
        owner->normalized_build_fingerprint,
        baseline->normalized_build_fingerprint);
    baseline->command_count = EVO_PROJECT_COMMAND_COUNT;
    baseline->commands = owner->commands;
    baseline->projection_complete = true;
    baseline->probabilistic_authority = false;
    baseline->private_owner = owner;
}

evo_project_status_t evo_project_capture_baseline(
    const evo_project_capture_config_t *config,
    evo_project_baseline_t *baseline)
{
    evo_project_baseline_owner_t *owner;
    char *manifest_text = NULL;
    size_t manifest_size = 0U;
    evo_project_status_t status;

    if (!evo_project_capture_config_valid(config, baseline)) {
        return EVO_PROJECT_ERROR_INVALID_ARGUMENT;
    }
    if (baseline->private_owner != NULL || baseline->schema_version != 0U) {
        return EVO_PROJECT_ERROR_RESULT_ACTIVE;
    }
    owner = evo_project_allocate_zeroed(1U, sizeof(*owner));
    if (owner == NULL) {
        return EVO_PROJECT_ERROR_OUT_OF_MEMORY;
    }
    status = evo_project_read_manifest(config, &manifest_text, &manifest_size);
    if (status == EVO_PROJECT_SUCCESS) {
        status = evo_project_manifest_parse(
            manifest_text, manifest_size, &config->limits, &owner->manifest);
    }
    evo_project_release(manifest_text);
    if (status == EVO_PROJECT_SUCCESS) {
        status = evo_project_snapshot_prepare(config, owner);
    }
    if (status == EVO_PROJECT_SUCCESS) {
        status = evo_project_run_baseline_gates(config, owner);
    }
    if (status == EVO_PROJECT_SUCCESS) {
        status = evo_project_snapshot_verify_source(owner);
    }
    if (status == EVO_PROJECT_SUCCESS) {
        status = evo_project_snapshot_remove_workspace(owner);
    }
    if (status == EVO_PROJECT_SUCCESS) {
        status = evo_project_write_baseline_evidence(owner);
    }
    if (status == EVO_PROJECT_SUCCESS) {
        status = evo_project_snapshot_commit(owner);
    }
    if (status != EVO_PROJECT_SUCCESS) {
        evo_project_snapshot_discard(owner);
        evo_project_manifest_destroy(&owner->manifest);
        evo_project_release(owner);
        return status;
    }
    evo_project_publish_baseline(owner, baseline);
    return EVO_PROJECT_SUCCESS;
}

void evo_project_baseline_destroy(evo_project_baseline_t *baseline)
{
    evo_project_baseline_owner_t *owner;

    if (baseline == NULL) {
        return;
    }
    owner = baseline->private_owner;
    if (owner != NULL) {
        evo_project_snapshot_discard(owner);
        evo_project_manifest_destroy(&owner->manifest);
        evo_project_release(owner);
    }
    *baseline = (evo_project_baseline_t){0};
}

const char *evo_project_status_name(evo_project_status_t status)
{
    switch (status) {
    case EVO_PROJECT_SUCCESS:
        return "success";
    case EVO_PROJECT_ERROR_INVALID_ARGUMENT:
        return "invalid-argument";
    case EVO_PROJECT_ERROR_RESULT_ACTIVE:
        return "result-active";
    case EVO_PROJECT_ERROR_MANIFEST_IO:
        return "manifest-io";
    case EVO_PROJECT_ERROR_MANIFEST_INVALID:
        return "manifest-invalid";
    case EVO_PROJECT_ERROR_RESOURCE_LIMIT:
        return "resource-limit";
    case EVO_PROJECT_ERROR_OUT_OF_MEMORY:
        return "out-of-memory";
    case EVO_PROJECT_ERROR_PATH_INVALID:
        return "path-invalid";
    case EVO_PROJECT_ERROR_SOURCE_IO:
        return "source-io";
    case EVO_PROJECT_ERROR_SOURCE_CHANGED:
        return "source-changed";
    case EVO_PROJECT_ERROR_OUTPUT_EXISTS:
        return "output-exists";
    case EVO_PROJECT_ERROR_EXECUTION_PROVIDER:
        return "execution-provider";
    case EVO_PROJECT_ERROR_EVIDENCE_IO:
        return "evidence-io";
    case EVO_PROJECT_ERROR_STATE:
    default:
        return "state";
    }
}

const char *evo_project_baseline_state_name(
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
