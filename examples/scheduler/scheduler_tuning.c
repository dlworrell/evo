#include "../common/reference_adapter.h"

typedef struct scheduler_fixture {
    unsigned int duration[6];
    unsigned int priority[6];
} scheduler_fixture_t;

static const scheduler_fixture_t scheduler_fixture = {
    .duration = {5, 2, 7, 3, 4, 6},
    .priority = {9, 5, 8, 4, 7, 6},
};

static void scheduler_decode(const unsigned char *bytes,
                             unsigned int *quantum,
                             unsigned int *lanes,
                             unsigned int *batch,
                             unsigned int *backoff)
{
    *quantum = 1U + ((unsigned int)bytes[0] % 8U);
    *lanes = 1U + ((unsigned int)bytes[1] % 8U);
    *batch = 1U + ((unsigned int)bytes[2] % 8U);
    *backoff = (unsigned int)bytes[3] % 5U;
}

static bool scheduler_is_valid(const void *genome, void *context)
{
    unsigned int quantum = 0;
    unsigned int lanes = 0;
    unsigned int batch = 0;
    unsigned int backoff = 0;

    (void)context;
    scheduler_decode(genome, &quantum, &lanes, &batch, &backoff);
    return lanes <= batch && quantum + backoff <= 10U;
}

static evo_fitness_t scheduler_evaluate(const void *genome, void *context)
{
    const scheduler_fixture_t *fixture = context;
    unsigned int quantum = 0;
    unsigned int lanes = 0;
    unsigned int batch = 0;
    unsigned int backoff = 0;
    unsigned int workload = 0;
    unsigned int urgency = 0;
    unsigned int performance = 0;
    unsigned int penalty = 0;

    scheduler_decode(genome, &quantum, &lanes, &batch, &backoff);
    for (size_t index = 0; index < 6; ++index) {
        workload += fixture->duration[index];
        urgency += fixture->priority[index];
    }
    performance = lanes * 180U + batch * 95U + quantum * 40U +
                  urgency * 3U;
    penalty = batch > 6U ? (batch - 6U) * 90U : 0U;

    return (evo_fitness_t){
        .correctness = 1000.0,
        .performance = (double)performance,
        .memory_use = (double)(workload + batch * lanes),
        .reliability = (double)(500U - backoff * 20U),
        .maintainability = 100.0,
        .constraint_penalty = (double)penalty,
        .total = (double)(1600U + performance - backoff * 20U - penalty),
    };
}

int main(void)
{
    static const adapter_definition_t definition = {
        .adapter_id = "scheduler-tuning",
        .domain = "bounded deterministic scheduler-parameter search",
        .fixture_id = "six-job-queue-v1",
        .fixture_json =
            "{\"jobs\":["
            "{\"id\":\"job-0\",\"duration\":5,\"priority\":9},"
            "{\"id\":\"job-1\",\"duration\":2,\"priority\":5},"
            "{\"id\":\"job-2\",\"duration\":7,\"priority\":8},"
            "{\"id\":\"job-3\",\"duration\":3,\"priority\":4},"
            "{\"id\":\"job-4\",\"duration\":4,\"priority\":7},"
            "{\"id\":\"job-5\",\"duration\":6,\"priority\":6}],"
            "\"parameters\":[\"quantum\",\"logical-lanes\",\"batch\","
            "\"backoff\"],"
            "\"hard_constraint\":\"logical lanes <= batch and quantum + backoff <= 10\","
            "\"soft_penalty\":\"90 per batch unit above 6\"}",
        .limitation =
            "models one immutable queue; logical worker identities and candidate-order commits, not native thread timing, are authoritative",
        .random_seed = UINT64_C(55003),
        .problem_identity = UINT64_C(0x5503000000000001),
        .context_identity = UINT64_C(0),
        .evaluate = scheduler_evaluate,
        .is_valid = scheduler_is_valid,
        .model = &scheduler_fixture,
        .evaluation_worker_count = 3,
    };

    return adapter_reference_main(&definition);
}
