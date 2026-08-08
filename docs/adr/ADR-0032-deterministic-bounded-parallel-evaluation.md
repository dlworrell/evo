# ADR-0032: Deterministic Bounded Parallel Evaluation

Status: Accepted
Date: 2026-08-08
Decision owner: EVO

## Context

Through EVO 0.30.0, hard-valid candidates are evaluated one at a time in
ascending population order. Candidate evaluation is independent once validity
has been established, so the evaluator is the first core phase that can use
bounded in-process parallelism without changing selection or operator streams.

Unconstrained callback concurrency would weaken three existing guarantees.
Worker creation and temporary memory must remain caller bounded. Completion
order must not become result, ranking, statistics, stopping, or checkpoint
authority. A failure must not publish a partially evaluated population, and
every started worker must stop before temporary state is released.

A runtime work queue is also an accelerated structure under ADR-0026. Physical
queue layout, operating-system thread identity, and timing cannot remain opaque
authority. Reviewers need an explicit candidate-to-worker assignment,
completion/cancellation disposition, and committed-result order independent of
the runtime scheduler.

## Decision

EVO 0.31.0 defines parallel-evaluation policy version 1 and evaluation-schedule
view version 1. It advances bounded-run policy to version 12,
child-evaluation and generation-advancement policies to version 9, and
checkpoint format and its two public views to version 3.

### Compatibility and callback declaration

`evo_problem_t` appends `evaluation_callback_thread_safety`:

- `EVO_EVALUATION_CALLBACK_SERIAL` is zero and declares that `evaluate` may be
  called only by the caller thread;
- `EVO_EVALUATION_CALLBACK_THREAD_SAFE` declares that concurrent calls may use
  the same consumer context and independent read-only genome ranges safely.

`evo_config_t` appends the worker count, maximum library worker-scratch bytes,
schedule observer, and observer context. A zero worker count requires a zero
scratch budget and null schedule observer and executes the exact pre-0.31.0
serial path. It preserves evaluation order, callback order, allocation count,
failure point, result bytes, and policy provenance.

A positive worker count requires the thread-safe declaration, is at most
`population_size`, and must have a scratch budget at least the exact value
reported by `evo_evaluation_worker_scratch_size`. The library creates exactly
that many POSIX worker threads. The calling thread coordinates them and never
invokes `evaluate` concurrently as a hidden additional worker.

Only `evaluate` is concurrent. Initialization, `is_valid`, distance,
selection, operators, stopping, generation observation, schedule observation,
storage observation, and checkpoint delivery remain synchronous. The consumer
owns synchronization for any state shared by concurrent `evaluate` calls.

### Fixed assignment and wave barrier

For population index `i` and configured worker count `W`:

```text
worker_identity(i) = (i mod W) + 1
dispatch_wave(i)   = floor(i / W)
```

Worker identities are stable one-based logical labels, not `pthread_t` values,
addresses, operating-system identifiers, or completion ranks. `is_valid` runs
serially for every candidate in ascending index order before the first
evaluation wave. Hard-invalid candidates retain their fixed assignment for
audit but never call `evaluate`.

One atomic epoch starts a wave. Each worker evaluates at most its one assigned
hard-valid candidate for that wave and then publishes a completed epoch with
release semantics. The coordinator waits with acquire semantics for every
worker before inspecting that wave. At most `W` evaluator callbacks are active,
and no later wave begins until the complete current wave is observable.

Worker execution order inside a wave is deliberately irrelevant. No clock,
duration, address, completion rank, or operating-system scheduling fact enters
fitness, stable-best reduction, statistics, stopping, or evidence.

### Stable validation and atomic commit

After a complete wave, the coordinator validates fitness in ascending candidate
order. If all waves succeed, it marks valid records evaluated, derives the
stable generation-local best, and assigns commit ordinals in ascending valid-
candidate order. Only then does population evaluation attach the record owner
and publish valid count, best index, policy version, worker count, and completed
state. Diversity measurement remains serial and must also succeed before the
population is externally usable.

Worker count `1` therefore has the same fitness, winner, statistics, stopping,
and result semantics as serial execution. Counts `1` through the configured
bound differ only in evaluator overlap and their declared schedule projection.
The scheduler performs no RNG operation and changes no initialization,
selection, crossover, mutation, or adaptive stream.

### Failure and cancellation

All workers initially wait at epoch zero. A worker-start failure sets the stop
flag and joins every worker that did start before validity or evaluation
callbacks occur. The schedule identifies the logical worker that failed to
start and leaves every candidate `NOT_VALIDATED`.

A non-finite fitness fails its complete wave. Other valid candidates in that
already dispatched wave may have completed, but none is committed. Every
still-pending candidate in later waves becomes `CANCELED`; no later evaluator
callback runs. The first failure index is the lowest failing index in the
wave. The coordinator then stops and joins every worker before observer
delivery or scratch release.

A join failure is a state failure. EVO waits until the affected worker has
terminated and makes one recovery join attempt so the native thread is reaped
before scratch release. If recovery also fails, EVO detaches the terminated
POSIX handle. It publishes no population and reports the join-failure schedule.
Evaluation-array allocation failure, worker-scratch allocation failure, worker
start/join failure, non-finite fitness, and later diversity failure all leave
the generation uncommitted. The enclosing public run releases or resets every
provisional owner and leaves an empty result.

Evaluation callbacks are ordinary C functions and cannot be forcibly unwound
safely. Cancellation therefore means that no future wave is dispatched after
a detected failure; all callbacks in the current wave are allowed to return and
are joined. EVO provides no signal-based, asynchronous, or time-based
cancellation.

### Bounded memory and worker lifetime

The positive worker path makes one library `calloc` whose exact size is:

- `W` private worker records;
- alignment padding checked with `size_t` arithmetic; and
- `population_size` explicit assignment records.

The size query rejects zero population, `W > population_size`, or arithmetic
overflow and performs no allocation, callback, or thread operation. Run
preflight rejects an insufficient budget before population allocation or any
consumer callback. The scratch range is zeroed and released only after every
worker has terminated and the synchronous schedule observer returns.

The budget covers EVO's temporary scheduler allocation. POSIX implementation
and operating-system thread stacks are platform runtime resources; their number
is bounded exactly by `W`, but their implementation-defined byte size is not
represented as library scratch. EVO creates no pool, spare worker, nested
worker, global executor, or thread that survives one population evaluation.

### Human-readable schedule projection

`evo_evaluation_schedule_t` is the mandatory ADR-0026 projection for every
configured worker attempt. It contains policy and view versions, population
generation and size, worker count, exact library scratch bytes, validation,
invalid, scheduled, completed, failed, canceled, and committed counts, first
failure and logical worker identity when applicable, final outcome, and
complete disposition.

Its candidate-ordered assignment array exposes population index, stable logical
worker identity, wave, final disposition, and an explicit commit-presence flag
plus commit ordinal. It is complete for the population and requires no
pagination. The observer receives the projection synchronously after all
workers terminate and may copy it during the call. It cannot stop execution,
modify the run, retain the borrowed array, or treat the view as evaluation
authority.

Private candidate records and the completed-population validator remain exact
authority. A runtime queue, atomic epoch, thread handle, callback completion
order, and the projection itself cannot independently commit a record. Failure
schedules are diagnostic evidence and explicitly contain zero committed
records.

### Checkpoint and resume amendment

Checkpoint format 3 uses magic `EVOCKPT3`. Canonical configuration appends the
thread-safety declaration, worker count, scratch budget, and schedule-observer
presence. Committed population state appends parallel policy version and worker
count. The checkpoint view projects both configuration and committed
provenance.

No thread handle, atomic epoch, runtime queue, in-progress assignment, callback
completion order, or transient scratch byte is serialized. Checkpoints are
delivered only after evaluation, worker join, stable commit, promotion, and
ordinary committed-generation callbacks complete. Resume validates format 3
and exact canonical parallel configuration before allocation, reattaches the
runtime observer and consumer context, and starts new workers only for the next
generation. Formats 1 and 2 reject by version rather than inventing missing
parallel-policy state.

## Human-Readable Abstraction Assessment

The bounded scheduler accelerates independent evaluator calls and therefore
falls under ADR-0026. Serial evaluation remains the exact executable reference
path. The complete schedule projection maps every candidate to a stable logical
worker and wave and records its final completion, failure, cancellation, or
commit state in domain order. It exposes no queue node, atomic object, pointer,
or platform thread identity.

Differential tests compare workers `1..W` with serial execution across invalid
candidates and multiple recycled generations. Separate tests cover fixed
assignment, stable commit order, non-finite wave cancellation, worker-start
failure before callbacks, allocation exhaustion, checkpoint/resume, and
ThreadSanitizer execution. EVO-HRA-004 retains the detailed audit.

## Consequences

- Consumers can opt into bounded concurrent fitness evaluation without making
  completion timing algorithmic input.
- The evaluator and shared consumer context must satisfy the caller's explicit
  thread-safety declaration.
- The worker path adds one exact temporary allocation per population
  evaluation and exactly `W` short-lived POSIX threads.
- Failed waves may execute more evaluator callbacks than the first failing
  index, but commit no record and dispatch no later wave.
- Public problem/configuration/checkpoint layouts grow; consumers must rebuild.
- Checkpoint format 3 is explicitly incompatible with formats 1 and 2.
- Serial zero initialization retains the complete 0.30.0 evaluation path.

## Alternatives considered

### Commit callbacks in completion order

Rejected because operating-system scheduling would become ranking, statistics,
failure, and replay authority.

### Use a shared dynamic work-stealing queue

Rejected for policy version 1. It adds hidden assignment, cancellation, and
allocation state without improving the bounded fixed-population contract.

### Evaluate validity concurrently

Rejected because validity has an established ascending callback order and need
not be thread-safe. Serial validity also fixes the exact evaluator domain before
workers run.

### Stop other callbacks immediately after one invalid fitness

Rejected because portable C cannot safely preempt an arbitrary consumer
callback. Wave-bounded cancellation joins the complete current wave and
suppresses all future work.

### Reuse a global worker pool

Rejected because cross-run lifetime, pool sizing, stale context, hidden queue
state, and cleanup authority exceed the issue's run-local bounded scope.

### Serialize the live scheduler in a checkpoint

Rejected because thread and queue state is platform-specific and provisional.
Only committed population provenance belongs in resumable authority.

## Verification

- `tests/evaluation_workers_test.c` proves configuration and scratch boundaries,
  serial equivalence for worker counts 1 through 4, stable mapping and commit
  order, hard-invalid exclusion, non-finite cancellation, injected start
  and join failures with complete termination, replay, and multi-generation
  recycling composition.
- `tests/allocation_failure_test.c` proves worker-scratch allocation failure is
  atomic and that the successful worker path has one additional allocation and
  release.
- `tests/checkpoint_test.c` proves format-3 projection, exact configuration
  binding, and parallel suffix replay.
- `tests/checkpoint_fuzz_test.c` rechecks every truncation and single-byte bit
  corruption against format 3.
- `.github/workflows/sanitizers.yml` runs the focused scheduler test under
  ThreadSanitizer in addition to the complete ASan/UBSan suite.
- CMake, GNU Autotools, and AES-BLD-001 enumerate the same source and test; CI
  covers GCC, Clang, Linux, macOS, installed consumers, and both build
  frontends.
