# EVO-HRA-009: Project-Recipe Human-Readable Abstraction Audit

Date: 2026-08-09

Audited implementation: EVO 0.36.0

Governing records: ADR-0026, ADR-0037, EVO-002, issue #60

## Inventory

The project-recipe addition consists of three production units and four private
recipe headers, plus a bounded signed-integer extension to the existing JSON
parser. Its authority remains explicit:

| Domain authority | Exact representation | Stable audit projection |
|---|---|---|
| Catalogue policy | Caller-owned ordered entry/schema/reference arrays | Identity/version, transformation, parameter, precondition, dependency, and conflict fields |
| Selected recipe | Owned ordered transformation-record array | Complete canonical JSON record array and Markdown record sections |
| Source target | Analysis-owned stable location and opportunity records | File/range/kind/spelling identity plus opportunity rank |
| Dependency closure | Explicit resolved record edges and stable topological traversal | Ordered dependency objects naming both record and transformation |
| Analysis provenance | Copied compiler/runtime identity arrays from direct scans | Complete provenance arrays and Markdown lists |
| Portable genome | Header, exact canonical JSON payload, mandatory zero padding | Embedded JSON plus derived Markdown; envelope fields are documented |

The audit found no hash table, bitmap, compact index, membership filter, cache,
probabilistic decision structure, address-derived identity, retained AST, or
raw-source genome. Catalogue lookup, target lookup, dependency resolution,
conflict detection, and provenance collection use bounded direct scans.
`qsort` orders parameters; a bounded Kahn traversal orders records. Neither is
a hidden authority.

## Projection Completeness and Ordering

- Every record binds the root baseline, analysis, and catalogue identities.
- Composition order is dependency-first, with stable record identity breaking
  ties among ready records.
- Parameters are identity ordered; catalogue preconditions and references must
  already be ordered and unique.
- Targets expose exact file, range, spelling/macro kind, and spelling identity.
- Provenance exposes every applicable missed compiler record and runtime
  record, not only aggregate counts.
- The canonical JSON declares its direct-array reference form, complete
  projection, absence of probabilistic authority, and absence of raw source
  bytes.
- Markdown enumerates the same global identities, record order, targets,
  parameters, edges, conflicts, and provenance.

The fixed genome is bounded caller storage, not an opaque compressed recipe.
The JSON payload remains directly enumerable, and mandatory zero padding has
no domain semantics.

## Invalidation, Corruption, and Reconstruction

- Unknown catalogue entries, unsupported or stale targets, malformed or
  missing parameters, incomplete or ambiguous dependencies, selected
  conflicts, and cycles fail before a recipe is published.
- Decode parses only the minimal proposal fields, reconstructs all ranges,
  preconditions, edges, conflicts, ranks, and provenance from the live
  baseline/analysis/catalogue, then compares the entire regenerated genome.
- Duplicate/unknown JSON fields, alternate JSON presentation, altered derived
  facts, noncanonical order, envelope corruption, and nonzero padding cannot
  become accepted authority.
- Snapshot path, mode, size, and byte evidence is verified before and after
  every build/decode operation. This milestone exposes no source-writing path.
- Failed operations release the private owner and leave the caller result
  inactive.

## Fingerprint Boundary

The versioned FNV label covers exact canonical JSON bytes and supports replay
diagnostics. It is not authentication, provenance, collision-resistant
identity, or acceptance authority. Exact canonical bytes and live-authority
reconstruction remain required even when the label matches.

## Differential Evidence

No accelerator requires a separate fast/reference differential path. The
normative test still supplies equivalent proposals and parameters in different
orders, requires exact genome and recipe equality, and compares the emitted
canonical JSON byte-for-byte with the retained golden. Decode independently
rebuilds the complete model before exact-byte comparison. The Python validator
separately checks the retained golden's shape, ordering, dependency precedence,
HRA declarations, raw-source exclusion, and diagnostic FNV vector.

## Result

EVO 0.36.0 transformation recipes conform to the Human-Readable Abstraction
Rule. Explicit bounded arrays and direct scans remain authority; canonical JSON
is a complete portable representation; Markdown is a complete derived human
projection; and no accelerated or probabilistic structure can hide or commit a
decision. This finding does not pre-approve the AST-aware transformation
catalogue, candidate materializer, recipe operators, orchestration scheduler,
artifact store, or standalone executable. Each remains subject to ADR-0026.
