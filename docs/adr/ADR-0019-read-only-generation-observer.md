# ADR-0019: Synchronous Read-Only Generation Observer

Status: Accepted
Date: 2026-08-02
Decision owner: EVO

## Context

EVO 0.18.0 computes deterministic statistics for generation zero and every
successfully promoted child, but retains only the latest record. That
constant-space result is sufficient for final reporting and insufficient for a
consumer that needs to stream each committed generation to a bounded log,
progress display, or evidence sink.

Giving a callback `evo_result_t *` would expose writable ownership of the
global genome. Passing internal population storage would expose private slabs
whose owner changes during atomic promotion. Retaining every record in the
result would instead make storage and allocation failure scale with
`generation_limit`.

## Decision

EVO 0.19.0 defines generation-result-view schema version 1 and the callback:

```c
typedef void (*evo_generation_observer_fn)(
    const evo_generation_result_view_t *result,
    const evo_generation_statistics_t *statistics,
    void *context);
```

`evo_config_t` appends `generation_observer` and
`generation_observer_context`. A null callback disables observation. The
context is caller-owned observer state; EVO neither interprets nor retains it,
and it is distinct from the evolutionary callback context passed to
`evo_run`.

For each event, EVO constructs two independent stack snapshots. The result
view contains its schema version, a bounded `const void *` global-best genome
view, the exact byte bound, complete global-best fitness, completed-generation
count, run seed, and the stop classification applicable at that generation.
The statistics snapshot is the complete schema-version-1 record for the
committed population.

The snapshot objects and the genome view are valid only until the callback
returns. The observer must not retain their addresses, cast away `const`, free
or reallocate the genome, or use the view after return. Copying scalar or
fitness values into caller-owned storage is permitted. EVO exposes no writable
population, result, or genome pointer through the callback.

## Ordering

The observer is synchronous and serial. One invocation completes before EVO
begins work for the next generation.

Generation zero is observed only after evaluation, statistics construction,
global-winner transfer, and generation-zero commitment all succeed. A
zero-limit run reports `EVO_TERMINATION_GENERATION_LIMIT`; a positive-limit run
reports `EVO_TERMINATION_NONE` because execution continues.

A child is observed only after its evaluation and statistics succeed, atomic
promotion completes, `generations_completed` and the latest statistics are
updated, any strict global improvement is copied, and the stop decision is
known. Intermediate children report `EVO_TERMINATION_NONE`. The last requested
child reports `EVO_TERMINATION_GENERATION_LIMIT`. A promoted all-invalid child
reports `EVO_TERMINATION_ALL_INVALID` while its result view still exposes the
earlier valid global winner.

The terminal reason in the callback view is decision evidence. The owning
public result continues to publish its final reason only after private cleanup
succeeds, preserving the atomic public-result contract established in
ADR-0017.

## Failure and Control Semantics

The callback returns `void`. It cannot request cancellation, change a status,
replace a winner, or alter EVO control flow. Application stopping is delivered
separately by EVO 0.20.0 and ADR-0020.

No event is emitted for an invalid configuration, failed generation-zero
evaluation, failed winner transfer, provisional child, failed child evaluation,
failed statistics reduction, or failed promotion. If a later generation fails,
events already delivered for earlier committed generations remain valid even
though `evo_run` destroys the final public result and returns an error.

Observer delivery allocates no memory, consumes no RNG word, changes no
selection or fitness comparison, and adds no retained history. It is not
asynchronous, concurrent, or reentrant delivery infrastructure.

## ABI Consequences

`evo_generation_result_view_t` and `evo_generation_observer_fn` are new public
types. The two observer fields are appended to `evo_config_t`, preserving every
pre-0.19.0 config member offset. `sizeof(evo_config_t)` and array stride change,
so consumers must rebuild against the 0.19.0 header. No installed function
signature or symbol changes.

## Alternatives Considered

### Pass `const evo_result_t *`

Rejected because its `best_genome` member has writable pointee type and the
structure contains owning state rather than a callback-lifetime view.

### Pass internal population storage

Rejected because it would expose mutable private ownership and couple the
public API to promotion and recycling implementation details.

### Retain an event array

Rejected because storage and allocation failure would grow with the configured
run bound.

### Let the observer return a stop decision

Rejected because observation and application stopping have different ordering,
status, replay, and recovery contracts. ADR-0020 owns stopping.

### Deliver events asynchronously

Rejected because copied ownership, queue bounds, synchronization, and callback
concurrency are outside the deterministic sequential core.

## Verification

- `tests/observer_test.c` proves one event for a zero-limit run, N+1 events for
  N transitions, exact winner/statistics/termination ordering, fixed-seed
  replay, all-invalid terminal evidence, independent snapshot addresses, and
  absence of events for failed or provisional generations.
- `tests/allocation_failure_test.c` proves observer delivery changes no
  allocation or release count and emits only already-committed events before a
  later injected failure.
- `tests/evo_lifecycle_test.c` locks the append-only config layout and result-
  view schema version.
- The installed consumer exercises the public observer signature and one
  terminal generation-zero event.
- GitHub issue: `https://github.com/dlworrell/evo/issues/41`
