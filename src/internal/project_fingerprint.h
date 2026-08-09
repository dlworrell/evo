#ifndef CATALYST_EVO_INTERNAL_PROJECT_FINGERPRINT_H
#define CATALYST_EVO_INTERNAL_PROJECT_FINGERPRINT_H

#include <stddef.h>
#include <stdint.h>

typedef struct evo_project_fingerprint {
    uint64_t value;
} evo_project_fingerprint_t;

void evo_project_fingerprint_begin(evo_project_fingerprint_t *fingerprint);

void evo_project_fingerprint_bytes(
    evo_project_fingerprint_t *fingerprint,
    const void *bytes,
    size_t byte_count);

void evo_project_fingerprint_u64(
    evo_project_fingerprint_t *fingerprint,
    uint64_t value);

void evo_project_fingerprint_string(
    evo_project_fingerprint_t *fingerprint,
    const char *value);

void evo_project_fingerprint_field(
    evo_project_fingerprint_t *fingerprint,
    const char *name,
    const char *value);

void evo_project_fingerprint_format(uint64_t value, char output[28]);

#endif
