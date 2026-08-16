#ifndef CATALYST_EVO_INTERNAL_PROJECT_MEASUREMENT_OWNER_H
#define CATALYST_EVO_INTERNAL_PROJECT_MEASUREMENT_OWNER_H

#include "internal/project_measurement.h"

#include <stddef.h>
#include <stdint.h>

typedef struct evo_project_measurement_owner {
    evo_project_measurement_t view;
    char *candidate_fingerprint;
    char *assurance_fingerprint;
    char *baseline_identity;
    char *policy_id;
    char *measurement_provider_identity;
    char *output_path;
    uint64_t policy_fingerprint_value;
    uint64_t condition_fingerprint_value;
    uint64_t measurement_fingerprint_value;
    evo_project_measurement_workload_result_t *workloads;
    char **workload_ids;
    evo_project_measurement_sample_t *samples;
    char **sample_workload_ids;
    char **sample_exclusion_reasons;
    size_t sample_capacity;
    char *canonical_json;
    char *audit_markdown;
} evo_project_measurement_owner_t;

#endif
