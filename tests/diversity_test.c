#include "internal/diversity.h"
#include "internal/rng.h"
#include "internal/selection.h"
#include "internal/statistics.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>

enum {
    TEST_POPULATION_CAPACITY = 5,
    TEST_GENOME_CAPACITY = 2,
    TEST_PAIR_CAPACITY = 10
};

typedef enum distance_mode {
    DISTANCE_MODE_PAIR_GOLDEN = 0,
    DISTANCE_MODE_BYTE_MISMATCH = 1,
    DISTANCE_MODE_FORCED = 2
} distance_mode_t;

typedef struct distance_pair {
    unsigned char left;
    unsigned char right;
} distance_pair_t;

typedef struct diversity_context {
    unsigned char genomes[TEST_POPULATION_CAPACITY * TEST_GENOME_CAPACITY];
    bool validity[TEST_POPULATION_CAPACITY];
    size_t population_size;
    size_t genome_size;
    size_t initialization_calls;
    size_t validation_calls;
    size_t evaluation_calls;
    size_t distance_calls;
    distance_pair_t pairs[TEST_PAIR_CAPACITY];
    distance_mode_t distance_mode;
    double forced_distance;
} diversity_context_t;

static void initialize_genome(void *genome, void *context)
{
    diversity_context_t *evidence = context;
    unsigned char *destination = genome;
    const size_t candidate = evidence->initialization_calls;

    assert(candidate < evidence->population_size);
    for (size_t offset = 0; offset < evidence->genome_size; ++offset) {
        destination[offset] =
            evidence->genomes[candidate * evidence->genome_size + offset];
    }
    ++evidence->initialization_calls;
}

static bool validate_genome(const void *genome, void *context)
{
    diversity_context_t *evidence = context;
    const size_t candidate = evidence->validation_calls;

    (void)genome;
    assert(candidate < evidence->population_size);
    ++evidence->validation_calls;
    return evidence->validity[candidate];
}

static evo_fitness_t evaluate_genome(const void *genome, void *context)
{
    const unsigned char *bytes = genome;
    diversity_context_t *evidence = context;

    ++evidence->evaluation_calls;
    return (evo_fitness_t){
        .correctness = (double)bytes[0],
        .total = (double)bytes[0],
    };
}

static double domain_distance(const void *genome_a,
                              const void *genome_b,
                              size_t genome_size,
                              void *context)
{
    const unsigned char *left = genome_a;
    const unsigned char *right = genome_b;
    diversity_context_t *evidence = context;
    const size_t call = evidence->distance_calls;
    size_t differences = 0;

    assert(genome_size == evidence->genome_size);
    assert(call < TEST_PAIR_CAPACITY);
    evidence->pairs[call] = (distance_pair_t){
        .left = left[0],
        .right = right[0],
    };
    ++evidence->distance_calls;

    if (evidence->distance_mode == DISTANCE_MODE_FORCED) {
        return evidence->forced_distance;
    }

    if (evidence->distance_mode == DISTANCE_MODE_PAIR_GOLDEN) {
        return left[0] == 0 && right[0] == 64 ? 0.0 : 1.0;
    }

    for (size_t offset = 0; offset < genome_size; ++offset) {
        if (left[offset] != right[offset]) {
            ++differences;
        }
    }
    return (double)differences / (double)genome_size;
}

static evo_problem_t make_problem(size_t genome_size,
                                  bool use_domain_distance,
                                  uint32_t distance_version)
{
    return (evo_problem_t){
        .genome_size = genome_size,
        .initialize = initialize_genome,
        .evaluate = evaluate_genome,
        .is_valid = validate_genome,
        .genome_distance = use_domain_distance ? domain_distance : NULL,
        .genome_distance_version = distance_version,
    };
}

static evo_config_t make_config(size_t population_size,
                                size_t genome_size,
                                size_t max_diversity_work)
{
    return (evo_config_t){
        .population_size = population_size,
        .random_seed = UINT64_C(719),
        .max_genome_bytes = genome_size,
        .max_population_bytes = population_size * genome_size,
        .max_evaluation_bytes =
            population_size * sizeof(evo_candidate_evaluation_t),
        .max_diversity_work = max_diversity_work,
    };
}

static void initialize_context(diversity_context_t *context,
                               size_t population_size,
                               size_t genome_size,
                               const unsigned char *genomes,
                               const bool *validity)
{
    *context = (diversity_context_t){0};
    context->population_size = population_size;
    context->genome_size = genome_size;
    for (size_t index = 0; index < population_size; ++index) {
        context->validity[index] = validity[index];
        for (size_t offset = 0; offset < genome_size; ++offset) {
            context->genomes[index * genome_size + offset] =
                genomes[index * genome_size + offset];
        }
    }
}

static evo_status_t evaluate_population(const evo_problem_t *problem,
                                        const evo_config_t *config,
                                        diversity_context_t *context,
                                        evo_population_t *population)
{
    evo_status_t status = evo_population_create(problem,
                                                config,
                                                population);

    if (status != EVO_SUCCESS) {
        return status;
    }
    status = evo_population_initialize(problem,
                                       config,
                                       context,
                                       population);
    if (status != EVO_SUCCESS) {
        return status;
    }
    return evo_population_evaluate(problem,
                                   config,
                                   context,
                                   population);
}

static void assert_diversity(const evo_population_t *population,
                             size_t valid_count,
                             size_t pair_count,
                             size_t work_units,
                             double diversity,
                             uint32_t metric_version,
                             bool uses_domain_distance)
{
    assert(population->valid_count == valid_count);
    assert(population->diversity_policy_version ==
           EVO_DIVERSITY_POLICY_VERSION);
    assert(population->diversity_metric_version == metric_version);
    assert(population->diversity_pair_count == pair_count);
    assert(population->diversity_work_units == work_units);
    assert(population->diversity == diversity);
    assert(population->diversity_uses_domain_distance ==
           uses_domain_distance);
}

static void test_builtin_golden_vectors(void)
{
    static const struct golden_vector {
        size_t population_size;
        unsigned char genomes[TEST_POPULATION_CAPACITY *
                              TEST_GENOME_CAPACITY];
        bool validity[TEST_POPULATION_CAPACITY];
        size_t valid_count;
        size_t pair_count;
        size_t work_units;
        double diversity;
    } vectors[] = {
        {
            .population_size = 3,
            .genomes = {0, 0, 1, 1, 2, 2},
            .validity = {false, false, false},
            .valid_count = 0,
            .pair_count = 0,
            .work_units = 0,
            .diversity = 0.0,
        },
        {
            .population_size = 3,
            .genomes = {0, 0, 1, 1, 2, 2},
            .validity = {false, true, false},
            .valid_count = 1,
            .pair_count = 0,
            .work_units = 0,
            .diversity = 0.0,
        },
        {
            .population_size = 4,
            .genomes = {0xaa, 0x55, 0xaa, 0x55,
                        0xaa, 0x55, 0xaa, 0x55},
            .validity = {true, true, true, true},
            .valid_count = 4,
            .pair_count = 6,
            .work_units = 12,
            .diversity = 0.0,
        },
        {
            .population_size = 2,
            .genomes = {0x00, 0x00, 0xff, 0xff},
            .validity = {true, true},
            .valid_count = 2,
            .pair_count = 1,
            .work_units = 2,
            .diversity = 1.0,
        },
        {
            .population_size = 3,
            .genomes = {0x00, 0x00, 0x00, 0xff, 0xff, 0xff},
            .validity = {true, true, true},
            .valid_count = 3,
            .pair_count = 3,
            .work_units = 6,
            .diversity = 2.0 / 3.0,
        },
        {
            .population_size = 5,
            .genomes = {9, 9, 0, 0, 8, 8, 7, 7, 0, 1},
            .validity = {false, true, false, false, true},
            .valid_count = 2,
            .pair_count = 1,
            .work_units = 2,
            .diversity = 0.5,
        },
    };

    for (size_t vector_index = 0;
         vector_index < sizeof(vectors) / sizeof(vectors[0]);
         ++vector_index) {
        const struct golden_vector *vector = &vectors[vector_index];
        diversity_context_t context = {0};
        evo_problem_t problem = make_problem(TEST_GENOME_CAPACITY,
                                             false,
                                             UINT32_C(0));
        evo_config_t config = make_config(vector->population_size,
                                          TEST_GENOME_CAPACITY,
                                          vector->population_size *
                                              (vector->population_size - 1) /
                                              2 * TEST_GENOME_CAPACITY);
        evo_population_t population = {0};
        size_t validated_count = SIZE_MAX;

        initialize_context(&context,
                           vector->population_size,
                           TEST_GENOME_CAPACITY,
                           vector->genomes,
                           vector->validity);
        assert(evaluate_population(&problem,
                                   &config,
                                   &context,
                                   &population) == EVO_SUCCESS);
        assert_diversity(&population,
                         vector->valid_count,
                         vector->pair_count,
                         vector->work_units,
                         vector->diversity,
                         EVO_BYTE_DIVERSITY_METRIC_VERSION,
                         false);
        assert(evo_population_validate_completed(&config,
                                                 &population,
                                                 &validated_count));
        assert(validated_count == vector->valid_count);
        assert(context.initialization_calls == vector->population_size);
        assert(context.validation_calls == vector->population_size);
        assert(context.evaluation_calls == vector->valid_count);
        assert(context.distance_calls == 0);
        evo_population_destroy(&population);
    }
}

static bool statistics_equal(const evo_generation_statistics_t *left,
                             const evo_generation_statistics_t *right)
{
    return left->version == right->version &&
           left->generation_index == right->generation_index &&
           left->population_size == right->population_size &&
           left->valid_count == right->valid_count &&
           left->invalid_count == right->invalid_count &&
           left->best_index == right->best_index &&
           left->best_fitness.total == right->best_fitness.total &&
           left->fitness_sums.total == right->fitness_sums.total &&
           left->has_best == right->has_best &&
           left->fitness_comparison_policy_version ==
               right->fitness_comparison_policy_version &&
           left->diversity_policy_version ==
               right->diversity_policy_version &&
           left->diversity_metric_version ==
               right->diversity_metric_version &&
           left->diversity_pair_count == right->diversity_pair_count &&
           left->diversity_work_units == right->diversity_work_units &&
           left->diversity == right->diversity &&
           left->diversity_uses_domain_distance ==
               right->diversity_uses_domain_distance;
}

static void test_domain_callback_order_statistics_and_replay(void)
{
    static const unsigned char genomes[] = {0, 64, 255};
    static const bool validity[] = {true, true, true};
    diversity_context_t first_context = {0};
    diversity_context_t replay_context = {0};
    evo_problem_t problem = make_problem(1, true, UINT32_C(7));
    evo_config_t config = make_config(3, 1, 3);
    evo_population_t first = {0};
    evo_population_t replay = {0};
    evo_generation_statistics_t first_statistics = {0};
    evo_generation_statistics_t replay_statistics = {0};

    initialize_context(&first_context, 3, 1, genomes, validity);
    initialize_context(&replay_context, 3, 1, genomes, validity);
    assert(evaluate_population(&problem,
                               &config,
                               &first_context,
                               &first) == EVO_SUCCESS);
    assert(evaluate_population(&problem,
                               &config,
                               &replay_context,
                               &replay) == EVO_SUCCESS);
    assert_diversity(&first, 3, 3, 3, 2.0 / 3.0, UINT32_C(7), true);
    assert_diversity(&replay, 3, 3, 3, 2.0 / 3.0, UINT32_C(7), true);

    assert(first_context.distance_calls == 3);
    assert(first_context.pairs[0].left == 0);
    assert(first_context.pairs[0].right == 64);
    assert(first_context.pairs[1].left == 0);
    assert(first_context.pairs[1].right == 255);
    assert(first_context.pairs[2].left == 64);
    assert(first_context.pairs[2].right == 255);
    for (size_t index = 0; index < first_context.distance_calls; ++index) {
        assert(first_context.pairs[index].left ==
               replay_context.pairs[index].left);
        assert(first_context.pairs[index].right ==
               replay_context.pairs[index].right);
    }

    assert(evo_generation_statistics_record(&config,
                                            &first,
                                            UINT64_C(0),
                                            &first_statistics) ==
           EVO_SUCCESS);
    assert(evo_generation_statistics_record(&config,
                                            &replay,
                                            UINT64_C(0),
                                            &replay_statistics) ==
           EVO_SUCCESS);
    assert(statistics_equal(&first_statistics, &replay_statistics));
    assert(first_statistics.version == EVO_GENERATION_STATISTICS_VERSION);
    assert(first_statistics.diversity_policy_version ==
           EVO_DIVERSITY_POLICY_VERSION);
    assert(first_statistics.diversity_metric_version == UINT32_C(7));
    assert(first_statistics.diversity_pair_count == 3);
    assert(first_statistics.diversity_work_units == 3);
    assert(first_statistics.diversity == 2.0 / 3.0);
    assert(first_statistics.diversity_uses_domain_distance);
    evo_population_destroy(&first);
    evo_population_destroy(&replay);
}

static void assert_result_empty(const evo_result_t *result)
{
    assert(result->best_genome == NULL);
    assert(result->best_fitness.total == 0.0);
    assert(result->generations_completed == 0);
    assert(result->random_seed == 0);
    assert(result->termination_reason == EVO_TERMINATION_NONE);
    assert(result->generation_statistics.version == 0);
    assert(result->generation_statistics.diversity_policy_version == 0);
    assert(result->generation_statistics.diversity_metric_version == 0);
    assert(result->generation_statistics.diversity_pair_count == 0);
    assert(result->generation_statistics.diversity_work_units == 0);
    assert(result->generation_statistics.diversity == 0.0);
    assert(!result->generation_statistics.diversity_uses_domain_distance);
    assert(result->best_genome_size == 0);
    assert(result->secure_erasure_policy_version == 0);
    assert(result->secure_erasure_backend ==
           EVO_SECURE_ERASURE_BACKEND_NONE);
    assert(!result->secure_erasure_enabled);
}

static void test_budget_and_arithmetic_preflight(void)
{
    static const unsigned char genomes[] = {0, 0, 1, 1, 2, 2};
    static const bool validity[] = {true, true, true};
    diversity_context_t builtin_context = {0};
    diversity_context_t domain_context = {0};
    evo_problem_t builtin_problem = make_problem(2, false, UINT32_C(0));
    evo_problem_t domain_problem = make_problem(2, true, UINT32_C(3));
    evo_config_t builtin_config = make_config(3, 2, 5);
    evo_config_t domain_config = make_config(3, 2, 2);
    evo_result_t result = {0};

    initialize_context(&builtin_context, 3, 2, genomes, validity);
    assert(evo_run(&builtin_problem,
                   &builtin_config,
                   &builtin_context,
                   &result) == EVO_ERROR_RESOURCE_LIMIT);
    assert_result_empty(&result);
    assert(builtin_context.initialization_calls == 0);
    assert(builtin_context.validation_calls == 0);
    assert(builtin_context.evaluation_calls == 0);

    initialize_context(&domain_context, 3, 2, genomes, validity);
    assert(evo_run(&domain_problem,
                   &domain_config,
                   &domain_context,
                   &result) == EVO_ERROR_RESOURCE_LIMIT);
    assert_result_empty(&result);
    assert(domain_context.initialization_calls == 0);
    assert(domain_context.validation_calls == 0);
    assert(domain_context.evaluation_calls == 0);
    assert(domain_context.distance_calls == 0);

    builtin_config.population_size = SIZE_MAX;
    builtin_config.max_diversity_work = SIZE_MAX;
    assert(evo_diversity_validate_config(&builtin_problem,
                                         &builtin_config) ==
           EVO_ERROR_RESOURCE_LIMIT);
    builtin_config.population_size = 3;
    builtin_problem.genome_size = SIZE_MAX;
    assert(evo_diversity_validate_config(&builtin_problem,
                                         &builtin_config) ==
           EVO_ERROR_RESOURCE_LIMIT);

    domain_config.population_size = SIZE_MAX;
    domain_config.max_diversity_work = SIZE_MAX;
    assert(evo_diversity_validate_config(&domain_problem,
                                         &domain_config) ==
           EVO_ERROR_RESOURCE_LIMIT);

    domain_problem.genome_distance_version = 0;
    assert(evo_diversity_validate_config(&domain_problem,
                                         &domain_config) ==
           EVO_ERROR_INVALID_ARGUMENT);
    builtin_problem.genome_size = 2;
    builtin_problem.genome_distance_version = 1;
    builtin_config.population_size = 1;
    assert(evo_diversity_validate_config(&builtin_problem,
                                         &builtin_config) ==
           EVO_ERROR_INVALID_ARGUMENT);
}

static void assert_population_evaluation_empty(
    const evo_population_t *population)
{
    assert(population->evaluations == NULL);
    assert(population->evaluation_bytes == 0);
    assert(population->valid_count == 0);
    assert(population->best_index == 0);
    assert(population->fitness_comparison_policy_version == 0);
    assert(population->diversity_policy_version == 0);
    assert(population->diversity_metric_version == 0);
    assert(population->diversity_pair_count == 0);
    assert(population->diversity_work_units == 0);
    assert(population->diversity == 0.0);
    assert(!population->has_best);
    assert(!population->evaluated);
    assert(!population->diversity_uses_domain_distance);
}

static void test_malformed_distance_rolls_back_and_public_failure_is_empty(void)
{
    static const unsigned char genomes[] = {1, 2};
    static const bool validity[] = {true, true};
    static const double malformed_values[] = {
        NAN, INFINITY, -0.25, 1.25};
    diversity_context_t context = {0};
    evo_problem_t problem = make_problem(1, true, UINT32_C(5));
    evo_config_t config = make_config(2, 1, 1);
    evo_population_t population = {0};
    evo_result_t result = {0};

    initialize_context(&context, 2, 1, genomes, validity);
    context.distance_mode = DISTANCE_MODE_FORCED;
    context.forced_distance = NAN;
    assert(evo_population_create(&problem, &config, &population) ==
           EVO_SUCCESS);
    assert(evo_population_initialize(&problem,
                                     &config,
                                     &context,
                                     &population) == EVO_SUCCESS);
    assert(evo_population_evaluate(&problem,
                                   &config,
                                   &context,
                                   &population) == EVO_ERROR_EVALUATION);
    assert_population_evaluation_empty(&population);
    assert(population.initialized);
    assert(context.distance_calls == 1);

    context.validation_calls = 0;
    context.evaluation_calls = 0;
    context.distance_calls = 0;
    context.distance_mode = DISTANCE_MODE_BYTE_MISMATCH;
    assert(evo_population_evaluate(&problem,
                                   &config,
                                   &context,
                                   &population) == EVO_SUCCESS);
    assert_diversity(&population, 2, 1, 1, 1.0, UINT32_C(5), true);
    evo_population_destroy(&population);

    for (size_t index = 0;
         index < sizeof(malformed_values) / sizeof(malformed_values[0]);
         ++index) {
        initialize_context(&context, 2, 1, genomes, validity);
        context.distance_mode = DISTANCE_MODE_FORCED;
        context.forced_distance = malformed_values[index];
        assert(evo_run(&problem, &config, &context, &result) ==
               EVO_ERROR_EVALUATION);
        assert_result_empty(&result);
    }
}

static void assert_rng_equal(const evo_rng_t *left,
                             const evo_rng_t *right)
{
    assert(left->state == right->state);
    assert(left->increment == right->increment);
    assert(left->seeded == right->seeded);
}

static void test_diversity_does_not_change_selection_rng(void)
{
    static const unsigned char genomes[] = {1, 2, 3, 4};
    static const bool validity[] = {true, true, true, true};
    diversity_context_t builtin_context = {0};
    diversity_context_t domain_context = {0};
    evo_problem_t builtin_problem = make_problem(1, false, UINT32_C(0));
    evo_problem_t domain_problem = make_problem(1, true, UINT32_C(9));
    evo_config_t config = make_config(4, 1, 6);
    evo_population_t builtin_population = {0};
    evo_population_t domain_population = {0};
    evo_rng_t builtin_rng = {0};
    evo_rng_t domain_rng = {0};
    size_t builtin_index = SIZE_MAX;
    size_t domain_index = SIZE_MAX;

    config.tournament_size = 3;
    initialize_context(&builtin_context, 4, 1, genomes, validity);
    initialize_context(&domain_context, 4, 1, genomes, validity);
    domain_context.distance_mode = DISTANCE_MODE_BYTE_MISMATCH;
    assert(evaluate_population(&builtin_problem,
                               &config,
                               &builtin_context,
                               &builtin_population) == EVO_SUCCESS);
    assert(evaluate_population(&domain_problem,
                               &config,
                               &domain_context,
                               &domain_population) == EVO_SUCCESS);
    assert(builtin_population.diversity == domain_population.diversity);
    assert(domain_context.distance_calls == 6);

    assert(evo_rng_seed(&builtin_rng, UINT64_C(991)));
    assert(evo_rng_seed(&domain_rng, UINT64_C(991)));
    assert(evo_population_select_tournament(&config,
                                            &builtin_population,
                                            &builtin_rng,
                                            &builtin_index) == EVO_SUCCESS);
    assert(evo_population_select_tournament(&config,
                                            &domain_population,
                                            &domain_rng,
                                            &domain_index) == EVO_SUCCESS);
    assert(builtin_index == domain_index);
    assert_rng_equal(&builtin_rng, &domain_rng);
    evo_population_destroy(&builtin_population);
    evo_population_destroy(&domain_population);
}

static void test_invalid_candidates_never_reach_domain_callback(void)
{
    static const unsigned char genomes[] = {9, 0, 8, 7, 64};
    static const bool validity[] = {false, true, false, false, true};
    static const bool one_valid[] = {false, true, false, false, false};
    diversity_context_t context = {0};
    evo_problem_t problem = make_problem(1, true, UINT32_C(11));
    evo_config_t config = make_config(5, 1, 10);
    evo_population_t population = {0};

    initialize_context(&context, 5, 1, genomes, validity);
    assert(evaluate_population(&problem,
                               &config,
                               &context,
                               &population) == EVO_SUCCESS);
    assert_diversity(&population, 2, 1, 1, 0.0, UINT32_C(11), true);
    assert(context.distance_calls == 1);
    assert(context.pairs[0].left == 0);
    assert(context.pairs[0].right == 64);
    evo_population_destroy(&population);

    initialize_context(&context, 5, 1, genomes, one_valid);
    assert(evaluate_population(&problem,
                               &config,
                               &context,
                               &population) == EVO_SUCCESS);
    assert_diversity(&population, 1, 0, 0, 0.0, UINT32_C(11), true);
    assert(context.distance_calls == 0);
    evo_population_destroy(&population);
}

int main(void)
{
    test_builtin_golden_vectors();
    test_domain_callback_order_statistics_and_replay();
    test_budget_and_arithmetic_preflight();
    test_malformed_distance_rolls_back_and_public_failure_is_empty();
    test_diversity_does_not_change_selection_rng();
    test_invalid_candidates_never_reach_domain_callback();
    return 0;
}
