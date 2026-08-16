# ADR-0039: Isolated Candidate Materialization

Status: Accepted

Date: 2026-08-16

## Context

ADR-0038 ends at a deliberately non-writing boundary: one accepted recipe
record can produce one exact owned edit or a deterministic
`already-satisfied` record, but no source tree is materialized. Issue #62 must
compose those accepted applications into an isolated, deterministic,
human-reviewable candidate without weakening ADR-0035 baseline immutability or
reinterpreting transformation semantics.

Candidate materialization is therefore a transaction boundary, not a new
optimization authority. The immutable snapshot remains the source of every
before byte. The canonical recipe determines the complete record set. Each
transformation application remains authoritative only for its exact retained
range and replacement. Materialization may order, conflict-check, compose,
copy, fingerprint, and publish those records; it may not invent an edit,
change a target, invoke a compiler, rank fitness, or write the source project or
baseline snapshot.

The materializer also creates filesystem state. Path traversal, symbolic-link
aliasing, partial publication, stale authority, resource exhaustion, or a
baseline mutation during the transaction must fail closed without leaving a
candidate identity or misleading completed patch.

## Decision

EVO 0.38.0 adds a private version-1 candidate materialization transaction to
the uninstalled source-optimizer foundation.

1. Materialization accepts one eligible committed immutable baseline, one
   canonical recipe derived from that baseline, exactly one current
   transformation application for every recipe record, one absolute output
   path, an explicit retain/discard workspace policy, and nonzero resource
   limits.
2. The materializer verifies the baseline immediately before any output is
   reserved and again before publication completes. Any mutation of the
   immutable snapshot fails as `baseline-changed` and publishes no candidate.
3. Recipe records and applications are matched by exact record identity.
   Missing, duplicate, stale, probabilistic, source-writing, snapshot-writing,
   or already-materialized application views reject. `already-satisfied`
   records contribute no edit but remain part of the complete recipe
   transaction.
4. Every recipe target must name a normalized relative path present in the
   immutable baseline. Empty components, `.` or `..`, absolute paths, control
   bytes, and backslash aliases reject. Every snapshot path component is opened
   without following symbolic links.
5. The requested output path must be absolute, have an existing canonical
   parent, not already exist, and not be contained by the source root, baseline
   output, or immutable snapshot. The transaction reserves the output with an
   incomplete marker and builds only beneath a private staging directory.
6. Candidate files are emitted in immutable baseline file order. Applications
   for each file are ordered by exact half-open `before` start offset with
   record identity as the deterministic tie breaker. Overlap is a hard
   conflict; there is no last-writer-wins behavior.
7. Before applying each edit, EVO revalidates its exact retained before bytes,
   before fingerprint, source range, replacement bytes, and replacement
   fingerprint against the immutable snapshot. The materializer never asks a
   provider to rediscover transformation intent.
8. The candidate tree contains every baseline file. Files without edits are
   byte-for-byte copies; files with edits are constructed in one forward pass
   from immutable bytes plus accepted replacements. Candidate generation never
   opens the source repository or baseline snapshot for writing.
9. One deterministic normalized unified patch is generated in baseline file
   order. Changed-file inventory records before/after size and diagnostic
   fingerprint plus edit count. No-change files are omitted from the patch and
   changed-file inventory.
10. The candidate diagnostic fingerprint is derived from a versioned domain,
    baseline fingerprint, recipe fingerprint, complete candidate file sequence
    with path/size/source mode/content fingerprint, and normalized patch bytes.
    It excludes the output path and retain/discard policy, so replay in a
    different destination has the same candidate identity and patch bytes.
11. Publication is atomic at the transaction boundary: patch, canonical JSON,
    Markdown audit, and optionally the candidate tree become visible only
    after all files and evidence have completed. Failure removes the reserved
    output and leaves the caller result inactive.
12. `retain` publishes the isolated tree as `candidate/`; `discard` removes the
    staged tree after its identity and patch have been computed but retains the
    evidence bundle. Result destruction releases in-memory ownership only and
    does not silently delete successfully published output.

## Output Layout

A successful retained transaction publishes:

```text
<output>/
  candidate/
  candidate.patch
  candidate.json
  candidate.md
```

A successful discard-policy transaction publishes only the patch and evidence
files. `.evo-incomplete-v1` and `.evo-stage-v1` are transaction-private and
must not survive successful publication.

## Resource and Ownership Rules

Caller limits bound strings, paths, file count, per-file bytes, total candidate
bytes, accepted edits, normalized patch bytes, and evidence bytes. Candidate
evidence also respects the immutable manifest evidence budget. All arithmetic
is checked before allocation or range composition.

The returned candidate owns copied baseline/recipe identities, changed-file
paths, patch bytes, JSON bytes, Markdown bytes, and publication paths through
one private owner. Borrowed baseline, recipe, and transformation application
views are used only during the call. Destruction is null-safe and resets the
complete public view.

## Human-Readable Abstraction Assessment

No cache, hash table, lookup index, probabilistic membership filter, compressed
edit representation, or alternate authority is introduced. Direct bounded
arrays, immutable baseline file order, exact record-identity scans, and exact
byte ranges are the reference implementation. The normalized patch is itself
a complete human-readable projection of every materialized edit, while
`candidate.json` and `candidate.md` expose the candidate identity, recipe and
baseline provenance, changed-file inventory, workspace policy, resource-bounded
publication result, and the explicit `source_modified:false` and
`snapshot_modified:false` claims.

Because no accelerated representation participates in acceptance, rejection,
ordering, selection, publication, or suppression, the ADR-0026 accelerator rule
is not applicable at this boundary. The change-specific HRA record retains this
assessment.

## Consequences

- An accepted recipe can now be reviewed as ordinary source files and a stable
  patch without touching the source project or immutable baseline.
- Replaying the same baseline, recipe, and applications in another output
  location produces the same candidate identity and patch bytes.
- Overlapping edits fail deterministically instead of being silently ordered.
- `already-satisfied` records remain observable recipe evidence without
  creating artificial patch hunks.
- Candidate creation still does not compile, test, benchmark, rank, commit, or
  push output. Issue #63 owns candidate correctness gates and later roadmap
  work owns fitness, orchestration, and product publication.
- EVO 0.38.0 remains an uninstalled private source-optimizer foundation; the
  installed library and product CLI contract do not change.

## Rejected Alternatives

- Editing the source repository in place was rejected because it breaks the
  immutable-baseline and review-isolation contracts.
- Editing the immutable snapshot was rejected because subsequent evidence
  could no longer prove what recipe authority was evaluated.
- Last-writer-wins overlap handling was rejected because application order
  would become hidden semantic authority.
- Regenerating edits from AST/provider evidence was rejected because ADR-0038
  already owns transformation semantics and exact application evidence.
- Publishing a candidate tree before patch/evidence completion was rejected
  because a partial transaction could be mistaken for a committed candidate.
- Including the destination path in the candidate fingerprint was rejected
  because location is not candidate content and would break deterministic
  replay.
- Using a cache or probabilistic filter to accelerate file/edit matching was
  rejected because the bounded exact scans are simpler and auditable.

## Verification

The normative candidate target must cover retained multi-file materialization,
byte-identical replay to a second output path, an `already-satisfied` record,
discard-policy evidence publication, overlapping edits, path traversal,
output-inside-snapshot rejection, patch/evidence resource exhaustion, immutable
snapshot bytes, and complete cleanup on failure. The emitted patch must apply
cleanly to a fresh copy of the recorded baseline and reproduce the retained
candidate bytes.

Hosted validation must exercise the candidate target through CMake/Clang and
Autotools/GNU, run sanitizer/static-analysis coverage for the implementation,
parse the public candidate evidence schema, and independently validate stable
patch/evidence fields and native-build manifest parity.

## Related Records

- ADR-0016
- ADR-0026
- ADR-0035
- ADR-0037
- ADR-0038
- EVO-002
- EVO-HRA-010
- Issues #38, #57, #58, #60, #61, #62, #63, #67, #83, and #93
