# ADR-0036: Clang/LLVM Analysis and Hotspot Model

Status: Accepted

Date: 2026-08-09

## Context

The immutable project baseline introduced by ADR-0035 provides exact source
bytes, normalized compilation units, declared workloads, and bounded execution
policy. It does not yet describe program structure, compiler optimization
evidence, runtime hotspots, or source locations that may justify later source
changes. Issue #59 requires that evidence without authorizing source writes,
candidate materialization, evolutionary operators, or claims that static cost
predicts runtime performance.

Clang and LLVM output is provider- and version-dependent. Treating raw compiler
text, an AST pointer graph, or an implementation-specific index as authority
would make replay opaque and would violate ADR-0026. Missing runtime profiling
also cannot be represented as zero samples because absence is not a performance
measurement.

## Decision

EVO 0.35.0 adds a private, version-1 project-analysis transaction to the
uninstalled source-optimizer foundation.

1. The caller supplies an eligible committed baseline, an absolute isolated
   output path, exact provider/Clang/LLVM/target/flag identities, an explicit
   runtime-profile state, outer record and evidence limits, and one bounded
   provider callback.
2. EVO validates the complete request and output path before invoking the
   provider. The request exposes only the committed read-only snapshot and the
   normalized compilation-unit registry. It carries the baseline's timeout,
   memory, process, storage, output, and `network_access=false` policy.
3. The provider returns explicit source-location, declaration, call,
   control-flow, data-flow, compiler-optimization, and runtime-sample arrays.
   EVO invokes it exactly once, deep-copies every accepted string and record,
   validates all enum values and cross-record references, and sorts each domain
   by stable record identity. Duplicate identities, missing references,
   undeclared workloads, zero-valued sample records, invalid UTF-8, malformed
   ranges, and over-limit results fail closed.
4. Stable location records distinguish spelling locations from macro-expansion
   locations. Each macro expansion names an accepted spelling record. The v1
   ingestion contract rejects generated sources, so provider-generated
   locations return the explicit `unsupported-evidence` status rather than
   becoming committed facts.
5. The runtime profile is exactly one of `not-configured`, `unavailable`, or
   `available`. The first state has no profile identity; the other states have
   one. Only `available` may carry positive sample-count records. Static and
   dynamic records remain separate in the API and evidence.
6. EVO derives an opportunity for each location with a missed compiler record
   or positive runtime sample. Ranking is exact: runtime evidence before absent
   runtime evidence, descending summed sample count, descending missed-record
   count, then stable location identity. A rank is evidence ordering only; it
   does not authorize a transformation or predict a speedup.
7. After every provider return, including an error return, and before any
   evidence publication, EVO re-enumerates the committed snapshot and verifies
   every path, size, hardened mode, and byte fingerprint. Any provider-side
   mutation returns `baseline-changed`.
8. EVO generates bounded `analysis.json` and `analysis.md` from the same owner,
   reserves the output atomically with an incomplete marker, publishes only
   after both representations fit, and hardens the completed output read-only.
   Failures publish no completed analysis and remove only the exact reserved
   partial output.

The complete ordered arrays and their direct deterministic scans are the exact
authority. `fnv1a64-v1:<hex>` analysis and baseline labels are replay
diagnostics only. They are not authentication, collision-resistant content
addresses, provenance, or independent authority.

## Evidence Schema

`analysis.json` conforms to
`catalyst.evo-project-analysis.v1` and retains:

- baseline and analysis diagnostic identities;
- provider, Clang, LLVM, target, flag, and runtime-profile identities;
- every normalized translation-unit identity;
- every accepted location, declaration, call, control-flow, data-flow,
  compiler-optimization, and runtime-hotspot record;
- every derived opportunity and its exact ranking inputs; and
- declarations that the projection is complete, no probabilistic structure is
  authoritative, source was not modified, and no evolutionary operator ran.

`analysis.md` enumerates the same domains in stable order and describes absent
runtime evidence explicitly. It cannot add, repair, suppress, rank, or approve
a fact independently of the retained model.

## Ownership and Resource Rules

The provider owns its result views only for the callback return boundary. EVO
retains no provider pointer: every string and array needed after return is
deep-copied into one analysis owner. The public private-layer view borrows from
that owner and is destroyed exactly once by a null-safe reset function.

All string, path, translation-unit, record, opportunity, and combined evidence
counts are bounded before allocation or publication. The effective evidence
limit is the lower of the caller's outer limit and the immutable manifest's
declared evidence budget. Addition and aggregation are checked against
`SIZE_MAX` or `UINT64_MAX`. An active result, config/result alias, owner alias,
baseline alias, provider-context alias, output overlap, pre-existing output,
malformed profile state, or malformed limit rejects before the provider is
called.

## Human-Readable Abstraction Assessment

No compressed, cached, indexed, probabilistic, or otherwise accelerated
structure is introduced. The implementation deliberately retains explicit
arrays and direct scans, even where an index could reduce lookup cost. Sorting
only establishes deterministic presentation order; it does not hide or replace
records. `analysis.json` is the complete machine-readable projection and
`analysis.md` is the complete human-readable projection of the same owner.
EVO-HRA-008 retains the change-specific assessment.

## Consequences

- Later transformation and search milestones receive stable, explainable
  evidence instead of raw compiler text or pointer identities.
- A concrete provider may invoke the declared Clang/LLVM toolchain in an
  external sandbox, but it must return this bounded versioned contract and is
  never allowed to make its internal caches authoritative.
- Runtime profiling remains optional and visibly unavailable when it cannot be
  collected.
- EVO 0.35.0 remains a private foundation, not a standalone optimizer. Issues
  #60 through #66 build the remaining optimization pipeline; issues #67 and #93
  define and package the executable boundary.

## Rejected Alternatives

- Parsing raw human-oriented compiler diagnostics inside the retained model was
  rejected because formatting and location conventions vary by tool version.
- Retaining Clang AST pointers, address-derived identities, compiler caches, or
  an opaque analysis index was rejected because replay and audit would depend
  on hidden process state.
- Treating an unavailable profile as an empty successful measurement was
  rejected because it would rank unknown runtime cost as zero.
- Letting provider order determine opportunity order was rejected because it
  would make replay dependent on traversal implementation.
- Modifying source while analyzing was rejected because issue #59 is an
  evidence-only boundary.

## Verification

The normative C test uses the immutable multi-translation-unit fixture and a
declared Clang/LLVM provider identity. It covers headers, spelling and macro
locations, calls, control/data flow, compiler misses, runtime records, exact
opportunity ranking, reversed provider order, golden replay identity,
unavailable profiling, malformed schemas, missing references, duplicate
identities, generated-source rejection, path and alias preflight, and malicious
snapshot mutation. CMake/Clang and Autotools/GNU run the same target in the
dedicated project-analysis workflow and AES-BLD-001 matrix.

## Related Records

- ADR-0026
- ADR-0035
- EVO-002
- EVO-HRA-008
- Issues #38, #57, #58, #59, #67, and #93
