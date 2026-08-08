#ifndef CATALYST_EVO_INTERNAL_SECURE_ERASURE_H
#define CATALYST_EVO_INTERNAL_SECURE_ERASURE_H

#include "catalyst/evo/evo.h"

/* Return the build-selected non-optimizable erasure implementation. */
evo_secure_erasure_backend_t evo_secure_erasure_selected_backend(void);

/*
 * Verify the canonical metadata retained beside one EVO-owned allocation.
 * Active owners always record policy version 1. Disabled owners use NONE;
 * enabled owners use the build-selected backend.
 */
bool evo_secure_erasure_metadata_is_valid(
    bool enabled,
    uint32_t policy_version,
    evo_secure_erasure_backend_t backend);

/* Erase exactly byte_count bytes. NULL or zero ranges are deliberate no-ops. */
void evo_secure_erase(void *allocation, size_t byte_count);

#endif
