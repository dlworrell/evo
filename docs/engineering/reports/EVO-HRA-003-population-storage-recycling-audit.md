# EVO-HRA-003: Population-Storage Recycling Human-Readable Abstraction Audit

Status: Complete
Date: 2026-08-08
Repository: `dlworrell/evo`
Audited implementation: EVO 0.30.0
Governing decisions: ADR-0026 and ADR-0031
Tracking issue: #52

## Question

Does deterministic population-storage recycling make allocator addresses, a
hidden pool, stale bytes, reset history, or a derived lifecycle record opaque
authority over an EVO run?

## Classification

The recycler is an exact allocation accelerator. It retains two fixed,
run-local population owners and alternates their active and reusable roles
instead of allocating and releasing a child population for every transition.
It does not replace population bytes, candidate evaluations, or their exact
pointer/count ownership invariants.

The implementation has no global pool, address-keyed lookup, hash registry,
compressed bitmap, membership filter, probabilistic decision, eviction
policy, or cached fitness authority. Stable owner identities `1` and `2` are
logical labels derived from the fixed lifecycle, never serialized or exposed
allocator addresses.

## Ordered Projection

`evo_population_storage_registry_t` is the mandatory allocation-free audit
projection. Entries are ordered by stable owner identity and expose the
complete two-slot scope:

| Logical fact | Registry projection | Exact authority |
|---|---|---|
| Policy | Registry version, recycling-policy version, and enabled state | Validated configuration and private policy constants |
| Roles | Active and reusable identities plus each entry's `EMPTY`, `ACTIVE`, or `REUSABLE` lifecycle | Private population objects and committed-generation parity |
| Provenance | Represented generation and source generation | Completed-population and production evidence |
| Capacity | Exact genome and evaluation byte capacities | Checked owner pointer/count pairs |
| History | Active-handoff and reset counts | Ordered successful commit history |
| Reset | Last reset disposition and per-range reset-erasure counts | Complete synchronous reset of the former active owner |
| Presence | Explicit genome-owner and evaluation-owner flags | Non-null exact owners after reconciliation |
| Security | Secure-erasure policy, selected backend, and enabled state | Owner-carried cleanup policy and reviewed erasure wrapper |

Generation zero has one active entry. Every later committed generation has one
active and one reusable entry, and the active identity alternates by generation
parity. Disabled mode has one canonical zero-entry projection. There is no
partial page, omitted bucket, approximate count, freshness guess, or hidden
continuation token.

## Authority and Failure Behavior

The registry never authorizes memory ownership. Before reuse, reset, observer
delivery, checkpoint encoding, or restored continuation, EVO reconciles it
against the exact private owners, capacities, configuration, secure-erasure
metadata, and committed generation. A malformed, stale, aliased, or
unreconcilable registry fails closed before bytes or roles change. The engine
does not repair a mismatch by consulting an address or guessing a lifecycle.

Promotion dry-validates both populations, both registries, all owned ranges,
and their independence before the no-fail commit phase. The former active
genome and evaluation ranges are then reset in full before that owner becomes
reusable. Ordinary mode records deterministic zero-byte reset; secure mode
records the reviewed secure-erasure backend. Reset counters describe reuse
events, while terminal release remains governed independently by the exact
owner/count cleanup contract.

Checkpoint format 2 persists the complete logical registry, not addresses or
allocator metadata. Inspection reconciles it before allocation. Resume creates
new local owners, reattaches the restoring build's secure-erasure backend, and
materializes the opposite slot only when a subsequent transition needs it.
The projected source lifecycle therefore remains explainable without becoming
authority over destination-process allocation.

## Differential Evidence

- The explicit disabled allocation path and the accelerated path run with the
  same problem, seed, and algorithmic configuration. Tests compare complete
  callback logs, results, statistics, generation events, stopping, and
  termination.
- Replay tests reproduce each mode independently and prove that recycling adds
  no RNG consumption, seed domain, callback, or algorithm-visible decision.
- Registry tests verify the complete generation-zero and later role sequence,
  stable identities, capacities, handoff counts, reset counts, and delivery
  order.
- Allocation-wrapper tests prove that enabled positive runs use exactly five
  successful allocation classes regardless of whether one or seven
  transitions execute, and that every construction failure releases all
  acquired owners.
- Secure-erasure tests prove full-range reset on every reuse, exact per-owner
  reset counts, provisional-evaluation rollback reset, and final-release
  erasure through the configured backend.
- Checkpoint tests reject registry tampering and prove that an enabled resumed
  suffix is byte-identical to uninterrupted execution.

## Result

EVO 0.30.0 population-storage recycling conforms to the Human-Readable
Abstraction Rule. The accelerator is optional, its reference allocate/release
path remains executable, and its complete lifecycle is projected in stable EVO
domain terms. Exact pointer/count owners remain authority; the registry is
validated audit evidence and cannot independently select or legitimize an
owner.

This finding does not pre-approve variable-size pools, global registries,
address-keyed caches, concurrent free lists, compressed owner maps, lazy reset,
or cross-run reuse. Any such design requires its own reference semantics,
complete projection, authority analysis, failure behavior, and differential
evidence under ADR-0026.
