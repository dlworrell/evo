# EVO-HRA-004: Parallel Evaluation Human-Readable Abstraction Audit

Status: Complete
Date: 2026-08-08
Repository: `dlworrell/evo`
Audited implementation: EVO 0.31.0
Governing decisions: ADR-0026 and ADR-0032
Tracking issue: #53

## Question

Does bounded parallel evaluation make a runtime queue, thread identity,
completion timing, cancellation race, or partial result opaque authority over
an EVO generation?

## Classification

The worker scheduler is an exact execution accelerator. It overlaps independent
calls to a caller-declared thread-safe evaluator. The explicit serial evaluator
path remains executable and defines the same candidate fitness, stable winner,
statistics, stopping, and result semantics.

The implementation contains no global executor, work-stealing deque, dynamic
queue node, address-keyed registry, cached fitness, compressed result set,
membership filter, probabilistic decision, or cross-run worker state. POSIX
thread handles and atomics coordinate execution only and cannot commit a
candidate.

## Ordered Projection

`evo_evaluation_schedule_t` is the complete synchronous audit projection for
one worker attempt. Its assignment rows are ordered by population index:

| Logical fact | Projection | Exact authority |
|---|---|---|
| Scope | Population generation, population size, policy/view version | Completed population lineage and policy constants |
| Bound | Configured logical worker count and exact library scratch bytes | Validated configuration and checked scratch-size query |
| Assignment | One-based worker identity and zero-based dispatch wave per candidate | `worker = index mod W`, `wave = floor(index / W)` |
| Eligibility | `NOT_VALIDATED`, `EXCLUDED`, or initially `PENDING` | Ascending serial validity pass and private record |
| Completion | `COMPLETED`, `FAILED`, or `CANCELED` plus aggregate counts | Acquire-observed complete wave and ascending fitness validation |
| Failure | Lowest failure index and stable logical worker identity | First malformed record in ascending wave order |
| Commit | Presence flag and ascending valid-candidate commit ordinal | Private record attachment and completed-population validation |
| Outcome | Committed, fitness rejected, worker start failed, or worker join failed | Joined attempt and population-evaluation status |

The array contains one row for every candidate. There is no omitted queue,
partial page, runtime completion-order list, pointer, native thread identifier,
timestamp, or hidden continuation token. The borrowed view exists only during
the observer call, after every worker has terminated.

## Authority and Failure Behavior

Private candidate-evaluation records remain exact authority. A release/acquire
wave barrier makes all assigned writes observable before the coordinator reads
them. The coordinator validates each complete wave and later commits records in
ascending candidate order. Neither callback completion order nor the schedule
view can change comparison, statistics, stopping, checkpoint, or result state.

Worker-start failure occurs while every started worker still waits at epoch
zero. EVO stops and joins those workers before validity or evaluator callbacks.
A non-finite result lets the current wave finish, identifies failures in stable
index order, marks every later pending assignment canceled, and commits zero
records. Join failure also commits nothing and does not release scratch until
the worker has terminated. Allocation or later diversity failure leaves no
completed population or public result.

Cancellation is deliberately wave bounded. It suppresses future waves after a
detected failure but does not attempt unsafe asynchronous unwinding of an
arbitrary C callback. The projection distinguishes already completed work from
canceled future work, so it never presents callback execution as commit.

Checkpoint format 3 binds the declaration, worker count, scratch budget,
observer presence, and committed population policy provenance. It contains no
transient scheduler state. Capture occurs only after join and commit; resume
reattaches local runtime resources and begins with the next generation.

## Differential and Concurrency Evidence

- Worker counts 1 through 4 are compared with serial evaluation using identical
  initialized genomes, invalid candidates, fitness records, stable best,
  diversity, and completed population state.
- Multi-generation runs compose parallel evaluation with the two-slot recycler
  and compare complete public results and final statistics with the serial
  reference.
- Every successful schedule proves the exact `index mod W` mapping, wave,
  exclusion disposition, and ascending valid-candidate commit order.
- A non-finite candidate in the first three-wide wave proves that the other two
  wave callbacks may complete, five later assignments cancel, and zero records
  commit.
- An injected second-worker creation failure proves the first worker joins and
  no validity or evaluator callback occurs.
- An injected join failure plus recovery join proves all three workers are
  reaped before schedule delivery and rollback; completed rows remain
  explicitly uncommitted.
- Wrapped allocation tests prove one exact worker-scratch allocation, complete
  release, and atomic failure at that allocation.
- Format-3 checkpoint tests prove configuration/provenance projection and
  deterministic parallel resume suffixes.
- Hosted ThreadSanitizer runs the focused scheduler test; strict GCC, Clang,
  ASan, UBSan, analyzer, and both native build frontends cover the same source.

## Result

EVO 0.31.0 bounded parallel evaluation conforms to the Human-Readable
Abstraction Rule. Runtime scheduling accelerates callback execution but cannot
become authority. The serial reference remains available, exact candidate
records commit in stable order, and the complete schedule is projected in
candidate-domain terms independent of physical threads and timing.

This finding does not pre-approve dynamic work stealing, persistent pools,
distributed workers, asynchronous cancellation, callback timeouts, cached
fitness, compressed assignment sets, or source-optimizer process scheduling.
Each requires its own reference semantics, projection, resource and cleanup
contract, and differential evidence under ADR-0026.
