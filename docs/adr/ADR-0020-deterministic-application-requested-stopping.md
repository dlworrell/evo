# ADR-0020: Deterministic Application-Requested Stopping

Status: Accepted
Date: 2026-08-02
Decision owner: EVO

## Context

EVO 0.19.0 can stream every committed generation to a read-only observer, but
that observer deliberately has no control return. Consumers that already have
a deterministic domain-specific target, budget signal, or externally computed
quality threshold would otherwise have to run the complete configured
generation bound and discard later work.

Stopping inside selection, production, evaluation, statistics, or promotion
would expose provisional state and make generation counts, winner ownership,
callback order, and replay dependent on partial work. Reusing the observer as a
control callback would also combine two contracts with different purposes and
make the final observer event unable to report the decision it caused.

## Decision

EVO 0.20.0 defines:

```c
typedef bool (*evo_generation_stop_fn)(
    const evo_generation_result_view_t *result,
    const evo_generation_statistics_t *statistics,
    void *context);
```

`evo_config_t` appends `generation_stop` and
`generation_stop_context`. A null callback disables application stopping. The
context is caller-owned decision state; EVO neither interprets nor retains it,
and it is independent of the evolutionary and observer contexts.

The callback receives independent stack snapshots with the same versioned,
read-only, callback-lifetime contract as the generation observer. Its result
view exposes the committed global winner, exact genome byte bound, complete
fitness, completed-transition count, run seed, and
`EVO_TERMINATION_NONE`. Its statistics view exposes the complete record for
that committed generation. Returning `true` requests successful termination at
that exact commit; returning `false` permits the next transition.

The callback cannot reject or retry a generation, return an error status,
replace a winner, mutate EVO ownership, or request rollback. A true decision
maps to `EVO_TERMINATION_APPLICATION_REQUESTED` and `EVO_SUCCESS`.

## Eligibility and Precedence

EVO asks for an application decision only when the current generation is
committed and another child transition would otherwise be permitted.

- Generation zero is eligible when `generation_limit` is positive.
- A promoted child is eligible when it has a valid generation-local winner and
  its completed-transition count is less than `generation_limit`.
- A zero-limit generation zero, the final permitted child, and an all-invalid
  child are structurally terminal and do not invoke the callback.

This rule gives limit and all-invalid termination unambiguous precedence. It
also preserves `generation_limit` as a hard upper bound and avoids invoking
application code after EVO already knows that no next transition is legal.

An immediate generation-zero request therefore returns a valid winner with
`generations_completed == 0`. An intermediate request includes every promoted
child through the requested stop generation. A never-stop callback produces
the same evolutionary work, winner, statistics, RNG schedule, and structural
termination as a null callback.

## Ordering

For every committed generation, EVO performs the following no-fail control
suffix after all fallible generation work succeeds:

1. commit statistics, completion count, and any strict global-winner update;
2. classify limit or all-invalid structural termination;
3. if still continuing, invoke `generation_stop` and classify a true return as
   application-requested termination;
4. invoke `generation_observer` with the final classification; and
5. begin the next child only when the classification remains `NONE`.

The stop callback therefore always sees `EVO_TERMINATION_NONE`; the observer
that follows it sees `EVO_TERMINATION_APPLICATION_REQUESTED` when the decision
was true. Both callbacks complete synchronously before any next-generation
allocation, RNG consumption, or consumer operation.

The owning public result publishes its final nonzero reason only after private
population cleanup succeeds. If later child work fails after earlier false
decisions, the final public result is destroyed and returned empty, while
already completed callback invocations remain historical caller-side effects.

## Resource and Replay Consequences

Decision delivery allocates no EVO memory, retains no history, consumes no RNG
word, changes no selection or fitness comparison, and introduces no new
resource budget. It is synchronous and serial. Deterministic replay requires
the callback and its caller-owned context to return the same decisions for the
same ordered views.

The callback must not retain either view address, retain or write through the
genome alias, cast away `const`, free or reallocate result storage, or use any
view after return. Its context must not provide a mutable alias through which it
changes the problem, configuration, public result, or EVO-owned genome. EVO
does not make application callback code safe against undefined behavior,
hidden entropy, blocking, or unrecorded external state.

## ABI Consequences

`evo_generation_stop_fn` is a new public type and
`EVO_TERMINATION_APPLICATION_REQUESTED` is appended to the public termination
enumeration. The two stop fields are appended to `evo_config_t`, preserving
every pre-0.20.0 member offset. `sizeof(evo_config_t)` and array stride change,
so consumers must rebuild against the 0.20.0 header. No installed function
signature, result layout, symbol, allocation class, or memory-policy field
changes.

## Alternatives Considered

### Let the observer return a decision

Rejected because observation remains a passive evidence sink and must receive
the final stop classification after control has been resolved.

### Invoke the callback on structurally terminal generations

Rejected because its return would need an arbitrary precedence rule and would
execute application control code when no further transition is possible.

### Poll during child work

Rejected because partial generation state is not committed, has no stable
public ownership, and cannot be represented by the existing result and
statistics views.

### Add signals, timeouts, or cross-thread cancellation

Rejected for this boundary because those mechanisms require asynchronous
ownership, synchronization, platform, and recovery contracts. They remain
outside issue #42.

## Verification

- `tests/application_stop_test.c` proves immediate, intermediate, never-stop,
  zero-limit, all-invalid, and failed-child behavior; exact stop-before-observer
  ordering; result and statistics evidence; structural precedence; null-
  callback replay; and callback-lifetime const views.
- `tests/allocation_failure_test.c` proves stop decisions add no allocation or
  release and occur only for generations committed before an injected later
  failure.
- `tests/evo_lifecycle_test.c` locks the append-only config layout and initial
  application termination value.
- The installed consumer exercises the public callback signature and immediate
  application-stop result.
- GitHub issue: `https://github.com/dlworrell/evo/issues/42`
