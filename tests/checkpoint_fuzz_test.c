#include "catalyst/evo/evo.h"

#include <assert.h>
#include <stdint.h>

enum {
    FUZZ_POPULATION_SIZE = 2,
    FUZZ_GENOME_SIZE = 4,
    FUZZ_CAPACITY = 4096,
    FUZZ_RANDOM_CASES = 2048
};

static unsigned char retained_checkpoint[FUZZ_CAPACITY];
static size_t retained_checkpoint_size;

static evo_fitness_t evaluate_genome(const void *genome, void *context)
{
    const unsigned char *bytes = genome;

    (void)context;
    return (evo_fitness_t){
        .correctness = (double)bytes[0],
        .total = (double)bytes[0],
    };
}

static void retain_checkpoint(const void *checkpoint,
                              size_t checkpoint_size,
                              const evo_checkpoint_view_t *view,
                              void *context)
{
    (void)context;
    assert(view->current_generation == 0);
    assert(checkpoint_size <= sizeof(retained_checkpoint));
    for (size_t index = 0; index < checkpoint_size; ++index) {
        retained_checkpoint[index] =
            ((const unsigned char *)checkpoint)[index];
    }
    retained_checkpoint_size = checkpoint_size;
}

static uint64_t next_random(uint64_t *state)
{
    uint64_t value = *state;

    value ^= value << 13;
    value ^= value >> 7;
    value ^= value << 17;
    *state = value;
    return value;
}

static void create_seed_checkpoint(void)
{
    unsigned char checkpoint_buffer[FUZZ_CAPACITY] = {0};
    const evo_problem_t problem = {
        .genome_size = FUZZ_GENOME_SIZE,
        .evaluate = evaluate_genome,
        .checkpoint_problem_identity = UINT64_C(0x51f02251f02251f0),
    };
    const evo_config_t config = {
        .population_size = FUZZ_POPULATION_SIZE,
        .generation_limit = 0,
        .tournament_size = 2,
        .crossover_rate = 0.0,
        .mutation_rate = 0.0,
        .random_seed = UINT64_C(0x5100f022),
        .max_genome_bytes = FUZZ_GENOME_SIZE,
        .max_population_bytes =
            FUZZ_POPULATION_SIZE * FUZZ_GENOME_SIZE,
        .max_evaluation_bytes = SIZE_MAX,
        .max_child_population_bytes =
            FUZZ_POPULATION_SIZE * FUZZ_GENOME_SIZE,
        .max_diversity_work = SIZE_MAX,
        .max_checkpoint_bytes = FUZZ_CAPACITY,
        .checkpoint_buffer = checkpoint_buffer,
        .checkpoint_buffer_size = sizeof(checkpoint_buffer),
        .checkpoint_observer = retain_checkpoint,
        .checkpoint_context_identity = UINT64_C(0x51f0c051f0c051f0),
    };
    evo_result_t result = {0};

    assert(evo_run(&problem, &config, NULL, &result) == EVO_SUCCESS);
    assert(retained_checkpoint_size != 0);
    evo_result_destroy(&result);
}

static void test_every_truncation_and_single_bit_corruption(void)
{
    unsigned char candidate[FUZZ_CAPACITY] = {0};
    evo_checkpoint_view_t view = {0};

    assert(evo_checkpoint_inspect(retained_checkpoint,
                                  retained_checkpoint_size,
                                  FUZZ_CAPACITY,
                                  &view) == EVO_SUCCESS);
    for (size_t size = 0; size < retained_checkpoint_size; ++size) {
        assert(evo_checkpoint_inspect(retained_checkpoint,
                                      size,
                                      FUZZ_CAPACITY,
                                      &view) != EVO_SUCCESS);
    }
    for (size_t offset = 0; offset < retained_checkpoint_size; ++offset) {
        for (size_t index = 0; index < retained_checkpoint_size; ++index) {
            candidate[index] = retained_checkpoint[index];
        }
        candidate[offset] ^= UINT8_C(1);
        assert(evo_checkpoint_inspect(candidate,
                                      retained_checkpoint_size,
                                      FUZZ_CAPACITY,
                                      &view) != EVO_SUCCESS);
    }
}

static void test_deterministic_random_inputs(void)
{
    unsigned char bytes[512] = {0};
    evo_checkpoint_view_t view = {0};
    uint64_t random_state = UINT64_C(0x51f0f0227a11c0de);

    for (size_t test_case = 0; test_case < FUZZ_RANDOM_CASES; ++test_case) {
        const size_t size =
            (size_t)(next_random(&random_state) % sizeof(bytes));

        for (size_t index = 0; index < size; ++index) {
            bytes[index] = (unsigned char)next_random(&random_state);
        }
        assert(evo_checkpoint_inspect(bytes,
                                      size,
                                      sizeof(bytes),
                                      &view) != EVO_SUCCESS);
    }
}

int main(void)
{
    create_seed_checkpoint();
    test_every_truncation_and_single_bit_corruption();
    test_deterministic_random_inputs();
    return 0;
}
