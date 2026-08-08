#ifndef CATALYST_EVO_INTERNAL_POPULATION_RECYCLING_H
#define CATALYST_EVO_INTERNAL_POPULATION_RECYCLING_H

#include "internal/population_storage.h"

/* Establish the generation-zero address-free storage registry. */
evo_status_t evo_population_storage_registry_initialize(
    const evo_config_t *config,
    const evo_population_t *active,
    evo_population_storage_registry_t *registry);

/* Reconcile one registry with the current committed population. */
bool evo_population_storage_registry_is_valid(
    const evo_config_t *config,
    const evo_population_t *active,
    uint64_t current_generation,
    const evo_population_storage_registry_t *registry);

/* Build the post-promotion registry without modifying either input. */
bool evo_population_storage_registry_prepare_transition(
    const evo_config_t *config,
    uint64_t current_generation,
    const evo_population_t *evaluated_children,
    evo_population_storage_registry_t *registry);

/* Validate and perform the structurally no-fail reset of a former parent. */
bool evo_population_reusable_reset_is_valid(
    const evo_config_t *config,
    const evo_population_t *population);
void evo_population_reset_for_reuse(const evo_config_t *config,
                                    evo_population_t *population);

/* Deliver one already validated address-free registry projection. */
void evo_population_storage_registry_notify(
    const evo_config_t *config,
    const evo_population_storage_registry_t *registry);

#endif
