#include "catalyst/evo/evo.h"

#include <assert.h>

int main(void)
{
    evo_result_t result = {0};
    assert(result.termination_reason == EVO_TERMINATION_NONE);
    evo_result_destroy(&result);
    assert(result.best_genome == 0);
    assert(result.termination_reason == EVO_TERMINATION_NONE);
    return 0;
}
