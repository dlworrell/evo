#include "../common/reference_adapter.h"

typedef struct compiler_fixture {
    unsigned int baseline_runtime;
    unsigned int baseline_size;
    unsigned int correctness_cases;
} compiler_fixture_t;

static const compiler_fixture_t compiler_fixture = {
    .baseline_runtime = 1000,
    .baseline_size = 400,
    .correctness_cases = 25,
};

static void compiler_decode(const unsigned char *bytes,
                            unsigned int *optimization,
                            unsigned int *unroll,
                            unsigned int *inlining,
                            unsigned int *vectorization)
{
    *optimization = (unsigned int)bytes[0] % 4U;
    *unroll = (unsigned int)bytes[1] % 5U;
    *inlining = (unsigned int)bytes[2] % 4U;
    *vectorization = (unsigned int)bytes[3] % 2U;
}

static bool compiler_is_valid(const void *genome, void *context)
{
    unsigned int optimization = 0;
    unsigned int unroll = 0;
    unsigned int inlining = 0;
    unsigned int vectorization = 0;

    (void)context;
    compiler_decode(genome,
                    &optimization,
                    &unroll,
                    &inlining,
                    &vectorization);
    return !(optimization == 0U && vectorization == 1U) &&
           !(unroll == 4U && inlining == 0U);
}

static evo_fitness_t compiler_evaluate(const void *genome, void *context)
{
    const compiler_fixture_t *fixture = context;
    unsigned int optimization = 0;
    unsigned int unroll = 0;
    unsigned int inlining = 0;
    unsigned int vectorization = 0;
    unsigned int performance = 0;
    unsigned int size_growth = 0;
    unsigned int penalty = 0;

    compiler_decode(genome,
                    &optimization,
                    &unroll,
                    &inlining,
                    &vectorization);
    performance = 300U * optimization + 75U * unroll +
                  60U * inlining + 200U * vectorization;
    size_growth = 40U * unroll + 25U * inlining +
                  30U * vectorization;
    penalty = size_growth > 180U ? (size_growth - 180U) * 3U : 0U;

    return (evo_fitness_t){
        .correctness = (double)(fixture->correctness_cases * 40U),
        .performance = (double)(fixture->baseline_runtime + performance),
        .memory_use = (double)(fixture->baseline_size + size_growth),
        .reliability = 100.0,
        .maintainability = 100.0,
        .constraint_penalty = (double)penalty,
        .total = (double)(1200U + performance - penalty),
    };
}

int main(void)
{
    static const adapter_definition_t definition = {
        .adapter_id = "compiler-options",
        .domain = "bounded compiler build-configuration search",
        .fixture_id = "compiler-option-model-v1",
        .fixture_json =
            "{\"baseline_runtime_units\":1000,\"baseline_size_units\":400,"
            "\"correctness_cases\":25,"
            "\"parameters\":[\"optimization-level\",\"unroll-level\","
            "\"inline-level\",\"vectorization-enabled\"],"
            "\"hard_constraints\":["
            "\"vectorization requires optimization level above zero\","
            "\"maximum unroll cannot pair with disabled inlining\"],"
            "\"soft_penalty\":\"3 per modeled size unit above 180\"}",
        .limitation =
            "searches a deterministic option model only; it does not parse, analyze, transform, compile, or emit C source",
        .random_seed = UINT64_C(55002),
        .problem_identity = UINT64_C(0x5502000000000001),
        .context_identity = UINT64_C(0),
        .evaluate = compiler_evaluate,
        .is_valid = compiler_is_valid,
        .model = &compiler_fixture,
        .stagnation_patience = 3,
    };

    return adapter_reference_main(&definition);
}
