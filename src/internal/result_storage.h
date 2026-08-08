#ifndef CATALYST_EVO_INTERNAL_RESULT_STORAGE_H
#define CATALYST_EVO_INTERNAL_RESULT_STORAGE_H

#include "catalyst/evo/evo.h"

/* Allocate and register the sole public best-genome owner. */
evo_status_t evo_result_storage_allocate(const evo_problem_t *problem,
                                         const evo_config_t *config,
                                         evo_result_t *result);

#endif
