# EVO-HRA-011: Candidate Materialization Human-Readable Abstraction Audit

Date: 2026-08-16

Audited implementation: EVO 0.38.0 candidate-materialization boundary

Governing records: ADR-0026, ADR-0039, EVO-002, issue #62

## Inventory

The candidate-materialization boundary introduces one private transaction that
consumes an immutable baseline, canonical recipe, and complete set of accepted
transformation applications. It produces one isolated candidate tree when
retention is requested, one normalized patch, one changed-file inventory, and
canonical JSON/Markdown evidence. It does not change the installed library API
or introduce an optimizer executable.

| Domain authority | Exact representation | Stable audit projection |
|---|---|---|
| Baseline file order | Committed immutable baseline file-record array | Candidate file count and changed-file records |
| Recipe completeness | Canonical identity-ordered recipe records | Baseline/recipe identities in candidate evidence |
| Edit authority | One exact transformation application per recipe record | Normalized patch plus changed-file edit counts |
| Composition order | Direct per-file scan ordered by half-open byte range and record identity | Patch hunks in immutable baseline file order |
| Candidate content | Complete isolated file tree derived from snapshot bytes and accepted replacements | Candidate fingerprint and retained tree when policy is `retain` |
| No-change authority | Explicit `already-satisfied` application | No artificial patch hunk; complete recipe still required |
| Publication state | One staging directory and incomplete marker | Final candidate/patch/JSON/Markdown bundle only after commit |

No hash table, file index, cached range map, bloom filter, compressed patch
authority, probabilistic membership test, content-addressed shortcut, or
alternate ordering structure participates in acceptance, conflict detection,
composition, or publication. Bounded direct scans and exact byte ranges are the
reference implementation.

## Exact Authority and Projection Completeness

- Every recipe record must have exactly one current application. Missing,
  duplicate, stale, probabilistic, source-writing, snapshot-writing, or already
  materialized application views fail closed.
- Every edit is rechecked against immutable before bytes and its retained
  diagnostic fingerprint before composition.
- Candidate files are constructed from complete immutable file bytes; an edit
  cannot authorize bytes outside its retained half-open range.
- Overlap is represented as an explicit conflict status rather than an implicit
  ordering choice.
- `candidate.patch` is a complete human-readable projection of every changed
  file and every materialized edit. No changed edit is suppressed from the
  patch by a cache or duplicate filter.
- `candidate.json` and `candidate.md` expose baseline, recipe, candidate
  identity, changed-file inventory, patch size, workspace policy, projection
  completeness, absence of probabilistic authority, and explicit source and
  snapshot non-modification claims.

## Freshness, Failure, and Fallback

The immutable baseline is verified before output reservation and again before
successful publication. Snapshot path components are opened without following
symbolic links. Output is reserved only beneath an existing canonical parent
outside the source root and baseline snapshot. A private incomplete marker and
staging directory distinguish an in-progress transaction from committed
output.

Any stale authority, overlap, path violation, byte mismatch, filesystem error,
or resource exhaustion publishes no candidate view. The reserved output is
removed on failure. There is no accelerated path requiring a fallback because
direct file/record scans and exact byte composition are already the reference
path.

## Fingerprint Boundary

Candidate fingerprints are deterministic diagnostics over a versioned domain,
baseline and recipe fingerprints, the complete candidate file sequence with
path/size/source mode/content fingerprint, and normalized patch bytes. Output
location and retain/discard policy are excluded so replay in another directory
retains the same candidate identity.

As with earlier EVO source-optimizer fingerprints, this label is not
cryptographic authentication, authorization, or a substitute for exact byte
validation. Immutable source bytes and current owner/view authority remain
required.

## Differential and Independent Evidence

No accelerator requires a separate fast/reference differential algorithm. The
normative target instead exercises deterministic replay to a second output
location and requires identical candidate fingerprints and patch bytes. It also
covers retained multi-file output, an explicit no-change record, discard-policy
evidence, overlap rejection, path traversal, output-inside-snapshot rejection,
resource exhaustion, immutable baseline bytes, and complete cleanup on failure.

Hosted independent validation applies the normalized patch to a fresh baseline
copy and requires byte equality with the retained candidate. CMake/Clang and
Autotools/GNU builds, sanitizer/static-analysis coverage, evidence-schema
validation, and native-build-manifest parity provide independent frontend and
publication checks.

## Result

The EVO 0.38.0 candidate-materialization design conforms to the
Human-Readable Abstraction Rule without invoking its accelerator-specific
requirements. Exact bounded arrays and source ranges remain authority; the
normalized patch and evidence expose every committed change; and no
probabilistic or compressed representation can accept, reject, order, suppress,
or publish a candidate.

This finding does not pre-approve candidate compilation/correctness gates,
benchmarking, fitness ranking, orchestration, persistent candidate caches,
artifact stores, or product CLI publication. Those remain separate ADR-0026
boundaries.
