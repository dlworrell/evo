#include "internal/project_fingerprint.h"

#include "internal/project_runtime.h"

#include <stdio.h>
#include <string.h>

#define EVO_PROJECT_FNV_OFFSET UINT64_C(14695981039346656037)
#define EVO_PROJECT_FNV_PRIME UINT64_C(1099511628211)

void evo_project_fingerprint_begin(evo_project_fingerprint_t *fingerprint)
{
    fingerprint->value = EVO_PROJECT_FNV_OFFSET;
}

void evo_project_fingerprint_bytes(
    evo_project_fingerprint_t *fingerprint,
    const void *bytes,
    size_t byte_count)
{
    const unsigned char *values = bytes;
    size_t index;

    for (index = 0U; index < byte_count; index += 1U) {
        fingerprint->value ^= (uint64_t)values[index];
        fingerprint->value *= EVO_PROJECT_FNV_PRIME;
    }
}

void evo_project_fingerprint_u64(
    evo_project_fingerprint_t *fingerprint,
    uint64_t value)
{
    unsigned int shift;

    for (shift = 0U; shift < 64U; shift += 8U) {
        const unsigned char byte =
            (unsigned char)((value >> shift) & UINT64_C(0xff));
        evo_project_fingerprint_bytes(fingerprint, &byte, 1U);
    }
}

void evo_project_fingerprint_string(
    evo_project_fingerprint_t *fingerprint,
    const char *value)
{
    const size_t size = strlen(value);

    evo_project_fingerprint_u64(fingerprint, (uint64_t)size);
    evo_project_fingerprint_bytes(fingerprint, value, size);
}

void evo_project_fingerprint_field(
    evo_project_fingerprint_t *fingerprint,
    const char *name,
    const char *value)
{
    evo_project_fingerprint_string(fingerprint, name);
    evo_project_fingerprint_string(fingerprint, value);
}

void evo_project_fingerprint_format(uint64_t value, char output[28])
{
    const int written = evo_project_format(
        output,
        28U,
        "fnv1a64-v1:%016llx",
        (unsigned long long)value);

    if (written != 27) {
        output[0] = '\0';
    }
}
