#include "internal/project_command.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static int check(bool condition)
{
    return condition ? 0 : 1;
}

static evo_project_manifest_t valid_manifest(void)
{
    evo_project_manifest_t manifest = {0};

    manifest.schema = "catalyst.evo-project-manifest.v1";
    manifest.manifest_id = "command-test-manifest";
    manifest.source_identity = "source:test";
    manifest.build_frontend = "cmake";
    manifest.language = "c17";
    manifest.budget.max_path_bytes = 4096U;
    manifest.budget.max_evidence_bytes = 1024U * 1024U;
    manifest.budget.command_timeout_ms = UINT64_C(1000);
    manifest.budget.max_processes = 8U;
    return manifest;
}

static evo_project_command_request_t valid_request(
    evo_project_command_operation_t operation,
    const evo_project_manifest_t *manifest)
{
    evo_project_command_request_t request = {0};
    size_t slot;

    request.schema_version = EVO_PROJECT_COMMAND_SCHEMA_VERSION;
    request.operation = operation;
    request.manifest_path = "manifest.json";
    request.manifest = manifest;
    request.input_path = "project";
    request.output_path = "output";
    request.evidence_path = "evidence";
    request.checkpoint_path = NULL;
    request.provider_policy_identity = EVO_PROJECT_COMMAND_PROVIDER_POLICY_ID;
    request.resume = false;
    request.overwrite_output = false;
    request.replay_identity_complete = true;
    request.external_inputs_declared = true;

    for (slot = 0U; slot < EVO_PROJECT_COMMAND_PROVIDER_COUNT; slot += 1U) {
        const evo_project_command_provider_requirement_t *requirement =
            evo_project_command_provider_requirement(
                (evo_project_command_provider_slot_t)slot);

        request.providers[slot].identity = requirement->identity;
        request.providers[slot].implementation_version =
            requirement->implementation_version;
        request.providers[slot].capabilities = requirement->required_capabilities;
        request.providers[slot].available = true;
    }
    return request;
}

static int test_registry(void)
{
    static const char *const expected_names[] = {
        "analyze",
        "evolve",
        "replay",
        "report"};
    size_t index;

    if (check(evo_project_command_registry_count() == EVO_PROJECT_COMMAND_COUNT) != 0) {
        return 1;
    }
    for (index = 0U; index < EVO_PROJECT_COMMAND_COUNT; index += 1U) {
        const evo_project_command_descriptor_t *descriptor =
            evo_project_command_registry_at(index);

        if (check(descriptor != NULL) != 0 ||
            check(descriptor->schema_version == EVO_PROJECT_COMMAND_SCHEMA_VERSION) != 0 ||
            check(descriptor->operation == (evo_project_command_operation_t)index) != 0 ||
            check(strcmp(descriptor->name, expected_names[index]) == 0) != 0 ||
            check(descriptor->request_schema != NULL && descriptor->request_schema[0] != '\0') != 0 ||
            check(descriptor->help != NULL && descriptor->help[0] != '\0') != 0 ||
            check(evo_project_command_find(expected_names[index]) == descriptor) != 0 ||
            check(strcmp(evo_project_command_operation_name(descriptor->operation), expected_names[index]) == 0) != 0) {
            return 1;
        }
    }
    if (check(evo_project_command_registry_at(EVO_PROJECT_COMMAND_COUNT) == NULL) != 0 ||
        check(evo_project_command_find("unknown") == NULL) != 0) {
        return 1;
    }
    return 0;
}

static int test_provider_contract(void)
{
    static const char *const expected_ids[] = {
        EVO_PROJECT_COMMAND_PROVIDER_CLANG_ANALYSIS_ID,
        EVO_PROJECT_COMMAND_PROVIDER_CLANG_AST_ID,
        EVO_PROJECT_COMMAND_PROVIDER_LINUX_BWRAP_ID,
        EVO_PROJECT_COMMAND_PROVIDER_LOCAL_EVALUATION_ID};
    size_t slot;

    for (slot = 0U; slot < EVO_PROJECT_COMMAND_PROVIDER_COUNT; slot += 1U) {
        const evo_project_command_provider_requirement_t *requirement =
            evo_project_command_provider_requirement(
                (evo_project_command_provider_slot_t)slot);

        if (check(requirement != NULL) != 0 ||
            check(strcmp(requirement->identity, expected_ids[slot]) == 0) != 0 ||
            check(requirement->implementation_version == 1U) != 0 ||
            check(requirement->required_capabilities != 0U) != 0) {
            return 1;
        }
    }
    return check(evo_project_command_provider_requirement(
                     EVO_PROJECT_COMMAND_PROVIDER_COUNT) == NULL);
}

static int test_operation_contracts(void)
{
    static const size_t expected_provider_count[] = {2U, 4U, 4U, 0U};
    evo_project_manifest_t manifest = valid_manifest();
    size_t operation_index;

    for (operation_index = 0U; operation_index < EVO_PROJECT_COMMAND_COUNT; operation_index += 1U) {
        const evo_project_command_operation_t operation =
            (evo_project_command_operation_t)operation_index;
        evo_project_command_request_t request = valid_request(operation, &manifest);
        evo_project_command_plan_t plan;

        if (evo_project_command_plan_build(&request, &plan) != EVO_PROJECT_COMMAND_SUCCESS) {
            return 1;
        }
        if (check(plan.schema_version == EVO_PROJECT_COMMAND_SCHEMA_VERSION) != 0 ||
            check(plan.interface_version == EVO_PROJECT_COMMAND_INTERFACE_VERSION) != 0 ||
            check(plan.operation == operation) != 0 ||
            check(plan.required_provider_count == expected_provider_count[operation_index]) != 0 ||
            check(plan.input_repository_read_only) != 0 ||
            check(plan.output_atomic) != 0 ||
            check(plan.existing_output_rejected) != 0 ||
            check(!plan.network_access_implicit) != 0 ||
            check(!plan.repository_mutation_permitted) != 0 ||
            check(plan.machine_evidence_authoritative) != 0 ||
            check(plan.human_summary_is_projection) != 0 ||
            check(plan.stdout_machine_output_only) != 0 ||
            check(plan.stderr_diagnostics_only) != 0 ||
            check(plan.execution_permitted == (operation != EVO_PROJECT_COMMAND_REPORT)) != 0) {
            return 1;
        }

        request.schema_version += 1U;
        if (check(evo_project_command_plan_build(&request, &plan) ==
                  EVO_PROJECT_COMMAND_ERROR_SCHEMA) != 0) {
            return 1;
        }
        request = valid_request(operation, &manifest);
        request.output_path = NULL;
        if (check(evo_project_command_plan_build(&request, &plan) ==
                  EVO_PROJECT_COMMAND_ERROR_PATH) != 0) {
            return 1;
        }
        request = valid_request(operation, &manifest);
        request.overwrite_output = true;
        if (check(evo_project_command_plan_build(&request, &plan) ==
                  EVO_PROJECT_COMMAND_ERROR_OUTPUT_POLICY) != 0) {
            return 1;
        }

        if (check(evo_project_command_exit_for_terminal(
                      operation,
                      EVO_PROJECT_COMMAND_TERMINAL_SUCCESS) ==
                  EVO_PROJECT_COMMAND_EXIT_SUCCESS) != 0 ||
            check(evo_project_command_exit_for_terminal(
                      operation,
                      EVO_PROJECT_COMMAND_TERMINAL_INVALID_INPUT) ==
                  EVO_PROJECT_COMMAND_EXIT_CONFIGURATION) != 0 ||
            check(evo_project_command_exit_for_terminal(
                      operation,
                      EVO_PROJECT_COMMAND_TERMINAL_RESOURCE_FAILURE) ==
                  EVO_PROJECT_COMMAND_EXIT_RESOURCE) != 0 ||
            check(evo_project_command_exit_for_terminal(
                      operation,
                      EVO_PROJECT_COMMAND_TERMINAL_INTERRUPTED) ==
                  EVO_PROJECT_COMMAND_EXIT_INTERRUPTED) != 0) {
            return 1;
        }
    }
    return 0;
}

static int test_fail_closed_provider_selection(void)
{
    evo_project_manifest_t manifest = valid_manifest();
    evo_project_command_request_t request =
        valid_request(EVO_PROJECT_COMMAND_EVOLVE, &manifest);
    evo_project_command_plan_t plan;

    request.providers[EVO_PROJECT_COMMAND_PROVIDER_EXECUTION].available = false;
    if (check(evo_project_command_plan_build(&request, &plan) ==
              EVO_PROJECT_COMMAND_ERROR_PROVIDER_UNAVAILABLE) != 0 ||
        check(!plan.execution_permitted) != 0) {
        return 1;
    }

    request = valid_request(EVO_PROJECT_COMMAND_EVOLVE, &manifest);
    request.providers[EVO_PROJECT_COMMAND_PROVIDER_ANALYSIS].implementation_version = 2U;
    if (check(evo_project_command_plan_build(&request, &plan) ==
              EVO_PROJECT_COMMAND_ERROR_PROVIDER_VERSION) != 0 ||
        check(!plan.execution_permitted) != 0) {
        return 1;
    }

    request = valid_request(EVO_PROJECT_COMMAND_EVOLVE, &manifest);
    request.providers[EVO_PROJECT_COMMAND_PROVIDER_TRANSFORMATION_AST].identity =
        "catalyst.evo.provider.fake.v1";
    if (check(evo_project_command_plan_build(&request, &plan) ==
              EVO_PROJECT_COMMAND_ERROR_PROVIDER_IDENTITY) != 0 ||
        check(!plan.execution_permitted) != 0) {
        return 1;
    }

    request = valid_request(EVO_PROJECT_COMMAND_EVOLVE, &manifest);
    request.providers[EVO_PROJECT_COMMAND_PROVIDER_EVALUATION].capabilities &=
        ~((uint64_t)EVO_PROJECT_COMMAND_CAPABILITY_DESCENDANT_CLEANUP);
    if (check(evo_project_command_plan_build(&request, &plan) ==
              EVO_PROJECT_COMMAND_ERROR_PROVIDER_CAPABILITY) != 0 ||
        check(!plan.execution_permitted) != 0) {
        return 1;
    }

    request = valid_request(EVO_PROJECT_COMMAND_EVOLVE, &manifest);
    request.provider_policy_identity = "catalyst.evo.provider-policy.v0";
    if (check(evo_project_command_plan_build(&request, &plan) ==
              EVO_PROJECT_COMMAND_ERROR_PROVIDER_POLICY) != 0 ||
        check(!plan.execution_permitted) != 0) {
        return 1;
    }
    return 0;
}

static int test_replay_and_checkpoint_contract(void)
{
    evo_project_manifest_t manifest = valid_manifest();
    evo_project_command_request_t request =
        valid_request(EVO_PROJECT_COMMAND_REPLAY, &manifest);
    evo_project_command_plan_t plan;

    request.replay_identity_complete = false;
    if (check(evo_project_command_plan_build(&request, &plan) ==
              EVO_PROJECT_COMMAND_ERROR_REPLAY_IDENTITY) != 0) {
        return 1;
    }
    request = valid_request(EVO_PROJECT_COMMAND_REPLAY, &manifest);
    request.external_inputs_declared = false;
    if (check(evo_project_command_plan_build(&request, &plan) ==
              EVO_PROJECT_COMMAND_ERROR_REPLAY_IDENTITY) != 0) {
        return 1;
    }

    request = valid_request(EVO_PROJECT_COMMAND_EVOLVE, &manifest);
    request.resume = true;
    request.checkpoint_path = "checkpoint.json";
    if (check(evo_project_command_plan_build(&request, &plan) ==
              EVO_PROJECT_COMMAND_SUCCESS) != 0 ||
        check(plan.resume) != 0) {
        return 1;
    }

    request = valid_request(EVO_PROJECT_COMMAND_EVOLVE, &manifest);
    request.resume = true;
    if (check(evo_project_command_plan_build(&request, &plan) ==
              EVO_PROJECT_COMMAND_ERROR_CHECKPOINT) != 0) {
        return 1;
    }

    request = valid_request(EVO_PROJECT_COMMAND_ANALYZE, &manifest);
    request.resume = true;
    request.checkpoint_path = "checkpoint.json";
    return check(evo_project_command_plan_build(&request, &plan) ==
                 EVO_PROJECT_COMMAND_ERROR_CHECKPOINT);
}

int main(void)
{
    if (test_registry() != 0 ||
        test_provider_contract() != 0 ||
        test_operation_contracts() != 0 ||
        test_fail_closed_provider_selection() != 0 ||
        test_replay_and_checkpoint_contract() != 0) {
        return 1;
    }
    return 0;
}
