#if defined(EVO_HAVE_EXPLICIT_BZERO) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE 1
#endif

#include "internal/secure_erasure.h"

#if defined(EVO_HAVE_EXPLICIT_BZERO)
#include <string.h>
#endif

evo_secure_erasure_backend_t evo_secure_erasure_selected_backend(void)
{
#if defined(EVO_HAVE_EXPLICIT_BZERO)
    return EVO_SECURE_ERASURE_BACKEND_EXPLICIT_BZERO;
#else
    return EVO_SECURE_ERASURE_BACKEND_VOLATILE_BYTES;
#endif
}

bool evo_secure_erasure_metadata_is_valid(
    bool enabled,
    uint32_t policy_version,
    evo_secure_erasure_backend_t backend)
{
    return policy_version == EVO_SECURE_ERASURE_POLICY_VERSION &&
           backend ==
               (enabled ? evo_secure_erasure_selected_backend()
                        : EVO_SECURE_ERASURE_BACKEND_NONE);
}

void evo_secure_erase(void *allocation, size_t byte_count)
{
    if (allocation == NULL || byte_count == 0) {
        return;
    }

#if defined(EVO_HAVE_EXPLICIT_BZERO)
    explicit_bzero(allocation, byte_count);
#else
    {
        volatile unsigned char *bytes = allocation;

        for (size_t index = 0; index < byte_count; ++index) {
            bytes[index] = 0;
        }
    }
#endif
}
