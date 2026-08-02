#include "internal/statistics.h"

#include <assert.h>
#include <float.h>
#include <math.h>

enum { TEST_POPULATION_CAPACITY = 8 };

typedef struct statistics_fixture {
    unsigned char genomes[TEST_POPULATION_CAPACITY];
    evo_candidate_evaluation_t evaluations[TEST_POPULATION_CAPACITY];
    evo_population_t population;
} statistics_fixture_t;

static evo_fitness_t make_fitness(double total)
{
    return (evo_fitness_t){
        .correctness = total + 1.0,
        .performance = total + 2.0,
        .memory_use = total + 3.0,
        .reliability = total + 4.0,
        .maintainability = total + 5.0,
        .constraint_penalty = total + 6.0,
        .total = total,
    };
}

static void initialize_fixture(statistics_fixture_t *fixture,
                               size_t population_size,
                               uint64_t generation_index)
{
    assert(population_size != 0);
    assert(population_size <= TEST_POPULATION_CAPACITY);
    *fixture = (statistics_fixture_t){0};
    fixture->population.genomes = fixture->genomes;
    fixture->population.evaluations = fixture->evaluations;
    fixture->population.population_size = population_size;
    fixture->population.genome_size = 1;
    fixture->population.storage_bytes = population_size;
    fixture->population.evaluation_bytes =
        population_size * sizeof(evo_candidate_evaluation_t);
    fixture->population.initialized = generation_index == UINT64_C(0);
    fixture->population.source_generation =
        generation_index == UINT64_C(0)
            ? UINT64_C(0)
            : generation_index - UINT64_C(1);
    fixture->population.evaluated = true;
}

static void add_valid_candidate(statistics_fixture_t *fixture,
                                size_t index,
                                double total)
{
    evo_candidate_evaluation_t *evaluation = NULL;

    assert(index < fixture->population.population_size);
    evaluation = &fixture->evaluations[index];
    assert(!evaluation->valid);
    evaluation->fitness = make_fitness(total);
    evaluation->valid = true;
    evaluation->evaluated = true;

    if (!fixture->population.has_best ||
        total > fixture->evaluations[fixture->population.best_index]
                    .fitness.total) {
        fixture->population.best_index = index;
        fixture->population.has_best = true;
    }
    ++fixture->population.valid_count;
}

static void poison_invalid_candidate(statistics_fixture_t *fixture,
                                     size_t index)
{
    assert(index < fixture->population.population_size);
    assert(!fixture->evaluations[index].valid);
    assert(!fixture->evaluations[index].evaluated);
    fixture->evaluations[index].fitness = (evo_fitness_t){
        .correctness = NAN,
        .performance = INFINITY,
        .memory_use = -INFINITY,
        .reliability = NAN,
        .maintainability = INFINITY,
        .constraint_penalty = -INFINITY,
        .total = NAN,
    };
}

static void assert_fitness(const evo_fitness_t *fitness,
                           double correctness,
                           double performance,
                           double memory_use,
                           double reliability,
                           double maintainability,
                           double constraint_penalty,
                           double total)
{
    assert(fitness->correctness == correctness);
    assert(fitness->performance == performance);
    assert(fitness->memory_use == memory_use);
    assert(fitness->reliability == reliability);
    assert(fitness->maintainability == maintainability);
    assert(fitness->constraint_penalty == constraint_penalty);
    assert(fitness->total == total);
}

static void assert_statistics(
    const evo_generation_statistics_t *statistics,
    uint64_t generation_index,
    size_t population_size,
    size_t valid_count,
    size_t best_index,
    double best_total,
    const evo_fitness_t *expected_sums)
{
    assert(statistics->version == EVO_GENERATION_STATISTICS_VERSION);
    assert(statistics->generation_index == generation_index);
    assert(statistics->population_size == population_size);
    assert(statistics->valid_count == valid_count);
    assert(statistics->invalid_count == population_size - valid_count);
    assert(statistics->has_best == (valid_count != 0));
    assert(statistics->best_index == best_index);

    if (valid_count == 0) {
        assert_fitness(&statistics->best_fitness,
                       0.0,
                       0.0,
                       0.0,
                       0.0,
                       0.0,
                       0.0,
                       0.0);
    } else {
        assert(statistics->best_fitness.total == best_total);
    }

    assert_fitness(&statistics->fitness_sums,
                   expected_sums->correctness,
                   expected_sums->performance,
                   expected_sums->memory_use,
                   expected_sums->reliability,
                   expected_sums->maintainability,
                   expected_sums->constraint_penalty,
                   expected_sums->total);
}

static void test_even_generation_zero_vector(void)
{
    const evo_fitness_t expected_sums = {
        .correctness = 14.0,
        .performance = 18.0,
        .memory_use = 22.0,
        .reliability = 26.0,
        .maintainability = 30.0,
        .constraint_penalty = 34.0,
        .total = 10.0,
    };
    statistics_fixture_t fixture = {0};
    evo_generation_statistics_t statistics = {0};

    initialize_fixture(&fixture, 4, UINT64_C(0));
    add_valid_candidate(&fixture, 0, 1.0);
    add_valid_candidate(&fixture, 1, 4.0);
    add_valid_candidate(&fixture, 2, 3.0);
    add_valid_candidate(&fixture, 3, 2.0);

    assert(evo_generation_statistics_record(&fixture.population,
                                            UINT64_C(0),
                                            &statistics) == EVO_SUCCESS);
    assert_statistics(&statistics,
                      UINT64_C(0),
                      4,
                      4,
                      1,
                      4.0,
                      &expected_sums);
    assert(fixture.population.best_index == 1);
}

static void test_odd_generation_vector(void)
{
    const evo_fitness_t expected_sums = {
        .correctness = 12.0,
        .performance = 17.0,
        .memory_use = 22.0,
        .reliability = 27.0,
        .maintainability = 32.0,
        .constraint_penalty = 37.0,
        .total = 7.0,
    };
    statistics_fixture_t fixture = {0};
    evo_generation_statistics_t statistics = {0};

    initialize_fixture(&fixture, 5, UINT64_C(7));
    add_valid_candidate(&fixture, 0, 5.0);
    add_valid_candidate(&fixture, 1, -2.0);
    add_valid_candidate(&fixture, 2, 1.0);
    add_valid_candidate(&fixture, 3, 0.0);
    add_valid_candidate(&fixture, 4, 3.0);

    assert(evo_generation_statistics_record(&fixture.population,
                                            UINT64_C(7),
                                            &statistics) == EVO_SUCCESS);
    assert_statistics(&statistics,
                      UINT64_C(7),
                      5,
                      5,
                      0,
                      5.0,
                      &expected_sums);
}

static void test_one_member_vector(void)
{
    const evo_fitness_t expected_sums = {
        .correctness = 10.0,
        .performance = 11.0,
        .memory_use = 12.0,
        .reliability = 13.0,
        .maintainability = 14.0,
        .constraint_penalty = 15.0,
        .total = 9.0,
    };
    statistics_fixture_t fixture = {0};
    evo_generation_statistics_t statistics = {0};

    initialize_fixture(&fixture, 1, UINT64_C(3));
    add_valid_candidate(&fixture, 0, 9.0);
    assert(evo_generation_statistics_record(&fixture.population,
                                            UINT64_C(3),
                                            &statistics) == EVO_SUCCESS);
    assert_statistics(&statistics,
                      UINT64_C(3),
                      1,
                      1,
                      0,
                      9.0,
                      &expected_sums);
}

static void test_tied_vector_preserves_stable_best(void)
{
    const evo_fitness_t expected_sums = {
        .correctness = 19.0,
        .performance = 22.0,
        .memory_use = 25.0,
        .reliability = 28.0,
        .maintainability = 31.0,
        .constraint_penalty = 34.0,
        .total = 16.0,
    };
    statistics_fixture_t fixture = {0};
    evo_generation_statistics_t statistics = {0};

    initialize_fixture(&fixture, 3, UINT64_C(2));
    add_valid_candidate(&fixture, 0, 2.0);
    add_valid_candidate(&fixture, 1, 7.0);
    add_valid_candidate(&fixture, 2, 7.0);

    assert(fixture.population.best_index == 1);
    assert(evo_generation_statistics_record(&fixture.population,
                                            UINT64_C(2),
                                            &statistics) == EVO_SUCCESS);
    assert_statistics(&statistics,
                      UINT64_C(2),
                      3,
                      3,
                      1,
                      7.0,
                      &expected_sums);
    assert(fixture.population.best_index == 1);
}

static void test_mixed_validity_skips_invalid_fitness(void)
{
    const evo_fitness_t expected_sums = {
        .correctness = 15.0,
        .performance = 18.0,
        .memory_use = 21.0,
        .reliability = 24.0,
        .maintainability = 27.0,
        .constraint_penalty = 30.0,
        .total = 12.0,
    };
    statistics_fixture_t fixture = {0};
    evo_generation_statistics_t statistics = {0};

    initialize_fixture(&fixture, 5, UINT64_C(4));
    add_valid_candidate(&fixture, 0, 1.0);
    poison_invalid_candidate(&fixture, 1);
    add_valid_candidate(&fixture, 2, 8.0);
    poison_invalid_candidate(&fixture, 3);
    add_valid_candidate(&fixture, 4, 3.0);

    assert(evo_generation_statistics_record(&fixture.population,
                                            UINT64_C(4),
                                            &statistics) == EVO_SUCCESS);
    assert_statistics(&statistics,
                      UINT64_C(4),
                      5,
                      3,
                      2,
                      8.0,
                      &expected_sums);
}

static void test_all_invalid_terminal_vector(void)
{
    const evo_fitness_t expected_sums = {0};
    statistics_fixture_t fixture = {0};
    evo_generation_statistics_t statistics = {0};

    initialize_fixture(&fixture, 4, UINT64_C(6));
    for (size_t index = 0; index < 4; ++index) {
        poison_invalid_candidate(&fixture, index);
    }

    assert(evo_generation_statistics_record(&fixture.population,
                                            UINT64_C(6),
                                            &statistics) == EVO_SUCCESS);
    assert_statistics(&statistics,
                      UINT64_C(6),
                      4,
                      0,
                      0,
                      0.0,
                      &expected_sums);
}

static void test_rejections_preserve_output(void)
{
    statistics_fixture_t fixture = {0};
    evo_generation_statistics_t statistics = {.version = UINT32_C(99)};

    initialize_fixture(&fixture, 2, UINT64_C(1));
    add_valid_candidate(&fixture, 0, 1.0);
    add_valid_candidate(&fixture, 1, 2.0);

    assert(evo_generation_statistics_record(NULL,
                                            UINT64_C(1),
                                            &statistics) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(statistics.version == UINT32_C(99));
    assert(evo_generation_statistics_record(&fixture.population,
                                            UINT64_C(1),
                                            NULL) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_generation_statistics_record(&fixture.population,
                                            UINT64_C(2),
                                            &statistics) == EVO_ERROR_STATE);
    assert(statistics.version == UINT32_C(99));

    fixture.population.valid_count = 1;
    assert(evo_generation_statistics_record(&fixture.population,
                                            UINT64_C(1),
                                            &statistics) == EVO_ERROR_STATE);
    assert(statistics.version == UINT32_C(99));
}

static void test_non_finite_valid_or_aggregate_rejects(void)
{
    statistics_fixture_t fixture = {0};
    evo_generation_statistics_t statistics = {.version = UINT32_C(77)};

    initialize_fixture(&fixture, 1, UINT64_C(0));
    add_valid_candidate(&fixture, 0, 1.0);
    fixture.evaluations[0].fitness.total = NAN;
    assert(evo_generation_statistics_record(&fixture.population,
                                            UINT64_C(0),
                                            &statistics) ==
           EVO_ERROR_EVALUATION);
    assert(statistics.version == UINT32_C(77));

    initialize_fixture(&fixture, 2, UINT64_C(0));
    add_valid_candidate(&fixture, 0, DBL_MAX);
    add_valid_candidate(&fixture, 1, DBL_MAX);
    assert(evo_generation_statistics_record(&fixture.population,
                                            UINT64_C(0),
                                            &statistics) ==
           EVO_ERROR_EVALUATION);
    assert(statistics.version == UINT32_C(77));
}

int main(void)
{
    test_even_generation_zero_vector();
    test_odd_generation_vector();
    test_one_member_vector();
    test_tied_vector_preserves_stable_best();
    test_mixed_validity_skips_invalid_fitness();
    test_all_invalid_terminal_vector();
    test_rejections_preserve_output();
    test_non_finite_valid_or_aggregate_rejects();
    return 0;
}
