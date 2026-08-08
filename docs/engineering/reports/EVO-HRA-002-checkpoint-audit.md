# EVO-HRA-002: Checkpoint Human-Readable Abstraction Audit

Status: Complete
Date: 2026-08-08
Repository: `dlworrell/evo`
Audited implementation: EVO 0.29.0
Governing decisions: ADR-0026 and ADR-0030
Tracking issue: #51

## Question

Does checkpoint format 1 make a compact binary, checksum, fingerprint, offset
table, or restored cache an opaque authority over EVO continuation?

## Classification

The checkpoint is a canonical persistence representation of direct EVO state.
It is not a compressed substitute, probabilistic summary, cache, membership
filter, or accelerated decision path. The six header offsets navigate explicit
sections but do not decide validity, fitness, rank, selection, stopping, or
resume compatibility.

CRC-32 detects accidental corruption and FNV-1a identifies the configuration
section. Neither is authority. Inspection validates decoded field invariants,
and resume canonically re-encodes and compares every deterministic
configuration byte before allocating. A hash collision cannot authorize a
different configuration.

## Ordered Projection

`evo_checkpoint_view_t` is the mandatory allocation-free projection:

| Logical order | Binary source | Projection | Exact authority |
|---|---|---|---|
| Configuration | Configuration section | Every deterministic scalar, callback-presence flag, and problem/context identity | Canonical section-byte equality plus normal configuration validation |
| Generation | Continuation section | Current generation and explicit termination reason | Bounded-run state and configured total limit |
| Population | State, evaluation, and genome sections | Size, valid count, current best, direct genome range, evaluation count/bytes | Full completed-population validation |
| RNG and operators | Continuation section | RNG, seed-schedule, bounded-run, selection, byte-operator, comparison, and diversity versions | Version constants plus deterministic tuple schedule |
| Statistics | Statistics section | Complete schema-4 generation record | Recomputed population statistics and exact adaptive validation |
| Adaptation and stopping | Continuation and statistics sections | Effective next rate, adaptive stagnant count, significant-best reference, stopping stagnant count | Persisted direct state reconciled with public projection |
| Ownership | State and byte sections | Source owner byte counts, secure-erasure disposition, direct population/global-best ranges | Restored local owner registry and exact destruction paths |
| Resume identities | Configuration section | Nonzero semantic problem/context identities and callback presence | Exact canonical comparison before allocation |

`evo_checkpoint_candidate_inspect` projects one explicit record by ascending
population index. The output contains that index, bounded genome view, exact
genome size, seven fitness components, validity, and evaluated disposition.
The accessor uses the projected direct ranges and fixed documented stride in
constant time. Enumerating all candidates is linear, complete, and stable.

## Authority and Failure Behavior

The ordered view never overrides decoded state. Inspection rejects malformed
magic, versions, lengths, offsets, section order, checksum, fingerprint,
booleans, enums, finite evidence, counts, best ordering, statistics sums,
owner lengths, adaptive relationships, or terminal state. Resume adds exact
configuration and native lifecycle validation.

No fallback accepts a plausible partial checkpoint. No corruption path uses a
filter or checksum as evidence of correctness. No pointer, address, native
padding, or allocator identity is serialized. Restored allocations are new
local owners, not a cache of source-process addresses.

## Differential Evidence

- Generation-zero, intermediate, and terminal resumes match uninterrupted
  final genome, fitness, statistics, adaptive evidence, generation count, and
  termination reason.
- Observer and checkpoint event sequences prove the restored generation is not
  delivered twice and every suffix generation remains in order.
- Patience and adaptive-mutation tests prove no hidden warm-up history is
  reconstructed.
- Every truncation and every one-bit mutation of a valid seed checkpoint is
  rejected; deterministic random inputs and a libFuzzer entry exercise the
  parser.
- Semantic tampering with configured owner budgets, population provenance, or
  termination is rejected after independently recomputing the CRC and
  configuration fingerprint, proving that neither diagnostic becomes
  authority.
- Every restore allocation failure releases all prior owners without callback
  work; secure-mode restore proves local-backend exact-once erasure.

## Result

Checkpoint format 1 conforms to the Human-Readable Abstraction Rule. Binary
persistence is not opaque authority: its logical configuration, generation,
population, RNG, statistics, adaptation, stopping, ownership, and identity
state is explicitly and deterministically projectable.

This finding does not pre-approve delta checkpoints, compression,
deduplication, compact indexes, lazy decoding, cached projections, or remote
checkpoint catalogues. Any such accelerator must remain derived, expose the
same logical sequence, prove reference equivalence, and reject or fall back to
the format-1 exact path.
