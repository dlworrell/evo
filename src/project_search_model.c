#include "internal/project_search.h"

const char *evo_project_search_status_name(evo_project_search_status_t status)
{
    switch (status) {
    case EVO_PROJECT_SEARCH_SUCCESS:
        return "success";
    case EVO_PROJECT_SEARCH_ERROR_INVALID_ARGUMENT:
        return "invalid-argument";
    case EVO_PROJECT_SEARCH_ERROR_RESULT_ACTIVE:
        return "result-active";
    case EVO_PROJECT_SEARCH_ERROR_AUTHORITY_STALE:
        return "authority-stale";
    case EVO_PROJECT_SEARCH_ERROR_POLICY_INVALID:
        return "policy-invalid";
    case EVO_PROJECT_SEARCH_ERROR_RESOURCE_LIMIT:
        return "resource-limit";
    case EVO_PROJECT_SEARCH_ERROR_OUT_OF_MEMORY:
        return "out-of-memory";
    case EVO_PROJECT_SEARCH_ERROR_NO_VALID_CANDIDATE:
        return "no-valid-candidate";
    case EVO_PROJECT_SEARCH_ERROR_CORE:
        return "core";
    case EVO_PROJECT_SEARCH_ERROR_PROVIDER:
        return "provider";
    case EVO_PROJECT_SEARCH_ERROR_EVIDENCE:
        return "evidence";
    case EVO_PROJECT_SEARCH_ERROR_STATE:
        return "state";
    default:
        return "unknown";
    }
}

const char *evo_project_search_operator_kind_name(
    evo_project_search_operator_kind_t kind)
{
    switch (kind) {
    case EVO_PROJECT_SEARCH_OPERATOR_INITIALIZE:
        return "initialize";
    case EVO_PROJECT_SEARCH_OPERATOR_MUTATION_ADD:
        return "mutation-add";
    case EVO_PROJECT_SEARCH_OPERATOR_MUTATION_REMOVE:
        return "mutation-remove";
    case EVO_PROJECT_SEARCH_OPERATOR_MUTATION_PARAMETERIZE:
        return "mutation-parameterize";
    case EVO_PROJECT_SEARCH_OPERATOR_MUTATION_REPLACE:
        return "mutation-replace";
    case EVO_PROJECT_SEARCH_OPERATOR_MUTATION_REORDER:
        return "mutation-reorder";
    case EVO_PROJECT_SEARCH_OPERATOR_CROSSOVER:
        return "crossover";
    case EVO_PROJECT_SEARCH_OPERATOR_CLONE:
        return "clone";
    default:
        return "unknown";
    }
}

const char *evo_project_search_rejection_reason_name(
    evo_project_search_rejection_reason_t reason)
{
    switch (reason) {
    case EVO_PROJECT_SEARCH_REJECTION_NONE:
        return "none";
    case EVO_PROJECT_SEARCH_REJECTION_RECIPE_INVALID:
        return "recipe-invalid";
    case EVO_PROJECT_SEARCH_REJECTION_NO_COMPATIBLE_OPERATION:
        return "no-compatible-operation";
    case EVO_PROJECT_SEARCH_REJECTION_RESOURCE_LIMIT:
        return "resource-limit";
    case EVO_PROJECT_SEARCH_REJECTION_PROVIDER:
        return "provider";
    case EVO_PROJECT_SEARCH_REJECTION_CORRECTNESS:
        return "correctness";
    case EVO_PROJECT_SEARCH_REJECTION_ASSURANCE:
        return "assurance";
    case EVO_PROJECT_SEARCH_REJECTION_MEASUREMENT:
        return "measurement";
    case EVO_PROJECT_SEARCH_REJECTION_STATE:
        return "state";
    default:
        return "unknown";
    }
}
