#ifndef CATALYST_EVO_INTERNAL_PROJECT_ASSURANCE_OWNER_H
#define CATALYST_EVO_INTERNAL_PROJECT_ASSURANCE_OWNER_H

#include "internal/project_assurance.h"

#include <stdint.h>

typedef struct evo_project_assurance_owner {
    char *candidate_fingerprint;
    char *policy_id;
    char *execution_provider_identity;
    char *output_path;
    char **gate_ids;
    char **profile_ids;
    char **diagnostic_excerpts;
    evo_project_assurance_gate_result_t *gates;
    size_t gate_count;
    char *canonical_json;
    size_t canonical_json_size;
    char *audit_markdown;
    size_t audit_markdown_size;
    uint64_t policy_fingerprint;
    uint64_t assurance_fingerprint;
    evo_project_assurance_t view;
} evo_project_assurance_owner_t;

#endif
