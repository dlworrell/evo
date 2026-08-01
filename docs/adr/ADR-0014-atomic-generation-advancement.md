# ADR-0014: Atomic Generation Advancement

Status: Accepted
Date: 2026-08-01
Decision owner: EVO

## Context

EVO 0.14.0 can construct an independently owned child slab, produce every
child through versioned operator streams and odd-tail policy, and evaluate the
complete slab through the shared deterministic evaluation engine. The result
is completed-population evidence, but ownership still resides in the child
handle and the original parent remains active.

The next boundary must transfer ownership without copying potentially large
genome and evaluation slabs, leaking the old parent, permitting double free,
or incrementing an unrepresentable generation. Combining this ownership change
with termination, recycling, or the public run loop would make failure and
recovery behavior harder to isolate.

## Decision

EVO 0.15.0 adds private generation-advancement policy version 1.

The operation accepts a problem, configuration, current generation, current
parent handle, evaluated-child handle, and caller-owned evidence object. It
requires all objects and every owned allocation to be independent. Both
populations must pass the common completed-population validator and match the
problem's genome size.

Lineage is explicit:

- an initialized generation-zero parent is current only at generation zero;
- a parent that originated as a child is current at generation `g > 0` only
  when its source-generation provenance is `g - 1`; and
- the incoming evaluated child must record source generation `g`, making it
  completed generation `g + 1`.

`UINT64_MAX` is rejected before increment. The configured generation limit is
not consulted because stopping policy is not part of an ownership transfer.

After every fallible validation succeeds, the operation prepares evidence,
moves the complete child structure into the parent handle, zeros the child
handle, destroys the inaccessible former parent, and commits evidence. That
suffix has no expected failure. It allocates no memory, copies no genome or
evaluation byte, consumes no RNG state, and invokes no callback.

The child's source-generation and production-policy metadata remain unchanged
after promotion. Together with the caller's completed-generation evidence,
this preserves the causal statement that generation `g + 1` was produced from
generation `g`.

An all-invalid evaluated child may be promoted. Extinction handling is a later
termination decision, not an implicit exception to ownership rules.

## Consequences

- Rejection preserves both population structures, their allocations and
  bytes, and caller-owned evidence exactly.
- Successful advancement preserves child allocation identities and transfers
  exclusive ownership without a second working-set allocation.
- The former parent is released exactly once and cannot remain aliased through
  the emptied child handle.
- The child handle may immediately receive a fresh independently owned child
  slab.
- Generation zero and later generations have independently verifiable lineage
  rules.
- Overflow never wraps or saturates a generation number.
- Atomicity describes the rejection-before-mutation/no-fail-commit contract;
  it does not provide synchronization for concurrent callers.
- Public `evo_run` behavior and installed public symbols remain unchanged.
- Generation-limit, convergence, stagnation, generalized elitism, checkpoint
  persistence, buffer recycling, and public multi-generation execution remain
  separate milestones.

## Alternatives considered

### Copy child bytes into the parent allocation

Rejected because it would require dimension-sensitive copying, retain two
owners during the transition, and either discard or separately copy complete
evaluation evidence.

### Swap the handles and retain the former parent as reusable child storage

Deferred because recycling requires a reset policy for genome bytes,
evaluation records, provenance, and caller budgets. Releasing the former
parent keeps this milestone's ownership result unambiguous.

### Reject all-invalid children

Rejected because evaluation has already established a structurally completed
population. Whether no valid candidate stops the run belongs to termination
policy.

### Enforce `generation_limit` during advancement

Rejected because a generation limit is a stop condition. The ownership move
must remain usable by a later loop whose stop policy can be reviewed
independently.

### Fold advancement into public `evo_run`

Deferred because iteration needs termination, result-update, recovery, and
checkpoint decisions beyond this atomic ownership boundary.

## Verification

- `tests/generation_advancement_test.c` covers generation-zero and later
  lineage, owner-identity and byte preservation, all-invalid promotion, child
  reuse, overflow, stale provenance, malformed state, allocation overlap,
  output aliasing, and repeated calls.
- `tests/allocation_failure_test.c` proves advancement succeeds while the next
  `calloc` call is configured to fail and records exactly two non-null release
  calls for the former parent's genome and evaluation owners.
- CMake, GNU Autotools, and AES-BLD-001 enumerate the production source and
  normative test.
- GitHub issue: `https://github.com/dlworrell/evo/issues/34`
