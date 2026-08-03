# ADR-0020: Deterministic Application-Requested Stopping

Status: Accepted
Date: 2026-08-02
Decision owner: EVO

## Context

EVO 0.19.0 can report every committed generation, but its observer deliberately
returns `void`. The bounded run can end only at the configured hard generation
limit or after promoting an all-invalid child. Consumers need a deterministic
way to stop after inspecting committed winner and statistics evidence without
exposing provisional state, weakening the hard bound, or coupling EVO to
signals, clocks, threads, or event loops.

Reusing the observer's return value would combine notification and control,
make callback ordering ambiguous, and break the accepted 0.19.0 observer
signature. Evaluating a decision before promotion would expose state that may
still fail. Evaluating it after the hard limit or all-invalid extinction would
compete with an already established natural reason.

## Decision

EVO 0.20.0 defines:

```c
typedef bool (*evo_generation_stop_fn)(
    const evo_generation_result_view_t *result,
    const evo_generation_statistics_t *statistics,
    void *context);
```

`evo_config_t` appends `generation_stop` and `generation_stop_context` after
the observer fields. A null callback disables application stopping. The
context is caller-owned and independent of both the observer context and the
evolutionary callback context passed to `evo_run`.

The callback receives independent stack snapshots using the existing result-
view and statistics schemas. Its result view contains the committed global
winner, exact genome byte bound, complete best fitness, completed-transition
count, run seed, and `EVO_TERMINATION_NONE`. Returning false continues the
bounded run unchanged. Returning true selects successful termination at that
exact commit and ultimately publishes
`EVO_TERMINATION_APPLICATION_REQUESTED`.

The views and bounded genome alias are non-owning and valid only until return.
The callback must not retain them, cast away `const`, mutate or release the
global genome, or operate on the active public result. It may copy bounded
evidence into caller-owned storage.

## Ordering and Precedence

EVO invokes the stop decision only after a generation is fully committed:

1. generation-zero evaluation, statistics, and global-winner transfer succeed;
   or child evaluation, statistics, atomic promotion, completion-count update,
   and any strict global-winner update succeed;
2. EVO classifies natural termination; and
3. only when the natural reason is `EVO_TERMINATION_NONE` and another child
   remains within the hard limit does EVO invoke the stop callback.

A zero-limit run, the final hard-limit generation, and a promoted all-invalid
child suppress the application decision. Natural reasons therefore retain
precedence and the configured limit remains a hard upper bound.

If both callbacks are configured, EVO invokes the stop decision first. It then
constructs fresh, independent observer snapshots. A true decision therefore
shows `EVO_TERMINATION_NONE` to the stop callback and
`EVO_TERMINATION_APPLICATION_REQUESTED` to the observer for the same committed
generation. Each synchronous callback returns before EVO continues or cleans
up.

No callback is invoked for provisional child state or a child that fails
production, evaluation, statistics, or promotion. Events already delivered for
earlier commits remain valid if later work fails, while the owning public
result still follows its complete empty-failure contract.

## Replay and Resource Semantics

Stop delivery allocates no engine storage, consumes no RNG word, changes no
fitness comparison, and retains no history. A null callback executes and
observes exactly the 0.19.0 sequence. A callback that always returns false sees
generations `0..generation_limit - 1`; it cannot replace the natural final
limit event at `generation_limit`.

Application code that requires replay must make its decision deterministic for
the provided snapshots and recorded context. This API does not define signal
handling, wall-clock deadlines, cross-thread cancellation, asynchronous
delivery, reentrancy, or I/O event-loop integration.

Bounded-run policy evidence advances to version 2 and records the final
termination reason plus whether application stopping selected it. The evidence
remains private and adds no public allocation or ownership class.

## ABI Consequences

`evo_generation_stop_fn` is a new public type. The two stop fields are appended
to `evo_config_t`, preserving every pre-0.20.0 member offset. The
`EVO_TERMINATION_APPLICATION_REQUESTED` enumerator is appended with value 3.
`sizeof(evo_config_t)` and array stride change, so consumers must rebuild
against the 0.20.0 header. No installed function signature, result layout,
symbol, or resource-budget field changes.

## Alternatives Considered

### Let the observer return `bool`

Rejected because it would break the accepted public signature and conflate an
evidence sink with a control decision.

### Evaluate before child promotion

Rejected because provisional state may still fail and cannot be exposed as a
valid retained result.

### Let application stopping override a natural reason

Rejected because the hard limit and all-invalid extinction are already known,
unambiguous outcomes. Callback suppression preserves reason precedence.

### Add signals, clocks, or atomics

Rejected because those introduce platform, concurrency, and replay contracts
outside this deterministic serial boundary.

## Verification

- `tests/application_stop_test.c` proves immediate and intermediate stopping,
  stop-before-observer ordering, independent snapshots, exact winner and
  statistics retention, natural-reason precedence, null/never-stop replay, and
  absence of callbacks for provisional or failed work.
- `tests/allocation_failure_test.c` proves immediate stopping adds no allocation
  or child transition and preserves exact cleanup counts.
- `tests/evo_lifecycle_test.c` locks the appended enum value and config layout.
- `tests/bounded_run_test.c` locks private policy version 2 and its terminal
  evidence.
- The installed consumer exercises both callbacks and the public application
  termination reason.
- GitHub issue: `https://github.com/dlworrell/evo/issues/42`
