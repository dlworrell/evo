# ADR-0037: Versioned Source-Transformation Recipes

Status: Accepted

Date: 2026-08-09

## Context

ADR-0036 supplies immutable baseline authority and a normalized analysis model
with stable evidence-backed source opportunities. Issue #60 must map those
facts into one complete source-optimization genome without treating raw C text,
compiler pointers, cached analysis state, or a checksum as transformation
authority. Later mutation, crossover, and materialization work also needs an
exact compatibility boundary: stale targets, unknown implementations, invalid
parameters, incomplete dependency closure, conflicts, and cycles must reject
before a source workspace is written.

A serialized recipe contains both caller-selected facts and derived facts. If a
decoder trusted serialized ranges, provenance, preconditions, or dependency
edges, a syntactically valid genome could replace the current baseline,
analysis, or catalogue as opaque authority. The format therefore needs a
bounded canonical reconstruction rule, not merely a JSON parser.

## Decision

EVO 0.36.0 adds a private version-1 transformation-recipe transaction to the
uninstalled source-optimizer foundation.

1. The caller supplies an eligible committed baseline, one completed analysis
   bound to that baseline, one canonical transformation catalogue, outer
   resource limits, a complete proposal-record array, and an exact fixed genome
   size.
2. A catalogue has one identity and version and a stable identity/version-
   ordered entry array. Each entry declares supported spelling or macro-
   expansion targets, a stable parameter schema, preconditions, dependencies,
   and conflicts. Catalogue references must name existing entries; self-
   references and dependency/conflict overlap reject.
3. A proposal contains only its stable record identity, target-location
   identity, transformation identity/version, and typed parameter values. EVO
   deep-copies those values, resolves the target against both the analysis
   location registry and ranked opportunity registry, and validates every
   parameter against the selected catalogue schema.
4. EVO derives the exact source range, spelling relationship, preconditions,
   dependency records, conflict declarations, opportunity rank, compiler
   evidence, and runtime evidence from the live baseline, analysis, and
   catalogue. Serialized or caller-supplied substitutes are never accepted as
   authority.
5. Every declared dependency must resolve to exactly one selected recipe
   record. Zero matches return `dependency-missing`; multiple matches return
   `dependency-ambiguous`. Any selected declared conflict returns `conflict`.
   A stable Kahn-style traversal emits ready records by ascending record
   identity. Failure to emit the complete set returns `dependency-cycle`.
6. The canonical JSON orders records by that composition order, parameters by
   identity, and catalogue-owned preconditions, dependencies, conflicts, and
   analysis provenance by their already validated stable domain order.
7. One fixed genome consists of the eight-byte magic `EVORCPG1`, an unsigned
   64-bit little-endian canonical-JSON byte count, those exact JSON bytes, and
   mandatory zero padding through the caller-selected genome size. At least one
   padding byte is required. Raw C source bytes are prohibited.
8. Decode is a reconstruction proof. A bounded parser extracts only proposal-
   bearing fields and global identities, rebuilds every derived field from the
   live authorities, regenerates the complete fixed genome, and requires an
   exact byte-for-byte match. Unknown keys, duplicate keys, altered derived
   evidence, alternate whitespace or escaping, noncanonical order, malformed
   lengths, and nonzero padding therefore fail closed.
9. EVO verifies every committed snapshot path, hardened mode, size, and byte
   fingerprint before and after build or decode. No callback, source write,
   workspace materialization, compiler invocation, or evolutionary operator is
   reachable in this milestone.
10. A deterministic FNV-1a label covers the exact canonical JSON bytes. It is a
    replay diagnostic and equality aid, not authentication, provenance,
    collision-resistant content addressing, or sole authority. Equality uses
    the complete canonical JSON bytes.

## Canonical Evidence

`catalyst.evo-project-recipe.v1` retains:

- exact baseline, analysis, catalogue, and schema identities;
- every record identity and stable resolved source target;
- transformation identity and implementation version;
- every typed parameter and catalogue precondition;
- resolved dependency record identities and declared conflicts;
- opportunity rank plus complete compiler/runtime record provenance;
- the Human-Readable Abstraction declaration; and
- an explicit `raw_source_bytes:false` claim.

The same owner derives a complete Markdown audit projection with global
identity, composition order, target ranges, parameters, dependencies,
conflicts, and provenance. Neither projection can add, repair, suppress, or
authorize a transformation independently of the canonical model and live
authorities.

## Ownership and Resource Rules

Catalogue and proposal views are borrowed only for the call. Every retained
string, parameter, edge, provenance identity, record view, genome byte, and
Markdown byte belongs to one recipe owner and is released by a null-safe fully
resetting destructor. Failure publishes no partial public recipe.

Catalogue entries, schemas, choices, records, parameters, preconditions,
dependencies, conflicts, provenance records, JSON tokens, nesting depth,
strings, and paths have explicit limits before their corresponding traversal,
allocation, or publication. Genome and Markdown capacities also have separate
limits; their combined evidence capacity is bounded by the lower of caller
policy and the immutable manifest evidence budget. Active results and exact
config/context/owner/view aliases reject before mutation of caller state.

## Human-Readable Abstraction Assessment

No compressed, probabilistic, cached, indexed, or otherwise accelerated
structure is introduced. Explicit arrays and direct deterministic scans remain
the reference implementation. Sorting and the topological traversal establish
canonical domain order but do not hide, summarize, or replace a record.

The embedded canonical JSON is the complete portable recipe representation;
the derived Markdown is its complete human-readable audit projection. Decode
does not trust either projection's derived claims: it reconstructs them from
exact live authority and compares the complete encoding. EVO-HRA-009 retains
the change-specific ADR-0026 assessment.

## Consequences

- One later EVO core genome can represent one complete structured recipe with
  exact fixed size and deterministic equality.
- Later AST transformation implementations can be versioned without embedding
  raw source or implementation pointers in genomes.
- Dependency repair is not silently invented. Invalid closure, ambiguity,
  conflicts, and cycles are explicit outcomes for later operator policy.
- A valid recipe still does not authorize or perform source materialization;
  issue #61 defines the transformation catalogue implementation and issue #62
  defines isolated application.
- EVO 0.36.0 remains a private foundation and not an installed standalone
  optimizer. Issues #67 and #93 retain the command and executable gates.

## Rejected Alternatives

- Raw C bytes or token fragments as genomes were rejected because arbitrary
  crossover and mutation cannot preserve source structure or reviewable
  transformation identity.
- Trusting serialized ranges, dependency edges, preconditions, or provenance
  was rejected because a genome would become stale opaque authority.
- Storing AST pointers or address-derived identities was rejected because they
  are process-specific and not replayable.
- Hash-only equality or integrity was rejected because FNV is diagnostic and
  cannot replace exact canonical bytes.
- Hidden dependency/conflict repair was rejected because its decisions would
  not be explicit, versioned, or replayable.
- A transformation lookup cache was rejected at this boundary; bounded direct
  scans keep the first recipe model explainable and provide a future reference
  oracle if acceleration becomes necessary.

## Verification

The normative C target uses an immutable read-only source fixture, explicit
analysis records, and a five-entry catalogue. It covers reverse proposal and
parameter order, exact composition, complete provenance, no-op recipes,
build/decode replay, exact canonical-JSON golden bytes, the fixed-envelope
diagnostic fingerprint, noncanonical JSON,
nonzero padding, corrupt/truncated envelopes, unknown transforms, stale
targets, invalid parameters, missing and ambiguous dependencies, cycles,
conflicts, duplicate identities, catalogue rejection, immutable budget caps,
analysis tampering, baseline eligibility, result aliases, active results, and
source byte/mode preservation. An independent Python validator checks the
schema, retained JSON golden, ordering, references, nullability, abstraction
claims, raw-source exclusion, and FNV vector. CMake/Clang and Autotools/GNU run
the same target in the dedicated project-recipe workflow and AES-BLD-001
matrix.

## Related Records

- ADR-0026
- ADR-0035
- ADR-0036
- EVO-002
- EVO-HRA-009
- Issues #38, #57, #60, #61, #62, #67, and #93
