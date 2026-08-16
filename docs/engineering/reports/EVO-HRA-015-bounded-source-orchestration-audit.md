# EVO-HRA-015: Bounded Source Orchestration Human-Readable Abstraction Audit

Date: 2026-08-16

Audited design: EVO 0.42.0 bounded parallel source-orchestration boundary

Governing records: ADR-0026, ADR-0043, EVO-002, issue #66

## Inventory

The 0.42.0 orchestration boundary introduces a private scheduler around
structured source-search evaluation and a product-level checkpoint envelope. It
coordinates candidate workspaces, caller-supplied external execution-provider
handles, explicit process/resource budgets, deterministic candidate-order
commit, mandatory cancellation/join/cleanup, and source-optimizer resume
identity validation.

It does not introduce an installed CLI, distributed workers, remote trust,
GPU scheduling, asynchronous public winners, or an alternate fitness/ranking
algorithm.

| Domain authority | Exact representation | Stable audit projection |
|---|---|---|
| Candidate domain | Generation/population ordered recipe evaluation requests | Generation, population index, recipe identity |
| Logical worker assignment | Direct formula over stable population index and configured worker count | Logical worker identity and dispatch wave |
| Workspace assignment | Canonical candidate key under declared orchestration root | Stable workspace label and candidate identity |
| Resource admission | Explicit numeric per-worker and aggregate limits | Complete resource policy and admission result |
| Runtime state | One bounded job record per candidate plus provider terminal evidence | Ordered state transitions and stable terminal reason |
| Completion | Exact provider terminal/join result | Diagnostic completion ordinal |
| Commit | Candidate-index ordered staged outcomes | Stable commit ordinal independent of completion |
| Failure propagation | One exact transaction failure latch plus complete job array | First hard failure identity and every cancellation/join result |
| Search authority | Existing structured-search lineage and EVO core commit semantics | Complete committed lineage prefix |
| Product checkpoint | Canonical outer identity record plus exact nested core checkpoint bytes | Complete dependency identity/checkpoint view |
| Resume admission | Direct equality validation of every required dependency identity | Per-identity match/mismatch and pre-execution decision |

No result cache, work-stealing authority, hash-indexed job map, bloom filter,
compressed checkpoint authority, learned scheduler, probabilistic admission
structure, or alternate winner record participates in committed behavior.

## Exact Scheduling Authority

The scheduler retains one explicit record for every logical candidate in stable
generation/population order. Logical external worker identity is derived directly
from the candidate index and configured worker count. Runtime thread IDs,
process IDs, queue addresses, wake order, provider handles, and wall-clock
completion timestamps are not stable authority and are excluded from replay
identity.

Provider completion order is retained only as diagnostic evidence. Candidate
outcomes do not become visible to selection, statistics, stopping, lineage, or
champion authority until the full generation satisfies the mandatory terminal
and cleanup boundary and the generic EVO core commits evaluations in stable
population order.

This explicit separation makes a serial schedule and a supported parallel
schedule comparable without pretending that their runtime traces are identical.
Their logical recipe/evaluation/selection results must match when provider
outcomes are deterministic; their completion and schedule fingerprints may
legitimately differ.

## External Process and Resource Projection

The external execution provider is a portability boundary, not hidden authority.
Every start request contains an explicit job identity, workspace, and complete
resource policy. Every terminal result states the controls actually enforced,
including CPU, address-space, descendant-process, storage, output/evidence,
wall-time, filesystem, network, cancellation, and descendant-cleanup evidence
where required by the selected evaluation path.

A required capability that is unavailable is an explicit rejection. The
scheduler cannot infer successful isolation from a zero exit code. Likewise,
provider handles are never serialized or rendered as stable identities; they
remain runtime-owned until join.

Admission uses direct checked arithmetic over explicit active-worker records and
aggregate budgets. There is no hidden admission cache or allocator-derived
capacity signal.

## Failure, Cancellation, and Cleanup Authority

The first hard orchestration failure sets one exact failure latch. That latch
prevents new starts and requests cancellation for every currently active sibling
job. Every started job must still reach a terminal provider result and mandatory
join before the transaction can return.

The complete candidate-ordered job array remains authority after failure. A
summary such as `failed=true`, a first-failure index, or a cancellation count is
only a projection. It cannot replace exact per-job terminal and cleanup state.

A join or cleanup failure invalidates generation/checkpoint authority. The
implementation therefore has no fallback that publishes a partial generation,
continues ranking successful siblings, or writes a trusted resumable checkpoint
while process escape is possible.

## Product Checkpoint Authority

Issue #51 intentionally checkpoints only generic EVO core state. The product
checkpoint introduced by ADR-0043 wraps exactly one validated core checkpoint
with the source-optimizer identities that determine whether continuation means
the same experiment.

The exact outer identity view includes baseline, analysis, catalogue,
recipe/search/operator policy, execution-provider policy, toolchain, workload,
artifact/evidence schema, seed, committed-generation, and committed-lineage
identity. Every field is rendered in canonical JSON and Markdown in stable
domain order.

The checkpoint fingerprint and integrity field are diagnostics over the complete
canonical record; they are not substitutes for the underlying identities and do
not provide authentication or encryption. Resume validates the explicit fields
before nested core resume and before an external provider can observe a start
request.

In-flight jobs are intentionally absent from checkpoint authority. Checkpoints
exist only at complete committed-generation boundaries with zero active external
jobs. That restriction removes host-local queues, process handles, lock state,
and completion races from deterministic resume semantics.

## Freshness and Invalidation

Resume compares every required dependency identity directly. Baseline,
analysis, catalogue, search/operator policy, toolchain, workload, provider
policy, and artifact-schema mismatch are separately visible reasons. No summary
configuration fingerprint can independently waive a mismatched exact field.

Runtime output paths and provider handles are reattached resources. They do not
participate in logical experiment identity unless a later ADR explicitly promotes
one of their semantics into a stable policy identity.

Corrupt, truncated, unsupported-version, over-budget, or internally inconsistent
checkpoint input is rejected atomically under caller limits. No partial parsed
state becomes resumable authority.

## Human-Readable Projection Completeness

Canonical orchestration evidence exposes, in deterministic order:

- complete product dependency identities and representation versions;
- configured worker count and all process/resource bounds;
- every candidate generation/population/recipe identity;
- logical worker and workspace assignment;
- every admitted/start/cancel/terminal/join/stage/commit state;
- diagnostic completion ordinal and authoritative commit ordinal;
- exact stable failure/rejection/cleanup reason;
- complete committed structured-search lineage prefix;
- checkpoint format/integrity/identity view;
- resume match/mismatch decisions for every dependency; and
- final termination and champion lineage.

Markdown renders the same decision domain as JSON rather than a sample or
summary. Counts and fingerprints are useful cross-checks but cannot replace the
ordered record set.

## Accelerator Assessment

No accelerated representation participates in acceptance, rejection, scheduling
admission, resource accounting, candidate commit, resume validation,
cancellation, termination, ranking, or champion selection. Direct bounded arrays
and deterministic scans are both the fast path and the reference path.

Runtime operating-system schedulers and provider queues are not domain
accelerators because their order is explicitly excluded from committed
authority. Their observable effects are normalized into exact per-candidate
terminal evidence and stable commit ordering before any evolutionary decision is
published.

ADR-0026 accelerator-specific differential requirements therefore do not apply
to a new accelerator at this boundary. Differential tests are still required
for serial-versus-parallel logical equivalence and uninterrupted-versus-resumed
execution because those are core correctness claims of issue #66, not because an
opaque acceleration structure requires a fallback.

## Independent Validation

Independent validation must reconstruct candidate commit order from the complete
job trace rather than trusting summary ordinals. It must verify that every
started job is terminal and joined before a committed generation/checkpoint,
that the first hard failure prevents later starts and triggers sibling
cancellation, and that no cleanup-failed transaction is represented as
committed.

Checkpoint validation must parse the canonical identity view independently,
recompute the deterministic checkpoint identity, and prove that every stale
identity fixture rejects before any external start. Uninterrupted and resumed
fixture evidence must compare the complete logical recipe/evaluation/termination
and champion lineage.

Hosted evidence must cover serial and bounded parallel execution, intentionally
out-of-order provider completion, CMake/Clang sanitizer and race-aware validation
where supported, Autotools/GNU parity, macOS portability, schema validation, and
AES-BLD-001/AES-SEC-001 inventory/governance parity.

## Result

The EVO 0.42.0 bounded source-orchestration design conforms to the
Human-Readable Abstraction Rule without introducing an accelerated authority.
Exact candidate job records, stable candidate-order commits, complete product
identity fields, and the nested exact core checkpoint remain authority. Runtime
completion order, provider handles, queues, fingerprints, and summary counters
cannot independently accept, reject, rank, terminate, resume, or publish a
candidate.

This finding does not pre-approve distributed scheduling, remote worker trust,
GPU orchestration, artifact publication, installed product commands, deployment,
automatic downstream commits/pushes, or any future cache/index/compressed
checkpoint representation. Those remain separate ADR-0026 boundaries.
