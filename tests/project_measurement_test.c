#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L

#include "internal/project_candidate_internal.h"
#include "internal/project_measurement.h"
#include "internal/project_runtime.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(condition)                                           \
    do {                                                           \
        if (!(condition)) {                                        \
            (void)fprintf(                                         \
                stderr, "measurement test failure at %s:%d: %s\n", \
                __FILE__, __LINE__, #condition);                   \
            return 1;                                              \
        }                                                          \
    } while (0)

typedef enum fake_mode {
    FAKE_FASTER = 1,
    FAKE_EQUAL = 2,
    FAKE_SLOWER = 3,
    FAKE_UNSTABLE = 4,
    FAKE_INCOMPLETE = 5,
    FAKE_OUTLIER = 6,
    FAKE_CONDITION_MISMATCH = 7
} fake_mode_t;

typedef struct fake_context {
    fake_mode_t mode;
} fake_context_t;

static evo_project_measurement_status_t fake_provider(
    const evo_project_measurement_request_t *request,
    void *context,
    evo_project_measurement_outcome_t *outcome)
{
    const fake_context_t *fake = context;
    uint64_t runtime = UINT64_C(1000000);

    outcome->schema_version = EVO_PROJECT_MEASUREMENT_SCHEMA_VERSION;
    outcome->condition_fingerprint = request->expected_condition_fingerprint;
    outcome->completed = true;
    outcome->peak_memory_bytes = UINT64_C(1000);
    outcome->binary_size_bytes = UINT64_C(500);
    outcome->reliability_ppm = UINT32_C(990000);
    outcome->maintainability_ppm = UINT32_C(900000);

    if (request->subject == EVO_PROJECT_MEASUREMENT_CANDIDATE) {
        outcome->peak_memory_bytes = UINT64_C(900);
        outcome->binary_size_bytes = UINT64_C(450);
        outcome->reliability_ppm = UINT32_C(995000);
        outcome->maintainability_ppm = UINT32_C(920000);
        switch (fake->mode) {
        case FAKE_FASTER:
            runtime = UINT64_C(800000);
            break;
        case FAKE_EQUAL:
            runtime = UINT64_C(1005000);
            break;
        case FAKE_SLOWER:
            runtime = UINT64_C(1200000);
            break;
        case FAKE_UNSTABLE:
            runtime = (request->pair_index % 2U) == 0U ? UINT64_C(700000)
                                                       : UINT64_C(1300000);
            break;
        case FAKE_INCOMPLETE:
            if (request->phase == EVO_PROJECT_MEASUREMENT_RECORDED &&
                request->pair_index == 1U) {
                outcome->completed = false;
                outcome->timed_out = true;
                outcome->runtime_ns = 0U;
                outcome->peak_memory_bytes = 0U;
                outcome->binary_size_bytes = 0U;
                outcome->reliability_ppm = 0U;
                outcome->maintainability_ppm = 0U;
                return EVO_PROJECT_MEASUREMENT_SUCCESS;
            }
            runtime = UINT64_C(800000);
            break;
        case FAKE_OUTLIER:
            runtime = request->pair_index == 1U ? UINT64_C(5000000)
                                                : UINT64_C(800000);
            break;
        case FAKE_CONDITION_MISMATCH:
            runtime = UINT64_C(800000);
            if (request->phase == EVO_PROJECT_MEASUREMENT_RECORDED &&
                request->pair_index == 1U) {
                outcome->condition_fingerprint ^= UINT64_C(1);
            }
            break;
        default:
            return EVO_PROJECT_MEASUREMENT_ERROR_PROVIDER;
        }
    }
    outcome->runtime_ns = runtime;
    return EVO_PROJECT_MEASUREMENT_SUCCESS;
}

static bool make_assurance(evo_project_assurance_t *assurance)
{
    int written;

    *assurance = (evo_project_assurance_t){0};
    assurance->schema_version = EVO_PROJECT_ASSURANCE_SCHEMA_VERSION;
    assurance->candidate_fingerprint = "fnv1a64:1111111111111111";
    written = evo_project_format(
        assurance->assurance_fingerprint,
        sizeof(assurance->assurance_fingerprint),
        "%s",
        "fnv1a64:2222222222222222");
    if (written <= 0 ||
        (size_t)written >= sizeof(assurance->assurance_fingerprint)) {
        return false;
    }
    assurance->performance_eligible = true;
    assurance->projection_complete = true;
    assurance->probabilistic_authority = false;
    assurance->private_owner = assurance;
    return true;
}

static evo_project_measurement_workload_policy_t make_policy(fake_mode_t mode)
{
    evo_project_measurement_workload_policy_t policy = {0};

    policy.workload_id = "oracle-workload";
    policy.warmup_count = 1U;
    policy.repetition_count = mode == FAKE_OUTLIER ? 5U : 3U;
    policy.minimum_included_repetitions = 3U;
    policy.order = EVO_PROJECT_MEASUREMENT_ALTERNATE_BASELINE_FIRST;
    policy.outlier_policy = mode == FAKE_OUTLIER
                                ? EVO_PROJECT_MEASUREMENT_OUTLIER_ABSOLUTE_MEDIAN
                                : EVO_PROJECT_MEASUREMENT_OUTLIER_NONE;
    policy.outlier_deviation_ns = UINT64_C(1000000);
    policy.max_runtime_range_ppm = UINT32_C(100000);
    policy.comparison_tolerance_ppm = UINT32_C(20000);
    policy.minimum_improvement_ppm = UINT32_C(50000);
    policy.timeout_ms = UINT64_C(1000);
    policy.workload_weight = 1.0;
    policy.peak_memory_mix_weight = 1.0;
    policy.binary_size_mix_weight = 1.0;
    return policy;
}

static evo_project_measurement_config_t make_config(
    const evo_project_assurance_t *assurance,
    const evo_project_measurement_workload_policy_t *policy,
    fake_context_t *context,
    const char *output_path)
{
    evo_project_measurement_config_t config = {0};

    config.assurance = assurance;
    config.baseline_identity = "baseline:oracle-v1";
    config.policy_id = "measurement-policy:oracle-v1";
    config.measurement_provider_identity = "fake-provider:v1";
    config.condition.hardware_identity = "hardware:test-host";
    config.condition.operating_system_identity = "os:test";
    config.condition.compiler_identity = "compiler:test";
    config.condition.linker_identity = "linker:test";
    config.condition.environment_identity = "environment:clean";
    config.condition.dataset_identity = "dataset:fixed-v1";
    config.condition.baseline_binary_identity = "binary:baseline-v1";
    config.condition.candidate_binary_identity = "binary:candidate-v1";
    config.workload_count = 1U;
    config.workloads = policy;
    config.fitness_weights.correctness = 1.0;
    config.fitness_weights.performance = 1.0;
    config.fitness_weights.memory_use = 1.0;
    config.fitness_weights.reliability = 1.0;
    config.fitness_weights.maintainability = 1.0;
    config.fitness_weights.constraint_penalty = 1.0;
    config.output_path = output_path;
    config.limits.max_string_bytes = 512U;
    config.limits.max_workloads = 4U;
    config.limits.max_samples = 64U;
    config.limits.max_evidence_bytes = 1024U * 1024U;
    config.limits.max_timeout_ms = UINT64_C(5000);
    config.provider = fake_provider;
    config.provider_context = context;
    return config;
}

static int run_case(
    const char *root,
    const char *name,
    fake_mode_t mode,
    evo_project_measurement_comparison_t expected,
    bool expect_fitness,
    char fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE])
{
    evo_project_assurance_t assurance;
    evo_project_measurement_workload_policy_t policy = make_policy(mode);
    fake_context_t context = {mode};
    evo_project_measurement_t measurement = {0};
    evo_project_measurement_config_t config;
    char output[512];
    const int written =
        evo_project_format(output, sizeof(output), "%s/%s", root, name);
    size_t index;

    CHECK(written > 0 && (size_t)written < sizeof(output));
    CHECK(make_assurance(&assurance));
    config = make_config(&assurance, &policy, &context, output);
    CHECK(evo_project_candidate_measure(&config, &measurement) ==
          EVO_PROJECT_MEASUREMENT_SUCCESS);
    CHECK(measurement.overall_comparison == expected);
    CHECK(measurement.fitness_available == expect_fitness);
    CHECK(measurement.correctness_preserved);
    CHECK(measurement.projection_complete);
    CHECK(!measurement.probabilistic_authority);
    CHECK(measurement.sample_count ==
          2U * (policy.warmup_count + policy.repetition_count));
    CHECK(measurement.canonical_json != NULL);
    CHECK(strstr(
              measurement.canonical_json,
              "\"correctness_preserved\":true") != NULL);
    CHECK(strstr(
              measurement.canonical_json,
              "\"order\":\"alternate-baseline-first\"") != NULL);
    CHECK(strstr(
              measurement.canonical_json,
              "\"outlier_policy\":\"none\"") != NULL ||
          mode == FAKE_OUTLIER);
    CHECK(strstr(measurement.canonical_json, "\"workload_weight\":1") != NULL);
    CHECK(strstr(
              measurement.canonical_json,
              "\"peak_memory_mix_weight\":1") != NULL);
    CHECK(strstr(
              measurement.canonical_json,
              "\"binary_size_mix_weight\":1") != NULL);
    CHECK(strstr(measurement.audit_markdown, "Measurement provider") != NULL);
    CHECK(strstr(measurement.audit_markdown, "hardware:test-host") != NULL);
    CHECK(strstr(measurement.audit_markdown, "| performance |") != NULL);
    CHECK(strstr(
              measurement.audit_markdown,
              "best verified candidate found within the recorded bounded search contract") !=
          NULL);

    if (expect_fitness) {
        CHECK(isfinite(measurement.fitness.total));
        CHECK(measurement.fitness.correctness == 1.0);
    } else {
        CHECK(measurement.fitness.total == 0.0);
    }
    if (mode == FAKE_OUTLIER) {
        bool saw_outlier = false;
        for (index = 0U; index < measurement.sample_count; index += 1U) {
            if (measurement.samples[index].excluded &&
                measurement.samples[index].exclusion_reason != NULL &&
                strcmp(
                    measurement.samples[index].exclusion_reason,
                    "runtime-median-deviation") == 0) {
                saw_outlier = true;
            }
        }
        CHECK(saw_outlier);
        CHECK(measurement.workloads[0].candidate.included_count == 4U);
    }
    if (mode == FAKE_CONDITION_MISMATCH) {
        bool saw_condition_mismatch = false;
        for (index = 0U; index < measurement.sample_count; index += 1U) {
            if (measurement.samples[index].excluded &&
                measurement.samples[index].exclusion_reason != NULL &&
                strcmp(
                    measurement.samples[index].exclusion_reason,
                    "condition-mismatch") == 0) {
                saw_condition_mismatch = true;
            }
        }
        CHECK(saw_condition_mismatch);
        CHECK(!measurement.fitness_available);
    }
    if (fingerprint != NULL) {
        const int fingerprint_written = evo_project_format(
            fingerprint,
            EVO_PROJECT_FINGERPRINT_TEXT_SIZE,
            "%s",
            measurement.measurement_fingerprint);
        CHECK(fingerprint_written > 0 &&
              (size_t)fingerprint_written < EVO_PROJECT_FINGERPRINT_TEXT_SIZE);
    }
    evo_project_measurement_destroy(&measurement);
    return 0;
}

int main(void)
{
    char root_template[] = "/tmp/evo-project-measurement-XXXXXX";
    int temp_fd = mkstemp(root_template);
    const char *root = root_template;
    char replay_a[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char replay_b[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    evo_project_assurance_t assurance;
    evo_project_measurement_workload_policy_t policy = make_policy(FAKE_FASTER);
    fake_context_t context = {FAKE_FASTER};
    evo_project_measurement_config_t config;
    evo_project_measurement_t measurement = {0};
    char invalid_output[512];
    int written;

    CHECK(temp_fd >= 0);
    CHECK(close(temp_fd) == 0);
    CHECK(unlink(root_template) == 0);
    CHECK(mkdir(root_template, 0700) == 0);
    CHECK(run_case(
              root,
              "faster",
              FAKE_FASTER,
              EVO_PROJECT_MEASUREMENT_FASTER,
              true,
              NULL) == 0);
    CHECK(run_case(
              root,
              "equal",
              FAKE_EQUAL,
              EVO_PROJECT_MEASUREMENT_EQUAL,
              true,
              NULL) == 0);
    CHECK(run_case(
              root,
              "slower",
              FAKE_SLOWER,
              EVO_PROJECT_MEASUREMENT_SLOWER,
              true,
              NULL) == 0);
    CHECK(run_case(
              root,
              "unstable",
              FAKE_UNSTABLE,
              EVO_PROJECT_MEASUREMENT_UNSTABLE,
              false,
              NULL) == 0);
    CHECK(run_case(
              root,
              "incomplete",
              FAKE_INCOMPLETE,
              EVO_PROJECT_MEASUREMENT_INCOMPLETE,
              false,
              NULL) == 0);
    CHECK(run_case(
              root,
              "outlier",
              FAKE_OUTLIER,
              EVO_PROJECT_MEASUREMENT_FASTER,
              true,
              NULL) == 0);
    CHECK(run_case(
              root,
              "condition-mismatch",
              FAKE_CONDITION_MISMATCH,
              EVO_PROJECT_MEASUREMENT_INCOMPLETE,
              false,
              NULL) == 0);
    CHECK(run_case(
              root,
              "replay-a",
              FAKE_FASTER,
              EVO_PROJECT_MEASUREMENT_FASTER,
              true,
              replay_a) == 0);
    CHECK(run_case(
              root,
              "replay-b",
              FAKE_FASTER,
              EVO_PROJECT_MEASUREMENT_FASTER,
              true,
              replay_b) == 0);
    CHECK(strcmp(replay_a, replay_b) == 0);

    CHECK(make_assurance(&assurance));
    assurance.performance_eligible = false;
    written = evo_project_format(
        invalid_output, sizeof(invalid_output), "%s/ineligible", root);
    CHECK(written > 0 && (size_t)written < sizeof(invalid_output));
    config = make_config(&assurance, &policy, &context, invalid_output);
    CHECK(evo_project_candidate_measure(&config, &measurement) ==
          EVO_PROJECT_MEASUREMENT_ERROR_ASSURANCE_INELIGIBLE);
    CHECK(measurement.private_owner == NULL);

    CHECK(evo_candidate_remove_tree(root) == EVO_PROJECT_CANDIDATE_SUCCESS);
    (void)printf("candidate measurement oracle fixtures: PASS\n");
    return 0;
}
