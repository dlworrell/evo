#include "internal/project_assurance_internal.h"

#include "internal/project_fingerprint.h"
#include "internal/project_runtime.h"

#include <ctype.h>
#include <limits.h>
#include <string.h>

static bool evo_assurance_stage_valid(evo_project_assurance_stage_t stage)
{
    return stage == EVO_PROJECT_ASSURANCE_STAGE_FAST ||
           stage == EVO_PROJECT_ASSURANCE_STAGE_FINALIST;
}

static bool evo_assurance_key_equals(
    const char *entry,
    const char *key)
{
    size_t index = 0U;

    while (key[index] != '\0' && entry[index] == key[index]) {
        index += 1U;
    }
    return key[index] == '\0' && entry[index] == '=';
}

static bool evo_assurance_environment_key_safe(const char *entry)
{
    static const char *const denied[] = {
        "BASH_ENV",
        "CDPATH",
        "ENV",
        "GLOBIGNORE",
        "IFS",
        "LD_PRELOAD",
        "LD_LIBRARY_PATH",
        "PS4",
        "SHELLOPTS"};
    size_t index;

    if (strncmp(entry, "DYLD_", 5U) == 0) {
        return false;
    }
    for (index = 0U; index < sizeof(denied) / sizeof(denied[0]); index += 1U) {
        if (evo_assurance_key_equals(entry, denied[index])) {
            return false;
        }
    }
    return true;
}

static bool evo_assurance_same_environment_key(
    const char *left,
    const char *right)
{
    size_t left_size = 0U;
    size_t right_size = 0U;

    while (left[left_size] != '\0' && left[left_size] != '=') {
        left_size += 1U;
    }
    while (right[right_size] != '\0' && right[right_size] != '=') {
        right_size += 1U;
    }
    return left_size == right_size &&
           strncmp(left, right, left_size) == 0;
}

static bool evo_assurance_shell_basename(const char *path)
{
    static const char *const shells[] = {
        "sh", "bash", "dash", "zsh", "fish", "csh", "tcsh", "ksh"};
    const char *basename = strrchr(path, '/');
    size_t index;

    basename = basename == NULL ? path : basename + 1;
    for (index = 0U; index < sizeof(shells) / sizeof(shells[0]); index += 1U) {
        if (strcmp(basename, shells[index]) == 0) {
            return true;
        }
    }
    return false;
}

bool evo_assurance_limits_valid(const evo_project_assurance_limits_t *limits)
{
    return limits != NULL && limits->max_string_bytes > 0U &&
           limits->max_path_bytes > 0U && limits->max_gates > 0U &&
           limits->max_arguments > 0U &&
           limits->max_environment_entries > 0U &&
           limits->max_command_bytes > 0U && limits->max_output_bytes > 0U &&
           limits->max_diagnostic_bytes > 0U &&
           limits->max_evidence_bytes > 0U && limits->max_timeout_ms > 0U &&
           limits->max_memory_bytes > 0U && limits->max_processes > 0U &&
           limits->max_storage_bytes > 0U;
}

bool evo_assurance_text_valid(const char *value, size_t maximum_bytes)
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

bool evo_assurance_diagnostic_valid(
    const char *value,
    size_t byte_count,
    size_t maximum_bytes)
{
    size_t index;

    if (byte_count == 0U) {
        return value == NULL || value[0] == '\0';
    }
    if (value == NULL || byte_count > maximum_bytes) {
        return false;
    }
    for (index = 0U; index < byte_count; index += 1U) {
        const unsigned char byte = (unsigned char)value[index];

        if (byte == 0U ||
            (byte < 0x20U && byte != (unsigned char)'\n' &&
             byte != (unsigned char)'\r' && byte != (unsigned char)'\t') ||
            byte == 0x7fU) {
            return false;
        }
    }
    return value[byte_count] == '\0';
}

bool evo_assurance_relative_path_valid(const char *path, size_t maximum_bytes)
{
    size_t start = 0U;
    size_t index;

    if (path == NULL || path[0] == '\0' || path[0] == '/') {
        return false;
    }
    if (strcmp(path, ".") == 0) {
        return true;
    }
    for (index = 0U;; index += 1U) {
        const unsigned char byte = (unsigned char)path[index];

        if (index >= maximum_bytes) {
            return false;
        }
        if (byte == (unsigned char)'\\' || byte < 0x20U || byte == 0x7fU) {
            return false;
        }
        if (byte == (unsigned char)'/' || byte == 0U) {
            const size_t component_size = index - start;

            if (component_size == 0U ||
                (component_size == 1U && path[start] == '.') ||
                (component_size == 2U && path[start] == '.' &&
                 path[start + 1U] == '.')) {
                return false;
            }
            if (byte == 0U) {
                return true;
            }
            start = index + 1U;
        }
    }
}

bool evo_assurance_environment_valid(
    const char *entry,
    size_t maximum_bytes)
{
    size_t index;

    if (!evo_assurance_text_valid(entry, maximum_bytes) ||
        !evo_assurance_environment_key_safe(entry)) {
        return false;
    }
    if (!(entry[0] == '_' || isalpha((unsigned char)entry[0]) != 0)) {
        return false;
    }
    for (index = 1U; entry[index] != '\0' && entry[index] != '='; index += 1U) {
        if (!(entry[index] == '_' || isalnum((unsigned char)entry[index]) != 0)) {
            return false;
        }
    }
    return entry[index] == '=';
}

bool evo_assurance_executable_valid(const char *path, size_t maximum_bytes)
{
    return evo_assurance_text_valid(path, maximum_bytes) && path[0] == '/' &&
           !evo_assurance_shell_basename(path);
}

char *evo_assurance_duplicate(const char *value)
{
    return value == NULL ? NULL : evo_assurance_duplicate_n(value, strlen(value));
}

char *evo_assurance_duplicate_n(const char *value, size_t byte_count)
{
    char *copy;
    size_t index;

    if (value == NULL || byte_count == SIZE_MAX) {
        return NULL;
    }
    copy = evo_project_allocate_zeroed(byte_count + 1U, sizeof(*copy));
    if (copy == NULL) {
        return NULL;
    }
    for (index = 0U; index < byte_count; index += 1U) {
        copy[index] = value[index];
    }
    copy[byte_count] = '\0';
    return copy;
}

bool evo_assurance_gate_selected(
    evo_project_assurance_stage_t requested_stage,
    evo_project_assurance_stage_t gate_stage)
{
    return gate_stage == EVO_PROJECT_ASSURANCE_STAGE_FAST ||
           (requested_stage == EVO_PROJECT_ASSURANCE_STAGE_FINALIST &&
            gate_stage == EVO_PROJECT_ASSURANCE_STAGE_FINALIST);
}

static bool evo_assurance_profile_required(
    const evo_project_assurance_config_t *config,
    const char *profile_id)
{
    size_t index;

    for (index = 0U; index < config->required_profile_count; index += 1U) {
        if (strcmp(config->required_profiles[index], profile_id) == 0) {
            return true;
        }
    }
    return false;
}

static bool evo_assurance_profile_has_gate(
    const evo_project_assurance_config_t *config,
    const char *profile_id)
{
    size_t index;

    for (index = 0U; index < config->gate_count; index += 1U) {
        const evo_project_assurance_gate_t *gate = &config->gates[index];

        if (gate->required &&
            evo_assurance_gate_selected(config->stage, gate->stage) &&
            strcmp(gate->profile_id, profile_id) == 0) {
            return true;
        }
    }
    return false;
}

static bool evo_assurance_gate_strings_valid(
    const evo_project_assurance_config_t *config,
    const evo_project_assurance_gate_t *gate)
{
    size_t index;
    size_t command_bytes = 0U;

    if (!evo_assurance_text_valid(gate->gate_id, config->limits.max_string_bytes) ||
        !evo_assurance_text_valid(gate->profile_id, config->limits.max_string_bytes) ||
        !evo_assurance_stage_valid(gate->stage) ||
        gate->argument_count == 0U ||
        gate->argument_count > config->limits.max_arguments ||
        gate->arguments == NULL ||
        !evo_assurance_executable_valid(
            gate->arguments[0], config->limits.max_path_bytes) ||
        gate->environment_count > config->limits.max_environment_entries ||
        (gate->environment_count > 0U && gate->environment == NULL) ||
        !evo_assurance_relative_path_valid(
            gate->working_directory, config->limits.max_path_bytes)) {
        return false;
    }
    for (index = 0U; index < gate->argument_count; index += 1U) {
        size_t argument_bytes;

        if (!evo_assurance_text_valid(
                gate->arguments[index], config->limits.max_string_bytes)) {
            return false;
        }
        argument_bytes = strlen(gate->arguments[index]);
        if (argument_bytes >= config->limits.max_command_bytes ||
            command_bytes > config->limits.max_command_bytes - argument_bytes - 1U) {
            return false;
        }
        command_bytes += argument_bytes + 1U;
    }
    for (index = 0U; index < gate->environment_count; index += 1U) {
        size_t other;

        if (!evo_assurance_environment_valid(
                gate->environment[index], config->limits.max_string_bytes)) {
            return false;
        }
        for (other = 0U; other < index; other += 1U) {
            if (evo_assurance_same_environment_key(
                    gate->environment[index], gate->environment[other])) {
                return false;
            }
        }
    }
    return true;
}

static bool evo_assurance_gate_resources_valid(
    const evo_project_assurance_config_t *config,
    const evo_project_assurance_gate_t *gate)
{
    return gate->timeout_ms > 0U &&
           gate->timeout_ms <= config->limits.max_timeout_ms &&
           gate->max_memory_bytes > 0U &&
           gate->max_memory_bytes <= config->limits.max_memory_bytes &&
           gate->max_processes > 0U &&
           gate->max_processes <= config->limits.max_processes &&
           gate->max_storage_bytes > 0U &&
           gate->max_storage_bytes <= config->limits.max_storage_bytes &&
           gate->max_output_bytes > 0U &&
           gate->max_output_bytes <= config->limits.max_output_bytes &&
           (!gate->network_access || config->allow_network_gates);
}

static void evo_assurance_fingerprint_gate(
    evo_project_fingerprint_t *fingerprint,
    const evo_project_assurance_gate_t *gate)
{
    size_t index;

    evo_project_fingerprint_string(fingerprint, gate->gate_id);
    evo_project_fingerprint_string(fingerprint, gate->profile_id);
    evo_project_fingerprint_u64(fingerprint, (uint64_t)gate->stage);
    evo_project_fingerprint_u64(fingerprint, gate->required ? 1U : 0U);
    evo_project_fingerprint_u64(fingerprint, (uint64_t)gate->argument_count);
    for (index = 0U; index < gate->argument_count; index += 1U) {
        evo_project_fingerprint_string(fingerprint, gate->arguments[index]);
    }
    evo_project_fingerprint_u64(fingerprint, (uint64_t)gate->environment_count);
    for (index = 0U; index < gate->environment_count; index += 1U) {
        evo_project_fingerprint_string(fingerprint, gate->environment[index]);
    }
    evo_project_fingerprint_string(fingerprint, gate->working_directory);
    evo_project_fingerprint_u64(fingerprint, gate->timeout_ms);
    evo_project_fingerprint_u64(fingerprint, gate->max_memory_bytes);
    evo_project_fingerprint_u64(fingerprint, (uint64_t)gate->max_processes);
    evo_project_fingerprint_u64(fingerprint, gate->max_storage_bytes);
    evo_project_fingerprint_u64(fingerprint, (uint64_t)gate->max_output_bytes);
    evo_project_fingerprint_u64(fingerprint, gate->network_access ? 1U : 0U);
}

evo_project_assurance_status_t evo_assurance_validate_policy(
    const evo_project_assurance_config_t *config,
    uint64_t *policy_fingerprint)
{
    evo_project_fingerprint_t fingerprint;
    size_t index;
    size_t required_selected = 0U;

    if (config == NULL || policy_fingerprint == NULL ||
        !evo_assurance_limits_valid(&config->limits) ||
        !evo_assurance_stage_valid(config->stage) ||
        !evo_assurance_text_valid(config->policy_id, config->limits.max_string_bytes) ||
        !evo_assurance_text_valid(
            config->execution_provider_identity, config->limits.max_string_bytes) ||
        config->required_profile_count == 0U || config->required_profiles == NULL ||
        config->gate_count == 0U || config->gate_count > config->limits.max_gates ||
        config->gates == NULL || config->runner == NULL) {
        return EVO_PROJECT_ASSURANCE_ERROR_POLICY_INVALID;
    }
    if (config->stage == EVO_PROJECT_ASSURANCE_STAGE_FINALIST &&
        config->required_profile_count < 2U) {
        return EVO_PROJECT_ASSURANCE_ERROR_POLICY_INVALID;
    }
    for (index = 0U; index < config->required_profile_count; index += 1U) {
        size_t other;

        if (!evo_assurance_text_valid(
                config->required_profiles[index], config->limits.max_string_bytes)) {
            return EVO_PROJECT_ASSURANCE_ERROR_POLICY_INVALID;
        }
        for (other = 0U; other < index; other += 1U) {
            if (strcmp(config->required_profiles[index],
                       config->required_profiles[other]) == 0) {
                return EVO_PROJECT_ASSURANCE_ERROR_POLICY_INVALID;
            }
        }
    }
    for (index = 0U; index < config->gate_count; index += 1U) {
        const evo_project_assurance_gate_t *gate = &config->gates[index];
        size_t other;

        if (!evo_assurance_gate_strings_valid(config, gate) ||
            !evo_assurance_gate_resources_valid(config, gate)) {
            return EVO_PROJECT_ASSURANCE_ERROR_POLICY_INVALID;
        }
        for (other = 0U; other < index; other += 1U) {
            if (strcmp(gate->gate_id, config->gates[other].gate_id) == 0) {
                return EVO_PROJECT_ASSURANCE_ERROR_POLICY_INVALID;
            }
        }
        if (gate->required && evo_assurance_gate_selected(config->stage, gate->stage)) {
            required_selected += 1U;
            if (!evo_assurance_profile_required(config, gate->profile_id)) {
                return EVO_PROJECT_ASSURANCE_ERROR_POLICY_INVALID;
            }
        }
    }
    if (required_selected == 0U) {
        return EVO_PROJECT_ASSURANCE_ERROR_POLICY_INVALID;
    }
    for (index = 0U; index < config->required_profile_count; index += 1U) {
        if (!evo_assurance_profile_has_gate(config, config->required_profiles[index])) {
            return EVO_PROJECT_ASSURANCE_ERROR_POLICY_INVALID;
        }
    }
    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_string(&fingerprint, "evo-project-assurance-policy-v1");
    evo_project_fingerprint_string(&fingerprint, config->policy_id);
    evo_project_fingerprint_string(&fingerprint, config->execution_provider_identity);
    evo_project_fingerprint_u64(&fingerprint, (uint64_t)config->stage);
    evo_project_fingerprint_u64(
        &fingerprint, (uint64_t)config->required_profile_count);
    for (index = 0U; index < config->required_profile_count; index += 1U) {
        evo_project_fingerprint_string(&fingerprint, config->required_profiles[index]);
    }
    evo_project_fingerprint_u64(&fingerprint, (uint64_t)config->gate_count);
    for (index = 0U; index < config->gate_count; index += 1U) {
        evo_assurance_fingerprint_gate(&fingerprint, &config->gates[index]);
    }
    *policy_fingerprint = fingerprint.value;
    return EVO_PROJECT_ASSURANCE_SUCCESS;
}

evo_project_assurance_disposition_t evo_assurance_outcome_disposition(
    const evo_project_assurance_gate_outcome_t *outcome)
{
    if (!outcome->available) {
        return EVO_PROJECT_ASSURANCE_GATE_UNAVAILABLE;
    }
    if (!outcome->filesystem_policy_enforced ||
        !outcome->network_policy_enforced || outcome->source_modified ||
        outcome->snapshot_modified) {
        return EVO_PROJECT_ASSURANCE_GATE_POLICY_FAILED;
    }
    if (!outcome->process_group_clean) {
        return EVO_PROJECT_ASSURANCE_GATE_CLEANUP_FAILED;
    }
    if (outcome->resource_exhausted) {
        return EVO_PROJECT_ASSURANCE_GATE_RESOURCE_EXHAUSTED;
    }
    if (outcome->timed_out) {
        return EVO_PROJECT_ASSURANCE_GATE_TIMED_OUT;
    }
    if (outcome->signaled) {
        return EVO_PROJECT_ASSURANCE_GATE_SIGNALED;
    }
    return outcome->exit_code == 0 ? EVO_PROJECT_ASSURANCE_GATE_PASSED
                                   : EVO_PROJECT_ASSURANCE_GATE_FAILED;
}

const char *evo_project_assurance_status_name(
    evo_project_assurance_status_t status)
{
    switch (status) {
    case EVO_PROJECT_ASSURANCE_SUCCESS:
        return "success";
    case EVO_PROJECT_ASSURANCE_ERROR_INVALID_ARGUMENT:
        return "invalid-argument";
    case EVO_PROJECT_ASSURANCE_ERROR_RESULT_ACTIVE:
        return "result-active";
    case EVO_PROJECT_ASSURANCE_ERROR_CANDIDATE_INELIGIBLE:
        return "candidate-ineligible";
    case EVO_PROJECT_ASSURANCE_ERROR_POLICY_INVALID:
        return "policy-invalid";
    case EVO_PROJECT_ASSURANCE_ERROR_PATH_INVALID:
        return "path-invalid";
    case EVO_PROJECT_ASSURANCE_ERROR_OUTPUT_EXISTS:
        return "output-exists";
    case EVO_PROJECT_ASSURANCE_ERROR_RESOURCE_LIMIT:
        return "resource-limit";
    case EVO_PROJECT_ASSURANCE_ERROR_OUT_OF_MEMORY:
        return "out-of-memory";
    case EVO_PROJECT_ASSURANCE_ERROR_EXECUTION_PROVIDER:
        return "execution-provider";
    case EVO_PROJECT_ASSURANCE_ERROR_EVIDENCE:
        return "evidence";
    case EVO_PROJECT_ASSURANCE_ERROR_STATE:
        return "state";
    default:
        return "unknown";
    }
}

const char *evo_project_assurance_stage_name(
    evo_project_assurance_stage_t stage)
{
    switch (stage) {
    case EVO_PROJECT_ASSURANCE_STAGE_FAST:
        return "fast";
    case EVO_PROJECT_ASSURANCE_STAGE_FINALIST:
        return "finalist";
    default:
        return "unknown";
    }
}

const char *evo_project_assurance_disposition_name(
    evo_project_assurance_disposition_t disposition)
{
    switch (disposition) {
    case EVO_PROJECT_ASSURANCE_GATE_NOT_RUN:
        return "not-run";
    case EVO_PROJECT_ASSURANCE_GATE_PASSED:
        return "passed";
    case EVO_PROJECT_ASSURANCE_GATE_FAILED:
        return "failed";
    case EVO_PROJECT_ASSURANCE_GATE_TIMED_OUT:
        return "timed-out";
    case EVO_PROJECT_ASSURANCE_GATE_SIGNALED:
        return "signaled";
    case EVO_PROJECT_ASSURANCE_GATE_RESOURCE_EXHAUSTED:
        return "resource-exhausted";
    case EVO_PROJECT_ASSURANCE_GATE_UNAVAILABLE:
        return "unavailable";
    case EVO_PROJECT_ASSURANCE_GATE_POLICY_FAILED:
        return "policy-failed";
    case EVO_PROJECT_ASSURANCE_GATE_CLEANUP_FAILED:
        return "cleanup-failed";
    default:
        return "unknown";
    }
}
