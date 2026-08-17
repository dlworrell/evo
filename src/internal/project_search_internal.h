#ifndef CATALYST_EVO_INTERNAL_PROJECT_SEARCH_INTERNAL_H
#define CATALYST_EVO_INTERNAL_PROJECT_SEARCH_INTERNAL_H

#include "internal/project_search_owner.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EVO_PROJECT_SEARCH_RECORD_IDENTITY_BYTES 40U

typedef struct evo_project_search_mutable_record {
    char identity[EVO_PROJECT_SEARCH_RECORD_IDENTITY_BYTES];
    const char *target_location_identity;
    const char *transformation_identity;
    uint32_t transformation_version;
    size_t parameter_count;
} evo_project_search_mutable_record_t;

typedef struct evo_project_search_mutable_recipe {
    size_t capacity;
    size_t parameter_capacity;
    size_t record_count;
    evo_project_search_mutable_record_t *records;
    evo_project_recipe_parameter_value_t *parameter_storage;
    evo_project_recipe_proposal_record_t *proposals;
} evo_project_search_mutable_recipe_t;

bool evo_search_config_valid(const evo_project_search_config_t *config);
char *evo_search_duplicate(const char *value, size_t maximum_bytes);
bool evo_search_copy_text(
    char *destination,
    size_t destination_size,
    const char *source);
bool evo_search_fitness_valid(const evo_fitness_t *fitness);
bool evo_search_fitness_equal(
    const evo_fitness_t *left,
    const evo_fitness_t *right);
uint64_t evo_search_selector(
    const evo_project_search_config_t *config,
    const char *domain,
    size_t ordinal,
    const unsigned char *first,
    size_t first_size,
    const unsigned char *second,
    size_t second_size);
void evo_search_zero_genome(unsigned char *genome, size_t genome_size);
bool evo_search_copy_genome(
    unsigned char *destination,
    const unsigned char *source,
    size_t genome_size);

bool evo_search_mutable_open(
    const evo_project_search_config_t *config,
    evo_project_search_mutable_recipe_t *mutable_recipe);
void evo_search_mutable_close(
    evo_project_search_mutable_recipe_t *mutable_recipe);
bool evo_search_mutable_from_recipe(
    const evo_project_search_config_t *config,
    const evo_project_recipe_t *recipe,
    evo_project_search_mutable_recipe_t *mutable_recipe);
bool evo_search_mutable_normalize(
    evo_project_search_mutable_recipe_t *mutable_recipe);
evo_project_recipe_status_t evo_search_mutable_build(
    const evo_project_search_config_t *config,
    evo_project_search_mutable_recipe_t *mutable_recipe,
    evo_project_recipe_t *recipe);

evo_project_recipe_status_t evo_search_initialize_recipe(
    const evo_project_search_config_t *config,
    uint64_t selector,
    evo_project_recipe_t *recipe);
evo_project_recipe_status_t evo_search_mutate_recipe(
    const evo_project_search_config_t *config,
    const unsigned char *parent_genome,
    evo_project_search_operator_kind_t operation,
    uint64_t selector,
    evo_project_recipe_t *recipe);
evo_project_recipe_status_t evo_search_crossover_recipes(
    const evo_project_search_config_t *config,
    const unsigned char *parent_a_genome,
    const unsigned char *parent_b_genome,
    uint64_t selector,
    evo_project_recipe_t *child_a,
    evo_project_recipe_t *child_b);

evo_project_search_rejection_reason_t evo_search_rejection_from_recipe(
    evo_project_recipe_status_t status);

bool evo_search_build_evidence(
    const evo_project_search_config_t *config,
    evo_project_search_owner_t *owner);

#endif
