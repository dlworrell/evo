#include <catalyst/evo/evo.h>

int main(void)
{
    const evo_problem_t problem = {
        .genome_size = 8,
    };
    const evo_config_t config = {
        .population_size = 1,
        .max_genome_bytes = 8,
    };
    evo_result_t result = {0};

    if (evo_run(&problem, &config, NULL, &result) != EVO_SUCCESS) {
        return 1;
    }
    evo_result_destroy(&result);
    return result.best_genome == NULL ? 0 : 1;
}
