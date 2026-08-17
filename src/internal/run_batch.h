#ifndef CATALYST_EVO_INTERNAL_RUN_BATCH_H
#define CATALYST_EVO_INTERNAL_RUN_BATCH_H

#include "internal/batch_evaluation.h"

evo_status_t evo_run_with_batch_evaluator(
    const evo_problem_t *problem,
    const evo_config_t *config,
    void *context,
    const evo_population_batch_evaluator_t *batch_evaluator,
    evo_result_t *result);

evo_status_t evo_resume_with_batch_evaluator(
    const evo_problem_t *problem,
    const evo_config_t *config,
    void *context,
    const void *checkpoint,
    size_t checkpoint_size,
    const evo_population_batch_evaluator_t *batch_evaluator,
    evo_result_t *result);

#endif
