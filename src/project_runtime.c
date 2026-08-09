#include "internal/project_runtime.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void *evo_project_allocate_zeroed(size_t count, size_t element_size)
{
    if (count == 0U || element_size == 0U ||
        count > SIZE_MAX / element_size) {
        return NULL;
    }
    return calloc(count, element_size);
}

void evo_project_release(void *allocation)
{
    free(allocation);
}

int evo_project_format(
    char *output,
    size_t output_size,
    const char *format,
    ...)
{
    va_list arguments;
    int written;

    if (output == NULL || output_size == 0U || format == NULL) {
        return -1;
    }
    va_start(arguments, format);
    written = vsnprintf(output, output_size, format, arguments);
    va_end(arguments);
    return written;
}
