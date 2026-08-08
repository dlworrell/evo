# ADR-0029: Opt-In Secure-Erasure Lifecycle

Status: Accepted
Date: 2026-08-07
Decision owner: EVO

## Context

Through EVO 0.27.0, population and result destruction released allocations
without claiming that genome bytes had been scrubbed. That behavior is correct
for ordinary evolutionary data, but it is not an adequate destruction boundary
for a consumer that deliberately places secret-bearing material in a genome.
Calling `free` does not erase an allocation, and an ordinary byte fill may be
removed by an optimizing compiler when the storage is dead immediately after
the write.

Secure destruction must remain opt-in and must not weaken the existing
ownership model. EVO needs the exact allocation size at every cleanup site,
including provisional evaluation failure, diversity rollback, child failure,
generation promotion, public-run failure, successful result destruction, and
partial allocation. The implementation also needs one reviewed portability
boundary whose supported primitive and fallback are selected consistently by
CMake and GNU Autotools.

The policy scrubs only the live EVO-owned allocation. It cannot erase copies
retained by consumer callbacks, non-owning aliases used after destruction,
allocator metadata, paging or crash artifacts, device caches, or storage below
the process abstraction. Those limits must remain explicit.

## Decision

EVO 0.28.0 defines secure-erasure policy version 1. `evo_config_t` appends:

```c
bool secure_erasure_enabled;
```

The zero-initialized default is disabled. Disabled destruction invokes no
secure-erasure primitive, uses ordinary release, and makes no scrubbing claim.
Its allocation order, callback behavior, operator RNG, population bytes,
selection, and complete pre-0.28.0 result prefix remain compatible with
0.27.0; the new appended audit fields are populated for every live result.

When enabled, every EVO-owned genome or candidate-evaluation allocation is
erased over its exact retained byte count once immediately before its sole
release. Erasure allocates no memory, invokes no consumer callback, consumes no
RNG state, and cannot change a committed search decision.

### Stable ownership-and-erasure registry

The canonical registry is the fixed set of named owners and their explicit
byte-count fields. It uses logical owner names rather than allocator addresses:

| Logical owner | Canonical owner field | Exact byte count | Policy evidence | Terminal path |
|---|---|---:|---|---|
| Public result genome | `evo_result_t.best_genome` | `best_genome_size` | result policy version, backend, and enabled flag | `evo_result_destroy` |
| Population genome slab | `evo_population_t.genomes` | `storage_bytes` | population policy version, backend, and enabled flag | `evo_population_destroy` |
| Population evaluation records | `evo_population_t.evaluations` | `evaluation_bytes` | population policy version, backend, and enabled flag | population destruction or evaluation rollback |
| Provisional evaluation records | local evaluation owner | checked `evaluation_bytes` | active configuration flag and policy version 1 | provisional discard before attachment |

For each row, the deterministic lifecycle projection is:

1. `empty`: the owner is null and its byte count and policy evidence are zero;
2. `owned/ordinary`: the owner and exact count are live, policy version is 1,
   the enabled flag is false, and backend is `NONE`;
3. `owned/erase-before-release`: the owner and exact count are live, policy
   version is 1, the enabled flag is true, and backend names the build-selected
   implementation;
4. `erased`: the exact live range has received one completed erasure call; and
5. `released`: the sole release immediately follows, after which the owning
   object is reset to the complete empty state.

The transient `erased` state is intentionally not retained in a destroyed
owner. The normative link-wrapper test observes the erase and release boundary
directly, verifies their order and exact byte count, rejects duplicate erase or
release events, and then verifies the public/private empty projection. No
address is part of retained audit evidence.

### Public result evidence

`evo_result_t` appends `best_genome_size`,
`secure_erasure_policy_version`, `secure_erasure_backend`, and
`secure_erasure_enabled` after the complete 0.27.0 result prefix. A successful
run always records the exact winner allocation size and policy version 1.
Disabled results record backend `NONE`; enabled results record the selected
backend. Destruction requires callers to preserve these owner fields, erases
when the enabled metadata is canonical, releases the sole owner, and zeros the
complete result. Repeated destruction is a no-op.

Observer and stop views use the result's retained `best_genome_size` rather
than independently reconstructing the allocation count. The view schema does
not change because it already exposed this exact field.

### Private population evidence and ownership transfer

Every successful population allocation records policy version 1, the enabled
flag, and either `NONE` or the build-selected backend next to the existing
`storage_bytes` and `evaluation_bytes`. Every private lifecycle validator
reconciles that metadata with the active configuration before using or moving
the owner.

Child construction copies the configuration policy into the new independent
owner. Atomic generation advancement moves both child allocations, their exact
counts, and their erasure metadata together, empties the child handle, and then
destroys the inaccessible former parent. The promoted allocations therefore
remain live while both former-parent ranges receive their one terminal
disposition.

Evaluation allocation remains provisional until all validity and fitness
callbacks succeed. Any failure erases that checked local range before release.
After attachment, diversity failure erases and releases the evaluation owner,
zeros all evaluation metadata, and leaves the genome owner available for its
own terminal cleanup. A partial population containing only the genome slab is
handled by the same per-owner rule.

### Portability boundary

`src/secure_erasure.c` is the sole non-optimizable erasure boundary.

- CMake uses a declaration check for `explicit_bzero` with `string.h` and
  defines `EVO_HAVE_EXPLICIT_BZERO=1` only for the library when available.
- GNU Autoconf checks the same symbol and emits the same internal definition.
- The implementation exposes `EXPLICIT_BZERO` as the selected backend when
  detected.
- Otherwise it uses a reviewed `volatile unsigned char` loop and reports
  `VOLATILE_BYTES`.

The fallback writes one zero to every index in ascending byte order. It does
not use `memset`, inline assembly, custom cryptography, or an unresolved size.
Both backend builds execute the same lifecycle test. The fallback is a process-
memory best effort at a C portability boundary; it is not a broader media-
sanitization guarantee.

## Human-Readable Abstraction Assessment

This change adds no compressed, probabilistic, cached, indexed, pooled, or
otherwise accelerated representation. Direct owner fields and exact byte
counts are canonical authority, so ADR-0026's accelerator-equivalence gate is
not applicable.

The stable registry table above is nevertheless the human-readable lifecycle
projection requested by issue #50. Tests traverse every registry class and
prove the projected exact-once disposition against the actual erasure/release
events. A future recycled-owner pool would be an accelerator and must project
its live and reusable owner registry independently under ADR-0026.

## Consequences

- Zero-initialized consumers preserve ordinary-release behavior without a new
  performance cost or security claim.
- Opted-in consumers receive one bounded process-memory erasure for every EVO-
  owned genome and evaluation allocation on success and failure paths.
- Result and private population owners retain exact allocation counts instead
  of recomputing destruction lengths from mutable configuration.
- Public structure sizes change, so consumers must rebuild against 0.28.0.
- Backend identity can differ across supported platforms while evolutionary
  search decisions and RNG traces remain unchanged.
- Consumers remain responsible for their own copies, callback state, aliases,
  checkpoints, logs, and platform-level data-remanence policy.

ADR-0030 composes checkpoint restore with this lifecycle in EVO 0.29.0.
Restored population-genome, evaluation, and result allocations use the same
reviewed owner constructors and register the restoring build's local backend.
Checkpoint input, output, and retained copies remain caller-owned cleartext
ranges that EVO never erases or releases. Source-process backend metadata is
audit evidence only and never selects the restoring disposition.

## Alternatives considered

### Claim that ordinary `free` scrubs storage

Rejected because the C allocation contract provides no such guarantee.

### Always erase every allocation

Rejected because it would silently change the established default lifecycle,
impose cost on ordinary data, and overstate a policy that consumers did not
request.

### Use `memset` before `free`

Rejected because dead-store elimination may remove the write and the repository
security profile treats generic `memset` as an inappropriate secret-erasure
boundary.

### Keep the result size only in the original problem object

Rejected because `evo_result_destroy` is deliberately independent of problem
and configuration lifetime. The owner must retain its own exact count.

### Maintain a global pointer registry

Rejected because it would introduce shared mutable state, address-based audit
identity, synchronization requirements, and an opaque secondary authority.
The direct fixed owner registry is simpler and exact.

## Verification

- `tests/secure_erasure_test.c` wraps allocation, erasure, and release to prove
  exact sizes, adjacent erase-before-release order, and exact-once disposition
  for ordinary mode, generation-zero success, result destruction, population
  registry state, generation promotion, every allocation failure, all-invalid
  winner-transfer failure, generation-zero and child provisional-evaluation
  failure, and attached-evaluation rollback.
- The test passes with detected `explicit_bzero` and with the volatile fallback.
- Checkpoint restore tests prove local-backend registration, exact-once erasure
  of every restored owner on success and failure, and the exclusion of caller-
  owned checkpoint buffers from EVO's erasure authority.
- Existing allocation-failure and lifecycle tests prove complete reset and
  repeatable destruction under the unchanged default.
- The installed consumer validates the appended result audit fields through
  only the installed public header and library.
- CMake, GNU Autotools, and AES-BLD-001 enumerate the same twenty-four
  production sources and twenty-eight normative tests.
- GitHub issue: `https://github.com/dlworrell/evo/issues/50`
