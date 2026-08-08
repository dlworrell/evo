# EVO-HRA-001: Human-Readable Abstraction Audit

Status: Complete
Date: 2026-08-04
Repository: `dlworrell/evo`
Audited implementation: EVO 0.25.0
Audited merged commit: `d189873411fdd0d127cec717a1a8caaae20a485c`
Audited tree: `eaa0ee42287f4d9d33974bb772bba691a047aef6`
Governing decision: ADR-0026
Tracking issue: #83

## Question

Does the implemented EVO 0.25.0 core contain a compressed, probabilistic,
cached, indexed, or otherwise accelerated structure that has become opaque
authority without an explainable human-readable projection?

## Method

The audit reviewed all twenty-two production C sources, their private headers,
the public API, lifecycle evidence, current ADRs, EVO-001, EVO-002, architecture,
and the remaining roadmap. It classified a structure as an accelerator when it
replaced or approximated an explicit logical registry, set, order, mapping, or
decision path for time or space performance.

Direct bounded arrays, scalar aggregates, and deterministic algorithms were
not classified as accelerators merely because they are machine-readable. The
review separately searched for caches, hash registries, bitmaps, compressed
indexes, membership filters, and probabilistic decision structures.

## Implemented-Core Findings

| Boundary | Implemented representation | Authority and auditability | Finding |
|---|---|---|---|
| Genome storage | Contiguous caller-bounded byte array | Population size, genome size, byte budget, and indexed ownership are explicit | Reference representation; no accelerator |
| Candidate evaluation | Explicit bounded evaluation-record array | Validity, fitness, stable best, and policy versions are directly enumerable | Reference representation; no accelerator |
| Random streams | Versioned scalar RNG and domain-separated stream state | Seed, algorithm version, domain, generation, and tuple index define replay | Deterministic algorithm state; no data-structure accelerator |
| Tournament selection | Direct valid-candidate scans and unbiased bounded draws | Winner derives from explicit evaluations and the common comparator | No cache, filter, or compressed index |
| Rank selection | Direct comparison-derived rank scans and exact integer weights | Stable ranks, weights, ticket traversal, and policy provenance are documented and golden-tested | Deliberately no rank cache |
| Crossover and mutation | Direct bounded genome views plus versioned callback or dispatcher evidence | Operator, stream, child index, and callback boundaries are explicit | No accelerator |
| Elite preservation | Direct stable comparison scans and explicit suffix copies | Requested/effective counts, source-valid count, ordering, and provenance are explicit | No ranking cache or compact set |
| Diversity | Direct bounded pair enumeration and explicit aggregate evidence | Metric version, pair count, work units, result, and budget are recorded | Aggregate measurement, not accelerated authority |
| Statistics and stopping | Explicit versioned generation records and scalar policy state | Inputs, ordering, thresholds, counters, and termination reason are directly inspectable | No accelerator |
| Checkpointing | Not implemented | Roadmap issue #51 now requires an ordered checkpoint projection | No current structure |
| Storage recycling | Not implemented | Roadmap issue #52 now requires a live/reusable owner registry | No current structure |
| Parallel evaluation | Not implemented | Roadmap issue #53 now requires assignment and commit-order projections | No current structure |
| Source-optimizer indexes and caches | Not implemented | Issues #58 through #69 now carry issue-specific projection gates | No current structure |

## Result

The EVO 0.25.0 implementation contains no accelerated structure that violates
the Human-Readable Abstraction Rule. The core currently operates on explicit
bounded reference representations and direct deterministic scans.

This is a boundary-specific finding, not a permanent certification. It does
not pre-approve future lookup tables, recycled-owner registries, compact
checkpoint indexes, work queues, analysis indexes, hotspot models, recipe
registries, caches, membership filters, or artifact indexes.

## Documentation Gap Found

Before issue #83, the repository required machine-readable evidence and
derived human-readable reports but did not require every accelerated structure
to expose its logical contents or exact authority. That gap could have allowed
a later structure to remain replayable yet opaque.

The issue #83 change series closes the gap through:

- ADR-0026;
- normative EVO-001 and EVO-002 contracts;
- the architecture and roadmap gates;
- governance and contributor rules;
- required issue and pull-request prompts; and
- explicit updates to parent roadmap #38 and every remaining child issue.

## Required Future Evidence

For each future accelerator, the implementing pull request must retain:

1. exact reference semantics and canonical authority;
2. a deterministic human-readable projection with stable domain ordering;
3. complete or reconstructable bounded scope;
4. source identity, construction version, provenance, and resource policy;
5. stale, corrupt, incompatible, invalidation, and fallback behavior;
6. differential reference-equivalence tests; and
7. false-positive or approximation tests proving that probabilistic prechecks
   cannot change committed results.

Failure to provide that evidence blocks conformance even if the accelerator is
faster, smaller, deterministic, checksummed, or replayable.

## EVO 0.29.0 Follow-Up

This report remains the historical 0.25.0 baseline; its checkpoint row is not
silently rewritten. EVO-HRA-002 separately audits the implemented format-1
checkpoint and its ordered allocation-free configuration, continuation,
statistics, ownership, and per-candidate projection. That follow-up confirms
that CRC-32, the configuration fingerprint, and the section offsets are never
authority and that decoded exact state remains fully enumerable.
