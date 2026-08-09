# ADR-0026: Human-Readable Abstraction and Audit Projection

Status: Accepted
Date: 2026-08-04
Decision owner: EVO
Scope: Repository-wide; clarifies EVO-001, EVO-002, and ADR-0016

## Context

Later EVO roadmap work may benefit from runtime registries, recycled storage,
compact result sets, monotone indexes, caches, membership filters, hotspot
indexes, and bounded work schedulers. These representations can materially
improve time or space use, but they can also hide the logical facts and
decisions that reviewers need to inspect.

EVO already requires deterministic evidence, explicit ownership, immutable
baselines, and reviewable source patches. Those requirements do not by
themselves prevent an accelerated structure from becoming an opaque source of
truth. A checksummed blob can be reproducible and still be unintelligible, and
a generated prose summary can be readable while omitting or misrepresenting
the structure that actually controlled a decision.

The architecture therefore needs one rule that preserves efficient internal
representations without sacrificing human review or exact authority.

## Decision

EVO adopts the Human-Readable Abstraction Rule:

```text
Machine-optimized structure
    +
Human-readable audit projection
```

No compressed, probabilistic, cached, indexed, or otherwise accelerated
structure may become opaque authority. Every such structure must remain behind
an interface expressed in EVO domain concepts and must provide a deterministic
audit projection of its logical contents and relevant decisions.

An audit projection is not a dump of buckets, compressed words, allocator
metadata, or implementation-specific addresses. It is an explicit registry,
ordered list, result set, event window, decision trace, or equivalent view that
a reviewer can understand without decoding the acceleration format.

### Required design record

Any issue and pull request that introduces or materially changes an
accelerated structure must identify:

1. the exact logical authority and its reference semantics;
2. the accelerated representation and why it is needed;
3. its construction algorithm, version, source identity, and resource budget;
4. the human-readable projection, stable ordering, schema, and provenance;
5. whether the projection is complete or a declared window or page;
6. invalidation, corruption, stale-state, and fallback behavior; and
7. differential evidence proving equivalence to the explicit reference form.

If no accelerated structure is involved, the issue and pull request state that
the rule is not applicable rather than silently omitting the review.

### Projection contract

A projection must:

- use stable domain identifiers and deterministic ordering;
- identify the authoritative input, construction policy, and representation
  version from which it was produced;
- show the logical members, mappings, order, ranges, or decision reasons
  relevant to the requested scope;
- state whether it covers the complete structure;
- when bounded, declare the range, page, total when known, continuation rule,
  and stable reconstruction order;
- be reproducible from the same authoritative state; and
- fail closed or fall back to the explicit reference path if it cannot be
  reconciled with that state.

Human-readable does not require unbounded retention. Large structures may
provide caller-bounded pages or windows, but the union of a complete traversal
must reconstruct the logical structure without gaps, duplication, or order
ambiguity. A partial projection may not be presented as the complete state.

Canonical machine-readable evidence remains authoritative for checksum,
schema validation, replay, and automated comparison. Its logical contents must
also be projectable into the required human-readable form. The projection is a
derived audit view, not an independent source that may drift from the canonical
evidence. Inability to generate or reconcile it is a conformance failure.

### Exact accelerators

An exact accelerator, such as a hash table, bitmap, compressed monotone index,
or sorted search index, may answer a logical query only under the same semantics
as its explicit reference form. Normative tests compare both paths across
boundary, empty, duplicate, tie, corruption, stale-identity, and budget cases.

Representative projections include:

| Accelerated structure | Required audit projection |
|---|---|
| Runtime hash table | Stable registry view ordered by domain key |
| Roaring or other bitmap | Explicit ordered result set with cardinality and scope |
| Elias-Fano or other monotone index | Ordered event or generation window with bounds and continuation |
| Search or dependency index | Explicit matching records and the relationships that justified them |
| Recycled owner pool | Stable live-owner and reusable-owner registry with lifecycle state |
| Parallel work queue | Stable candidate-to-work assignment and committed-result order |

Corrupt or incompatible exact structures must reject or rebuild from exact
authority; they must not silently return a plausible partial answer.

### Caches

A cache is derived state, never the sole canonical source. It records the exact
source identity, construction policy and version, freshness or epoch,
invalidation rule, and resource ownership. A cache hit may be used only when
those identities validate and its result is equivalent to canonical
recomputation. EVO must retain a bounded exact fallback or recomputation path.

The audit projection identifies whether a result was served from cache, which
canonical state it represents, why it was considered fresh, and the projected
logical result. A stale, corrupt, ambiguous, or over-budget cache fails closed
or falls back; it never becomes authority by availability.

### Probabilistic structures

Membership filters and other probabilistic structures are prechecks only. They
may prioritize or batch exact work, but they may not independently accept,
reject, rank, select, publish, suppress, or terminate a candidate or result.
Every committed decision must be confirmed by exact authority.

The audit projection records the precheck outcome, the possibility of false
positives or other approximation, the exact authority consulted, and the final
exact result. Tests inject false positives and prove that they cannot change a
committed EVO decision. No probability claim substitutes for that authority
boundary.

### Current implementation audit

EVO 0.25.0 contains no compressed, probabilistic, cached, or indexed
accelerator that controls a run. Population genomes and evaluation records are
explicit bounded arrays; selection, elite ordering, diversity, statistics, and
stopping use direct deterministic scans and explicit evidence. Stable rank
selection deliberately chose constant-storage comparison scans over a hidden
rank cache.

The current core therefore has no violation to remediate. This finding applies
only to the reviewed 0.25.0 boundary and does not grant automatic conformance to
later storage recycling, checkpoint, parallelism, analysis, recipe, or artifact
work.

ADR-0027 performs the next change-specific assessment for EVO 0.26.0. Its
reference byte operators act directly on explicit bounded arrays and introduce
no accelerated structure, so the rule is explicitly not applicable to a new
projection API for that change. The operator decisions and affected ranges are
nevertheless specified and golden-vector tested for reviewability.

ADR-0028 performs the next change-specific assessment for EVO 0.27.0. Adaptive
mutation uses a direct constant-space rate/count record rather than an
accelerated substitute. Public schema-4 statistics and their ordered observer
delivery provide the explicit human-readable decision projection: source
generation, prior/effective rates, bounds, evidence facts, clamp/reset state,
and reason. No opaque cache, filter, index, or compressed authority is added.

ADR-0029 performs the change-specific assessment for EVO 0.28.0. Secure
erasure uses direct owner fields and retained exact allocation counts, not an
accelerated, pooled, cached, or address-keyed substitute. Its stable ownership-
and-erasure registry names each logical range, lifecycle disposition, policy
version, and backend. The accelerator-equivalence requirement is therefore not
applicable, while exact-once event tests reconcile the registry with actual
erase-before-release behavior.

ADR-0030 performs the change-specific assessment for EVO 0.29.0. Its canonical
binary checkpoint has an allocation-free ordered view plus per-candidate
projection; CRC-32, the configuration fingerprint, and section offsets remain
diagnostics rather than authority. EVO-HRA-002 retains that reconciliation.

ADR-0031 performs the change-specific assessment for EVO 0.30.0. The optional
two-slot recycler is an exact allocation accelerator whose complete address-
free owner registry is reconciled against private pointer/count authority.
EVO-HRA-003 retains the reference-equivalence and lifecycle audit.

ADR-0032 performs the change-specific assessment for EVO 0.31.0. The bounded
worker scheduler retains serial evaluation as its exact reference and projects
every candidate's stable logical worker, wave, completion/cancellation
disposition, and commit order independent of runtime threads and timing.
EVO-HRA-004 retains the differential and concurrency audit.

ADR-0033 performs the change-specific assessment for EVO 0.32.0. Core
benchmark JSON retains complete ordered cases, seeds, generation traces, and
raw measurements as authority. Its FNV value and aggregates are explicitly
non-authoritative, and the Markdown projection is regenerated only by parsing
and validating the canonical JSON. EVO-HRA-005 retains the benchmark evidence
and projection audit.

ADR-0034 performs the change-specific assessment for EVO 0.33.0. Installed
reference consumers use direct immutable fixtures and bounded capture arrays,
then retain a complete stable JSON registry with exact replay, generation
traces, checkpoint candidates, and logical worker schedules. No accelerator is
introduced; exact golden-object comparison remains authority and Markdown is
derived only after validation. EVO-HRA-006 retains the adapter evidence and
projection audit.

ADR-0035 performs the change-specific assessment for EVO 0.34.0. Project
ingestion uses direct bounded arrays and scans; exact snapshot bytes plus the
complete file, compilation-unit, policy, and gate registries remain authority.
Its FNV labels are diagnostic only, and JSON/Markdown expose the same stable
domain records without a cache, compact index, filter, or hidden registry.
EVO-HRA-007 retains the ingestion evidence and projection audit.

## Consequences

- EVO may use efficient internal data structures without exposing their
  physical layout as product architecture.
- Reviewers retain an explicit path from accelerated state to logical domain
  facts and committed decisions.
- Exact reference semantics and differential tests become part of the cost of
  every accelerator.
- Human-readable views may be bounded, but their scope and completeness cannot
  be ambiguous.
- Machine-readable evidence remains canonical for automation and replay while
  no longer being permitted to remain opaque to human audit.
- Probabilistic prechecks cannot become correctness, admissibility, ranking, or
  publication authorities.
- Performance claims include the cost and budget of projections, validation,
  invalidation, and exact fallback where those paths are exercised.

## Alternatives considered

### Prohibit accelerated structures

Rejected because explicit reference representations are not always practical
for the scale of source analysis, candidate orchestration, or retained
evidence. The rule preserves optimization while constraining authority.

### Treat raw internal dumps as sufficient

Rejected because buckets, bit patterns, compressed blocks, and pointer graphs
expose implementation details without explaining domain membership, order, or
decisions.

### Treat generated prose as authority

Rejected because prose may omit facts and is difficult to validate
automatically. Human-readable reports derive from canonical evidence and must
reconcile with it.

### Permit membership filters to decide negative results

Rejected. Even a structure designed without false negatives can be stale,
corrupt, built from the wrong identity, or used under an incompatible policy.
Probabilistic structures remain prechecks; exact authority commits results.

### Require every projection to be one unbounded document

Rejected because that would defeat resource bounds. Stable, complete,
reconstructable pages and windows satisfy the audit contract.

## Verification

- The EVO code-change issue template requires an acceleration and projection
  design or a not-applicable rationale.
- The pull-request template requires projection and reference-equivalence
  evidence.
- EVO-001 and EVO-002 make the rule normative for the core and product layers.
- Architecture, governance, contributing guidance, README, and roadmap carry
  the invariant into later work.
- `docs/engineering/reports/EVO-HRA-001-human-readable-abstraction-audit.md`
  records the implemented 0.25.0 audit and its limitations.
- EVO-HRA-002, EVO-HRA-003, and EVO-HRA-004 record the checkpoint, recycler,
  and bounded-worker assessments for later accelerated boundaries.
- EVO-HRA-005, EVO-HRA-006, and EVO-HRA-007 record the benchmark, adapter, and
  project-ingestion evidence assessments.
- Parent roadmap issue #38 and the affected child issues record the gate.
- Governance issue: `https://github.com/dlworrell/evo/issues/83`
