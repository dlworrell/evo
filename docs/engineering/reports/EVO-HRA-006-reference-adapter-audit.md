# EVO-HRA-006: Reference Adapter Human-Readable Abstraction Audit

Status: Complete
Date: 2026-08-09
Repository: `dlworrell/evo`
Audited implementation: EVO 0.33.0
Governing decisions: ADR-0026 and ADR-0034
Tracking issue: #55

## Question

Can a fixture shortcut, checkpoint digest, scheduler queue, golden record, or
Markdown summary become opaque authority over an adapter result?

## Classification

The four adapters introduce no accelerated structure. Their immutable fixture
records, four-byte genomes, fixed capture arrays, public EVO results, and
ordered evidence are direct explicit reference forms. There is no cache,
compressed collection, bitmap, membership filter, probabilistic precheck,
registry hash table, hidden scheduler index, or summary-only result.

Existing core recycling and parallel evaluation remain the only accelerated
paths exercised. Their exact allocation and serial reference paths and public
registry/schedule projections were already accepted by EVO-HRA-003 and
EVO-HRA-004. This audit verifies that the adapters preserve rather than hide
those projections.

## Stable Adapter Registry

The canonical JSON orders entries as repository scoring, compiler options,
scheduler tuning, then FPGA placement. Every entry contains:

| Required fact | Explicit projection |
|---|---|
| Identity | Adapter ID, domain, fixture ID, evidence schema, EVO version |
| Source state | Complete immutable fixture values and mappings |
| Search contract | Seed, population/transition bounds, selection, operators, elite, recycling |
| Resource contract | Genome, population, evaluation, child, diversity, checkpoint, worker bounds |
| Domain authority | Hard constraint, soft penalty, result, and stated limitation |
| Replay | Two-run direct equality outcome and complete generation trace |
| Product boundary | `source_optimizer_claimed: false` in every entry |
| Accelerator assessment | `accelerated_structure: null`, complete projection, no probabilistic authority |

The registry is fixed and complete; it requires no pagination, continuation,
lookup index, digest, or reconstruction guess.

## Capability Projections

Repository scoring retains the complete format-3 checkpoint bytes for replay
and all 12 decoded candidate records in population order. The CRC-32 is
reported as corruption evidence only. Exact resume compares the complete final
result and every uninterrupted trace record after the restored generation.
A matching CRC cannot substitute for inspection, configuration validation, or
resume equality.

Scheduler tuning retains every generation's public schedule and all 12
candidate assignments. Stable logical worker identity, wave, exclusion or
completion disposition, commit presence, and commit order are explicit. The
private queue, operating-system thread ID, and callback completion timing are
absent by design and cannot commit fitness. Candidate-order publication remains
the core authority.

Compiler and FPGA fixtures require no specialized projection beyond their
explicit inputs, hard constraints, penalties, stopping configuration, result,
and complete trace. Stagnation and application-request termination names are
retained directly.

## Golden and Summary Boundary

The reviewed golden is the complete expected logical object, not a digest or
compressed oracle. Validation executes each installed consumer, parses its
bounded JSON, checks stable semantics, builds the same ordered top-level object,
and requires direct object equality.

The emitted `EVO-ADAPTERS-001.json` is canonical evidence. Markdown is derived
only after validation and shows a navigation table. It cannot add, replace,
approve, or repair a JSON fact. All fixtures, traces, checkpoint candidates,
and schedule assignments remain in JSON even when omitted from the table.

## Failure and Reconstruction

- Missing, reordered, malformed, oversized, timed-out, or stderr-producing
  adapter output fails validation.
- Any replay, resume, projection-completeness, source-optimizer-boundary, or
  golden mismatch fails before an artifact is accepted.
- Callback capture overflow sets an explicit error and prevents output.
- A failed installed build cannot fall back to source headers or an in-tree
  library.
- Reconstruction uses the recorded source revision, installed package,
  explicit fixture/configuration, and fixed seed; no prior cache is consulted.

## Result

EVO 0.33.0 reference adapters conform to the Human-Readable Abstraction Rule.
They introduce no new accelerator, retain the existing accelerator projections,
and provide a complete stable adapter registry whose explicit evidence—not a
digest, summary, cache, queue, or probabilistic precheck—is authoritative.

This finding does not pre-approve a repository index, compiler-result cache,
scheduler simulator shortcut, FPGA database, remote evidence store, or any
source-optimizer structure. Each future structure must independently define
reference semantics, freshness/invalidation, bounds, fallback, projection, and
differential equivalence under ADR-0026.
