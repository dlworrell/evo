#ifndef CATALYST_EVO_INTERNAL_PROJECT_TRANSFORMATION_EVIDENCE_H
#define CATALYST_EVO_INTERNAL_PROJECT_TRANSFORMATION_EVIDENCE_H

#include "internal/project_transformation_model.h"

evo_project_transformation_status_t
evo_project_transformation_application_generate_evidence(
    const evo_project_transformation_limits_t *limits,
    size_t manifest_evidence_limit,
    evo_project_transformation_application_owner_t *owner);

#endif
