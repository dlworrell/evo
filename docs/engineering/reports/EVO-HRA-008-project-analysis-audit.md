# EVO-HRA-008: Project-Analysis Human-Readable Abstraction Audit

Date: 2026-08-09

Audited implementation: EVO 0.35.0

Governing records: ADR-0026, ADR-0036, EVO-002, issue #59

## Inventory

The project-analysis addition consists of three production units and four
private headers. Its authority is held in explicit bounded arrays and direct
owner fields:

| Domain authority | Exact representation | Stable audit projection |
|---|---|---|
| Translation units | Ordered copied path array | `translation_units` plus Markdown table |
| Source identity | Ordered spelling and macro-location records | Complete source-location arrays and table |
| Program structure | Declaration, call, control-flow, and data-flow arrays | Complete JSON arrays and Markdown tables |
| Compiler evidence | Ordered pass records with explicit disposition | Compiler-optimization array and table |
| Runtime evidence | Ordered positive sample-count records plus explicit profile state | Runtime-profile object and hotspot table |
| Opportunity order | Direct aggregation and deterministic sort over retained records | Ranked opportunity array and table |

The audit found no hash table, bitmap, compressed index, membership filter,
cache, probabilistic decision structure, retained compiler AST, address-based
identity, or hidden ranking store. Linear scans resolve references against the
explicit arrays. `qsort` establishes canonical order while every record remains
enumerable and authoritative.

## Projection Completeness and Ordering

- Translation units retain baseline normalized source-path order.
- Every provider domain sorts by stable record identity after deep copy.
- Macro expansions retain a stable link to an explicit spelling location.
- Static compiler records and dynamic runtime records remain separate.
- Opportunities sort by runtime-presence, descending summed samples,
  descending missed-record count, and stable location identity.
- Both projections include the provider, Clang, LLVM, target, flags, baseline,
  runtime-profile state, and every retained record.

`analysis.json` and `analysis.md` derive from the same owner before atomic
publication. Both outputs are bounded; if either cannot fit, no completed
output is published. Markdown cannot independently accept or suppress a fact.

## Invalidation, Corruption, and Reconstruction

- Unknown enums, malformed text or ranges, duplicate identities, missing
  references, undeclared workloads, zero samples, generated-source locations,
  and over-limit provider results fail closed.
- Provider results are borrowed only at callback return and then deep-copied;
  later provider mutation cannot change retained authority.
- The committed snapshot is re-enumerated and byte/mode verified after every
  provider return, including an error return. Any mutation aborts before output
  reservation.
- Replay reconstructs from the committed baseline, declared provider/toolchain
  identities, explicit runtime-profile state, and complete provider records.
  No prior analysis output, clock, process identifier, address, entropy source,
  cache, or network result is consulted.

## Fingerprint Boundary

The versioned FNV label is a deterministic replay diagnostic over the complete
ordered retained model. It is not authentication, provenance, collision-proof
content addressing, or authority. Exact snapshot bytes and complete records
remain independently available, so a matching label cannot hide a differing
record.

## Differential Evidence

No accelerator requires a separate reference/fast-path differential test. The
normative test nevertheless supplies the same records in forward and reverse
provider order, requires byte-identical canonical JSON and the same golden
analysis label, and checks the direct ranking inputs and outputs. This protects
the explicit reference form against ordering drift without inventing a second
opaque implementation.

## Result

EVO 0.35.0 project analysis conforms to the Human-Readable Abstraction Rule.
It introduces no accelerated structure, preserves complete exact arrays and
direct scans as authority, and exposes deterministic machine- and
human-readable projections. This finding does not pre-approve later
transformation catalogues, candidate caches, search schedulers, artifact
stores, or standalone orchestration; each remains subject to ADR-0026.
