#ifndef CATALYST_EVO_INTERNAL_PROJECT_ORCHESTRATION_CHECKPOINT_H
#define CATALYST_EVO_INTERNAL_PROJECT_ORCHESTRATION_CHECKPOINT_H

#include "internal/project_fingerprint.h"

#include <stddef.h>
#include <stdint.h>

#define EVO_PROJECT_ORCHESTRATION_CHECKPOINT_FORMAT_VERSION 1U
#define EVO_PROJECT_ORCHESTRATION_CHECKPOINT_INTEGRITY_FNV1A64 1U

typedef enum evo_project_orchestration_checkpoint_status {
    EVO_PROJECT_ORCHESTRATION_CHECKPOINT_SUCCESS = 0,
    EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_INVALID_ARGUMENT = 1,
    EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_RESULT_ACTIVE = 2,
    EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_RESOURCE_LIMIT = 3,
    EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_OUT_OF_MEMORY = 4,
    EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_FORMAT = 5,
    EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_INTEGRITY = 6,
    EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_IDENTITY_MISMATCH = 7,
    EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_STATE = 8
} evo_project_orchestration_checkpoint_status_t;

typedef struct evo_project_orchestration_checkpoint_identity {
    const char *baseline_fingerprint;
    const char *analysis_fingerprint;
    const char *catalogue_identity;
    uint32_t catalogue_version;
    uint32_t recipe_schema_version;
    uint32_t search_schema_version;
    uint32_t mutation_policy_version;
    uint32_t crossover_policy_version;
    uint32_t repair_policy_version;
    const char *search_policy_identity;
    const char *evaluation_provider_identity;
    const char *orchestration_policy_identity;
    const char *toolchain_identity;
    const char *workload_identity;
    const char *artifact_schema_identity;
    uint64_t random_seed;
    uint64_t committed_generation;
    const char *committed_lineage_fingerprint;
} evo_project_orchestration_checkpoint_identity_t;

typedef struct evo_project_orchestration_checkpoint_limits {
    size_t max_string_bytes;
    size_t max_core_checkpoint_bytes;
    size_t max_checkpoint_bytes;
} evo_project_orchestration_checkpoint_limits_t;

typedef struct evo_project_orchestration_checkpoint {
    uint32_t format_version;
    uint32_t integrity_algorithm;
    char checkpoint_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    evo_project_orchestration_checkpoint_identity_t identity;
    size_t core_checkpoint_size;
    const unsigned char *core_checkpoint;
    size_t serialized_size;
    const unsigned char *serialized;
    void *private_owner;
} evo_project_orchestration_checkpoint_t;

evo_project_orchestration_checkpoint_status_t
evo_project_orchestration_checkpoint_create(
    const evo_project_orchestration_checkpoint_identity_t *identity,
    const void *core_checkpoint,
    size_t core_checkpoint_size,
    const evo_project_orchestration_checkpoint_limits_t *limits,
    evo_project_orchestration_checkpoint_t *checkpoint);

evo_project_orchestration_checkpoint_status_t
evo_project_orchestration_checkpoint_validate(
    const evo_project_orchestration_checkpoint_identity_t *expected_identity,
    const void *serialized,
    size_t serialized_size,
    const evo_project_orchestration_checkpoint_limits_t *limits,
    evo_project_orchestration_checkpoint_t *checkpoint);

void evo_project_orchestration_checkpoint_destroy(
    evo_project_orchestration_checkpoint_t *checkpoint);

const char *evo_project_orchestration_checkpoint_status_name(
    evo_project_orchestration_checkpoint_status_t status);

#endif
