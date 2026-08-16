#ifndef CATALYST_EVO_INTERNAL_PROJECT_SEARCH_OWNER_H
#define CATALYST_EVO_INTERNAL_PROJECT_SEARCH_OWNER_H

#include "internal/project_search.h"

typedef struct evo_project_search_birth_event {
    const void *genome_address;
    size_t operator_event_start;
    size_t operator_event_count;
    evo_project_search_rejection_reason_t rejection_reason;
    evo_project_recipe_status_t recipe_status;
    char parent_a_recipe_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char parent_b_recipe_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    bool consumed;
} evo_project_search_birth_event_t;

typedef struct evo_project_search_owner {
    evo_project_search_t view;
    char *baseline_fingerprint;
    char *analysis_fingerprint;
    char *catalogue_identity;
    char *policy_identity;
    char *evaluation_provider_identity;
    unsigned char *best_genome;
    evo_project_search_operator_event_t *operator_events;
    size_t operator_event_capacity;
    size_t operator_event_count;
    evo_project_search_lineage_record_t *lineage;
    const void **lineage_genome_addresses;
    size_t lineage_capacity;
    size_t lineage_count;
    evo_project_search_birth_event_t *birth_events;
    size_t birth_capacity;
    size_t birth_count;
    size_t validation_ordinal;
    char *canonical_json;
    char *audit_markdown;
    uint64_t search_fingerprint_value;
} evo_project_search_owner_t;

#endif
