#include "catalyst/evo/evo.h"

#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    evo_checkpoint_view_t view = {0};

    if (size != 0 &&
        evo_checkpoint_inspect(data, size, size, &view) == EVO_SUCCESS) {
        for (size_t index = 0; index < view.population_size; ++index) {
            evo_checkpoint_candidate_view_t candidate = {0};

            (void)evo_checkpoint_candidate_inspect(&view,
                                                   index,
                                                   &candidate);
        }
    }
    return 0;
}
