#ifndef CATALYST_EVO_INTERNAL_PROJECT_MEASUREMENT_H
#define CATALYST_EVO_INTERNAL_PROJECT_MEASUREMENT_H

#include "internal/project_assurance.h"

#include "catalyst/evo/evo.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EVO_PROJECT_MEASUREMENT_SCHEMA_VERSION 1U
#define EVO_PROJECT_MEASUREMENT_PPM_SCALE UINT32_C(1000000)

typedef enum evo_project_measurement_status {
    EVO_PROJECT_MEASUREMENT_SUCCESS = 0,
    EVO_PROJECT_MEASUREMENT_ERROR_INVALID_ARGUMENT = 1,
    EVO_PROJECT_MEASUREMENT_ERROR_RESULT_ACTIVE = 2,
    EVO_PROJECT_MEASUREMENT_ERROR_ASSURANCE_INELIGIBLE = 3,
    EVO_PROJECT_MEASUREMENT_ERROR_POLICY_INVALID = 4,
    EVO_PROJECT_MEASUREMENT_ERROR_RESOURCE_LIMIT = 5,
    EVO_PROJECT_MEASUREMENT_ERROR_OUT_OF_MEMORY = 6,
    EVO_PROJECT_MEASUREMENT_ERROR_PROVIDER = 7,
    EVO_PROJECT_MEASUREMENT_ERROR_OUTPUT_EXISTS = 8,
    EVO_PROJECT_MEASUREMENT_ERROR_EVIDENCE = 9,
    EVO_PROJECT_MEASUREMENT_ERROR_STATE = 10
} evo_project_measurement_status_t;

typedef enum evo_project_measurement_subject {
    EVO_PROJECT_MEASUREMENT_BASELINE = 1,
    EVO_PROJECT_MEASUREMENT_CANDIDATE = 2
} evo_project_measurement_subject_t;

typedef enum evo_project_measurement_phase {
    EVO_PROJECT_MEASUREMENT_WARMUP = 1,
    EVO_PROJECT_MEASUREMENT_RECORDED = 2
} evo_project_measurement_phase_t;

typedef enum evo_project_measurement_order {
    EVO_PROJECT_MEASUREMENT_ALTERNATE_BASELINE_FIRST = 1,
    EVO_PROJECT_MEASUREMENT_ALTERNATE_CANDIDATE_FIRST = 2
} evo_project_measurement_order_t;

typedef enum evo_project_measurement_outlier_policy {
    EVO_PROJECT_MEASUREMENT_OUTLIER_NONE = 0,
    EVO_PROJECT_MEASUREMENT_OUTLIER_ABSOLUTE_MEDIAN = 1
} evo_project_measurement_outlier_policy_t;

typedef enum evo_project_measurement_comparison {
    EVO_PROJECT_MEASUREMENT_INCOMPLETE = 0,
    EVO_PROJECT_MEASUREMENT_UNSTABLE = 1,
    EVO_PROJECT_MEASUREMENT_SLOWER = 2,
    EVO_PROJECT_MEASUREMENT_EQUAL = 3,
    EVO_PROJECT_MEASUREMENT_FASTER = 4
} evo_project_measurement_comparison_t;

typedef struct evo_project_measurement_limits {
    size_t max_string_bytes;
    size_t max_workloads;
    size_t max_samples;
    size_t max_evidence_bytes;
    uint64_t max_timeout_ms;
} evo_project_measurement_limits_t;

typedef struct evo_project_measurement_condition {
    const char *hardware_identity;
    const char *operating_system_identity;
    const char *compiler_identity;
    const char *linker_identity;
    const char *environment_identity;
    const char *dataset_identity;
    const char *baseline_binary_identity;
    const char *candidate_binary_identity;
} evo_project_measurement_condition_t;

typedef struct evo_project_measurement_workload_policy {
    const char *workload_id;
    size_t warmup_count;
    size_t repetition_count;
    size_t minimum_included_repetitions;
    evo_project_measurement_order_t order;
    evo_project_measurement_outlier_policy_t outlier_policy;
    uint64_t outlier_deviation_ns;
    uint32_t max_runtime_range_ppm;
    uint32_t comparison_tolerance_ppm;
    uint32_t minimum_improvement_ppm;
    uint64_t timeout_ms;
    double workload_weight;
    double peak_memory_mix_weight;
    double binary_size_mix_weight;
} evo_project_measurement_workload_policy_t;

typedef struct evo_project_measurement_fitness_weights {
    double correctness;
    double performance;
    double memory_use;
    double reliability;
    double maintainability;
    double constraint_penalty;
} evo_project_measurement_fitness_weights_t;

typedef struct evo_project_measurement_request {
    uint32_t schema_version;
    const char *workload_id;
    evo_project_measurement_subject_t subject;
    evo_project_measurement_phase_t phase;
    size_t pair_index;
    size_t sequence_index;
    uint64_t timeout_ms;
    uint64_t expected_condition_fingerprint;
} evo_project_measurement_request_t;

typedef struct evo_project_measurement_outcome {
    uint32_t schema_version;
    bool completed;
    bool timed_out;
    bool failed;
    uint64_t condition_fingerprint;
    uint64_t runtime_ns;
    uint64_t peak_memory_bytes;
    uint64_t binary_size_bytes;
    uint32_t reliability_ppm;
    uint32_t maintainability_ppm;
} evo_project_measurement_outcome_t;

typedef evo_project_measurement_status_t (*evo_project_measurement_provider_fn)(
    const evo_project_measurement_request_t *request,
    void *context,
    evo_project_measurement_outcome_t *outcome);

typedef struct evo_project_measurement_sample {
    const char *workload_id;
    evo_project_measurement_subject_t subject;
    evo_project_measurement_phase_t phase;
    size_t pair_index;
    size_t sequence_index;
    bool completed;
    bool timed_out;
    bool failed;
    bool excluded;
    const char *exclusion_reason;
    uint64_t condition_fingerprint;
    uint64_t runtime_ns;
    uint64_t peak_memory_bytes;
    uint64_t binary_size_bytes;
    uint32_t reliability_ppm;
    uint32_t maintainability_ppm;
} evo_project_measurement_sample_t;

typedef struct evo_project_measurement_aggregate {
    uint64_t runtime_ns;
    uint64_t peak_memory_bytes;
    uint64_t binary_size_bytes;
    uint32_t reliability_ppm;
    uint32_t maintainability_ppm;
    uint64_t runtime_min_ns;
    uint64_t runtime_max_ns;
    uint32_t runtime_range_ppm;
    size_t included_count;
    size_t excluded_count;
} evo_project_measurement_aggregate_t;

typedef struct evo_project_measurement_workload_result {
    const char *workload_id;
    evo_project_measurement_comparison_t comparison;
    evo_project_measurement_aggregate_t baseline;
    evo_project_measurement_aggregate_t candidate;
    double runtime_improvement;
    double memory_improvement;
    double reliability_improvement;
    double maintainability_improvement;
    bool complete;
    bool stable;
} evo_project_measurement_workload_result_t;

typedef struct evo_project_measurement_config {
    const evo_project_assurance_t *assurance;
    const char *baseline_identity;
    const char *policy_id;
    const char *measurement_provider_identity;
    evo_project_measurement_condition_t condition;
    size_t workload_count;
    const evo_project_measurement_workload_policy_t *workloads;
    evo_project_measurement_fitness_weights_t fitness_weights;
    const char *output_path;
    evo_project_measurement_limits_t limits;
    evo_project_measurement_provider_fn provider;
    void *provider_context;
} evo_project_measurement_config_t;

typedef struct evo_project_measurement {
    uint32_t schema_version;
    const char *candidate_fingerprint;
    const char *assurance_fingerprint;
    const char *baseline_identity;
    const char *policy_id;
    char policy_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    const char *measurement_provider_identity;
    char condition_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    size_t workload_count;
    const evo_project_measurement_workload_result_t *workloads;
    size_t sample_count;
    const evo_project_measurement_sample_t *samples;
    evo_project_measurement_comparison_t overall_comparison;
    evo_fitness_t fitness;
    evo_project_measurement_fitness_weights_t fitness_weights;
    bool fitness_available;
    bool correctness_preserved;
    bool projection_complete;
    bool probabilistic_authority;
    char measurement_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    const char *output_path;
    size_t canonical_json_size;
    const char *canonical_json;
    size_t audit_markdown_size;
    const char *audit_markdown;
    void *private_owner;
} evo_project_measurement_t;

evo_project_measurement_status_t evo_project_candidate_measure(
    const evo_project_measurement_config_t *config,
    evo_project_measurement_t *measurement);

void evo_project_measurement_destroy(evo_project_measurement_t *measurement);

const char *evo_project_measurement_status_name(evo_project_measurement_status_t status);
const char *evo_project_measurement_subject_name(evo_project_measurement_subject_t subject);
const char *evo_project_measurement_phase_name(evo_project_measurement_phase_t phase);
const char *evo_project_measurement_comparison_name(evo_project_measurement_comparison_t comparison);

#endif
