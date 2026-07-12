#ifndef CATALYST_EVO_EVO_H
#define CATALYST_EVO_EVO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct evo_fitness {
    double correctness;
    double performance;
    double memory_use;
    double reliability;
    double maintainability;
    double constraint_penalty;
    double total;
} evo_fitness_t;

typedef struct evo_problem {
    size_t genome_size;
    void (*initialize)(void *genome, void *context);
    void (*mutate)(void *genome, double mutation_rate, void *context);
    void (*crossover)(const void *parent_a, const void *parent_b, void *child_a,
                      void *child_b, void *context);
    evo_fitness_t (*evaluate)(const void *genome, void *context);
    bool (*is_valid)(const void *genome, void *context);
} evo_problem_t;

typedef struct evo_config {
    size_t population_size;
    size_t generation_limit;
    size_t tournament_size;
    double crossover_rate;
    double mutation_rate;
    uint64_t random_seed;
} evo_config_t;

typedef struct evo_result {
    void *best_genome;
    evo_fitness_t best_fitness;
    size_t generations_completed;
    uint64_t random_seed;
} evo_result_t;

int evo_run(const evo_problem_t *problem, const evo_config_t *config, void *context,
            evo_result_t *result);

void evo_result_destroy(evo_result_t *result);

#ifdef __cplusplus
}
#endif

#endif
