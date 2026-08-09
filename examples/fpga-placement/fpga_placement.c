#include "../common/reference_adapter.h"

typedef struct fpga_fixture {
    unsigned int grid_width;
    unsigned int grid_height;
    unsigned int lut_budget;
    unsigned int bram_budget;
    unsigned int target_period;
} fpga_fixture_t;

static const fpga_fixture_t fpga_fixture = {
    .grid_width = 8,
    .grid_height = 8,
    .lut_budget = 900,
    .bram_budget = 12,
    .target_period = 10,
};

static void fpga_decode(const unsigned char *bytes,
                        unsigned int *x,
                        unsigned int *y,
                        unsigned int *pipeline,
                        unsigned int *bank)
{
    *x = (unsigned int)bytes[0] % 8U;
    *y = (unsigned int)bytes[1] % 8U;
    *pipeline = (unsigned int)bytes[2] % 8U;
    *bank = (unsigned int)bytes[3] % 8U;
}

static bool fpga_is_valid(const void *genome, void *context)
{
    const fpga_fixture_t *fixture = context;
    unsigned int x = 0;
    unsigned int y = 0;
    unsigned int pipeline = 0;
    unsigned int bank = 0;

    fpga_decode(genome, &x, &y, &pipeline, &bank);
    return x < fixture->grid_width && y < fixture->grid_height &&
           x + y <= 10U && pipeline <= 6U && bank < fixture->bram_budget;
}

static evo_fitness_t fpga_evaluate(const void *genome, void *context)
{
    const fpga_fixture_t *fixture = context;
    unsigned int x = 0;
    unsigned int y = 0;
    unsigned int pipeline = 0;
    unsigned int bank = 0;
    unsigned int lut_use = 0;
    unsigned int period = 0;
    unsigned int timing_penalty = 0;
    unsigned int resource_penalty = 0;
    unsigned int placement_bonus = 0;
    unsigned int penalty = 0;

    fpga_decode(genome, &x, &y, &pipeline, &bank);
    lut_use = 300U + pipeline * 80U + bank * 35U;
    period = 18U - pipeline - (bank / 2U);
    timing_penalty =
        period > fixture->target_period
            ? (period - fixture->target_period) * 120U
            : 0U;
    resource_penalty =
        lut_use > fixture->lut_budget
            ? (lut_use - fixture->lut_budget) * 2U
            : 0U;
    placement_bonus =
        300U - ((x > 4U ? x - 4U : 4U - x) +
                (y > 4U ? y - 4U : 4U - y)) *
                   20U;
    penalty = timing_penalty + resource_penalty;

    return (evo_fitness_t){
        .correctness = 1000.0,
        .performance = (double)(pipeline * 220U + placement_bonus),
        .memory_use = (double)lut_use,
        .reliability = (double)(bank * 60U),
        .maintainability = 100.0,
        .constraint_penalty = (double)penalty,
        .total = (double)(1100U + pipeline * 220U + placement_bonus +
                          bank * 60U - penalty),
    };
}

int main(void)
{
    static const adapter_definition_t definition = {
        .adapter_id = "fpga-placement",
        .domain = "bounded FPGA placement-parameter exploration",
        .fixture_id = "fpga-grid-model-v1",
        .fixture_json =
            "{\"grid\":{\"width\":8,\"height\":8},"
            "\"budgets\":{\"lut\":900,\"bram\":12,"
            "\"target_period\":10},"
            "\"parameters\":[\"x\",\"y\",\"pipeline-depth\","
            "\"memory-bank\"],"
            "\"hard_constraint\":\"x + y <= 10, pipeline <= 6, and bank < 12\","
            "\"soft_penalties\":[\"timing above target\","
            "\"LUT use above budget\"]}",
        .limitation =
            "uses a small integer placement model and invokes no vendor placer, timing engine, bitstream generator, or hardware",
        .random_seed = UINT64_C(55004),
        .problem_identity = UINT64_C(0x5504000000000001),
        .context_identity = UINT64_C(0),
        .evaluate = fpga_evaluate,
        .is_valid = fpga_is_valid,
        .model = &fpga_fixture,
        .application_stop_generation = UINT64_C(3),
    };

    return adapter_reference_main(&definition);
}
