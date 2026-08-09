#ifndef CATALYST_EVO_INTERNAL_PROJECT_ANALYSIS_OWNER_H
#define CATALYST_EVO_INTERNAL_PROJECT_ANALYSIS_OWNER_H

#include "internal/project_analysis.h"

typedef struct evo_project_analysis_owner {
    char *output_path;
    char *baseline_fingerprint;
    char *provider_identity;
    char *clang_identity;
    char *llvm_identity;
    char *target_identity;
    char *flags_identity;
    char *runtime_profile_identity;
    evo_project_runtime_profile_state_t runtime_profile_state;
    char **translation_units;
    size_t translation_unit_count;
    evo_project_source_location_record_t *source_locations;
    size_t source_location_count;
    evo_project_declaration_record_t *declarations;
    size_t declaration_count;
    evo_project_call_record_t *calls;
    size_t call_count;
    evo_project_control_flow_record_t *control_flows;
    size_t control_flow_count;
    evo_project_data_flow_record_t *data_flows;
    size_t data_flow_count;
    evo_project_optimization_record_t *optimization_records;
    size_t optimization_record_count;
    evo_project_runtime_record_t *runtime_records;
    size_t runtime_record_count;
    evo_project_opportunity_record_t *opportunities;
    size_t opportunity_count;
    uint64_t analysis_fingerprint;
    bool output_reserved;
    bool committed;
} evo_project_analysis_owner_t;

#endif
