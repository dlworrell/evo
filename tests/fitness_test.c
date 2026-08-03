#include "internal/fitness.h"
#include "internal/population_storage.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>

_Static_assert(EVO_FITNESS_COMPARISON_POLICY_VERSION == UINT32_C(1),
               "the initial fitness-comparison policy must remain stable");

enum {
    TEST_POPULATION_SIZE = 3
};

typedef struct fitness_context {
    bool malformed_last;
    size_t initialization_calls;
    size_t validation_calls;
    size_t evaluation_calls;
} fitness_context_t;

static evo_fitness_t fitness(double total, double penalty)
{
    return (evo_fitness_t){
        .correctness = total + 1.0,
        .performance = total + 2.0,
        .memory_use = total + 3.0,
        .reliability = total + 4.0,
        .maintainability = total + 5.0,
        .constraint_penalty = penalty,
        .total = total,
    };
}

static evo_fitness_candidate_view_t candidate(
    const evo_fitness_t *evidence,
    uint64_t generation,
    size_t population_index)
{
    return (evo_fitness_candidate_view_t){
        .fitness = evidence,
        .generation = generation,
        .population_index = population_index,
        .hard_valid = true,
        .evaluated = true,
    };
}

static void assert_result_empty(const evo_result_t *result)
{
    const evo_result_t empty = {0};
    const unsigned char *left = (const unsigned char *)result;
    const unsigned char *right = (const unsigned char *)&empty;

    for (size_t offset = 0; offset < sizeof(*result); ++offset) {
        assert(left[offset] == right[offset]);
    }
}

static void test_penalty_evidence_rules(void)
{
    evo_fitness_t evidence = fitness(10.0, 2.0);

    assert(evo_fitness_evidence_is_valid(&evidence));
    evidence.constraint_penalty = 0.0;
    assert(evo_fitness_evidence_is_valid(&evidence));
    evidence.constraint_penalty = -0.0;
    assert(evo_fitness_evidence_is_valid(&evidence));
    evidence.constraint_penalty = -1.0;
    assert(!evo_fitness_evidence_is_valid(&evidence));
    evidence.constraint_penalty = INFINITY;
    assert(!evo_fitness_evidence_is_valid(&evidence));
    evidence.constraint_penalty = NAN;
    assert(!evo_fitness_evidence_is_valid(&evidence));
    assert(!evo_fitness_evidence_is_valid(NULL));
}

static void test_hard_gate_and_stable_comparison(void)
{
    evo_fitness_t high_total_large_penalty = fitness(12.0, 100.0);
    evo_fitness_t low_total_small_penalty = fitness(11.0, 0.0);
    evo_fitness_t tied_total = fitness(12.0, 1.0);
    evo_fitness_candidate_view_t left =
        candidate(&high_total_large_penalty, UINT64_C(2), 7);
    evo_fitness_candidate_view_t right =
        candidate(&low_total_small_penalty, UINT64_C(0), 0);
    evo_fitness_order_t order = EVO_FITNESS_ORDER_EQUAL;

    assert(evo_fitness_compare_candidates(&left, &right, &order));
    assert(order == EVO_FITNESS_ORDER_LEFT);

    right = candidate(&tied_total, UINT64_C(1), 9);
    assert(evo_fitness_compare_candidates(&left, &right, &order));
    assert(order == EVO_FITNESS_ORDER_RIGHT);

    left = candidate(&high_total_large_penalty, UINT64_C(1), 3);
    right = candidate(&tied_total, UINT64_C(1), 4);
    assert(evo_fitness_compare_candidates(&left, &right, &order));
    assert(order == EVO_FITNESS_ORDER_LEFT);

    right.population_index = left.population_index;
    assert(evo_fitness_compare_candidates(&left, &right, &order));
    assert(order == EVO_FITNESS_ORDER_EQUAL);

    left.hard_valid = false;
    order = EVO_FITNESS_ORDER_RIGHT;
    assert(!evo_fitness_candidate_is_rankable(&left));
    assert(!evo_fitness_compare_candidates(&left, &right, &order));
    assert(order == EVO_FITNESS_ORDER_RIGHT);

    left.hard_valid = true;
    left.evaluated = false;
    assert(!evo_fitness_candidate_is_rankable(&left));
    assert(!evo_fitness_compare_candidates(&left, &right, &order));
    assert(order == EVO_FITNESS_ORDER_RIGHT);
}

static void initialize_genome(void *genome, void *opaque)
{
    fitness_context_t *context = opaque;
    unsigned char *value = genome;

    assert(context->initialization_calls < TEST_POPULATION_SIZE);
    *value = (unsigned char)context->initialization_calls;
    ++context->initialization_calls;
}

static bool validate_genome(const void *genome, void *opaque)
{
    fitness_context_t *context = opaque;
    const unsigned char value = *(const unsigned char *)genome;

    ++context->validation_calls;
    return value != 1;
}

static evo_fitness_t evaluate_genome(const void *genome, void *opaque)
{
    fitness_context_t *context = opaque;
    const unsigned char value = *(const unsigned char *)genome;

    assert(value != 1);
    ++context->evaluation_calls;
    if (context->malformed_last && value == 2) {
        return fitness(2.0, -1.0);
    }
    if (value == 0) {
        return fitness(10.0, 50.0);
    }
    return fitness(10.0, 0.0);
}

static evo_problem_t make_problem(void)
{
    return (evo_problem_t){
        .genome_size = 1,
        .initialize = initialize_genome,
        .evaluate = evaluate_genome,
        .is_valid = validate_genome,
    };
}

static evo_config_t make_config(void)
{
    return (evo_config_t){
        .population_size = TEST_POPULATION_SIZE,
        .random_seed = UINT64_C(43),
        .max_genome_bytes = 1,
        .max_population_bytes = TEST_POPULATION_SIZE,
        .max_evaluation_bytes =
            TEST_POPULATION_SIZE * sizeof(evo_candidate_evaluation_t),
        .max_diversity_work = SIZE_MAX,
    };
}

static void test_public_hard_gate_and_total_authority(void)
{
    evo_problem_t problem = make_problem();
    evo_config_t config = make_config();
    fitness_context_t context = {0};
    evo_result_t result = {0};

    assert(evo_run(&problem, &config, &context, &result) == EVO_SUCCESS);
    assert(context.initialization_calls == TEST_POPULATION_SIZE);
    assert(context.validation_calls == TEST_POPULATION_SIZE);
    assert(context.evaluation_calls == 2);
    assert(result.best_genome != NULL);
    assert(*(const unsigned char *)result.best_genome == 0);
    assert(result.best_fitness.total == 10.0);
    assert(result.best_fitness.constraint_penalty == 50.0);
    assert(result.generation_statistics.best_index == 0);
    assert(result.generation_statistics.version ==
           EVO_GENERATION_STATISTICS_VERSION);
    assert(result.generation_statistics.fitness_comparison_policy_version ==
           EVO_FITNESS_COMPARISON_POLICY_VERSION);
    evo_result_destroy(&result);
    assert_result_empty(&result);
}

static void test_malformed_penalty_fails_atomically(void)
{
    evo_problem_t problem = make_problem();
    evo_config_t config = make_config();
    fitness_context_t context = {
        .malformed_last = true,
    };
    evo_result_t result = {0};

    assert(evo_run(&problem, &config, &context, &result) ==
           EVO_ERROR_EVALUATION);
    assert(context.initialization_calls == TEST_POPULATION_SIZE);
    assert(context.validation_calls == TEST_POPULATION_SIZE);
    assert(context.evaluation_calls == 2);
    assert_result_empty(&result);
}

static evo_fitness_t evaluate_total_only(const void *genome, void *context)
{
    (void)context;
    return (evo_fitness_t){
        .total = (double)*(const unsigned char *)genome,
    };
}

static void test_total_only_replay_compatibility(void)
{
    evo_problem_t problem = make_problem();
    evo_config_t config = make_config();
    fitness_context_t first_context = {0};
    fitness_context_t second_context = {0};
    evo_result_t first = {0};
    evo_result_t second = {0};

    problem.evaluate = evaluate_total_only;
    problem.is_valid = NULL;
    assert(evo_run(&problem, &config, &first_context, &first) ==
           EVO_SUCCESS);
    assert(evo_run(&problem, &config, &second_context, &second) ==
           EVO_SUCCESS);
    assert(first.best_genome != NULL);
    assert(second.best_genome != NULL);
    assert(*(const unsigned char *)first.best_genome == 2);
    assert(*(const unsigned char *)second.best_genome == 2);
    assert(first.best_fitness.total == 2.0);
    assert(second.best_fitness.total == 2.0);
    assert(first.best_fitness.constraint_penalty == 0.0);
    assert(second.best_fitness.constraint_penalty == 0.0);
    assert(first.generation_statistics.best_index == 2);
    assert(second.generation_statistics.best_index == 2);
    assert(first.generation_statistics.fitness_comparison_policy_version ==
           EVO_FITNESS_COMPARISON_POLICY_VERSION);
    assert(second.generation_statistics.fitness_comparison_policy_version ==
           EVO_FITNESS_COMPARISON_POLICY_VERSION);
    evo_result_destroy(&first);
    evo_result_destroy(&second);
}

int main(void)
{
    test_penalty_evidence_rules();
    test_hard_gate_and_stable_comparison();
    test_public_hard_gate_and_total_authority();
    test_malformed_penalty_fails_atomically();
    test_total_only_replay_compatibility();
    return 0;
}
