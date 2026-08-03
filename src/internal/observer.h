#ifndef CATALYST_EVO_INTERNAL_OBSERVER_H
#define CATALYST_EVO_INTERNAL_OBSERVER_H

#include "catalyst/evo/evo.h"

/*
 * Evaluate optional stopping and then deliver independent observer snapshots
 * for one committed generation. A natural terminal reason suppresses the stop
 * decision. This helper allocates no storage and returns the final reason that
 * applies to the committed generation.
 */
evo_termination_reason_t evo_generation_callbacks_notify(
    const evo_problem_t *problem,
    const evo_config_t *config,
    const evo_result_t *result,
    evo_termination_reason_t natural_reason);

#endif
