# ADR-0043: Bounded Parallel Source Orchestration

Status: Accepted

Date: 2026-08-16

## Context

EVO 0.41.0 can evolve canonical source-transformation recipes and synchronously
map one recipe to candidate, assurance, measurement, and fitness evidence. The
structured-search adapter deliberately leaves external process concurrency and
product-level resume to issue #66. The generic core already provides two useful
but insufficient primitives: deterministic bounded in-process evaluation workers
(issue #53) and a versioned deterministic core checkpoint (issue #51).

Compiler, linker, test, sanitizer, analyzer, and benchmark programs are not safe
to treat as ordinary in-process callbacks. They require isolated workspaces,
explicit process/resource policy, complete cancellation and join semantics, and
stable product identities that are outside the generic core checkpoint. Runtime
completion order also cannot become source-search authority: a faster worker may
finish first without becoming the first committed candidate.

The product boundary therefore needs a scheduler that is distinct from EVO's
in-process callback workers while still reusing the core's deterministic
candidate-order commit semantics. It also needs a checkpoint envelope that binds
the core checkpoint to the exact source-optimization experiment rather than
mistaking a generic evolutionary checkpoint for resumable project state.

## Decision

EVO 0.42.0 adds a private version-1 source-orchestration transaction to the
uninstalled source-optimizer foundation.

1. The orchestration transaction consumes one current structured-search
   configuration, one versioned external-worker policy, one exact product
   identity set, one caller-supplied external execution provider, and explicit
   nonzero resource bounds. It does not install a new public executable or alter
   generic EVO ranking semantics.
2. The structured-search adapter exposes the existing core parallel-evaluation
   controls for this private product use. Core evaluation workers are only
   deterministic callback dispatchers. They are not external candidate workers,
   do not execute compiler/test processes directly, and do not weaken the
   external scheduler's independent worker and resource limits.
3. A thread-safe orchestration evaluation adapter sits behind the structured
   search evaluation-provider boundary. Concurrent core callback invocations
   submit independent recipe evaluations to the external scheduler. Each
   submission receives one deterministic candidate key containing generation,
   population index, recipe fingerprint, and policy domain.
4. External logical worker assignment is deterministic and address-free.
   Version 1 assigns eligible candidate index `i` to logical worker
   `(i mod worker_count) + 1` within its generation. Runtime process IDs, thread
   IDs, queue nodes, completion timestamps, and workspace addresses are never
   authority.
5. Each candidate receives a private workspace derived from generation,
   population index, and candidate identity beneath one caller-declared
   orchestration root. Workspace names are evidence labels, not security
   authority. Canonical-path checks, candidate materialization, and the existing
   assurance boundary remain responsible for immutable baseline/source
   protection.
6. The scheduler enforces an explicit admission budget before provider start:
   maximum external workers, per-worker and aggregate CPU allowance, address
   space, descendant process count, storage bytes, output/evidence bytes, wall
   timeout, and retained workspace bytes. Arithmetic is checked before
   allocation or admission. No hidden worker or oversubscription path is
   permitted.
7. The external execution provider owns platform-specific process creation and
   enforcement. Its versioned capability/result record must state which CPU,
   memory, process-count, storage, filesystem, network, timeout, cancellation,
   and descendant-cleanup controls were actually enforced. A required control
   that cannot be enforced rejects the candidate before performance fitness.
8. The provider interface is asynchronous but bounded: start returns one opaque
   provider handle associated with a stable logical job identity; poll reports
   only state; cancel requests termination; join is mandatory and returns exact
   terminal evidence. Provider handles are runtime resources and never appear
   in deterministic evidence or checkpoints.
9. The scheduler maintains a transaction-wide failure latch. The first hard
   worker/provider failure prevents further external starts, requests
   cancellation of every active sibling job, and requires every started job to
   join before the evaluation transaction returns. A cleanup or join failure is
   a hard orchestration failure and produces no committed generation or trusted
   checkpoint.
10. Runtime completion order is diagnostic only. Candidate outcomes are staged
    by generation/population identity and become visible to structured search
    only through the generic core's stable candidate-order evaluation commit.
    Completion-order variation therefore cannot change selection, statistics,
    stopping, lineage, or champion authority.
11. A successful generation boundary is atomic. Every scheduled candidate for
    the generation has reached a terminal joined state, the core has committed
    candidate evaluations in stable population order, the structured-search
    lineage for that generation is complete, and no external worker remains
    active before an orchestration checkpoint or generation evidence bundle may
    be published.
12. Product checkpoint format version 1 wraps one validated generic EVO core
    checkpoint and a complete source-optimizer identity view. The wrapper binds
    baseline fingerprint, analysis fingerprint, catalogue identity/version,
    recipe schema, search policy, mutation/crossover/repair policy versions,
    evaluation-provider identity, external scheduler/provider policy identity,
    toolchain identity, workload identity, artifact/evidence schema identity,
    random seed, committed generation, and the complete committed
    source-search lineage prefix.
13. Product checkpoint integrity detection is explicit and is not
    authentication or encryption. The wrapper uses a versioned deterministic
    integrity field and canonical lengths before any nested checkpoint is
    inspected. Untrusted input is parsed under caller limits with checked
    arithmetic and atomic failure cleanup.
14. Resume validates the complete product identity view before any candidate
    process may start and before the nested core checkpoint is resumed. A stale
    baseline, analysis, catalogue, search/operator policy, toolchain, workload,
    execution-provider policy, or artifact schema rejects as an identity
    mismatch. Runtime paths and process handles are reattached resources and are
    never serialized.
15. Checkpoints are emitted only after committed generations. In-flight
    external jobs, partial candidate outputs, runtime queues, process handles,
    locks, and uncommitted generation results are never checkpoint authority.
    This makes resume equivalent to restarting immediately after the same
    committed generation boundary.
16. A resumed run reconstructs the scheduler from policy and stable candidate
    identities, reattaches provider callbacks/context, validates the retained
    lineage prefix against the product checkpoint, and continues using the
    nested core checkpoint. With unchanged identities and fixed seed, the
    resumed run must produce the same logical candidate sequence, rejection
    decisions, termination, and champion as uninterrupted execution.
17. Worker count is a scheduling/resource policy field, not a fitness or tie
    breaker. Separate serial and supported parallel runs may therefore have
    different orchestration schedule fingerprints while still being required
    to produce the same logical recipes, evaluation outcomes, committed search
    lineage, termination reason, and champion when the external provider is
    deterministic.
18. Orchestration evidence is canonical JSON plus complete Markdown. It records
    product identities, scheduler/resource policy, stable candidate-to-workspace
    and logical-worker assignment, start/terminal state, diagnostic completion
    ordinal, stable commit ordinal, cancellation cause, join/cleanup state,
    checkpoint identity, resume validation, and final champion lineage.
19. The initial implementation uses bounded arrays indexed by stable generation
    and population order plus direct deterministic scans. No work-stealing
    queue, cache, hash index, probabilistic membership structure, compressed
    checkpoint authority, or alternate ranking structure participates in
    scheduling admission, candidate commit, resume validation, cancellation,
    termination, or champion selection.
20. Destruction releases orchestrator-owned memory and runtime synchronization
    objects only after all started external jobs are joined. It does not remove
    independently committed candidate evidence, modify the source repository,
    commit, push, deploy, or publish product CLI behavior.

## External Worker State Machine

Each candidate job follows one exact state progression:

`unassigned -> admitted -> started -> terminal -> joined -> staged -> committed`

A job may instead move from `admitted` or `started` to `cancel-requested`, then
must still reach `terminal -> joined`. A provider-start failure moves directly
to terminal failure without inventing a process handle. `committed` is possible
only after the whole generation is known clean and the core commits the
candidate in stable population order.

Version 1 records stable terminal reasons including success, candidate rejected,
provider start failure, timeout, signal, CPU limit, memory limit, process limit,
storage limit, output limit, cancellation, join failure, cleanup failure,
capability unavailable, and provider protocol failure. Platform diagnostics may
be retained separately but cannot replace the stable reason.

## Deterministic Scheduling and Commit

The logical schedule is derived entirely from generation, population size,
configured external worker count, and stable candidate order. Provider runtime
completion may occur in any order. A monotonically increasing completion
ordinal is retained for diagnostics, while committed evidence is always ordered
by population index.

A candidate cannot observe another candidate's committed result while its own
generation is being evaluated. Selection, stopping, statistics, and champion
updates therefore see the same complete candidate set independent of operating
system scheduling.

## Product Checkpoint and Resume

The product checkpoint contains a canonical identity header, bounded source
lineage prefix, and exactly one generic EVO core checkpoint blob. The core blob
remains governed by its own format/integrity rules. The outer wrapper adds the
source-optimizer identities that issue #51 intentionally excluded.

Resume order is fail-closed:

1. validate outer envelope, lengths, resource bounds, and integrity;
2. validate every source-optimizer dependency identity;
3. validate the retained lineage prefix and committed-generation boundary;
4. validate the nested core checkpoint without executing candidate code;
5. reattach exact provider/context semantics;
6. reconstruct an empty external scheduler with zero active jobs; and
7. continue from the next uncommitted generation.

Any mismatch stops before step 6. No stale dependency can be repaired by
silently substituting the current value.

## Resource and Ownership Rules

Caller limits bound external worker count, scheduler records, active provider
handles, path bytes, workspace bytes, per-worker CPU/address-space/process/
storage/output/time budgets, aggregate active budgets, lineage bytes,
checkpoint bytes, JSON/Markdown bytes, and total orchestrator-owned memory.
Every allocation and aggregate budget calculation is checked for overflow.

The orchestration owner retains copied stable identity strings, scheduler/job
records, checkpoint bytes, checkpoint audit projection, resume evidence, and
canonical JSON/Markdown. Search configuration authorities and provider callback
contexts are borrowed only during a synchronous orchestration call. Runtime
provider handles are owned until mandatory join and are never copied into a
published result.

## Human-Readable Abstraction Assessment

No accelerated authority is introduced. The exact reference representation is
the complete candidate-ordered job array, complete committed lineage prefix,
and complete product checkpoint identity record. Assignment, admission, failure
propagation, cancellation, commit ordering, and resume validation use direct
bounded scans over those records.

Runtime queues, mutexes, condition variables, provider handles, and operating
system scheduling are implementation mechanisms only. Canonical evidence
projects every stable job identity and transition needed to reconstruct why a
candidate was admitted, canceled, rejected, staged, or committed. No summary
counter or fingerprint can independently accept, reject, rank, terminate, or
resume a run.

ADR-0026 accelerator-specific differential requirements are therefore not
applicable to a new accelerator. EVO-HRA-015 retains the change-specific audit.

## Consequences

- External compiler/test/benchmark concurrency remains bounded separately from
  generic EVO callback threading.
- Serial and parallel schedules must be logically equivalent even when their
  diagnostic completion order differs.
- Worker failure cannot expose a partial generation or leave a trusted
  checkpoint while sibling processes remain alive.
- Product resume rejects stale external identities before candidate execution.
- A core checkpoint alone remains insufficient to resume source optimization.
- Distributed workers, remote trust, GPUs, asynchronous public winners, product
  commands, deployment, and automatic downstream publication remain later or
  non-goal boundaries.

## Rejected Alternatives

- Reusing core callback worker count as the only external-process limit was
  rejected because callback threads are not process/resource isolation.
- Committing results in provider completion order was rejected because runtime
  timing would alter deterministic evolutionary authority.
- Checkpointing in-flight process handles or queues was rejected because those
  are host-local runtime resources and cannot be replay authority.
- Treating the generic core checkpoint as complete product state was rejected
  because it intentionally excludes baseline, toolchain, catalogue, workload,
  and artifact identities.
- Continuing after a sibling worker cleanup failure was rejected because escaped
  candidate processes make generation and checkpoint authority untrustworthy.
- A hash-indexed job/result cache was rejected because direct bounded arrays are
  sufficient and easier to audit for the initial implementation.

## Verification

Normative tests must cover worker counts one through the supported fixture bound,
forced out-of-order completion, deterministic candidate-to-worker/workspace
assignment, identical logical winners across serial/parallel runs, start failure,
timeout, cancellation propagation, join failure, cleanup failure, resource
admission exhaustion, and proof that no partial generation becomes committed.

Checkpoint fixtures must cover generation-zero and intermediate resume,
uninterrupted/resumed equivalence, truncation, corruption, unsupported version,
length overflow, stale baseline, stale analysis, stale catalogue, stale search
policy, stale toolchain, stale workload, stale provider policy, stale artifact
schema, active-result rejection, and allocation failure. Every stale identity
fixture must fail before the external provider observes a start request.

Hosted validation must exercise Linux CMake/Clang with sanitizers and race-aware
coverage where supported, Linux Autotools/GNU, macOS/Clang portability, schema
validation, independent reconstruction of commit order and checkpoint identity,
and AES-BLD-001/AES-SEC-001 inventory parity.

## Related Records

- ADR-0016
- ADR-0026
- ADR-0039
- ADR-0040
- ADR-0041
- ADR-0042
- EVO-001
- EVO-002
- EVO-HRA-014
- Issues #38, #51, #53, #57, #65, #66, #83, and #93
