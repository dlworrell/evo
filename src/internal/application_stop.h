#ifndef CATALYST_EVO_INTERNAL_APPLICATION_STOP_H
#define CATALYST_EVO_INTERNAL_APPLICATION_STOP_H

#include "catalyst/evo/evo.h"

/*
 * Ask the configured callback whether an otherwise-continuing committed
 * generation should terminate successfully. This helper allocates no storage,
 * consumes no RNG state, and returns false when stopping is disabled.
 */
bool evo_application_stop_requested(const evo_problem_t *problem,
                                    const evo_config_t *config,
                                    const evo_result_t *result);

#endif
