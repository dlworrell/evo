#ifndef CATALYST_EVO_INTERNAL_PROJECT_JSON_H
#define CATALYST_EVO_INTERNAL_PROJECT_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum evo_project_json_type {
    EVO_PROJECT_JSON_OBJECT = 1,
    EVO_PROJECT_JSON_ARRAY = 2,
    EVO_PROJECT_JSON_STRING = 3,
    EVO_PROJECT_JSON_NUMBER = 4,
    EVO_PROJECT_JSON_TRUE = 5,
    EVO_PROJECT_JSON_FALSE = 6,
    EVO_PROJECT_JSON_NULL = 7
} evo_project_json_type_t;

typedef struct evo_project_json_token {
    evo_project_json_type_t type;
    size_t start;
    size_t end;
    size_t child_count;
    size_t parent;
} evo_project_json_token_t;

typedef enum evo_project_json_status {
    EVO_PROJECT_JSON_SUCCESS = 0,
    EVO_PROJECT_JSON_INVALID = 1,
    EVO_PROJECT_JSON_RESOURCE_LIMIT = 2,
    EVO_PROJECT_JSON_OUT_OF_MEMORY = 3
} evo_project_json_status_t;

evo_project_json_status_t evo_project_json_parse(
    const char *text,
    size_t text_size,
    evo_project_json_token_t *tokens,
    size_t token_capacity,
    size_t maximum_depth,
    size_t *token_count);

size_t evo_project_json_next(
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t token_index);

int evo_project_json_object_get(
    const char *text,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const char *key,
    size_t *value_index);

bool evo_project_json_object_has_only(
    const char *text,
    const evo_project_json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const char *const *allowed_keys,
    size_t allowed_key_count);

evo_project_json_status_t evo_project_json_decode_string(
    const char *text,
    const evo_project_json_token_t *token,
    size_t maximum_bytes,
    char **decoded);

bool evo_project_json_parse_u64(
    const char *text,
    const evo_project_json_token_t *token,
    uint64_t *value);

bool evo_project_json_parse_bool(
    const evo_project_json_token_t *token,
    bool *value);

#endif
