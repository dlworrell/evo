#ifndef CATALYST_EVO_INTERNAL_PROJECT_SEARCH_H
#define CATALYST_EVO_INTERNAL_PROJECT_SEARCH_H

#include "internal/project_recipe.h"

#include "catalyst/evo/evo.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EVO_PROJECT_SEARCH_SCHEMA_VERSION 1U
#define EVO_PROJECT_SEARCH_MUTATION_POLICY_VERSION 1U
#define EVO_PROJECT_SEARCH_CROSSOVER_POLICY_VERSION 1U
#define EVO_PROJECT_SEARCH_REPAIR_POLICY_VERSION 1U

#define EVO_PROJECT_SEARCH_MUTATION_ADD UINT32_C(1)
#define EVO_PROJECT_SEARCH_MUTATION_REMOVE UINT32_C(2)
#define EVO_PROJECT_SEARCH_MUTATION_PARAMETERIZE UINT32_C(4)
#define EVO_PROJECT_SEARCH_MUTATION_REPLACE UINT32_C(8)
#define EVO_PROJECT_SEARCH_MUTATION_REORDER UINT32_C(16)
#define EVO_PROJECT_SEARCH_MUTATION_ALL                                        \
    (EVO_PROJECT_SEARCH_MUTATION_ADD | EVO_PROJECT_SEARCH_MUTATION_REMOVE |    \
     EVO_PROJECT_SEARCH_MUTATION_PARAMETERIZE |                                \
     EVO_PROJECT_SEARCH_MUTATION_REPLACE | EVO_PROJECT_SEARCH_MUTATION_REORDER)

typedef enum evo_project_search_status {
    EVO_PROJECT_SEARCH_SUCCESS = 0,
    EVO_PROJECT_SEARCH_ERROR_INVALID_ARGUMENT = 1,
    EVO_PROJECT_SEARCH_ERROR_RESULT_ACTIVE = 2,
    EVO_PROJECT_SEARCH_ERROR_AUTHORITY_STALE = 3,
    EVO_PROJECT_SEARCH_ERROR_POLICY_INVALID = 4,
    EVO_PROJECT_SEARCH_ERROR_RESOURCE_LIMIT = 5,
    EVO_PROJECT_SEARCH_ERROR_OUT_OF_MEMORY = 6,
    EVO_PROJECT_SEARCH_ERROR_NO_VALID_CANDIDATE = 7,
    EVO_PROJECT_SEARCH_ERROR_CORE = 8,
    EVO_PROJECT_SEARCH_ERROR_PROVIDER = 9,
    EVO_PROJECT_SEARCH_ERROR_EVIDENCE = 10,
    EVO_PROJECT_SEARCH_ERROR_STATE = 11
} evo_project_search_status_t;

typedef enum evo_project_search_operator_kind {
    EVO_PROJECT_SEARCH_OPERATOR_INITIALIZE = 1,
    EVO_PROJECT_SEARCH_OPERATOR_MUTATION_ADD = 2,
    EVO_PROJECT_SEARCH_OPERATOR_MUTATION_REMOVE = 3,
    EVO_PROJECT_SEARCH_OPERATOR_MUTATION_PARAMETERIZE = 4,
    EVO_PROJECT_SEARCH_OPERATOR_MUTATION_REPLACE = 5,
    EVO_PROJECT_SEARCH_OPERATOR_MUTATION_REORDER = 6,
    EVO_PROJECT_SEARCH_OPERATOR_CROSSOVER = 7,
    EVO_PROJECT_SEARCH_OPERATOR_CLONE = 8
} evo_project_search_operator_kind_t;

typedef enum evo_project_search_rejection_reason {
    EVO_PROJECT_SEARCH_REJECTION_NONE = 0,
    EVO_PROJECT_SEARCH_REJECTION_RECIPE_INVALID = 1,
    EVO_PROJECT_SEARCH_REJECTION_NO_COMPATIBLE_OPERATION = 2,
    EVO_PROJECT_SEARCH_REJECTION_RESOURCE_LIMIT = 3,
    EVO_PROJECT_SEARCH_REJECTION_PROVIDER = 4,
    EVO_PROJECT_SEARCH_REJECTION_CORRECTNESS = 5,
    EVO_PROJECT_SEARCH_REJECTION_ASSURANCE = 6,
    EVO_PROJECT_SEARCH_REJECTION_MEASUREMENT = 7,
    EVO_PROJECT_SEARCH_REJECTION_STATE = 8
} evo_project_search_rejection_reason_t;

typedef struct evo_project_search_limits {
    size_t max_string_bytes;
    size_t max_records;
    size_t max_parameters_per_record;
    size_t max_mutations_per_event;
    size_t max_lineage_records;
    size_t max_operator_events;
    size_t max_evidence_bytes;
    size_t max_total_bytes;
} evo_project_search_limits_t;

typedef struct evo_project_search_policy {
    uint32_t schema_version;
    const char *identity;
    uint32_t mutation_policy_version;
    uint32_t crossover_policy_version;
    uint32_t repair_policy_version;
    size_t initial_record_count;
    size_t maximum_record_count;
    uint32_t mutation_operation_mask;
    size_t max_mutations_per_event;
    bool integer_parameter_wrap;
} evo_project_search_policy_t;

typedef struct evo_project_search_evaluation_request {
    uint32_t schema_version;
    uint64_t random_seed;
    size_t generation;
    size_t population_index;
    const char *provider_identity;
    const evo_project_recipe_t *recipe;
} evo_project_search_evaluation_request_t;

typedef struct evo_project_search_evaluation_outcome {
    uint32_t schema_version;
    bool accepted;
    bool correctness_preserved;
    bool performance_eligible;
    bool fitness_available;
    const char *candidate_fingerprint;
    const char *assurance_fingerprint;
    const char *measurement_fingerprint;
    evo_fitness_t fitness;
} evo_project_search_evaluation_outcome_t;

typedef evo_project_search_status_t (*evo_project_search_evaluation_provider_fn)(
    const evo_project_search_evaluation_request_t *request,
    void *context,
    evo_project_search_evaluation_outcome_t *outcome);

typedef struct evo_project_search_config {
    evo_project_recipe_context_t recipe_context;
    size_t genome_size;
    size_t population_size;
    size_t generation_limit;
    size_t tournament_size;
    double crossover_rate;
    double mutation_rate;
    uint64_t random_seed;
    size_t max_core_population_bytes;
    size_t max_core_evaluation_bytes;
    size_t max_core_child_population_bytes;
    size_t max_core_diversity_work;
    evo_project_search_policy_t policy;
    const char *evaluation_provider_identity;
    evo_project_search_evaluation_provider_fn evaluation_provider;
    void *evaluation_provider_context;
    evo_project_search_limits_t limits;
} evo_project_search_config_t;

typedef struct evo_project_search_lineage_record {
    size_t generation;
    size_t population_index;
    size_t operator_ordinal;
    evo_project_search_operator_kind_t operator_kind;
    evo_project_search_rejection_reason_t rejection_reason;
    evo_project_recipe_status_t recipe_status;
    char recipe_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char parent_a_recipe_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char parent_b_recipe_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char candidate_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char assurance_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char measurement_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    evo_fitness_t fitness;
    bool valid;
    bool evaluated;
    bool winner;
} evo_project_search_lineage_record_t;

typedef struct evo_project_search {
    uint32_t schema_version;
    const char *baseline_fingerprint;
    const char *analysis_fingerprint;
    const char *catalogue_identity;
    uint32_t catalogue_version;
    const char *policy_identity;
    const char *evaluation_provider_identity;
    uint64_t random_seed;
    size_t population_size;
    size_t generations_completed;
    evo_termination_reason_t termination_reason;
    char best_recipe_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char best_candidate_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char best_assurance_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char best_measurement_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    evo_fitness_t best_fitness;
    size_t best_genome_size;
    const unsigned char *best_genome;
    size_t lineage_count;
    const evo_project_search_lineage_record_t *lineage;
    char search_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    size_t canonical_json_size;
    const char *canonical_json;
    size_t audit_markdown_size;
    const char *audit_markdown;
    bool projection_complete;
    bool probabilistic_authority;
    bool raw_source_bytes;
    void *private_owner;
} evo_project_search_t;

evo_project_search_status_t evo_project_search_run(
    const evo_project_search_config_t *config,
    evo_project_search_t *search);

void evo_project_search_destroy(evo_project_search_t *search);

const char *evo_project_search_status_name(evo_project_search_status_t status);
const char *evo_project_search_operator_kind_name(evo_project_search_operator_kind_t kind);
const char *evo_project_search_rejection_reason_name(evo_project_search_rejection_reason_t reason);

#endif
