#include "internal/child_evaluation.h"

#include "internal/child_pair.h"
#include "internal/child_single.h"
#include "internal/child_tail.h"
#include "internal/elite.h"

#include <assert.h>
#include <math.h>

enum {
    TEST_POPULATION_CAPACITY = 5,
    TEST_GENOME_SIZE = 2,
    TEST_STORAGE_CAPACITY =
        TEST_POPULATION_CAPACITY * TEST_GENOME_SIZE,
    TEST_EVENT_CAPACITY = TEST_POPULATION_CAPACITY * 2
};

typedef struct evaluation_event {
    char operation;
    size_t index;
} evaluation_event_t;

typedef struct evaluation_fixture {
    evo_problem_t problem;
    evo_config_t config;
    evo_population_t parents;
    evo_population_t children;
    bool child_validity[TEST_POPULATION_CAPACITY];
    evo_fitness_t child_fitness[TEST_POPULATION_CAPACITY];
    const unsigned char *evaluation_base;
    evaluation_event_t events[TEST_EVENT_CAPACITY];
    size_t event_count;
    size_t validation_calls;
    size_t evaluation_calls;
    size_t initialization_calls;
    size_t crossover_calls;
    size_t mutation_calls;
    uint64_t source_generation;
    bool capture_child_evaluation;
} evaluation_fixture_t;

typedef struct unevaluated_snapshot {
    evo_population_t metadata;
    unsigned char genomes[TEST_STORAGE_CAPACITY];
} unevaluated_snapshot_t;

typedef struct completed_snapshot {
    evo_population_t metadata;
    unsigned char genomes[TEST_STORAGE_CAPACITY];
    evo_candidate_evaluation_t
        evaluations[TEST_POPULATION_CAPACITY];
} completed_snapshot_t;

static evo_fitness_t fitness_with_total(double total)
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

static void assert_fitness_equal(const evo_fitness_t *left,
                                 const evo_fitness_t *right)
{
    assert(left->correctness == right->correctness);
    assert(left->performance == right->performance);
    assert(left->memory_use == right->memory_use);
    assert(left->reliability == right->reliability);
    assert(left->maintainability == right->maintainability);
    assert(left->constraint_penalty == right->constraint_penalty);
    assert(left->total == right->total);
}

static size_t child_index(const void *genome,
                          const evaluation_fixture_t *fixture)
{
    const unsigned char *bytes = genome;
    const size_t offset =
        (size_t)(bytes - fixture->evaluation_base);

    assert(fixture->evaluation_base != NULL);
    assert(offset % TEST_GENOME_SIZE == 0);
    assert(offset / TEST_GENOME_SIZE <
           fixture->config.population_size);
    return offset / TEST_GENOME_SIZE;
}

static void record_event(evaluation_fixture_t *fixture,
                         char operation,
                         size_t index)
{
    assert(fixture->event_count < TEST_EVENT_CAPACITY);
    fixture->events[fixture->event_count].operation = operation;
    fixture->events[fixture->event_count].index = index;
    ++fixture->event_count;
}

static void deterministic_initializer(void *genome, void *opaque)
{
    evaluation_fixture_t *fixture = opaque;

    (void)genome;
    ++fixture->initialization_calls;
}

static void deterministic_crossover(const void *parent_a,
                                    const void *parent_b,
                                    void *child_a,
                                    void *child_b,
                                    void *opaque)
{
    const unsigned char *source_a = parent_a;
    const unsigned char *source_b = parent_b;
    unsigned char *destination_a = child_a;
    unsigned char *destination_b = child_b;
    evaluation_fixture_t *fixture = opaque;

    ++fixture->crossover_calls;
    for (size_t offset = 0; offset < TEST_GENOME_SIZE; ++offset) {
        destination_a[offset] = source_a[offset];
        destination_b[offset] = source_b[offset];
    }
}

static void deterministic_mutator(void *genome,
                                  double mutation_rate,
                                  void *opaque)
{
    unsigned char *bytes = genome;
    evaluation_fixture_t *fixture = opaque;

    assert(mutation_rate == 1.0);
    ++fixture->mutation_calls;
    bytes[0] ^= UINT8_C(0x5a);
}

static bool deterministic_validator(const void *genome, void *opaque)
{
    evaluation_fixture_t *fixture = opaque;

    if (!fixture->capture_child_evaluation) {
        return true;
    }

    const size_t index = child_index(genome, fixture);
    record_event(fixture, 'V', index);
    ++fixture->validation_calls;
    return fixture->child_validity[index];
}

static evo_fitness_t deterministic_evaluator(const void *genome,
                                             void *opaque)
{
    evaluation_fixture_t *fixture = opaque;

    if (!fixture->capture_child_evaluation) {
        const unsigned char *bytes = genome;
        return fitness_with_total((double)bytes[0]);
    }

    const size_t index = child_index(genome, fixture);
    record_event(fixture, 'E', index);
    ++fixture->evaluation_calls;
    return fixture->child_fitness[index];
}

static void fixture_initialize(evaluation_fixture_t *fixture,
                               size_t population_size,
                               uint64_t source_generation)
{
    evo_child_pair_evidence_t pair = {0};
    evo_child_tail_evidence_t tail = {0};
    evo_elite_evidence_t elite = {0};

    assert(population_size != 0);
    assert(population_size <= TEST_POPULATION_CAPACITY);
    *fixture = (evaluation_fixture_t){0};
    fixture->problem = (evo_problem_t){
        .genome_size = TEST_GENOME_SIZE,
        .initialize = deterministic_initializer,
        .mutate = deterministic_mutator,
        .crossover = deterministic_crossover,
        .evaluate = deterministic_evaluator,
        .is_valid = deterministic_validator,
    };
    fixture->config = (evo_config_t){
        .population_size = population_size,
        .generation_limit = 8,
        .tournament_size = population_size,
        .crossover_rate = 1.0,
        .mutation_rate = 1.0,
        .random_seed = 42,
        .max_genome_bytes = TEST_GENOME_SIZE,
        .max_population_bytes =
            population_size * TEST_GENOME_SIZE,
        .max_evaluation_bytes =
            population_size *
            sizeof(evo_candidate_evaluation_t),
        .max_child_population_bytes =
            population_size * TEST_GENOME_SIZE,
        .max_diversity_work = SIZE_MAX,
    };
    fixture->source_generation = source_generation;

    assert(evo_population_create(&fixture->problem,
                                 &fixture->config,
                                 &fixture->parents) == EVO_SUCCESS);
    assert(evo_population_initialize(&fixture->problem,
                                     &fixture->config,
                                     fixture,
                                     &fixture->parents) == EVO_SUCCESS);
    assert(evo_population_evaluate(&fixture->problem,
                                   &fixture->config,
                                   fixture,
                                   &fixture->parents) == EVO_SUCCESS);
    assert(evo_child_population_create(&fixture->problem,
                                       &fixture->config,
                                       &fixture->parents,
                                       &fixture->children) == EVO_SUCCESS);

    for (size_t pair_index = 0;
         pair_index < population_size / 2;
         ++pair_index) {
        assert(evo_child_pair_produce(&fixture->problem,
                                      &fixture->config,
                                      fixture,
                                      &fixture->parents,
                                      source_generation,
                                      pair_index,
                                      &fixture->children,
                                      &pair) == EVO_SUCCESS);
    }

    if (population_size % 2 != 0) {
        assert(evo_child_tail_produce(&fixture->problem,
                                      &fixture->config,
                                      &fixture->parents,
                                      source_generation,
                                      &fixture->children,
                                      &tail) == EVO_SUCCESS);
    } else {
        assert(evo_elite_population_complete(&fixture->problem,
                                             &fixture->config,
                                             &fixture->parents,
                                             source_generation,
                                             &fixture->children,
                                             &elite) == EVO_SUCCESS);
    }

    assert(fixture->children.produced_count == population_size);
    fixture->evaluation_base = fixture->children.genomes;
    fixture->event_count = 0;
    fixture->validation_calls = 0;
    fixture->evaluation_calls = 0;
}

static void fixture_destroy(evaluation_fixture_t *fixture)
{
    evo_population_destroy(&fixture->children);
    evo_population_destroy(&fixture->parents);
}

static evo_child_evaluation_evidence_t sentinel_evidence(void)
{
    return (evo_child_evaluation_evidence_t){
        .population_size = 31,
        .evaluation_bytes = 37,
        .valid_count = 41,
        .best_index = 43,
        .elite_count = 45,
        .elite_source_valid_count = 46,
        .source_generation = 47,
        .operator_seed_schedule_version = 53,
        .selection_policy_version = 59,
        .selection_policy = EVO_SELECTION_RANK,
        .byte_operator_policy_version = 61,
        .crossover_operator = EVO_CROSSOVER_BYTE_UNIFORM,
        .mutation_operator = EVO_MUTATION_BYTE_XOR,
        .mutation_rate_used = 0.625,
        .odd_child_policy_version = 67,
        .elite_policy_version = 71,
        .singleton_child_policy_version = 73,
        .policy_version = 79,
        .has_best = true,
        .elite_count_explicit = true,
        .complete = true,
    };
}

static void assert_evidence_equal(
    const evo_child_evaluation_evidence_t *left,
    const evo_child_evaluation_evidence_t *right)
{
    assert(left->population_size == right->population_size);
    assert(left->evaluation_bytes == right->evaluation_bytes);
    assert(left->valid_count == right->valid_count);
    assert(left->best_index == right->best_index);
    assert(left->elite_count == right->elite_count);
    assert(left->elite_source_valid_count ==
           right->elite_source_valid_count);
    assert(left->source_generation == right->source_generation);
    assert(left->operator_seed_schedule_version ==
           right->operator_seed_schedule_version);
    assert(left->selection_policy_version ==
           right->selection_policy_version);
    assert(left->selection_policy == right->selection_policy);
    assert(left->byte_operator_policy_version ==
           right->byte_operator_policy_version);
    assert(left->crossover_operator == right->crossover_operator);
    assert(left->mutation_operator == right->mutation_operator);
    assert(left->mutation_rate_used == right->mutation_rate_used);
    assert(left->odd_child_policy_version ==
           right->odd_child_policy_version);
    assert(left->elite_policy_version == right->elite_policy_version);
    assert(left->singleton_child_policy_version ==
           right->singleton_child_policy_version);
    assert(left->fitness_comparison_policy_version ==
           right->fitness_comparison_policy_version);
    assert(left->diversity_policy_version ==
           right->diversity_policy_version);
    assert(left->diversity_metric_version ==
           right->diversity_metric_version);
    assert(left->policy_version == right->policy_version);
    assert(left->has_best == right->has_best);
    assert(left->elite_count_explicit ==
           right->elite_count_explicit);
    assert(left->complete == right->complete);
}

static void snapshot_unevaluated(
    const evo_population_t *population,
    unevaluated_snapshot_t *snapshot)
{
    assert(population->storage_bytes <= TEST_STORAGE_CAPACITY);
    snapshot->metadata = *population;
    for (size_t index = 0; index < population->storage_bytes; ++index) {
        snapshot->genomes[index] = population->genomes[index];
    }
}

static void assert_unevaluated_unchanged(
    const evo_population_t *population,
    const unevaluated_snapshot_t *snapshot)
{
    const evo_population_t *before = &snapshot->metadata;

    assert(population->genomes == before->genomes);
    assert(population->evaluations == before->evaluations);
    assert(population->population_size == before->population_size);
    assert(population->genome_size == before->genome_size);
    assert(population->storage_bytes == before->storage_bytes);
    assert(population->evaluation_bytes == before->evaluation_bytes);
    assert(population->valid_count == before->valid_count);
    assert(population->best_index == before->best_index);
    assert(population->produced_count == before->produced_count);
    assert(population->elite_count == before->elite_count);
    assert(population->elite_source_valid_count ==
           before->elite_source_valid_count);
    assert(population->initialization_seed ==
           before->initialization_seed);
    assert(population->source_generation == before->source_generation);
    assert(population->rng_algorithm_version ==
           before->rng_algorithm_version);
    assert(population->operator_seed_schedule_version ==
           before->operator_seed_schedule_version);
    assert(population->selection_policy_version ==
           before->selection_policy_version);
    assert(population->selection_policy == before->selection_policy);
    assert(population->mutation_rate_used == before->mutation_rate_used);
    assert(population->odd_child_policy_version ==
           before->odd_child_policy_version);
    assert(population->elite_policy_version ==
           before->elite_policy_version);
    assert(population->singleton_child_policy_version ==
           before->singleton_child_policy_version);
    assert(population->fitness_comparison_policy_version ==
           before->fitness_comparison_policy_version);
    assert(population->diversity_policy_version ==
           before->diversity_policy_version);
    assert(population->diversity_metric_version ==
           before->diversity_metric_version);
    assert(population->diversity_pair_count ==
           before->diversity_pair_count);
    assert(population->diversity_work_units ==
           before->diversity_work_units);
    assert(population->diversity == before->diversity);
    assert(population->diversity_uses_domain_distance ==
           before->diversity_uses_domain_distance);
    assert(population->initialized == before->initialized);
    assert(population->has_best == before->has_best);
    assert(population->evaluated == before->evaluated);
    assert(population->elite_count_explicit ==
           before->elite_count_explicit);

    for (size_t index = 0; index < population->storage_bytes; ++index) {
        assert(population->genomes[index] == snapshot->genomes[index]);
    }
}

static void snapshot_completed(const evo_population_t *population,
                               completed_snapshot_t *snapshot)
{
    assert(population->storage_bytes <= TEST_STORAGE_CAPACITY);
    assert(population->population_size <= TEST_POPULATION_CAPACITY);
    assert(population->evaluations != NULL);
    snapshot->metadata = *population;
    for (size_t index = 0; index < population->storage_bytes; ++index) {
        snapshot->genomes[index] = population->genomes[index];
    }
    for (size_t index = 0; index < population->population_size; ++index) {
        snapshot->evaluations[index] = population->evaluations[index];
    }
}

static void assert_completed_unchanged(
    const evo_population_t *population,
    const completed_snapshot_t *snapshot)
{
    const evo_population_t *before = &snapshot->metadata;

    assert(population->genomes == before->genomes);
    assert(population->evaluations == before->evaluations);
    assert(population->population_size == before->population_size);
    assert(population->genome_size == before->genome_size);
    assert(population->storage_bytes == before->storage_bytes);
    assert(population->evaluation_bytes == before->evaluation_bytes);
    assert(population->valid_count == before->valid_count);
    assert(population->best_index == before->best_index);
    assert(population->produced_count == before->produced_count);
    assert(population->elite_count == before->elite_count);
    assert(population->elite_source_valid_count ==
           before->elite_source_valid_count);
    assert(population->initialization_seed ==
           before->initialization_seed);
    assert(population->source_generation == before->source_generation);
    assert(population->rng_algorithm_version ==
           before->rng_algorithm_version);
    assert(population->operator_seed_schedule_version ==
           before->operator_seed_schedule_version);
    assert(population->selection_policy_version ==
           before->selection_policy_version);
    assert(population->selection_policy == before->selection_policy);
    assert(population->odd_child_policy_version ==
           before->odd_child_policy_version);
    assert(population->elite_policy_version ==
           before->elite_policy_version);
    assert(population->singleton_child_policy_version ==
           before->singleton_child_policy_version);
    assert(population->fitness_comparison_policy_version ==
           before->fitness_comparison_policy_version);
    assert(population->diversity_policy_version ==
           before->diversity_policy_version);
    assert(population->diversity_metric_version ==
           before->diversity_metric_version);
    assert(population->diversity_pair_count ==
           before->diversity_pair_count);
    assert(population->diversity_work_units ==
           before->diversity_work_units);
    assert(population->diversity == before->diversity);
    assert(population->diversity_uses_domain_distance ==
           before->diversity_uses_domain_distance);
    assert(population->initialized == before->initialized);
    assert(population->has_best == before->has_best);
    assert(population->evaluated == before->evaluated);
    assert(population->elite_count_explicit ==
           before->elite_count_explicit);

    for (size_t index = 0; index < population->storage_bytes; ++index) {
        assert(population->genomes[index] == snapshot->genomes[index]);
    }
    for (size_t index = 0; index < population->population_size; ++index) {
        assert(population->evaluations[index].valid ==
               snapshot->evaluations[index].valid);
        assert(population->evaluations[index].evaluated ==
               snapshot->evaluations[index].evaluated);
        assert_fitness_equal(&population->evaluations[index].fitness,
                             &snapshot->evaluations[index].fitness);
    }
}

static void assert_event(const evaluation_fixture_t *fixture,
                         size_t event_index,
                         char operation,
                         size_t candidate_index)
{
    assert(event_index < fixture->event_count);
    assert(fixture->events[event_index].operation == operation);
    assert(fixture->events[event_index].index == candidate_index);
}

static void test_odd_child_order_tie_and_completed_authority(void)
{
    evaluation_fixture_t fixture = {0};
    evo_child_evaluation_evidence_t evidence = {0};
    evo_population_t next_children = {0};
    unevaluated_snapshot_t before = {0};
    size_t validated_valid_count = 0;
    size_t initializer_calls = 0;
    size_t crossover_calls = 0;
    size_t mutation_calls = 0;

    fixture_initialize(&fixture, 5, 7);
    fixture.child_validity[0] = true;
    fixture.child_validity[1] = false;
    fixture.child_validity[2] = true;
    fixture.child_validity[3] = true;
    fixture.child_validity[4] = true;
    fixture.child_fitness[0] = fitness_with_total(1.0);
    fixture.child_fitness[1] = fitness_with_total(1000.0);
    fixture.child_fitness[2] = fitness_with_total(3.0);
    fixture.child_fitness[3] = fitness_with_total(3.0);
    fixture.child_fitness[4] = fitness_with_total(2.0);
    snapshot_unevaluated(&fixture.children, &before);
    initializer_calls = fixture.initialization_calls;
    crossover_calls = fixture.crossover_calls;
    mutation_calls = fixture.mutation_calls;
    fixture.capture_child_evaluation = true;

    assert(evo_child_population_evaluate(&fixture.problem,
                                         &fixture.config,
                                         &fixture,
                                         fixture.source_generation,
                                         &fixture.children,
                                         &evidence) == EVO_SUCCESS);

    assert(fixture.validation_calls == 5);
    assert(fixture.evaluation_calls == 4);
    assert(fixture.event_count == 9);
    for (size_t index = 0; index < 5; ++index) {
        assert_event(&fixture, index, 'V', index);
    }
    assert_event(&fixture, 5, 'E', 0);
    assert_event(&fixture, 6, 'E', 2);
    assert_event(&fixture, 7, 'E', 3);
    assert_event(&fixture, 8, 'E', 4);
    assert(fixture.initialization_calls == initializer_calls);
    assert(fixture.crossover_calls == crossover_calls);
    assert(fixture.mutation_calls == mutation_calls);

    assert(fixture.children.evaluations != NULL);
    assert(fixture.children.valid_count == 4);
    assert(fixture.children.best_index == 2);
    assert(fixture.children.has_best);
    assert(fixture.children.evaluated);
    assert(!fixture.children.initialized);
    assert(fixture.children.produced_count == 5);
    assert(fixture.children.source_generation == 7);
    assert(fixture.children.rng_algorithm_version == 0);
    assert(fixture.children.operator_seed_schedule_version ==
           EVO_OPERATOR_SEED_SCHEDULE_VERSION);
    assert(fixture.children.selection_policy_version ==
           EVO_SELECTION_POLICY_VERSION);
    assert(fixture.children.selection_policy == EVO_SELECTION_TOURNAMENT);
    assert(fixture.children.byte_operator_policy_version ==
           EVO_BYTE_OPERATOR_POLICY_VERSION);
    assert(fixture.children.crossover_operator == EVO_CROSSOVER_CONSUMER);
    assert(fixture.children.mutation_operator == EVO_MUTATION_CONSUMER);
    assert(fixture.children.odd_child_policy_version ==
           EVO_ODD_CHILD_POLICY_VERSION);
    assert(fixture.children.elite_count == 1);
    assert(fixture.children.elite_source_valid_count == 5);
    assert(fixture.children.elite_policy_version ==
           EVO_ELITE_POLICY_VERSION);
    assert(fixture.children.singleton_child_policy_version == 0);
    assert(!fixture.children.elite_count_explicit);
    assert(fixture.children.fitness_comparison_policy_version ==
           EVO_FITNESS_COMPARISON_POLICY_VERSION);
    for (size_t index = 0; index < fixture.children.storage_bytes;
         ++index) {
        assert(fixture.children.genomes[index] == before.genomes[index]);
    }

    assert(!fixture.children.evaluations[1].valid);
    assert(!fixture.children.evaluations[1].evaluated);
    assert_fitness_equal(&fixture.children.evaluations[1].fitness,
                         &(evo_fitness_t){0});
    assert(evidence.population_size == 5);
    assert(evidence.evaluation_bytes ==
           fixture.config.max_evaluation_bytes);
    assert(evidence.valid_count == 4);
    assert(evidence.best_index == 2);
    assert(evidence.source_generation == 7);
    assert(evidence.operator_seed_schedule_version ==
           EVO_OPERATOR_SEED_SCHEDULE_VERSION);
    assert(evidence.selection_policy_version ==
           EVO_SELECTION_POLICY_VERSION);
    assert(evidence.selection_policy == EVO_SELECTION_TOURNAMENT);
    assert(evidence.byte_operator_policy_version ==
           EVO_BYTE_OPERATOR_POLICY_VERSION);
    assert(evidence.crossover_operator == EVO_CROSSOVER_CONSUMER);
    assert(evidence.mutation_operator == EVO_MUTATION_CONSUMER);
    assert(evidence.mutation_rate_used == fixture.config.mutation_rate);
    assert(evidence.odd_child_policy_version ==
           EVO_ODD_CHILD_POLICY_VERSION);
    assert(evidence.elite_count == 1);
    assert(evidence.elite_source_valid_count == 5);
    assert(evidence.elite_policy_version == EVO_ELITE_POLICY_VERSION);
    assert(evidence.singleton_child_policy_version == 0);
    assert(!evidence.elite_count_explicit);
    assert(evidence.fitness_comparison_policy_version ==
           EVO_FITNESS_COMPARISON_POLICY_VERSION);
    assert(evidence.diversity_policy_version ==
           EVO_DIVERSITY_POLICY_VERSION);
    assert(evidence.diversity_metric_version ==
           EVO_BYTE_DIVERSITY_METRIC_VERSION);
    assert(evidence.policy_version ==
           EVO_CHILD_EVALUATION_POLICY_VERSION);
    assert(evidence.has_best);
    assert(evidence.complete);

    assert(evo_population_validate_completed(
        &fixture.config,
        &fixture.children,
        &validated_valid_count));
    assert(validated_valid_count == 4);
    assert(evo_child_population_create(&fixture.problem,
                                       &fixture.config,
                                       &fixture.children,
                                       &next_children) == EVO_SUCCESS);
    assert(next_children.genomes != fixture.children.genomes);
    assert(next_children.produced_count == 0);
    assert(!next_children.evaluated);
    evo_population_destroy(&next_children);
    fixture_destroy(&fixture);
}

static void test_even_all_invalid_child_completes_without_best(void)
{
    evaluation_fixture_t fixture = {0};
    evo_child_evaluation_evidence_t evidence = {0};
    size_t validated_valid_count = TEST_POPULATION_CAPACITY;

    fixture_initialize(&fixture, 4, 3);
    for (size_t index = 0; index < 4; ++index) {
        fixture.child_validity[index] = false;
        fixture.child_fitness[index] = fitness_with_total(100.0);
    }
    fixture.capture_child_evaluation = true;

    assert(evo_child_population_evaluate(&fixture.problem,
                                         &fixture.config,
                                         &fixture,
                                         fixture.source_generation,
                                         &fixture.children,
                                         &evidence) == EVO_SUCCESS);
    assert(fixture.validation_calls == 4);
    assert(fixture.evaluation_calls == 0);
    assert(fixture.children.valid_count == 0);
    assert(fixture.children.best_index == 0);
    assert(!fixture.children.has_best);
    assert(fixture.children.evaluated);
    assert(fixture.children.odd_child_policy_version == 0);
    assert(fixture.children.elite_count == 0);
    assert(fixture.children.elite_source_valid_count == 4);
    assert(fixture.children.elite_policy_version ==
           EVO_ELITE_POLICY_VERSION);
    assert(fixture.children.singleton_child_policy_version == 0);
    assert(!fixture.children.elite_count_explicit);
    assert(fixture.children.fitness_comparison_policy_version ==
           EVO_FITNESS_COMPARISON_POLICY_VERSION);
    assert(evidence.valid_count == 0);
    assert(evidence.best_index == 0);
    assert(!evidence.has_best);
    assert(evidence.odd_child_policy_version == 0);
    assert(evidence.elite_count == 0);
    assert(evidence.elite_source_valid_count == 4);
    assert(evidence.elite_policy_version == EVO_ELITE_POLICY_VERSION);
    assert(evidence.singleton_child_policy_version == 0);
    assert(!evidence.elite_count_explicit);
    assert(evidence.fitness_comparison_policy_version ==
           EVO_FITNESS_COMPARISON_POLICY_VERSION);
    assert(evo_population_validate_completed(
        &fixture.config,
        &fixture.children,
        &validated_valid_count));
    assert(validated_valid_count == 0);
    fixture_destroy(&fixture);
}

static void test_one_member_child_is_evaluated(void)
{
    evaluation_fixture_t fixture = {0};
    evo_child_evaluation_evidence_t evidence = {0};

    fixture_initialize(&fixture, 1, 0);
    fixture.child_validity[0] = true;
    fixture.child_fitness[0] = fitness_with_total(9.0);
    fixture.capture_child_evaluation = true;

    assert(fixture.children.produced_count == 1);
    assert(fixture.children.operator_seed_schedule_version ==
           EVO_OPERATOR_SEED_SCHEDULE_VERSION);
    assert(fixture.children.selection_policy_version ==
           EVO_SELECTION_POLICY_VERSION);
    assert(fixture.children.selection_policy == EVO_SELECTION_TOURNAMENT);
    assert(fixture.children.odd_child_policy_version ==
           EVO_ODD_CHILD_POLICY_VERSION);
    assert(fixture.children.elite_count == 1);
    assert(fixture.children.elite_source_valid_count == 1);
    assert(fixture.children.elite_policy_version ==
           EVO_ELITE_POLICY_VERSION);
    assert(fixture.children.singleton_child_policy_version == 0);
    assert(!fixture.children.elite_count_explicit);
    assert(fixture.children.fitness_comparison_policy_version == 0);
    assert(evo_child_population_evaluate(&fixture.problem,
                                         &fixture.config,
                                         &fixture,
                                         0,
                                         &fixture.children,
                                         &evidence) == EVO_SUCCESS);
    assert(fixture.validation_calls == 1);
    assert(fixture.evaluation_calls == 1);
    assert(fixture.children.valid_count == 1);
    assert(fixture.children.best_index == 0);
    assert(fixture.children.has_best);
    assert(fixture.children.fitness_comparison_policy_version ==
           EVO_FITNESS_COMPARISON_POLICY_VERSION);
    assert(evidence.source_generation == 0);
    assert(evidence.selection_policy_version ==
           EVO_SELECTION_POLICY_VERSION);
    assert(evidence.selection_policy == EVO_SELECTION_TOURNAMENT);
    assert(evidence.elite_count == 1);
    assert(evidence.elite_source_valid_count == 1);
    assert(evidence.elite_policy_version == EVO_ELITE_POLICY_VERSION);
    assert(evidence.singleton_child_policy_version == 0);
    assert(!evidence.elite_count_explicit);
    assert(evidence.fitness_comparison_policy_version ==
           EVO_FITNESS_COMPARISON_POLICY_VERSION);
    fixture_destroy(&fixture);
}

static void test_explicit_elite_provenance_is_evaluated(void)
{
    evaluation_fixture_t fixture = {0};
    evo_child_pair_evidence_t pair = {0};
    evo_child_single_evidence_t single = {0};
    evo_elite_evidence_t elite = {0};
    evo_child_evaluation_evidence_t evaluation = {0};

    fixture_initialize(&fixture, 5, 31);
    evo_population_destroy(&fixture.children);
    fixture.config.elite_count_enabled = true;
    fixture.config.elite_count = 2;

    assert(evo_child_population_create(&fixture.problem,
                                       &fixture.config,
                                       &fixture.parents,
                                       &fixture.children) == EVO_SUCCESS);
    assert(evo_child_pair_produce(&fixture.problem,
                                  &fixture.config,
                                  &fixture,
                                  &fixture.parents,
                                  fixture.source_generation,
                                  0,
                                  &fixture.children,
                                  &pair) == EVO_SUCCESS);
    assert(evo_child_single_produce(&fixture.problem,
                                    &fixture.config,
                                    &fixture,
                                    &fixture.parents,
                                    fixture.source_generation,
                                    &fixture.children,
                                    &single) == EVO_SUCCESS);
    assert(evo_elite_population_complete(&fixture.problem,
                                         &fixture.config,
                                         &fixture.parents,
                                         fixture.source_generation,
                                         &fixture.children,
                                         &elite) == EVO_SUCCESS);
    assert(single.child_index == 2);
    assert(single.selection_stream_index == 1);
    assert(elite.requested_count == 2);
    assert(elite.effective_count == 2);
    assert(elite.offspring_count == 3);

    for (size_t index = 0; index < 5; ++index) {
        fixture.child_validity[index] = true;
        fixture.child_fitness[index] =
            fitness_with_total((double)(index + 1));
    }
    fixture.evaluation_base = fixture.children.genomes;
    fixture.event_count = 0;
    fixture.validation_calls = 0;
    fixture.evaluation_calls = 0;
    fixture.capture_child_evaluation = true;
    assert(evo_child_population_evaluate(&fixture.problem,
                                         &fixture.config,
                                         &fixture,
                                         fixture.source_generation,
                                         &fixture.children,
                                         &evaluation) == EVO_SUCCESS);

    assert(evaluation.elite_count == 2);
    assert(evaluation.elite_source_valid_count == 5);
    assert(evaluation.elite_policy_version ==
           EVO_ELITE_POLICY_VERSION);
    assert(evaluation.singleton_child_policy_version ==
           EVO_SINGLETON_CHILD_POLICY_VERSION);
    assert(evaluation.elite_count_explicit);
    assert(evaluation.policy_version ==
           EVO_CHILD_EVALUATION_POLICY_VERSION);
    assert(fixture.children.elite_count == 2);
    assert(fixture.children.elite_source_valid_count == 5);
    assert(fixture.children.singleton_child_policy_version ==
           EVO_SINGLETON_CHILD_POLICY_VERSION);
    assert(fixture.children.elite_count_explicit);
    fixture_destroy(&fixture);
}

static void test_replay_is_byte_record_and_evidence_identical(void)
{
    evaluation_fixture_t first = {0};
    evaluation_fixture_t replay = {0};
    evo_child_evaluation_evidence_t first_evidence = {0};
    evo_child_evaluation_evidence_t replay_evidence = {0};

    fixture_initialize(&first, 5, 11);
    fixture_initialize(&replay, 5, 11);
    for (size_t index = 0; index < 5; ++index) {
        const bool valid = index != 1;
        const evo_fitness_t fitness =
            fitness_with_total((double)(index * 2));

        first.child_validity[index] = valid;
        replay.child_validity[index] = valid;
        first.child_fitness[index] = fitness;
        replay.child_fitness[index] = fitness;
    }
    first.capture_child_evaluation = true;
    replay.capture_child_evaluation = true;

    assert(evo_child_population_evaluate(&first.problem,
                                         &first.config,
                                         &first,
                                         11,
                                         &first.children,
                                         &first_evidence) == EVO_SUCCESS);
    assert(evo_child_population_evaluate(&replay.problem,
                                         &replay.config,
                                         &replay,
                                         11,
                                         &replay.children,
                                         &replay_evidence) == EVO_SUCCESS);
    assert_evidence_equal(&first_evidence, &replay_evidence);
    for (size_t index = 0; index < first.children.storage_bytes;
         ++index) {
        assert(first.children.genomes[index] ==
               replay.children.genomes[index]);
    }
    for (size_t index = 0; index < first.children.population_size;
         ++index) {
        assert(first.children.evaluations[index].valid ==
               replay.children.evaluations[index].valid);
        assert(first.children.evaluations[index].evaluated ==
               replay.children.evaluations[index].evaluated);
        assert_fitness_equal(&first.children.evaluations[index].fitness,
                             &replay.children.evaluations[index].fitness);
    }
    fixture_destroy(&replay);
    fixture_destroy(&first);
}

static void test_preflight_and_budget_rejections_preserve_state(void)
{
    evaluation_fixture_t fixture = {0};
    evo_child_evaluation_evidence_t evidence = sentinel_evidence();
    const evo_child_evaluation_evidence_t evidence_before = evidence;
    unevaluated_snapshot_t before = {0};

    fixture_initialize(&fixture, 5, 13);
    snapshot_unevaluated(&fixture.children, &before);
    fixture.capture_child_evaluation = true;

    assert(evo_child_population_evaluate(NULL,
                                         &fixture.config,
                                         &fixture,
                                         13,
                                         &fixture.children,
                                         &evidence) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_child_population_evaluate(&fixture.problem,
                                         NULL,
                                         &fixture,
                                         13,
                                         &fixture.children,
                                         &evidence) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_child_population_evaluate(&fixture.problem,
                                         &fixture.config,
                                         &fixture,
                                         13,
                                         NULL,
                                         &evidence) ==
           EVO_ERROR_INVALID_ARGUMENT);
    assert(evo_child_population_evaluate(&fixture.problem,
                                         &fixture.config,
                                         &fixture,
                                         13,
                                         &fixture.children,
                                         NULL) ==
           EVO_ERROR_INVALID_ARGUMENT);

    --fixture.children.produced_count;
    assert(evo_child_population_evaluate(&fixture.problem,
                                         &fixture.config,
                                         &fixture,
                                         13,
                                         &fixture.children,
                                         &evidence) == EVO_ERROR_STATE);
    ++fixture.children.produced_count;

    assert(evo_child_population_evaluate(&fixture.problem,
                                         &fixture.config,
                                         &fixture,
                                         14,
                                         &fixture.children,
                                         &evidence) == EVO_ERROR_STATE);

    fixture.children.operator_seed_schedule_version = 0;
    assert(evo_child_population_evaluate(&fixture.problem,
                                         &fixture.config,
                                         &fixture,
                                         13,
                                         &fixture.children,
                                         &evidence) == EVO_ERROR_STATE);
    fixture.children.operator_seed_schedule_version =
        EVO_OPERATOR_SEED_SCHEDULE_VERSION;

    fixture.children.selection_policy_version = 0;
    assert(evo_child_population_evaluate(&fixture.problem,
                                         &fixture.config,
                                         &fixture,
                                         13,
                                         &fixture.children,
                                         &evidence) == EVO_ERROR_STATE);
    fixture.children.selection_policy_version =
        EVO_SELECTION_POLICY_VERSION;

    fixture.children.selection_policy = EVO_SELECTION_RANK;
    assert(evo_child_population_evaluate(&fixture.problem,
                                         &fixture.config,
                                         &fixture,
                                         13,
                                         &fixture.children,
                                         &evidence) == EVO_ERROR_STATE);
    fixture.children.selection_policy = EVO_SELECTION_TOURNAMENT;

    fixture.children.odd_child_policy_version = 0;
    assert(evo_child_population_evaluate(&fixture.problem,
                                         &fixture.config,
                                         &fixture,
                                         13,
                                         &fixture.children,
                                         &evidence) == EVO_ERROR_STATE);
    fixture.children.odd_child_policy_version =
        EVO_ODD_CHILD_POLICY_VERSION;

    fixture.children.initialized = true;
    assert(evo_child_population_evaluate(&fixture.problem,
                                         &fixture.config,
                                         &fixture,
                                         13,
                                         &fixture.children,
                                         &evidence) == EVO_ERROR_STATE);
    fixture.children.initialized = false;

    fixture.children.rng_algorithm_version =
        EVO_RNG_ALGORITHM_VERSION;
    assert(evo_child_population_evaluate(&fixture.problem,
                                         &fixture.config,
                                         &fixture,
                                         13,
                                         &fixture.children,
                                         &evidence) == EVO_ERROR_STATE);
    fixture.children.rng_algorithm_version = 0;

    --fixture.children.storage_bytes;
    assert(evo_child_population_evaluate(&fixture.problem,
                                         &fixture.config,
                                         &fixture,
                                         13,
                                         &fixture.children,
                                         &evidence) == EVO_ERROR_STATE);
    ++fixture.children.storage_bytes;

    --fixture.config.max_evaluation_bytes;
    assert(evo_child_population_evaluate(&fixture.problem,
                                         &fixture.config,
                                         &fixture,
                                         13,
                                         &fixture.children,
                                         &evidence) ==
           EVO_ERROR_RESOURCE_LIMIT);
    ++fixture.config.max_evaluation_bytes;

    assert_evidence_equal(&evidence, &evidence_before);
    assert_unevaluated_unchanged(&fixture.children, &before);
    assert(fixture.validation_calls == 0);
    assert(fixture.evaluation_calls == 0);
    fixture_destroy(&fixture);
}

static void test_nonfinite_fitness_discards_provisional_records(void)
{
    evaluation_fixture_t fixture = {0};
    evo_child_evaluation_evidence_t evidence = sentinel_evidence();
    const evo_child_evaluation_evidence_t evidence_before = evidence;
    unevaluated_snapshot_t before = {0};

    fixture_initialize(&fixture, 3, 17);
    for (size_t index = 0; index < 3; ++index) {
        fixture.child_validity[index] = true;
        fixture.child_fitness[index] =
            fitness_with_total((double)index);
    }
    fixture.child_fitness[1].total = NAN;
    snapshot_unevaluated(&fixture.children, &before);
    fixture.capture_child_evaluation = true;

    assert(evo_child_population_evaluate(&fixture.problem,
                                         &fixture.config,
                                         &fixture,
                                         17,
                                         &fixture.children,
                                         &evidence) ==
           EVO_ERROR_EVALUATION);
    assert(fixture.validation_calls == 3);
    assert(fixture.evaluation_calls == 2);
    assert(fixture.event_count == 5);
    assert_event(&fixture, 0, 'V', 0);
    assert_event(&fixture, 1, 'V', 1);
    assert_event(&fixture, 2, 'V', 2);
    assert_event(&fixture, 3, 'E', 0);
    assert_event(&fixture, 4, 'E', 1);
    assert_evidence_equal(&evidence, &evidence_before);
    assert_unevaluated_unchanged(&fixture.children, &before);
    fixture_destroy(&fixture);
}

static void test_repeated_evaluation_rejects_without_callbacks(void)
{
    evaluation_fixture_t fixture = {0};
    evo_child_evaluation_evidence_t first = {0};
    evo_child_evaluation_evidence_t repeated = sentinel_evidence();
    const evo_child_evaluation_evidence_t repeated_before = repeated;
    completed_snapshot_t completed = {0};
    size_t validation_calls = 0;
    size_t evaluation_calls = 0;

    fixture_initialize(&fixture, 3, 19);
    for (size_t index = 0; index < 3; ++index) {
        fixture.child_validity[index] = true;
        fixture.child_fitness[index] =
            fitness_with_total((double)index);
    }
    fixture.capture_child_evaluation = true;
    assert(evo_child_population_evaluate(&fixture.problem,
                                         &fixture.config,
                                         &fixture,
                                         19,
                                         &fixture.children,
                                         &first) == EVO_SUCCESS);
    snapshot_completed(&fixture.children, &completed);
    validation_calls = fixture.validation_calls;
    evaluation_calls = fixture.evaluation_calls;

    assert(evo_child_population_evaluate(&fixture.problem,
                                         &fixture.config,
                                         &fixture,
                                         19,
                                         &fixture.children,
                                         &repeated) == EVO_ERROR_STATE);
    assert(fixture.validation_calls == validation_calls);
    assert(fixture.evaluation_calls == evaluation_calls);
    assert_evidence_equal(&repeated, &repeated_before);
    assert_completed_unchanged(&fixture.children, &completed);
    fixture_destroy(&fixture);
}

int main(void)
{
    test_odd_child_order_tie_and_completed_authority();
    test_even_all_invalid_child_completes_without_best();
    test_one_member_child_is_evaluated();
    test_explicit_elite_provenance_is_evaluated();
    test_replay_is_byte_record_and_evidence_identical();
    test_preflight_and_budget_rejections_preserve_state();
    test_nonfinite_fitness_discards_provisional_records();
    test_repeated_evaluation_rejects_without_callbacks();
    return 0;
}
