#ifndef CATALYST_EVO_INTERNAL_OBSERVER_H
#define CATALYST_EVO_INTERNAL_OBSERVER_H

#include "catalyst/evo/evo.h"

/*
 * Deliver independent stack snapshots for one committed generation. This
 * helper allocates no storage, returns no control signal, and invokes no
 * callback when observation is disabled.
 */
void evo_generation_observer_notify(const evo_problem_t *problem,
                                    const evo_config_t *config,
                                    const evo_result_t *result,
                                    evo_termination_reason_t termination_reason);

#endif
