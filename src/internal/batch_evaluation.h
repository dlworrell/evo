#ifndef CATALYST_EVO_INTERNAL_BATCH_EVALUATION_H
#define CATALYST_EVO_INTERNAL_BATCH_EVALUATION_H

#include "catalyst/evo/evo.h"

#include <stddef.h>
#include <stdint.h>

typedef evo_status_t (*evo_population_batch_evaluator_fn)(
    const evo_problem_t *problem,
    const evo_config_t *config,
    void *context,
    uint64_t generation,
    const evo_population_t *population,
    evo_candidate_evaluation_t *evaluations,
    size_t evaluation_count,
    void *batch_context);

typedef struct evo_population_batch_evaluator {
    evo_population_batch_evaluator_fn evaluate;
    void *context;
} evo_population_batch_evaluator_t;

#endif
