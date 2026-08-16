#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include "internal/project_assurance.h"
#include "internal/project_candidate_internal.h"
#include "internal/project_runtime.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define TEST_ASSERT(condition)                                                 \
    do {                                                                       \
        if (!(condition)) {                                                    \
            (void)fprintf(                                                     \
                stderr, "assertion failed at %s:%d: %s\n", __FILE__, __LINE__, \
                #condition);                                                   \
            return false;                                                      \
        }                                                                      \
    } while (0)

typedef enum fake_mode {
    FAKE_PASS = 0,
    FAKE_FAIL = 1,
    FAKE_TIMEOUT = 2,
    FAKE_SIGNAL = 3,
    FAKE_RESOURCE = 4,
    FAKE_UNAVAILABLE = 5,
    FAKE_POLICY = 6,
    FAKE_CLEANUP = 7,
    FAKE_SOURCE_MUTATION = 8,
    FAKE_SNAPSHOT_MUTATION = 9
} fake_mode_t;

typedef struct fake_runner_context {
    const char *target_gate;
    fake_mode_t mode;
    size_t calls;
} fake_runner_context_t;

typedef struct assurance_fixture {
    char root[512];
    char workspace[512];
    evo_project_candidate_t candidate;
} assurance_fixture_t;

static const char *const cmake_configure_args[] = {
    "/usr/bin/cmake", "-S", ".", "-B", "build-cmake", "-DCMAKE_C_COMPILER=clang"};
static const char *const cmake_build_args[] = {
    "/usr/bin/cmake", "--build", "build-cmake"};
static const char *const focused_test_args[] = {
    "/usr/bin/ctest", "--test-dir", "build-cmake", "--output-on-failure"};
static const char *const autotools_args[] = {
    "/usr/bin/make", "check"};
static const char *const sanitizer_args[] = {
    "/usr/bin/cmake", "--build", "build-sanitize"};
static const char *const governance_args[] = {
    "/usr/bin/python3", "tests/validate_project_assurance.py"};
static const char *const safe_environment[] = {
    "LC_ALL=C", "PATH=/usr/bin:/bin"};
static const char *const fast_profiles[] = {"cmake-clang"};
static const char *const finalist_profiles[] = {"cmake-clang", "autotools-gcc"};

static const evo_project_assurance_gate_t policy_gates[] = {
    {"cmake-configure",
     "cmake-clang",
     EVO_PROJECT_ASSURANCE_STAGE_FAST,
     true,
     sizeof(cmake_configure_args) / sizeof(cmake_configure_args[0]),
     cmake_configure_args,
     sizeof(safe_environment) / sizeof(safe_environment[0]),
     safe_environment,
     ".",
     30000U,
     512U * 1024U * 1024U,
     32U,
     512U * 1024U * 1024U,
     1024U * 1024U,
     false},
    {"cmake-build",
     "cmake-clang",
     EVO_PROJECT_ASSURANCE_STAGE_FAST,
     true,
     sizeof(cmake_build_args) / sizeof(cmake_build_args[0]),
     cmake_build_args,
     sizeof(safe_environment) / sizeof(safe_environment[0]),
     safe_environment,
     ".",
     30000U,
     512U * 1024U * 1024U,
     32U,
     512U * 1024U * 1024U,
     1024U * 1024U,
     false},
    {"focused-tests",
     "cmake-clang",
     EVO_PROJECT_ASSURANCE_STAGE_FAST,
     true,
     sizeof(focused_test_args) / sizeof(focused_test_args[0]),
     focused_test_args,
     sizeof(safe_environment) / sizeof(safe_environment[0]),
     safe_environment,
     ".",
     30000U,
     512U * 1024U * 1024U,
     32U,
     512U * 1024U * 1024U,
     1024U * 1024U,
     false},
    {"autotools-release",
     "autotools-gcc",
     EVO_PROJECT_ASSURANCE_STAGE_FINALIST,
     true,
     sizeof(autotools_args) / sizeof(autotools_args[0]),
     autotools_args,
     sizeof(safe_environment) / sizeof(safe_environment[0]),
     safe_environment,
     ".",
     30000U,
     512U * 1024U * 1024U,
     32U,
     512U * 1024U * 1024U,
     1024U * 1024U,
     false},
    {"sanitizers",
     "cmake-clang",
     EVO_PROJECT_ASSURANCE_STAGE_FINALIST,
     true,
     sizeof(sanitizer_args) / sizeof(sanitizer_args[0]),
     sanitizer_args,
     sizeof(safe_environment) / sizeof(safe_environment[0]),
     safe_environment,
     ".",
     30000U,
     512U * 1024U * 1024U,
     32U,
     512U * 1024U * 1024U,
     1024U * 1024U,
     false},
    {"aes-governance",
     "autotools-gcc",
     EVO_PROJECT_ASSURANCE_STAGE_FINALIST,
     true,
     sizeof(governance_args) / sizeof(governance_args[0]),
     governance_args,
     sizeof(safe_environment) / sizeof(safe_environment[0]),
     safe_environment,
     ".",
     30000U,
     512U * 1024U * 1024U,
     32U,
     512U * 1024U * 1024U,
     1024U * 1024U,
     false}};

static evo_project_assurance_status_t fake_runner(
    const evo_project_assurance_gate_view_t *gate,
    const char *candidate_workspace_path,
    void *context,
    evo_project_assurance_gate_outcome_t *outcome)
{
    fake_runner_context_t *runner = (fake_runner_context_t *)context;
    fake_mode_t mode = FAKE_PASS;
    static const char diagnostic[] = "deterministic diagnostic";
    struct stat workspace_stat;

    if (gate == NULL || candidate_workspace_path == NULL || runner == NULL ||
        outcome == NULL || gate->shell_interpretation ||
        !gate->workspace_only_filesystem || stat(candidate_workspace_path, &workspace_stat) != 0 ||
        !S_ISDIR(workspace_stat.st_mode)) {
        return EVO_PROJECT_ASSURANCE_ERROR_EXECUTION_PROVIDER;
    }
    runner->calls += 1U;
    if (runner->target_gate != NULL && strcmp(runner->target_gate, gate->gate_id) == 0) {
        mode = runner->mode;
    }
    outcome->schema_version = EVO_PROJECT_ASSURANCE_SCHEMA_VERSION;
    outcome->completed = true;
    outcome->available = true;
    outcome->filesystem_policy_enforced = true;
    outcome->network_policy_enforced = true;
    outcome->process_group_clean = true;
    outcome->exit_code = 0;
    outcome->signal_number = 0;
    outcome->stdout_bytes = 4U;
    outcome->stdout_fingerprint = 0x1111U;
    outcome->stderr_bytes = 0U;
    outcome->stderr_fingerprint = 0U;
    outcome->toolchain_fingerprint = 0x2222U;
    outcome->diagnostic_excerpt = diagnostic;
    outcome->diagnostic_excerpt_bytes = sizeof(diagnostic) - 1U;

    switch (mode) {
    case FAKE_PASS:
        break;
    case FAKE_FAIL:
        outcome->exit_code = 2;
        break;
    case FAKE_TIMEOUT:
        outcome->timed_out = true;
        outcome->exit_code = -1;
        break;
    case FAKE_SIGNAL:
        outcome->signaled = true;
        outcome->signal_number = 9;
        outcome->exit_code = -1;
        break;
    case FAKE_RESOURCE:
        outcome->resource_exhausted = true;
        outcome->exit_code = -1;
        break;
    case FAKE_UNAVAILABLE:
        outcome->available = false;
        outcome->exit_code = -1;
        outcome->stdout_bytes = 0U;
        outcome->stderr_bytes = 0U;
        outcome->diagnostic_excerpt = NULL;
        outcome->diagnostic_excerpt_bytes = 0U;
        break;
    case FAKE_POLICY:
        outcome->filesystem_policy_enforced = false;
        break;
    case FAKE_CLEANUP:
        outcome->process_group_clean = false;
        break;
    case FAKE_SOURCE_MUTATION:
        outcome->source_modified = true;
        break;
    case FAKE_SNAPSHOT_MUTATION:
        outcome->snapshot_modified = true;
        break;
    default:
        return EVO_PROJECT_ASSURANCE_ERROR_EXECUTION_PROVIDER;
    }
    return EVO_PROJECT_ASSURANCE_SUCCESS;
}

static bool fixture_open(assurance_fixture_t *fixture)
{
    char template_path[] = "/tmp/evo-assurance-test-XXXXXX";
    char *root = mkdtemp(template_path);
    int written;

    TEST_ASSERT(root != NULL);
    written = evo_project_format(fixture->root, sizeof(fixture->root), "%s", root);
    TEST_ASSERT(written > 0 && (size_t)written < sizeof(fixture->root));
    written = evo_project_format(
        fixture->workspace, sizeof(fixture->workspace), "%s/candidate", root);
    TEST_ASSERT(written > 0 && (size_t)written < sizeof(fixture->workspace));
    TEST_ASSERT(mkdir(fixture->workspace, 0700) == 0);
    fixture->candidate.schema_version = EVO_PROJECT_CANDIDATE_SCHEMA_VERSION;
    fixture->candidate.baseline_fingerprint = "fnv1a64:0000000000000001";
    fixture->candidate.recipe_fingerprint = "fnv1a64:0000000000000002";
    written = evo_project_format(
        fixture->candidate.candidate_fingerprint,
        sizeof(fixture->candidate.candidate_fingerprint),
        "%s",
        "fnv1a64:0000000000000003");
    TEST_ASSERT(
        written > 0 &&
        (size_t)written < sizeof(fixture->candidate.candidate_fingerprint));
    fixture->candidate.output_path = fixture->root;
    fixture->candidate.workspace_path = fixture->workspace;
    fixture->candidate.workspace_policy = EVO_PROJECT_CANDIDATE_WORKSPACE_RETAIN;
    fixture->candidate.projection_complete = true;
    fixture->candidate.probabilistic_authority = false;
    fixture->candidate.source_modified = false;
    fixture->candidate.snapshot_modified = false;
    fixture->candidate.private_owner = fixture;
    return true;
}

static void fixture_close(assurance_fixture_t *fixture)
{
    if (fixture->root[0] != '\0') {
        (void)evo_candidate_remove_tree(fixture->root);
    }
}

static evo_project_assurance_limits_t test_limits(void)
{
    evo_project_assurance_limits_t limits = {0};

    limits.max_string_bytes = 512U;
    limits.max_path_bytes = 1024U;
    limits.max_gates = 32U;
    limits.max_arguments = 32U;
    limits.max_environment_entries = 16U;
    limits.max_command_bytes = 4096U;
    limits.max_output_bytes = 1024U * 1024U;
    limits.max_diagnostic_bytes = 4096U;
    limits.max_evidence_bytes = 512U * 1024U;
    limits.max_timeout_ms = 60000U;
    limits.max_memory_bytes = 1024U * 1024U * 1024U;
    limits.max_processes = 64U;
    limits.max_storage_bytes = 1024U * 1024U * 1024U;
    return limits;
}

static evo_project_assurance_config_t make_config(
    assurance_fixture_t *fixture,
    fake_runner_context_t *runner,
    evo_project_assurance_stage_t stage,
    const char *output_path)
{
    evo_project_assurance_config_t config = {0};

    config.candidate = &fixture->candidate;
    config.policy_id = "candidate-release-v1";
    config.execution_provider_identity = "test-isolated-provider-v1";
    config.stage = stage;
    config.required_profile_count =
        stage == EVO_PROJECT_ASSURANCE_STAGE_FAST
            ? sizeof(fast_profiles) / sizeof(fast_profiles[0])
            : sizeof(finalist_profiles) / sizeof(finalist_profiles[0]);
    config.required_profiles =
        stage == EVO_PROJECT_ASSURANCE_STAGE_FAST ? fast_profiles : finalist_profiles;
    config.gate_count = sizeof(policy_gates) / sizeof(policy_gates[0]);
    config.gates = policy_gates;
    config.output_path = output_path;
    config.allow_network_gates = false;
    config.limits = test_limits();
    config.runner = fake_runner;
    config.runner_context = runner;
    return config;
}

static bool run_success_cases(assurance_fixture_t *fixture)
{
    fake_runner_context_t runner = {0};
    evo_project_assurance_t fast = {0};
    evo_project_assurance_t finalist = {0};
    evo_project_assurance_config_t config;
    char output[512];
    int written;

    written = evo_project_format(output, sizeof(output), "%s/fast-success", fixture->root);
    TEST_ASSERT(written > 0 && (size_t)written < sizeof(output));
    config = make_config(fixture, &runner, EVO_PROJECT_ASSURANCE_STAGE_FAST, output);
    TEST_ASSERT(
        evo_project_candidate_assure(&config, &fast) == EVO_PROJECT_ASSURANCE_SUCCESS);
    TEST_ASSERT(fast.performance_eligible);
    TEST_ASSERT(!fast.champion_eligible);
    TEST_ASSERT(fast.projection_complete);
    TEST_ASSERT(!fast.probabilistic_authority);
    TEST_ASSERT(runner.calls == 3U);
    TEST_ASSERT(strstr(fast.canonical_json, "\"performance_eligible\":true") != NULL);
    TEST_ASSERT(strstr(fast.canonical_json, "\"shell_interpretation\":false") != NULL);
    evo_project_assurance_destroy(&fast);

    runner.calls = 0U;
    written = evo_project_format(
        output, sizeof(output), "%s/finalist-success", fixture->root);
    TEST_ASSERT(written > 0 && (size_t)written < sizeof(output));
    config = make_config(fixture, &runner, EVO_PROJECT_ASSURANCE_STAGE_FINALIST, output);
    TEST_ASSERT(
        evo_project_candidate_assure(&config, &finalist) ==
        EVO_PROJECT_ASSURANCE_SUCCESS);
    TEST_ASSERT(finalist.performance_eligible);
    TEST_ASSERT(finalist.champion_eligible);
    TEST_ASSERT(runner.calls == sizeof(policy_gates) / sizeof(policy_gates[0]));
    TEST_ASSERT(strstr(finalist.canonical_json, "\"champion_eligible\":true") != NULL);
    evo_project_assurance_destroy(&finalist);
    return true;
}

static bool run_rejection_case(
    assurance_fixture_t *fixture,
    const char *name,
    const char *target_gate,
    fake_mode_t mode,
    evo_project_assurance_stage_t stage,
    evo_project_assurance_disposition_t expected)
{
    fake_runner_context_t runner = {target_gate, mode, 0U};
    evo_project_assurance_t result = {0};
    evo_project_assurance_config_t config;
    char output[512];
    int written;
    size_t index;
    bool found = false;

    written = evo_project_format(output, sizeof(output), "%s/%s", fixture->root, name);
    TEST_ASSERT(written > 0 && (size_t)written < sizeof(output));
    config = make_config(fixture, &runner, stage, output);
    TEST_ASSERT(
        evo_project_candidate_assure(&config, &result) == EVO_PROJECT_ASSURANCE_SUCCESS);
    TEST_ASSERT(!result.performance_eligible);
    TEST_ASSERT(!result.champion_eligible);
    for (index = 0U; index < result.gate_count; index += 1U) {
        if (strcmp(result.gates[index].gate_id, target_gate) == 0) {
            TEST_ASSERT(result.gates[index].disposition == expected);
            found = true;
        }
    }
    TEST_ASSERT(found);
    evo_project_assurance_destroy(&result);
    return true;
}

static bool run_rejection_cases(assurance_fixture_t *fixture)
{
    TEST_ASSERT(run_rejection_case(
        fixture,
        "build-failure",
        "cmake-build",
        FAKE_FAIL,
        EVO_PROJECT_ASSURANCE_STAGE_FAST,
        EVO_PROJECT_ASSURANCE_GATE_FAILED));
    TEST_ASSERT(run_rejection_case(
        fixture,
        "timeout",
        "focused-tests",
        FAKE_TIMEOUT,
        EVO_PROJECT_ASSURANCE_STAGE_FAST,
        EVO_PROJECT_ASSURANCE_GATE_TIMED_OUT));
    TEST_ASSERT(run_rejection_case(
        fixture,
        "signal",
        "focused-tests",
        FAKE_SIGNAL,
        EVO_PROJECT_ASSURANCE_STAGE_FAST,
        EVO_PROJECT_ASSURANCE_GATE_SIGNALED));
    TEST_ASSERT(run_rejection_case(
        fixture,
        "resource",
        "focused-tests",
        FAKE_RESOURCE,
        EVO_PROJECT_ASSURANCE_STAGE_FAST,
        EVO_PROJECT_ASSURANCE_GATE_RESOURCE_EXHAUSTED));
    TEST_ASSERT(run_rejection_case(
        fixture,
        "unavailable",
        "focused-tests",
        FAKE_UNAVAILABLE,
        EVO_PROJECT_ASSURANCE_STAGE_FAST,
        EVO_PROJECT_ASSURANCE_GATE_UNAVAILABLE));
    TEST_ASSERT(run_rejection_case(
        fixture,
        "policy",
        "focused-tests",
        FAKE_POLICY,
        EVO_PROJECT_ASSURANCE_STAGE_FAST,
        EVO_PROJECT_ASSURANCE_GATE_POLICY_FAILED));
    TEST_ASSERT(run_rejection_case(
        fixture,
        "cleanup",
        "focused-tests",
        FAKE_CLEANUP,
        EVO_PROJECT_ASSURANCE_STAGE_FAST,
        EVO_PROJECT_ASSURANCE_GATE_CLEANUP_FAILED));
    TEST_ASSERT(run_rejection_case(
        fixture,
        "source-mutation",
        "focused-tests",
        FAKE_SOURCE_MUTATION,
        EVO_PROJECT_ASSURANCE_STAGE_FAST,
        EVO_PROJECT_ASSURANCE_GATE_POLICY_FAILED));
    TEST_ASSERT(run_rejection_case(
        fixture,
        "snapshot-mutation",
        "focused-tests",
        FAKE_SNAPSHOT_MUTATION,
        EVO_PROJECT_ASSURANCE_STAGE_FAST,
        EVO_PROJECT_ASSURANCE_GATE_POLICY_FAILED));
    TEST_ASSERT(run_rejection_case(
        fixture,
        "finalist-failure",
        "autotools-release",
        FAKE_FAIL,
        EVO_PROJECT_ASSURANCE_STAGE_FINALIST,
        EVO_PROJECT_ASSURANCE_GATE_FAILED));
    return true;
}

static bool run_policy_rejection_cases(assurance_fixture_t *fixture)
{
    static const char *const shell_args[] = {"/bin/sh", "-c", "echo injected"};
    evo_project_assurance_gate_t gates[sizeof(policy_gates) / sizeof(policy_gates[0])];
    fake_runner_context_t runner = {0};
    evo_project_assurance_t result = {0};
    evo_project_assurance_config_t config;
    char output[512];
    int written;
    size_t index;

    for (index = 0U; index < sizeof(gates) / sizeof(gates[0]); index += 1U) {
        gates[index] = policy_gates[index];
    }
    gates[0].arguments = shell_args;
    gates[0].argument_count = sizeof(shell_args) / sizeof(shell_args[0]);
    written = evo_project_format(output, sizeof(output), "%s/shell-rejected", fixture->root);
    TEST_ASSERT(written > 0 && (size_t)written < sizeof(output));
    config = make_config(fixture, &runner, EVO_PROJECT_ASSURANCE_STAGE_FAST, output);
    config.gates = gates;
    TEST_ASSERT(
        evo_project_candidate_assure(&config, &result) ==
        EVO_PROJECT_ASSURANCE_ERROR_POLICY_INVALID);
    TEST_ASSERT(runner.calls == 0U);

    gates[0] = policy_gates[0];
    gates[0].working_directory = "../escape";
    written = evo_project_format(output, sizeof(output), "%s/path-rejected", fixture->root);
    TEST_ASSERT(written > 0 && (size_t)written < sizeof(output));
    config.output_path = output;
    TEST_ASSERT(
        evo_project_candidate_assure(&config, &result) ==
        EVO_PROJECT_ASSURANCE_ERROR_POLICY_INVALID);

    gates[0] = policy_gates[0];
    gates[0].network_access = true;
    written = evo_project_format(output, sizeof(output), "%s/network-rejected", fixture->root);
    TEST_ASSERT(written > 0 && (size_t)written < sizeof(output));
    config.output_path = output;
    TEST_ASSERT(
        evo_project_candidate_assure(&config, &result) ==
        EVO_PROJECT_ASSURANCE_ERROR_POLICY_INVALID);
    return true;
}

static bool run_deterministic_replay(assurance_fixture_t *fixture)
{
    fake_runner_context_t first_runner = {"cmake-build", FAKE_FAIL, 0U};
    fake_runner_context_t second_runner = {"cmake-build", FAKE_FAIL, 0U};
    evo_project_assurance_t first = {0};
    evo_project_assurance_t second = {0};
    evo_project_assurance_config_t first_config;
    evo_project_assurance_config_t second_config;
    char first_output[512];
    char second_output[512];
    int written;

    written = evo_project_format(
        first_output, sizeof(first_output), "%s/replay-a", fixture->root);
    TEST_ASSERT(written > 0 && (size_t)written < sizeof(first_output));
    written = evo_project_format(
        second_output, sizeof(second_output), "%s/replay-b", fixture->root);
    TEST_ASSERT(written > 0 && (size_t)written < sizeof(second_output));
    first_config = make_config(
        fixture, &first_runner, EVO_PROJECT_ASSURANCE_STAGE_FAST, first_output);
    second_config = make_config(
        fixture, &second_runner, EVO_PROJECT_ASSURANCE_STAGE_FAST, second_output);
    TEST_ASSERT(
        evo_project_candidate_assure(&first_config, &first) ==
        EVO_PROJECT_ASSURANCE_SUCCESS);
    TEST_ASSERT(
        evo_project_candidate_assure(&second_config, &second) ==
        EVO_PROJECT_ASSURANCE_SUCCESS);
    TEST_ASSERT(strcmp(first.assurance_fingerprint, second.assurance_fingerprint) == 0);
    TEST_ASSERT(first.canonical_json_size == second.canonical_json_size);
    TEST_ASSERT(strcmp(first.canonical_json, second.canonical_json) == 0);
    evo_project_assurance_destroy(&second);
    evo_project_assurance_destroy(&first);
    return true;
}

int main(void)
{
    assurance_fixture_t fixture = {0};
    bool passed;

    if (!fixture_open(&fixture)) {
        fixture_close(&fixture);
        return 1;
    }
    passed = run_success_cases(&fixture) && run_rejection_cases(&fixture) &&
             run_policy_rejection_cases(&fixture) &&
             run_deterministic_replay(&fixture);
    fixture_close(&fixture);
    return passed ? 0 : 1;
}
