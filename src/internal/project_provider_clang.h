#ifndef CATALYST_EVO_INTERNAL_PROJECT_PROVIDER_CLANG_H
#define CATALYST_EVO_INTERNAL_PROJECT_PROVIDER_CLANG_H

#include "internal/project_analysis.h"

#include <stddef.h>

typedef struct evo_project_clang_analysis_context {
    evo_project_source_location_record_t *source_locations;
    size_t source_location_count;
    size_t source_location_capacity;
    evo_project_declaration_record_t *declarations;
    size_t declaration_count;
    size_t declaration_capacity;
} evo_project_clang_analysis_context_t;

evo_project_analysis_status_t evo_project_clang_analysis_provider(
    const evo_project_analysis_request_t *request,
    void *context,
    evo_project_analysis_provider_result_t *result);

void evo_project_clang_analysis_context_destroy(
    evo_project_clang_analysis_context_t *context);

#endif
