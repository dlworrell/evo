#ifndef CATALYST_EVO_INTERNAL_PROJECT_RUNTIME_H
#define CATALYST_EVO_INTERNAL_PROJECT_RUNTIME_H

#include <stddef.h>

/*
 * Reviewed boundary for the private source-optimizer foundation's allocation,
 * release, and bounded formatting primitives. Callers retain type-specific
 * ownership and must check every allocation and formatting result.
 */
void *evo_project_allocate_zeroed(size_t count, size_t element_size);

void evo_project_release(void *allocation);

int evo_project_format(
    char *output,
    size_t output_size,
    const char *format,
    ...);

#endif
