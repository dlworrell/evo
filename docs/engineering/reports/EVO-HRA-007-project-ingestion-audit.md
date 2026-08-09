# EVO-HRA-007: Project-Ingestion Human-Readable Abstraction Audit

Date: 2026-08-09

Audited implementation: EVO 0.34.0

Governing records: ADR-0026, ADR-0035, EVO-002, issue #58

## Inventory

The project-ingestion foundation consists of eight production units and nine
private headers. Its domain state is held in explicit bounded arrays and direct
owner fields:

| Domain authority | Exact representation | Stable audit projection |
|---|---|---|
| Authorized input | Read-only snapshot bytes | Ordered file table in `baseline.json` and `baseline.md` |
| Manifest policy | Decoded required fields and ordered registries | Complete `manifest` object plus Markdown policy lists |
| Build description | Ordered compilation-unit records | Complete `compilation_units` array and Markdown table |
| Baseline execution | Four fixed command records | Candidate-independent configure/compile/correctness/benchmark trace |
| Capture outcome | Explicit status or baseline-state enum | Named status/state in API and evidence |

The audit found no hash table, bitmap, compressed index, membership filter,
cache, probabilistic decision structure, hidden filesystem index, or retained
work queue. `qsort` establishes canonical order over the explicit arrays; the
array entries remain directly enumerable and authoritative.

`project_runtime` centralizes three implementation primitives—zeroed
allocation, release, and bounded formatting—to keep the AES-SEC-001 review
surface finite. It stores no state, registry, cache, policy, or identity and
cannot approve a baseline. The calling domain modules remain the readable
authority for allocation dimensions, ownership, cleanup, and truncation
handling.

## Fingerprint Boundary

The manifest, normalized build, files, command outputs, and baseline carry
versioned FNV-1a labels. Every schema and both human-readable outputs identify
those labels as deterministic diagnostics and set fingerprint authority to
false where applicable. Exact snapshot bytes and complete logical registries
remain authority. A collision cannot accept a path, compile unit, command
outcome, or completed baseline because every corresponding exact record is
retained and validated independently.

## Projection Completeness and Ordering

- Files sort by UTF-8 bytewise relative path.
- Compilation units sort by normalized source path, directory, output, command
  form, and exact invocation.
- Named manifest registries sort by their stable identifiers; command argv
  order remains semantic and is never sorted.
- Gate records remain in configure, compile, correctness, benchmark order,
  including explicit `not-run` entries after a failed gate.
- JSON contains every captured fact. Markdown derives from the same owner and
  declares itself a projection; no separate cache or summary source exists.

Both projection files are bounded by the manifest and caller limits. A
projection that cannot fit aborts the transaction and publishes no completed
baseline, so truncation cannot masquerade as completeness.

## Invalidation, Corruption, and Reconstruction

- Unknown, duplicate, malformed, over-depth, over-token, over-byte, ambiguous,
  missing, symlinked, or out-of-root input fails closed before project commands
  whenever it is knowable at preflight.
- The authorized input is fully re-enumerated and byte-compared after commands;
  drift aborts even when size and path are unchanged.
- An incomplete marker distinguishes a reserved transaction. Any failure
  recursively removes only that exact owned output without following symlinks.
- Replay reconstructs from the manifest, authorized input bytes, declared
  provider identity, and fixed policies. No cache, prior output, address,
  process identifier, clock, entropy source, or network result is consulted.

## Differential Evidence

No accelerator requires a separate reference/fast-path differential test.
Nevertheless, the tests compare independently implemented Python and C
manifest/build/baseline calculations, relative and project-root-absolute
compilation databases, CMake and Autotools fixtures, manifest reorderings, and
two exact captures. These comparisons protect the direct reference form
against parser and ordering drift.

## Result

EVO 0.34.0 project ingestion conforms to the Human-Readable Abstraction Rule.
It introduces no accelerated structure, retains exact snapshot and registry
authority, and exposes a complete deterministic audit projection. This finding
does not pre-approve the Clang indexes, transformation catalogues, candidate
caches, external schedulers, or artifact stores planned by later issues; each
must independently satisfy ADR-0026.
