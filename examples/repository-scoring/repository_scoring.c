#include "../common/reference_adapter.h"

typedef struct repository_fixture {
    unsigned int tests[4];
    unsigned int review[4];
    unsigned int documentation[4];
    unsigned int security[4];
} repository_fixture_t;

static const repository_fixture_t repository_fixture = {
    .tests = {8, 6, 9, 7},
    .review = {7, 9, 6, 8},
    .documentation = {6, 8, 7, 9},
    .security = {9, 7, 8, 6},
};

static unsigned int repository_weight(unsigned char byte)
{
    return 1U + ((unsigned int)byte % 8U);
}

static bool repository_is_valid(const void *genome, void *context)
{
    const unsigned char *bytes = genome;
    unsigned int total_weight = 0;

    (void)context;
    for (size_t index = 0; index < ADAPTER_GENOME_SIZE; ++index) {
        total_weight += repository_weight(bytes[index]);
    }
    return total_weight <= 24U;
}

static evo_fitness_t repository_evaluate(const void *genome, void *context)
{
    const unsigned char *bytes = genome;
    const repository_fixture_t *fixture = context;
    const unsigned int test_weight = repository_weight(bytes[0]);
    const unsigned int review_weight = repository_weight(bytes[1]);
    const unsigned int documentation_weight =
        repository_weight(bytes[2]);
    const unsigned int security_weight = repository_weight(bytes[3]);
    unsigned int test_score = 0;
    unsigned int review_score = 0;
    unsigned int documentation_score = 0;
    unsigned int security_score = 0;
    const unsigned int penalty =
        security_weight < 3U ? (3U - security_weight) * 50U : 0U;

    for (size_t index = 0; index < 4; ++index) {
        test_score += test_weight * fixture->tests[index];
        review_score += review_weight * fixture->review[index];
        documentation_score +=
            documentation_weight * fixture->documentation[index];
        security_score += security_weight * fixture->security[index];
    }

    return (evo_fitness_t){
        .correctness = (double)test_score,
        .performance = (double)review_score,
        .reliability = (double)security_score,
        .maintainability = (double)documentation_score,
        .constraint_penalty = (double)penalty,
        .total = (double)(test_score + review_score +
                          documentation_score + security_score - penalty),
    };
}

int main(void)
{
    static const adapter_definition_t definition = {
        .adapter_id = "repository-scoring",
        .domain = "bounded repository scoring weight search",
        .fixture_id = "repository-review-corpus-v1",
        .fixture_json =
            "{\"repositories\":["
            "{\"id\":\"alpha\",\"tests\":8,\"review\":7,"
            "\"documentation\":6,\"security\":9},"
            "{\"id\":\"beta\",\"tests\":6,\"review\":9,"
            "\"documentation\":8,\"security\":7},"
            "{\"id\":\"gamma\",\"tests\":9,\"review\":6,"
            "\"documentation\":7,\"security\":8},"
            "{\"id\":\"delta\",\"tests\":7,\"review\":8,"
            "\"documentation\":9,\"security\":6}],"
            "\"weight_mapping\":\"1 + byte modulo 8\","
            "\"hard_constraint\":\"sum(weights) <= 24\","
            "\"soft_penalty\":\"50 per missing security weight below 3\"}",
        .limitation =
            "scores only the four immutable fixture records; it reads, writes, and publishes no repository",
        .random_seed = UINT64_C(55001),
        .problem_identity = UINT64_C(0x5501000000000001),
        .context_identity = UINT64_C(0x5501000000000002),
        .evaluate = repository_evaluate,
        .is_valid = repository_is_valid,
        .model = &repository_fixture,
        .checkpoint_resume = true,
    };

    return adapter_reference_main(&definition);
}
