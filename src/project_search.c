#include "internal/project_search.h"

#include "internal/project_fingerprint.h"
#include "internal/project_runtime.h"
#include "internal/project_search_internal.h"
#include "internal/project_search_owner.h"
#include "internal/project_search_orchestration.h"
#include "internal/project_search_orchestration_trace.h"
#include "internal/run_batch.h"

#include <string.h>

typedef struct evo_search_run_context {
    const evo_project_search_config_t *config;
    const evo_project_search_orchestration_policy_t *orchestration_policy;
    evo_project_search_orchestration_trace_owner_t *orchestration_trace_owner;
    evo_project_search_owner_t *owner;
    bool fatal_state;
} evo_search_run_context_t;

static bool evo_search_copy_fingerprint(
    char destination[EVO_PROJECT_FINGERPRINT_TEXT_SIZE],
    const char *source)
{
    if (source == NULL) {
        destination[0] = '\0';
        return true;
    }
    return evo_search_copy_text(
        destination, EVO_PROJECT_FINGERPRINT_TEXT_SIZE, source);
}

static bool evo_search_copy_recipe_genome(
    unsigned char *destination,
    const evo_project_recipe_t *recipe,
    size_t genome_size)
{
    if (recipe == NULL || recipe->private_owner == NULL ||
        recipe->genome == NULL || recipe->genome_size != genome_size) {
        return false;
    }
    return evo_search_copy_genome(destination, recipe->genome, genome_size);
}

static evo_project_search_birth_event_t *evo_search_find_birth(
    evo_project_search_owner_t *owner,
    const void *genome_address)
{
    size_t index = owner->birth_count;

    while (index > 0U) {
        evo_project_search_birth_event_t *event =
            &owner->birth_events[index - 1U];

        if (!event->consumed && event->genome_address == genome_address) {
            return event;
        }
        index -= 1U;
    }
    return NULL;
}

static evo_project_search_operator_event_t *evo_search_record_operator_event(
    evo_search_run_context_t *context,
    const void *genome_address,
    evo_project_search_operator_kind_t operator_kind,
    const char *parent_a,
    const char *parent_b,
    const char *result_recipe,
    evo_project_recipe_status_t recipe_status)
{
    evo_project_search_owner_t *owner = context->owner;
    evo_project_search_operator_event_t *event;
    const size_t index = owner->operator_event_count;

    if (index >= owner->operator_event_capacity) {
        context->fatal_state = true;
        return NULL;
    }
    event = &owner->operator_events[index];
    *event = (evo_project_search_operator_event_t){0};
    event->ordinal = index;
    event->operator_kind = operator_kind;
    event->recipe_status = recipe_status;
    event->rejection_reason = evo_search_rejection_from_recipe(recipe_status);
    if (!evo_search_copy_fingerprint(event->parent_a_recipe_fingerprint, parent_a) ||
        !evo_search_copy_fingerprint(event->parent_b_recipe_fingerprint, parent_b) ||
        !evo_search_copy_fingerprint(event->result_recipe_fingerprint, result_recipe)) {
        context->fatal_state = true;
        return NULL;
    }
    owner->operator_event_genome_addresses[index] = genome_address;
    owner->operator_event_count += 1U;
    return event;
}

static evo_project_search_birth_event_t *evo_search_record_birth(
    evo_search_run_context_t *context,
    const void *genome_address,
    const char *parent_a,
    const char *parent_b,
    evo_project_recipe_status_t recipe_status)
{
    evo_project_search_owner_t *owner = context->owner;
    evo_project_search_birth_event_t *event =
        evo_search_find_birth(owner, genome_address);

    if (event == NULL) {
        if (owner->birth_count >= owner->birth_capacity) {
            context->fatal_state = true;
            return NULL;
        }
        event = &owner->birth_events[owner->birth_count];
        owner->birth_count += 1U;
        *event = (evo_project_search_birth_event_t){0};
        event->genome_address = genome_address;
        if (!evo_search_copy_fingerprint(
                event->parent_a_recipe_fingerprint, parent_a) ||
            !evo_search_copy_fingerprint(
                event->parent_b_recipe_fingerprint, parent_b)) {
            context->fatal_state = true;
            return NULL;
        }
    }
    event->recipe_status = recipe_status;
    event->rejection_reason = evo_search_rejection_from_recipe(recipe_status);
    return event;
}

static size_t evo_search_bind_operator_events(
    evo_project_search_owner_t *owner,
    const void *genome_address,
    evo_project_search_lineage_record_t *record)
{
    size_t index;
    size_t count = 0U;

    for (index = 0U; index < owner->operator_event_count; index += 1U) {
        evo_project_search_operator_event_t *event = &owner->operator_events[index];

        if (event->bound || owner->operator_event_genome_addresses[index] != genome_address) {
            continue;
        }
        event->bound = true;
        event->generation = record->generation;
        event->population_index = record->population_index;
        if (count == 0U) {
            record->operator_ordinal = event->ordinal;
        }
        record->operator_kind = event->operator_kind;
        count += 1U;
    }
    record->operator_event_count = count;
    return count;
}

static evo_project_search_operator_kind_t evo_search_select_mutation_kind(
    const evo_project_search_config_t *config,
    uint64_t selector)
{
    evo_project_search_operator_kind_t choices[5];
    size_t count = 0U;

    if ((config->policy.mutation_operation_mask &
         EVO_PROJECT_SEARCH_MUTATION_ADD) != 0U) {
        choices[count++] = EVO_PROJECT_SEARCH_OPERATOR_MUTATION_ADD;
    }
    if ((config->policy.mutation_operation_mask &
         EVO_PROJECT_SEARCH_MUTATION_REMOVE) != 0U) {
        choices[count++] = EVO_PROJECT_SEARCH_OPERATOR_MUTATION_REMOVE;
    }
    if ((config->policy.mutation_operation_mask &
         EVO_PROJECT_SEARCH_MUTATION_PARAMETERIZE) != 0U) {
        choices[count++] = EVO_PROJECT_SEARCH_OPERATOR_MUTATION_PARAMETERIZE;
    }
    if ((config->policy.mutation_operation_mask &
         EVO_PROJECT_SEARCH_MUTATION_REPLACE) != 0U) {
        choices[count++] = EVO_PROJECT_SEARCH_OPERATOR_MUTATION_REPLACE;
    }
    if ((config->policy.mutation_operation_mask &
         EVO_PROJECT_SEARCH_MUTATION_REORDER) != 0U) {
        choices[count++] = EVO_PROJECT_SEARCH_OPERATOR_MUTATION_REORDER;
    }
    return count == 0U
               ? EVO_PROJECT_SEARCH_OPERATOR_MUTATION_ADD
               : choices[(size_t)(selector % (uint64_t)count)];
}

static void evo_search_initialize_callback(void *genome, void *opaque)
{
    evo_search_run_context_t *context = opaque;
    evo_project_recipe_t recipe = {0};
    unsigned char *bytes = genome;
    const size_t ordinal = context->owner->operator_event_count;
    const uint64_t selector = evo_search_selector(
        context->config,
        "initialize",
        ordinal,
        bytes,
        context->config->genome_size,
        NULL,
        0U);
    evo_project_recipe_status_t status = evo_search_initialize_recipe(
        context->config, selector, &recipe);

    if (status == EVO_PROJECT_RECIPE_SUCCESS &&
        evo_search_copy_recipe_genome(
            bytes, &recipe, context->config->genome_size)) {
        if (evo_search_record_operator_event(
                context,
                genome,
                EVO_PROJECT_SEARCH_OPERATOR_INITIALIZE,
                NULL,
                NULL,
                recipe.recipe_fingerprint,
                EVO_PROJECT_RECIPE_SUCCESS) != NULL) {
            (void)evo_search_record_birth(
                context,
                genome,
                NULL,
                NULL,
                EVO_PROJECT_RECIPE_SUCCESS);
        }
    } else {
        if (status == EVO_PROJECT_RECIPE_SUCCESS) {
            status = EVO_PROJECT_RECIPE_ERROR_STATE;
        }
        evo_search_zero_genome(bytes, context->config->genome_size);
        if (evo_search_record_operator_event(
                context,
                genome,
                EVO_PROJECT_SEARCH_OPERATOR_INITIALIZE,
                NULL,
                NULL,
                NULL,
                status) != NULL) {
            (void)evo_search_record_birth(
                context, genome, NULL, NULL, status);
        }
    }
    evo_project_recipe_destroy(&recipe);
}

static void evo_search_mutate_callback(
    void *genome,
    double mutation_rate,
    void *opaque)
{
    evo_search_run_context_t *context = opaque;
    evo_project_recipe_t parent = {0};
    evo_project_recipe_t current = {0};
    evo_project_recipe_t next = {0};
    unsigned char *bytes = genome;
    char original_parent[EVO_PROJECT_FINGERPRINT_TEXT_SIZE] = {0};
    char source_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE] = {0};
    char rate_text[64];
    uint64_t selector;
    evo_project_recipe_status_t status;
    evo_project_search_operator_kind_t last_operation =
        EVO_PROJECT_SEARCH_OPERATOR_MUTATION_ADD;
    size_t operation_count;
    size_t operation_index;
    const int rate_written = evo_project_format(
        rate_text, sizeof(rate_text), "%.17g", mutation_rate);

    status = evo_project_recipe_decode(
        &context->config->recipe_context,
        bytes,
        context->config->genome_size,
        &parent);
    if (status != EVO_PROJECT_RECIPE_SUCCESS || rate_written <= 0 ||
        (size_t)rate_written >= sizeof(rate_text) ||
        !evo_search_copy_fingerprint(
            original_parent,
            status == EVO_PROJECT_RECIPE_SUCCESS
                ? parent.recipe_fingerprint
                : NULL) ||
        !evo_search_copy_fingerprint(source_fingerprint, original_parent)) {
        if (status == EVO_PROJECT_RECIPE_SUCCESS) {
            status = EVO_PROJECT_RECIPE_ERROR_STATE;
        }
        evo_search_zero_genome(bytes, context->config->genome_size);
        if (evo_search_record_operator_event(
                context,
                genome,
                last_operation,
                original_parent,
                NULL,
                NULL,
                status) != NULL) {
            (void)evo_search_record_birth(
                context, genome, original_parent, NULL, status);
        }
        evo_project_recipe_destroy(&parent);
        return;
    }
    selector = evo_search_selector(
        context->config,
        rate_text,
        context->owner->operator_event_count,
        bytes,
        context->config->genome_size,
        NULL,
        0U);
    operation_count = 1U +
                      (size_t)(selector %
                               (uint64_t)context->config->policy
                                   .max_mutations_per_event);
    for (operation_index = 0U;
         operation_index < operation_count;
         operation_index += 1U) {
        const uint64_t operation_selector =
            selector + UINT64_C(0x9e3779b97f4a7c15) *
                           (uint64_t)(operation_index + 1U);
        const evo_project_search_operator_kind_t operation =
            evo_search_select_mutation_kind(
                context->config, operation_selector);
        const unsigned char *source_genome =
            operation_index == 0U ? parent.genome : current.genome;

        evo_project_recipe_destroy(&next);
        status = evo_search_mutate_recipe(
            context->config,
            source_genome,
            operation,
            operation_selector,
            &next);
        last_operation = operation;
        if (evo_search_record_operator_event(
                context,
                genome,
                operation,
                source_fingerprint,
                NULL,
                status == EVO_PROJECT_RECIPE_SUCCESS
                    ? next.recipe_fingerprint
                    : NULL,
                status) == NULL) {
            status = EVO_PROJECT_RECIPE_ERROR_STATE;
            break;
        }
        if (status != EVO_PROJECT_RECIPE_SUCCESS) {
            break;
        }
        if (!evo_search_copy_fingerprint(
                source_fingerprint, next.recipe_fingerprint)) {
            status = EVO_PROJECT_RECIPE_ERROR_STATE;
            break;
        }
        evo_project_recipe_destroy(&current);
        current = next;
        next = (evo_project_recipe_t){0};
    }
    if (status == EVO_PROJECT_RECIPE_SUCCESS && current.private_owner != NULL &&
        evo_search_copy_recipe_genome(
            bytes, &current, context->config->genome_size)) {
        (void)evo_search_record_birth(
            context,
            genome,
            original_parent,
            NULL,
            EVO_PROJECT_RECIPE_SUCCESS);
    } else {
        if (status == EVO_PROJECT_RECIPE_SUCCESS) {
            status = EVO_PROJECT_RECIPE_ERROR_STATE;
        }
        evo_search_zero_genome(bytes, context->config->genome_size);
        (void)evo_search_record_birth(
            context, genome, original_parent, NULL, status);
    }
    (void)last_operation;
    evo_project_recipe_destroy(&next);
    evo_project_recipe_destroy(&current);
    evo_project_recipe_destroy(&parent);
}

static void evo_search_crossover_callback(
    const void *parent_a,
    const void *parent_b,
    void *child_a,
    void *child_b,
    void *opaque)
{
    evo_search_run_context_t *context = opaque;
    evo_project_recipe_t decoded_a = {0};
    evo_project_recipe_t decoded_b = {0};
    evo_project_recipe_t crossed_a = {0};
    evo_project_recipe_t crossed_b = {0};
    char parent_a_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE] = {0};
    char parent_b_fingerprint[EVO_PROJECT_FINGERPRINT_TEXT_SIZE] = {0};
    evo_project_recipe_status_t status_a;
    evo_project_recipe_status_t status_b;
    evo_project_recipe_status_t status;
    const uint64_t selector = evo_search_selector(
        context->config,
        "crossover",
        context->owner->operator_event_count,
        parent_a,
        context->config->genome_size,
        parent_b,
        context->config->genome_size);

    status_a = evo_project_recipe_decode(
        &context->config->recipe_context,
        parent_a,
        context->config->genome_size,
        &decoded_a);
    status_b = evo_project_recipe_decode(
        &context->config->recipe_context,
        parent_b,
        context->config->genome_size,
        &decoded_b);
    if (status_a == EVO_PROJECT_RECIPE_SUCCESS) {
        (void)evo_search_copy_fingerprint(
            parent_a_fingerprint, decoded_a.recipe_fingerprint);
    }
    if (status_b == EVO_PROJECT_RECIPE_SUCCESS) {
        (void)evo_search_copy_fingerprint(
            parent_b_fingerprint, decoded_b.recipe_fingerprint);
    }
    if (status_a != EVO_PROJECT_RECIPE_SUCCESS ||
        status_b != EVO_PROJECT_RECIPE_SUCCESS) {
        status = status_a != EVO_PROJECT_RECIPE_SUCCESS ? status_a : status_b;
    } else {
        status = evo_search_crossover_recipes(
            context->config,
            parent_a,
            parent_b,
            selector,
            &crossed_a,
            &crossed_b);
    }
    if (status == EVO_PROJECT_RECIPE_SUCCESS &&
        evo_search_copy_recipe_genome(
            child_a, &crossed_a, context->config->genome_size) &&
        evo_search_copy_recipe_genome(
            child_b, &crossed_b, context->config->genome_size)) {
        if (evo_search_record_operator_event(
                context,
                child_a,
                EVO_PROJECT_SEARCH_OPERATOR_CROSSOVER,
                parent_a_fingerprint,
                parent_b_fingerprint,
                crossed_a.recipe_fingerprint,
                EVO_PROJECT_RECIPE_SUCCESS) != NULL) {
            (void)evo_search_record_birth(
                context,
                child_a,
                parent_a_fingerprint,
                parent_b_fingerprint,
                EVO_PROJECT_RECIPE_SUCCESS);
        }
        if (evo_search_record_operator_event(
                context,
                child_b,
                EVO_PROJECT_SEARCH_OPERATOR_CROSSOVER,
                parent_b_fingerprint,
                parent_a_fingerprint,
                crossed_b.recipe_fingerprint,
                EVO_PROJECT_RECIPE_SUCCESS) != NULL) {
            (void)evo_search_record_birth(
                context,
                child_b,
                parent_b_fingerprint,
                parent_a_fingerprint,
                EVO_PROJECT_RECIPE_SUCCESS);
        }
    } else {
        if (status == EVO_PROJECT_RECIPE_SUCCESS) {
            status = EVO_PROJECT_RECIPE_ERROR_STATE;
        }
        evo_search_zero_genome(child_a, context->config->genome_size);
        evo_search_zero_genome(child_b, context->config->genome_size);
        if (evo_search_record_operator_event(
                context,
                child_a,
                EVO_PROJECT_SEARCH_OPERATOR_CROSSOVER,
                parent_a_fingerprint,
                parent_b_fingerprint,
                NULL,
                status) != NULL) {
            (void)evo_search_record_birth(
                context,
                child_a,
                parent_a_fingerprint,
                parent_b_fingerprint,
                status);
        }
        if (evo_search_record_operator_event(
                context,
                child_b,
                EVO_PROJECT_SEARCH_OPERATOR_CROSSOVER,
                parent_b_fingerprint,
                parent_a_fingerprint,
                NULL,
                status) != NULL) {
            (void)evo_search_record_birth(
                context,
                child_b,
                parent_b_fingerprint,
                parent_a_fingerprint,
                status);
        }
    }
    evo_project_recipe_destroy(&crossed_a);
    evo_project_recipe_destroy(&crossed_b);
    evo_project_recipe_destroy(&decoded_a);
    evo_project_recipe_destroy(&decoded_b);
}

static bool evo_search_outcome_identity_valid(
    const evo_project_search_config_t *config,
    const char *value)
{
    size_t length = 0U;

    if (value == NULL) {
        return false;
    }
    while (length <= config->limits.max_string_bytes && value[length] != '\0') {
        length += 1U;
    }
    return length > 0U && length <= config->limits.max_string_bytes &&
           length < EVO_PROJECT_FINGERPRINT_TEXT_SIZE;
}

static bool evo_search_copy_outcome(
    const evo_project_search_config_t *config,
    const evo_project_search_evaluation_outcome_t *outcome,
    evo_project_search_lineage_record_t *record)
{
    if (!evo_search_outcome_identity_valid(
            config, outcome->candidate_fingerprint) ||
        !evo_search_outcome_identity_valid(
            config, outcome->assurance_fingerprint) ||
        !evo_search_outcome_identity_valid(
            config, outcome->measurement_fingerprint) ||
        !evo_search_fitness_valid(&outcome->fitness) ||
        !evo_search_copy_fingerprint(
            record->candidate_fingerprint, outcome->candidate_fingerprint) ||
        !evo_search_copy_fingerprint(
            record->assurance_fingerprint, outcome->assurance_fingerprint) ||
        !evo_search_copy_fingerprint(
            record->measurement_fingerprint,
            outcome->measurement_fingerprint)) {
        return false;
    }
    record->fitness = outcome->fitness;
    return true;
}

static bool evo_search_prepare_structural_record(
    evo_search_run_context_t *context,
    const void *genome,
    evo_project_recipe_t *recipe,
    evo_project_search_lineage_record_t **record_out)
{
    evo_project_search_owner_t *owner = context->owner;
    evo_project_search_birth_event_t *birth =
        evo_search_find_birth(owner, genome);
    evo_project_search_lineage_record_t *record;
    evo_project_recipe_status_t recipe_status;
    const size_t ordinal = owner->validation_ordinal;

    if (record_out == NULL || recipe == NULL || recipe->private_owner != NULL) {
        context->fatal_state = true;
        return false;
    }
    *record_out = NULL;
    owner->validation_ordinal += 1U;
    if (owner->lineage_count >= owner->lineage_capacity ||
        context->config->population_size == 0U) {
        context->fatal_state = true;
        return false;
    }
    record = &owner->lineage[owner->lineage_count];
    *record = (evo_project_search_lineage_record_t){0};
    record->generation = ordinal / context->config->population_size;
    record->population_index = ordinal % context->config->population_size;
    owner->lineage_genome_addresses[owner->lineage_count] = genome;
    owner->lineage_count += 1U;
    *record_out = record;

    if (birth != NULL) {
        birth->consumed = true;
        record->rejection_reason = birth->rejection_reason;
        record->recipe_status = birth->recipe_status;
        (void)evo_search_copy_fingerprint(
            record->parent_a_recipe_fingerprint,
            birth->parent_a_recipe_fingerprint);
        (void)evo_search_copy_fingerprint(
            record->parent_b_recipe_fingerprint,
            birth->parent_b_recipe_fingerprint);
        if (evo_search_bind_operator_events(owner, genome, record) == 0U) {
            record->rejection_reason = EVO_PROJECT_SEARCH_REJECTION_STATE;
            context->fatal_state = true;
            return false;
        }
        if (birth->rejection_reason != EVO_PROJECT_SEARCH_REJECTION_NONE) {
            return false;
        }
    }

    recipe_status = evo_project_recipe_decode(
        &context->config->recipe_context,
        genome,
        context->config->genome_size,
        recipe);
    record->recipe_status = recipe_status;
    if (birth == NULL) {
        if (evo_search_record_operator_event(
                context,
                genome,
                EVO_PROJECT_SEARCH_OPERATOR_CLONE,
                recipe_status == EVO_PROJECT_RECIPE_SUCCESS
                    ? recipe->recipe_fingerprint
                    : NULL,
                NULL,
                recipe_status == EVO_PROJECT_RECIPE_SUCCESS
                    ? recipe->recipe_fingerprint
                    : NULL,
                recipe_status) == NULL ||
            evo_search_bind_operator_events(owner, genome, record) == 0U) {
            record->rejection_reason = EVO_PROJECT_SEARCH_REJECTION_STATE;
            context->fatal_state = true;
            evo_project_recipe_destroy(recipe);
            return false;
        }
    }
    if (recipe_status != EVO_PROJECT_RECIPE_SUCCESS) {
        record->rejection_reason =
            evo_search_rejection_from_recipe(recipe_status);
        evo_project_recipe_destroy(recipe);
        return false;
    }
    if (!evo_search_copy_fingerprint(
            record->recipe_fingerprint, recipe->recipe_fingerprint)) {
        record->rejection_reason = EVO_PROJECT_SEARCH_REJECTION_STATE;
        context->fatal_state = true;
        evo_project_recipe_destroy(recipe);
        return false;
    }
    if (birth == NULL) {
        (void)evo_search_copy_fingerprint(
            record->parent_a_recipe_fingerprint, recipe->recipe_fingerprint);
    }
    return true;
}

static bool evo_search_validate_callback(const void *genome, void *opaque)
{
    evo_search_run_context_t *context = opaque;
    evo_project_search_lineage_record_t *record = NULL;
    evo_project_recipe_t recipe = {0};
    evo_project_search_evaluation_request_t request = {0};
    evo_project_search_evaluation_outcome_t outcome = {0};
    evo_project_search_status_t provider_status;

    if (!evo_search_prepare_structural_record(
            context, genome, &recipe, &record)) {
        return false;
    }
    request.schema_version = EVO_PROJECT_SEARCH_SCHEMA_VERSION;
    request.random_seed = context->config->random_seed;
    request.generation = record->generation;
    request.population_index = record->population_index;
    request.provider_identity = context->config->evaluation_provider_identity;
    request.recipe = &recipe;
    outcome.schema_version = EVO_PROJECT_SEARCH_SCHEMA_VERSION;
    provider_status = context->config->evaluation_provider(
        &request,
        context->config->evaluation_provider_context,
        &outcome);
    if (provider_status != EVO_PROJECT_SEARCH_SUCCESS ||
        outcome.schema_version != EVO_PROJECT_SEARCH_SCHEMA_VERSION ||
        !outcome.accepted) {
        record->rejection_reason = EVO_PROJECT_SEARCH_REJECTION_PROVIDER;
        evo_project_recipe_destroy(&recipe);
        return false;
    }
    if (!outcome.correctness_preserved) {
        record->rejection_reason = EVO_PROJECT_SEARCH_REJECTION_CORRECTNESS;
        evo_project_recipe_destroy(&recipe);
        return false;
    }
    if (!outcome.performance_eligible) {
        record->rejection_reason = EVO_PROJECT_SEARCH_REJECTION_ASSURANCE;
        evo_project_recipe_destroy(&recipe);
        return false;
    }
    if (!outcome.fitness_available) {
        record->rejection_reason = EVO_PROJECT_SEARCH_REJECTION_MEASUREMENT;
        evo_project_recipe_destroy(&recipe);
        return false;
    }
    if (!evo_search_copy_outcome(context->config, &outcome, record)) {
        record->rejection_reason = EVO_PROJECT_SEARCH_REJECTION_PROVIDER;
        context->fatal_state = true;
        evo_project_recipe_destroy(&recipe);
        return false;
    }
    record->valid = true;
    record->rejection_reason = EVO_PROJECT_SEARCH_REJECTION_NONE;
    evo_project_recipe_destroy(&recipe);
    return true;
}

static evo_fitness_t evo_search_evaluate_callback(
    const void *genome,
    void *opaque)
{
    evo_search_run_context_t *context = opaque;
    evo_project_search_owner_t *owner = context->owner;
    size_t index = owner->lineage_count;

    while (index > 0U) {
        evo_project_search_lineage_record_t *record =
            &owner->lineage[index - 1U];

        if (owner->lineage_genome_addresses[index - 1U] == genome &&
            record->valid && !record->evaluated) {
            record->evaluated = true;
            return record->fitness;
        }
        index -= 1U;
    }
    context->fatal_state = true;
    return (evo_fitness_t){0};
}

#define EVO_SEARCH_ORCHESTRATION_WORKSPACE_BYTES 96U

static bool evo_search_orchestration_policy_valid(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *policy)
{
    return config != NULL && policy != NULL &&
           policy->schema_version == EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION &&
           policy->identity != NULL && policy->identity[0] != '\0' &&
           policy->provider.identity != NULL &&
           policy->provider.identity[0] != '\0' &&
           policy->provider.start != NULL && policy->provider.poll != NULL &&
           policy->provider.cancel != NULL && policy->provider.join != NULL &&
           policy->resources.schema_version ==
               EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION &&
           policy->resources.external_worker_count > 0U &&
           policy->limits.max_candidates >= config->population_size &&
           policy->limits.max_string_bytes >=
               EVO_SEARCH_ORCHESTRATION_WORKSPACE_BYTES &&
           policy->limits.max_external_workers >=
               policy->resources.external_worker_count;
}

static void evo_search_destroy_recipe_array(
    evo_project_recipe_t *recipes,
    size_t count)
{
    size_t index;

    if (recipes == NULL) {
        return;
    }
    for (index = 0U; index < count; index += 1U) {
        evo_project_recipe_destroy(&recipes[index]);
    }
    evo_project_release(recipes);
}

static evo_status_t evo_search_batch_evaluation_callback(
    const evo_problem_t *problem,
    const evo_config_t *core_config,
    void *opaque,
    uint64_t generation,
    const evo_population_t *population,
    evo_candidate_evaluation_t *evaluations,
    size_t evaluation_count,
    void *batch_context)
{
    evo_search_run_context_t *context = batch_context;
    const evo_project_search_orchestration_policy_t *policy;
    evo_project_recipe_t *recipes = NULL;
    evo_project_search_lineage_record_t **records = NULL;
    evo_project_orchestration_candidate_request_t *requests = NULL;
    char *workspace_storage = NULL;
    evo_project_orchestration_t orchestration = {0};
    evo_project_orchestration_config_t orchestration_config = {0};
    size_t scheduled_count = 0U;
    size_t index;
    evo_status_t status = EVO_SUCCESS;

    (void)problem;
    (void)core_config;
    (void)opaque;
    if (context == NULL || context->config == NULL ||
        context->owner == NULL || context->orchestration_policy == NULL ||
        population == NULL || evaluations == NULL ||
        evaluation_count != population->population_size ||
        population->population_size != context->config->population_size ||
        population->genome_size != context->config->genome_size ||
        generation > SIZE_MAX ||
        context->owner->validation_ordinal / context->config->population_size !=
            (size_t)generation) {
        return EVO_ERROR_EVALUATION;
    }
    policy = context->orchestration_policy;
    if (evaluation_count >
            SIZE_MAX / sizeof(*recipes) ||
        evaluation_count > SIZE_MAX / sizeof(*records) ||
        evaluation_count > SIZE_MAX / sizeof(*requests) ||
        evaluation_count >
            SIZE_MAX / EVO_SEARCH_ORCHESTRATION_WORKSPACE_BYTES) {
        context->fatal_state = true;
        return EVO_ERROR_RESOURCE_LIMIT;
    }
    recipes = evo_project_allocate_zeroed(evaluation_count, sizeof(*recipes));
    records = evo_project_allocate_zeroed(evaluation_count, sizeof(*records));
    requests = evo_project_allocate_zeroed(evaluation_count, sizeof(*requests));
    workspace_storage = evo_project_allocate_zeroed(
        evaluation_count, EVO_SEARCH_ORCHESTRATION_WORKSPACE_BYTES);
    if (recipes == NULL || records == NULL || requests == NULL ||
        workspace_storage == NULL) {
        status = EVO_ERROR_OUT_OF_MEMORY;
        goto finish;
    }

    for (index = 0U; index < evaluation_count; index += 1U) {
        const unsigned char *genome =
            (const unsigned char *)population->genomes +
            index * context->config->genome_size;
        evo_project_search_lineage_record_t *record = NULL;
        char *workspace =
            workspace_storage +
            scheduled_count * EVO_SEARCH_ORCHESTRATION_WORKSPACE_BYTES;
        int written;

        if (!evo_search_prepare_structural_record(
                context, genome, &recipes[index], &record)) {
            if (context->fatal_state) {
                status = EVO_ERROR_EVALUATION;
                goto finish;
            }
            continue;
        }
        if (record == NULL || record->generation != (size_t)generation ||
            record->population_index != index) {
            context->fatal_state = true;
            status = EVO_ERROR_EVALUATION;
            goto finish;
        }
        records[index] = record;
        written = evo_project_format(
            workspace,
            EVO_SEARCH_ORCHESTRATION_WORKSPACE_BYTES,
            "generation-%llu-candidate-%zu",
            (unsigned long long)generation,
            index);
        if (written <= 0 ||
            (size_t)written >= EVO_SEARCH_ORCHESTRATION_WORKSPACE_BYTES) {
            context->fatal_state = true;
            status = EVO_ERROR_RESOURCE_LIMIT;
            goto finish;
        }
        requests[scheduled_count].schema_version =
            EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION;
        requests[scheduled_count].generation = (size_t)generation;
        requests[scheduled_count].population_index = index;
        requests[scheduled_count].recipe_fingerprint =
            recipes[index].recipe_fingerprint;
        requests[scheduled_count].workspace_identity = workspace;
        requests[scheduled_count].random_seed = context->config->random_seed;
        requests[scheduled_count].recipe = &recipes[index];
        scheduled_count += 1U;
    }

    if (scheduled_count == 0U) {
        goto finish;
    }
    orchestration_config.policy_identity = policy->identity;
    orchestration_config.resources = policy->resources;
    orchestration_config.candidate_count = scheduled_count;
    orchestration_config.candidates = requests;
    orchestration_config.provider = policy->provider;
    orchestration_config.limits = policy->limits;
    if (evo_project_orchestration_run_batch(
            &orchestration_config, &orchestration) !=
        EVO_PROJECT_ORCHESTRATION_SUCCESS) {
        context->fatal_state = true;
        status = EVO_ERROR_EVALUATION;
        goto finish;
    }
    if (context->orchestration_trace_owner != NULL &&
        !evo_project_search_orchestration_trace_append(
            context->orchestration_trace_owner, &orchestration)) {
        context->fatal_state = true;
        status = EVO_ERROR_EVALUATION;
        goto finish;
    }
    if (orchestration.has_hard_failure ||
        !orchestration.cleanup_complete ||
        !orchestration.generation_committed ||
        orchestration.committed_count != scheduled_count) {
        context->fatal_state = true;
        status = EVO_ERROR_EVALUATION;
        goto finish;
    }
    for (index = 0U; index < orchestration.job_count; index += 1U) {
        const evo_project_orchestration_job_record_t *job =
            &orchestration.jobs[index];
        evo_project_search_lineage_record_t *record;
        const size_t population_index = job->population_index;

        if (population_index >= evaluation_count ||
            records[population_index] == NULL || !job->committed) {
            context->fatal_state = true;
            status = EVO_ERROR_EVALUATION;
            goto finish;
        }
        record = records[population_index];
        if (job->terminal_reason ==
            EVO_PROJECT_ORCHESTRATION_TERMINAL_CANDIDATE_REJECTED) {
            record->rejection_reason = EVO_PROJECT_SEARCH_REJECTION_PROVIDER;
            continue;
        }
        if (job->terminal_reason !=
                EVO_PROJECT_ORCHESTRATION_TERMINAL_SUCCESS ||
            !evo_search_copy_outcome(
                context->config, &job->evaluation, record)) {
            record->rejection_reason = EVO_PROJECT_SEARCH_REJECTION_PROVIDER;
            context->fatal_state = true;
            status = EVO_ERROR_EVALUATION;
            goto finish;
        }
        record->valid = true;
        record->evaluated = true;
        record->rejection_reason = EVO_PROJECT_SEARCH_REJECTION_NONE;
        evaluations[population_index].valid = true;
        evaluations[population_index].evaluated = true;
        evaluations[population_index].fitness = record->fitness;
    }

finish:
    evo_project_orchestration_destroy(&orchestration);
    evo_search_destroy_recipe_array(recipes, evaluation_count);
    evo_project_release(records);
    evo_project_release(requests);
    evo_project_release(workspace_storage);
    return status;
}

static bool evo_search_allocate_owner(
    const evo_project_search_config_t *config,
    evo_project_search_owner_t **owner_out)
{
    evo_project_search_owner_t *owner;
    size_t lineage_capacity;

    if (config->generation_limit == SIZE_MAX ||
        config->generation_limit + 1U >
            SIZE_MAX / config->population_size) {
        return false;
    }
    lineage_capacity =
        (config->generation_limit + 1U) * config->population_size;
    owner = evo_project_allocate_zeroed(1U, sizeof(*owner));
    if (owner == NULL) {
        return false;
    }
    owner->lineage = evo_project_allocate_zeroed(
        lineage_capacity, sizeof(*owner->lineage));
    owner->lineage_genome_addresses = evo_project_allocate_zeroed(
        lineage_capacity, sizeof(*owner->lineage_genome_addresses));
    owner->operator_events = evo_project_allocate_zeroed(
        config->limits.max_operator_events, sizeof(*owner->operator_events));
    owner->operator_event_genome_addresses = evo_project_allocate_zeroed(
        config->limits.max_operator_events,
        sizeof(*owner->operator_event_genome_addresses));
    owner->birth_events = evo_project_allocate_zeroed(
        config->limits.max_operator_events, sizeof(*owner->birth_events));
    if (owner->lineage == NULL || owner->lineage_genome_addresses == NULL ||
        owner->operator_events == NULL ||
        owner->operator_event_genome_addresses == NULL ||
        owner->birth_events == NULL) {
        evo_project_release(owner->lineage);
        evo_project_release(owner->lineage_genome_addresses);
        evo_project_release(owner->operator_events);
        evo_project_release(owner->operator_event_genome_addresses);
        evo_project_release(owner->birth_events);
        evo_project_release(owner);
        return false;
    }
    owner->lineage_capacity = lineage_capacity;
    owner->operator_event_capacity = config->limits.max_operator_events;
    owner->birth_capacity = config->limits.max_operator_events;
    *owner_out = owner;
    return true;
}

static void evo_search_owner_destroy(evo_project_search_owner_t *owner)
{
    if (owner == NULL) {
        return;
    }
    evo_project_release(owner->baseline_fingerprint);
    evo_project_release(owner->analysis_fingerprint);
    evo_project_release(owner->catalogue_identity);
    evo_project_release(owner->policy_identity);
    evo_project_release(owner->evaluation_provider_identity);
    evo_project_release(owner->best_genome);
    evo_project_release(owner->operator_events);
    evo_project_release(owner->operator_event_genome_addresses);
    evo_project_release(owner->lineage);
    evo_project_release(owner->lineage_genome_addresses);
    evo_project_release(owner->birth_events);
    evo_project_release(owner->canonical_json);
    evo_project_release(owner->audit_markdown);
    evo_project_release(owner);
}

static bool evo_search_copy_owner_identity(
    const evo_project_search_config_t *config,
    evo_project_search_owner_t *owner)
{
    owner->baseline_fingerprint = evo_search_duplicate(
        config->recipe_context.baseline->baseline_fingerprint,
        config->limits.max_string_bytes);
    owner->analysis_fingerprint = evo_search_duplicate(
        config->recipe_context.analysis->analysis_fingerprint,
        config->limits.max_string_bytes);
    owner->catalogue_identity = evo_search_duplicate(
        config->recipe_context.catalogue->identity,
        config->limits.max_string_bytes);
    owner->policy_identity = evo_search_duplicate(
        config->policy.identity, config->limits.max_string_bytes);
    owner->evaluation_provider_identity = evo_search_duplicate(
        config->evaluation_provider_identity,
        config->limits.max_string_bytes);
    return owner->baseline_fingerprint != NULL &&
           owner->analysis_fingerprint != NULL &&
           owner->catalogue_identity != NULL && owner->policy_identity != NULL &&
           owner->evaluation_provider_identity != NULL;
}

static void evo_search_publish_identity(
    const evo_project_search_config_t *config,
    evo_project_search_owner_t *owner)
{
    owner->view.schema_version = EVO_PROJECT_SEARCH_SCHEMA_VERSION;
    owner->view.baseline_fingerprint = owner->baseline_fingerprint;
    owner->view.analysis_fingerprint = owner->analysis_fingerprint;
    owner->view.catalogue_identity = owner->catalogue_identity;
    owner->view.catalogue_version =
        config->recipe_context.catalogue->catalogue_version;
    owner->view.policy_identity = owner->policy_identity;
    owner->view.evaluation_provider_identity =
        owner->evaluation_provider_identity;
    owner->view.random_seed = config->random_seed;
    owner->view.population_size = config->population_size;
    owner->view.operator_events = owner->operator_events;
    owner->view.lineage = owner->lineage;
    owner->view.projection_complete = true;
    owner->view.probabilistic_authority = false;
    owner->view.raw_source_bytes = false;
}

static void evo_search_fingerprint_fitness(
    evo_project_fingerprint_t *fingerprint,
    const evo_fitness_t *fitness)
{
    char text[64];
    const double values[] = {
        fitness->correctness,
        fitness->performance,
        fitness->memory_use,
        fitness->reliability,
        fitness->maintainability,
        fitness->constraint_penalty,
        fitness->total};
    size_t index;

    for (index = 0U; index < sizeof(values) / sizeof(values[0]); index += 1U) {
        const int written =
            evo_project_format(text, sizeof(text), "%.17g", values[index]);

        if (written > 0 && (size_t)written < sizeof(text)) {
            evo_project_fingerprint_string(fingerprint, text);
        }
    }
}

static bool evo_search_finalize_result(
    const evo_project_search_config_t *config,
    evo_project_search_owner_t *owner,
    const evo_result_t *core_result)
{
    evo_project_recipe_t best_recipe = {0};
    evo_project_fingerprint_t fingerprint;
    size_t index;
    bool winner_found = false;

    if (core_result->best_genome == NULL ||
        core_result->best_genome_size != config->genome_size) {
        return false;
    }
    owner->best_genome = evo_project_allocate_zeroed(
        config->genome_size, sizeof(*owner->best_genome));
    if (owner->best_genome == NULL ||
        !evo_search_copy_genome(
            owner->best_genome,
            core_result->best_genome,
            config->genome_size)) {
        return false;
    }
    if (evo_project_recipe_decode(
            &config->recipe_context,
            owner->best_genome,
            config->genome_size,
            &best_recipe) != EVO_PROJECT_RECIPE_SUCCESS ||
        !evo_search_copy_fingerprint(
            owner->view.best_recipe_fingerprint,
            best_recipe.recipe_fingerprint)) {
        evo_project_recipe_destroy(&best_recipe);
        return false;
    }
    owner->view.best_fitness = core_result->best_fitness;
    owner->view.best_genome = owner->best_genome;
    owner->view.best_genome_size = config->genome_size;
    owner->view.generations_completed = core_result->generations_completed;
    owner->view.termination_reason = core_result->termination_reason;
    owner->view.operator_event_count = owner->operator_event_count;
    owner->view.lineage_count = owner->lineage_count;

    for (index = 0U; index < owner->operator_event_count; index += 1U) {
        if (!owner->operator_events[index].bound) {
            evo_project_recipe_destroy(&best_recipe);
            return false;
        }
    }

    for (index = 0U; index < owner->lineage_count; index += 1U) {
        evo_project_search_lineage_record_t *record = &owner->lineage[index];

        if (!winner_found && record->valid && record->evaluated &&
            strcmp(
                record->recipe_fingerprint,
                owner->view.best_recipe_fingerprint) == 0 &&
            evo_search_fitness_equal(&record->fitness, &core_result->best_fitness)) {
            record->winner = true;
            winner_found = true;
            (void)evo_search_copy_fingerprint(
                owner->view.best_candidate_fingerprint,
                record->candidate_fingerprint);
            (void)evo_search_copy_fingerprint(
                owner->view.best_assurance_fingerprint,
                record->assurance_fingerprint);
            (void)evo_search_copy_fingerprint(
                owner->view.best_measurement_fingerprint,
                record->measurement_fingerprint);
        }
    }
    evo_project_recipe_destroy(&best_recipe);
    if (!winner_found) {
        return false;
    }

    evo_project_fingerprint_begin(&fingerprint);
    evo_project_fingerprint_string(
        &fingerprint, "evo-project-structured-search-v1");
    evo_project_fingerprint_string(
        &fingerprint, owner->view.baseline_fingerprint);
    evo_project_fingerprint_string(
        &fingerprint, owner->view.analysis_fingerprint);
    evo_project_fingerprint_string(
        &fingerprint, owner->view.catalogue_identity);
    evo_project_fingerprint_u64(
        &fingerprint, (uint64_t)owner->view.catalogue_version);
    evo_project_fingerprint_string(
        &fingerprint, owner->view.policy_identity);
    evo_project_fingerprint_string(
        &fingerprint, owner->view.evaluation_provider_identity);
    evo_project_fingerprint_u64(&fingerprint, owner->view.random_seed);
    evo_project_fingerprint_u64(
        &fingerprint, (uint64_t)owner->view.population_size);
    evo_project_fingerprint_u64(
        &fingerprint, (uint64_t)owner->view.generations_completed);
    evo_project_fingerprint_u64(
        &fingerprint, (uint64_t)owner->view.termination_reason);
    for (index = 0U; index < owner->operator_event_count; index += 1U) {
        const evo_project_search_operator_event_t *event =
            &owner->operator_events[index];

        evo_project_fingerprint_u64(&fingerprint, (uint64_t)event->ordinal);
        evo_project_fingerprint_u64(&fingerprint, (uint64_t)event->generation);
        evo_project_fingerprint_u64(
            &fingerprint, (uint64_t)event->population_index);
        evo_project_fingerprint_u64(
            &fingerprint, (uint64_t)event->operator_kind);
        evo_project_fingerprint_u64(
            &fingerprint, (uint64_t)event->rejection_reason);
        evo_project_fingerprint_u64(
            &fingerprint, (uint64_t)event->recipe_status);
        evo_project_fingerprint_string(
            &fingerprint, event->parent_a_recipe_fingerprint);
        evo_project_fingerprint_string(
            &fingerprint, event->parent_b_recipe_fingerprint);
        evo_project_fingerprint_string(
            &fingerprint, event->result_recipe_fingerprint);
    }
    for (index = 0U; index < owner->lineage_count; index += 1U) {
        const evo_project_search_lineage_record_t *record =
            &owner->lineage[index];

        evo_project_fingerprint_u64(
            &fingerprint, (uint64_t)record->generation);
        evo_project_fingerprint_u64(
            &fingerprint, (uint64_t)record->population_index);
        evo_project_fingerprint_u64(
            &fingerprint, (uint64_t)record->operator_ordinal);
        evo_project_fingerprint_u64(
            &fingerprint, (uint64_t)record->operator_kind);
        evo_project_fingerprint_u64(
            &fingerprint, (uint64_t)record->operator_event_count);
        evo_project_fingerprint_u64(
            &fingerprint, (uint64_t)record->rejection_reason);
        evo_project_fingerprint_u64(
            &fingerprint, (uint64_t)record->recipe_status);
        evo_project_fingerprint_string(
            &fingerprint, record->recipe_fingerprint);
        evo_project_fingerprint_string(
            &fingerprint, record->parent_a_recipe_fingerprint);
        evo_project_fingerprint_string(
            &fingerprint, record->parent_b_recipe_fingerprint);
        evo_project_fingerprint_string(
            &fingerprint, record->candidate_fingerprint);
        evo_project_fingerprint_string(
            &fingerprint, record->assurance_fingerprint);
        evo_project_fingerprint_string(
            &fingerprint, record->measurement_fingerprint);
        evo_search_fingerprint_fitness(&fingerprint, &record->fitness);
        evo_project_fingerprint_u64(
            &fingerprint, record->valid ? UINT64_C(1) : UINT64_C(0));
        evo_project_fingerprint_u64(
            &fingerprint, record->evaluated ? UINT64_C(1) : UINT64_C(0));
        evo_project_fingerprint_u64(
            &fingerprint, record->winner ? UINT64_C(1) : UINT64_C(0));
    }
    owner->search_fingerprint_value = fingerprint.value;
    evo_project_fingerprint_format(
        owner->search_fingerprint_value, owner->view.search_fingerprint);
    return true;
}

static evo_project_search_status_t evo_search_map_core_status(
    evo_status_t status)
{
    if (status == EVO_SUCCESS) {
        return EVO_PROJECT_SEARCH_SUCCESS;
    }
    if (status == EVO_ERROR_OUT_OF_MEMORY) {
        return EVO_PROJECT_SEARCH_ERROR_OUT_OF_MEMORY;
    }
    if (status == EVO_ERROR_RESOURCE_LIMIT) {
        return EVO_PROJECT_SEARCH_ERROR_RESOURCE_LIMIT;
    }
    if (status == EVO_ERROR_NO_VALID_CANDIDATE) {
        return EVO_PROJECT_SEARCH_ERROR_NO_VALID_CANDIDATE;
    }
    return EVO_PROJECT_SEARCH_ERROR_CORE;
}

static evo_project_search_status_t evo_project_search_run_common(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *orchestration_policy,
    evo_project_search_orchestration_trace_owner_t *orchestration_trace_owner,
    evo_project_search_t *search)
{
    evo_project_search_owner_t *owner = NULL;
    evo_search_run_context_t run_context = {0};
    evo_problem_t problem = {0};
    evo_config_t core_config = {0};
    evo_result_t core_result = {0};
    evo_status_t core_status;
    evo_project_search_status_t status;

    if (config == NULL || search == NULL || !evo_search_config_valid(config) ||
        (orchestration_policy != NULL &&
         !evo_search_orchestration_policy_valid(config, orchestration_policy))) {
        return EVO_PROJECT_SEARCH_ERROR_INVALID_ARGUMENT;
    }
    if (search->private_owner != NULL || search->schema_version != 0U) {
        return EVO_PROJECT_SEARCH_ERROR_RESULT_ACTIVE;
    }
    if (!evo_search_allocate_owner(config, &owner)) {
        return EVO_PROJECT_SEARCH_ERROR_OUT_OF_MEMORY;
    }
    if (!evo_search_copy_owner_identity(config, owner)) {
        evo_search_owner_destroy(owner);
        return EVO_PROJECT_SEARCH_ERROR_OUT_OF_MEMORY;
    }
    evo_search_publish_identity(config, owner);
    run_context.config = config;
    run_context.orchestration_policy = orchestration_policy;
    run_context.orchestration_trace_owner = orchestration_trace_owner;
    run_context.owner = owner;

    problem.genome_size = config->genome_size;
    problem.initialize = evo_search_initialize_callback;
    problem.mutate = evo_search_mutate_callback;
    problem.crossover = evo_search_crossover_callback;
    problem.evaluate = evo_search_evaluate_callback;
    problem.is_valid = evo_search_validate_callback;
    problem.evaluation_callback_thread_safety =
        EVO_EVALUATION_CALLBACK_SERIAL;

    core_config.population_size = config->population_size;
    core_config.generation_limit = config->generation_limit;
    core_config.tournament_size = config->tournament_size;
    core_config.crossover_rate = config->crossover_rate;
    core_config.mutation_rate = config->mutation_rate;
    core_config.random_seed = config->random_seed;
    core_config.max_genome_bytes = config->genome_size;
    core_config.max_population_bytes = config->max_core_population_bytes;
    core_config.max_evaluation_bytes = config->max_core_evaluation_bytes;
    core_config.max_child_population_bytes =
        config->max_core_child_population_bytes;
    core_config.max_diversity_work = config->max_core_diversity_work;
    core_config.crossover_operator = EVO_CROSSOVER_CONSUMER;
    core_config.mutation_operator = EVO_MUTATION_CONSUMER;
    core_config.evaluation_worker_count = 0U;

    if (orchestration_policy == NULL) {
        core_status = evo_run(
            &problem, &core_config, &run_context, &core_result);
    } else {
        const evo_population_batch_evaluator_t batch_evaluator = {
            evo_search_batch_evaluation_callback, &run_context};

        core_status = evo_run_with_batch_evaluator(
            &problem,
            &core_config,
            &run_context,
            &batch_evaluator,
            &core_result);
    }
    status = evo_search_map_core_status(core_status);
    if (run_context.fatal_state && status == EVO_PROJECT_SEARCH_SUCCESS) {
        status = EVO_PROJECT_SEARCH_ERROR_STATE;
    }
    if (status == EVO_PROJECT_SEARCH_SUCCESS &&
        (!evo_search_finalize_result(config, owner, &core_result) ||
         !evo_search_build_evidence(config, owner))) {
        status = EVO_PROJECT_SEARCH_ERROR_EVIDENCE;
    }
    evo_result_destroy(&core_result);
    if (status != EVO_PROJECT_SEARCH_SUCCESS) {
        evo_search_owner_destroy(owner);
        return status;
    }
    owner->view.private_owner = owner;
    *search = owner->view;
    return EVO_PROJECT_SEARCH_SUCCESS;
}

evo_project_search_status_t evo_project_search_run(
    const evo_project_search_config_t *config,
    evo_project_search_t *search)
{
    return evo_project_search_run_common(config, NULL, NULL, search);
}

evo_project_search_status_t evo_project_search_run_orchestrated(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *orchestration_policy,
    evo_project_search_t *search)
{
    if (orchestration_policy == NULL) {
        return EVO_PROJECT_SEARCH_ERROR_INVALID_ARGUMENT;
    }
    return evo_project_search_run_common(
        config, orchestration_policy, NULL, search);
}

evo_project_search_status_t evo_project_search_run_orchestrated_with_trace(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *orchestration_policy,
    evo_project_search_t *search,
    evo_project_search_orchestration_trace_t *trace)
{
    evo_project_search_orchestration_trace_owner_t *trace_owner = NULL;
    evo_project_search_status_t status;

    if (config == NULL || orchestration_policy == NULL || search == NULL ||
        trace == NULL || trace->private_owner != NULL ||
        !evo_search_config_valid(config) ||
        !evo_search_orchestration_policy_valid(config, orchestration_policy)) {
        return EVO_PROJECT_SEARCH_ERROR_INVALID_ARGUMENT;
    }
    if (search->private_owner != NULL || search->schema_version != 0U) {
        return EVO_PROJECT_SEARCH_ERROR_RESULT_ACTIVE;
    }
    *trace = (evo_project_search_orchestration_trace_t){0};
    if (!evo_project_search_orchestration_trace_owner_create(
            config, orchestration_policy, &trace_owner)) {
        return EVO_PROJECT_SEARCH_ERROR_RESOURCE_LIMIT;
    }
    status = evo_project_search_run_common(
        config, orchestration_policy, trace_owner, search);
    evo_project_search_orchestration_trace_publish(
        trace_owner, status == EVO_PROJECT_SEARCH_SUCCESS, trace);
    return status;
}

void evo_project_search_destroy(evo_project_search_t *search)
{
    evo_project_search_owner_t *owner;

    if (search == NULL) {
        return;
    }
    owner = search->private_owner;
    if (owner != NULL) {
        evo_search_owner_destroy(owner);
    }
    *search = (evo_project_search_t){0};
}
