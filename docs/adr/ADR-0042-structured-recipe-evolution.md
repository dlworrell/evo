# ADR-0042: Structured Recipe Evolution

Status: Accepted

Date: 2026-08-16

## Context

EVO 0.40.0 completes the serial foundation needed to turn one versioned source
recipe into an isolated candidate, admit that candidate through correctness
assurance, and measure reproducible performance fitness. Issue #65 is the first
boundary that connects those source-optimizer records back to the deterministic
EVO core so a population represents structured source-transformation strategies
rather than arbitrary source bytes.

The existing core deliberately treats genomes as opaque fixed-size byte ranges.
The source optimizer therefore needs an adapter that preserves the core's
selection, generation, tie, and seed semantics while ensuring that every genome
visible to source evaluation is a canonical `catalyst.evo-project-recipe.v1`
envelope. Built-in byte crossover or byte mutation would violate that boundary:
they can cut JSON, padding, record identifiers, or parameter encodings without
respecting transformation structure. Raw C source crossover is prohibited by
the product contract.

A structured search also needs reproducible lineage. A winning fitness value is
not enough to explain which recipe produced which source candidate, assurance
result, and measurement result, or why another recipe was rejected before
compilation. Operator decisions and deterministic repair therefore become part
of the source-optimizer evidence boundary.

## Decision

EVO 0.41.0 adds a private version-1 structured recipe-search adapter to the
uninstalled source-optimizer foundation.

1. The adapter runs the existing `evo_run` core in consumer-operator mode with a
   fixed recipe genome size. It does not fork the core's selection, ranking,
   elite, stopping, diversity, generation, or exact-tie semantics.
2. Every committed source-search genome is a canonical project-recipe v1 fixed
   envelope. Initialization may inspect the deterministic bytes supplied by the
   core only as selector entropy; it must replace the complete buffer with a
   successfully built canonical recipe before the genome may become valid.
3. Mutation and crossover decode complete recipes into bounded proposal-record
   arrays. They operate only on transformation records, stable source targets,
   typed parameters, dependencies, conflicts, and record order. They never
   splice raw C source bytes and never apply generic byte-genome operators.
4. Mutation policy version 1 exposes five structured operations: `add`,
   `remove`, `parameterize`, `replace`, and `reorder`. Operator selection,
   record selection, target selection, parameter movement, and replacement
   choice are derived from a versioned deterministic selector domain containing
   the run seed, operator callback ordinal, parent recipe identity, mutation
   intensity, and policy version.
5. `add` selects only a live analysis opportunity and a catalogue entry allowed
   at that target kind. `remove` removes one whole proposal record.
   `parameterize` changes one typed parameter within its declared schema.
   `replace` swaps one whole transformation reference for another catalogue
   entry compatible with the same live target. `reorder` changes only the
   logical ordering of whole independent records and then renormalizes stable
   record identities; it never reorders bytes inside source text.
6. Crossover policy version 1 combines whole parent records using deterministic
   record cuts and stable inherited order. Canonical deduplication is by the
   complete logical transformation key, not by hashes alone. Child record
   identities are normalized after composition so canonical recipe order is
   independent of stale parent-local ordinal labels.
7. Repair policy version 1 is explicit and deterministic. It may canonicalize
   duplicate inherited records, add a uniquely resolvable missing dependency,
   drop a later conflicting record, and trim a canonical tail to satisfy the
   caller's record budget. Unknown transformations, stale targets, invalid
   parameters, ambiguous dependencies, dependency cycles, impossible budget
   closure, or an unmaterializable final recipe reject rather than being
   guessed or silently rewritten.
8. Every operator attempt is rebuilt through the existing project-recipe
   transaction against the live baseline, analysis, and catalogue. The recipe
   builder remains authority for stale-target, parameter, dependency, conflict,
   cycle, canonical encoding, genome-size, and immutable-baseline checks.
9. A rejected mutation or crossover never reaches candidate materialization or
   compilation. The adapter records the stable structured rejection reason and
   marks that candidate invalid for the core. Rejection is deterministic for a
   fixed seed, lineage, policy, and authority set.
10. Source-search evaluation uses a versioned synchronous evaluation-provider
    boundary. For one decoded valid recipe the provider returns stable source
    candidate, assurance, and measurement identities together with complete
    `evo_fitness_t` evidence and explicit stage booleans. The adapter verifies
    identity presence, finite fitness, correctness preservation, assurance
    admission, and measurement fitness availability before returning fitness to
    the core. Issue #66 owns external-process scheduling and stronger parallel
    orchestration; 0.41.0 remains serial at this product boundary.
11. The adapter records a complete mapping from generation/population genome to
    recipe fingerprint, parent lineage, operator decision, candidate
    fingerprint, assurance fingerprint, measurement fingerprint, fitness, and
    rejection reason. Fitness never substitutes for correctness or recipe
    authority.
12. Search identity is deterministic over a versioned domain containing the
    baseline, analysis, catalogue, recipe/operator/repair/evaluation-provider
    policy identities, run seed and core configuration, ordered population
    lineage, ordered evaluation mappings, and final result. Output paths and
    wall-clock timestamps are excluded.
13. Exact ties preserve the existing EVO core ordering: earlier generation,
    then lower stable population index. The source-search layer adds no alternate
    tie breaker. A later selected result may be described only as the best
    verified candidate found within the recorded bounded search contract.
14. Publication uses canonical JSON plus a complete Markdown lineage projection.
    Evidence states the complete population/generation domain, every attempted
    operator, every invalid/rejected candidate, every accepted evaluation
    mapping, the winning lineage, termination reason, and search bounds.
15. The initial implementation uses direct bounded arrays and deterministic
    scans. No cache, hash index, probabilistic membership filter, compressed
    population, or alternate ranking authority participates in recipe
    compatibility, deduplication, dependency repair, lineage, or winner
    selection.
16. Result destruction releases adapter-owned memory only. It does not remove
    candidate evidence created by an evaluation provider, modify the source
    repository, commit, push, deploy, or publish a product CLI.

## Structured Mutation Semantics

Mutation intensity is interpreted as a bounded request for structured edits,
not a byte probability. Version 1 converts the finite core mutation rate into a
bounded operation count between one and the caller's `max_mutations_per_event`
when the core invokes the mutation callback. Each operation consumes the next
stable selector ordinal. If an operation cannot produce a legal proposal under
the declared repair policy, the whole candidate is rejected atomically rather
than partially accepting an undocumented fallback.

Typed parameter movement is schema-aware. Booleans toggle. Integer values move
one deterministic bounded step and wrap only when the policy explicitly states
that wrap is enabled. Choice parameters move to another catalogue-declared
choice by stable choice order. Required parameters are synthesized only from a
versioned default rule declared by the search policy; optional parameters may be
added or removed only through the same rule.

## Structured Crossover Semantics

A crossover receives two canonical parent recipes and creates two complete
children. Version 1 selects record cut positions in each parent, combines
prefix/suffix record sets as whole logical records, deduplicates in stable
inherited order, normalizes record identities, applies deterministic repair,
and rebuilds both children through the recipe transaction. Parent genome bytes
are never copied by arbitrary byte ranges.

If only one child can be repaired and rebuilt, the crossover attempt fails for
both children. This preserves the core callback contract that one crossover
initializes a complete pair and prevents partial pair authority.

## Evaluation and Lineage Contract

One accepted evaluation record contains at minimum:

- generation and population index;
- exact genome fingerprint and recipe fingerprint;
- parent generation/index identities when applicable;
- operator kind and policy/selector identity;
- candidate fingerprint;
- assurance fingerprint and performance-admission state;
- measurement fingerprint and fitness-available state;
- complete fitness components and total;
- rejection reason when evaluation is absent; and
- whether the record is the final stable winner lineage member.

The evaluation provider may create materialized candidate, assurance, and
measurement artifacts using the existing 0.38.0-0.40.0 transactions. Those
artifacts remain independently authoritative. The search adapter copies their
stable identities and fitness evidence into lineage but never edits or
reinterprets their canonical evidence.

## Resource and Ownership Rules

Caller limits bound population size, generation limit, fixed recipe genome
bytes, retained lineage records, operator events, structured mutations per
event, temporary proposal records, temporary parameter values, repair passes,
evaluation records, canonical JSON, Markdown, and total adapter-owned bytes.
All multiplication and addition are checked before allocation.

The search owner retains copied stable policy/provider identities, complete
lineage records, canonical JSON/Markdown, and the winning recipe genome. Input
baseline, analysis, catalogue, registry, provider callbacks, and provider
contexts are borrowed only for the synchronous call.

## Human-Readable Abstraction Assessment

No accelerated representation is introduced at this boundary. The exact
reference model is the ordered recipe population plus the ordered generation
lineage and evaluation mapping. Deduplication, compatibility checks, dependency
repair, tie handling, and winner selection use direct bounded records and scans.

Canonical JSON records the complete replay/ranking state. Markdown is a
complete deterministic domain projection ordered by generation, population
index, operator ordinal, and record identity. No hash or summary bit may accept,
reject, rank, select, suppress, or publish a candidate independently of the
explicit records and the existing EVO core result.

ADR-0026 accelerator-specific differential requirements are therefore not
applicable to a new accelerator. EVO-HRA-014 retains the change-specific audit
and independently validates the complete projection.

## Consequences

- The EVO core can search source-transformation strategies without gaining
  source-language knowledge or weakening its stable opaque-genome API.
- Fixed seeds and fixed live authorities reproduce recipe populations,
  structured rejection reasons, fitness mappings, and champion lineage.
- Invalid structured children fail before source candidate compilation.
- Exact tie behavior remains the already-governed core behavior.
- Multiple-file recipes remain ordinary recipe records and retain per-record
  transformation provenance through materialization and measurement.
- Parallel candidate execution, resumable product checkpoints, persistent
  artifact orchestration, installed commands, and automatic downstream source
  publication remain later boundaries.

## Rejected Alternatives

- Built-in byte crossover/mutation over recipe envelopes was rejected because
  it would corrupt structure and make most children meaningless byte edits.
- Raw C text genomes were rejected by the product contract and ADR-0037.
- A hidden repair heuristic was rejected because replay could not explain why a
  conflicting or incomplete recipe survived.
- Fitness-only lineage was rejected because it loses the mapping from recipe to
  source candidate and correctness/measurement evidence.
- A hash-set deduplication index was rejected because bounded direct scans are
  sufficient for the initial implementation and provide exact audit semantics.
- Parallel evaluation in this transaction was rejected because issue #66 owns
  the external-process scheduler, cleanup, checkpoint, and completion-order
  boundary.

## Verification

Normative tests must cover fixed-seed generation replay, analysis-driven
initialization, every mutation kind, record-level crossover, canonical
record-identity normalization, duplicate handling, uniquely repairable
missing dependency, ambiguous dependency rejection, conflict repair,
over-budget rejection/trim policy, stale target, invalid parameter, cycle,
raw-source exclusion, multi-file recipes, invalid candidate admission,
provider rejection, strict fitness improvement, exact fitness ties, lineage
mapping, deterministic evidence replay, and result destruction.

The end-to-end fixture must show at least one structured recipe producing a
strictly better verified fitness than its baseline competitor and a separate
exact-tie fixture preserving the stable earlier/lower-index winner.

Hosted validation must run the same normative target with CMake/Clang and
Autotools/GNU, macOS portability, sanitizers/static analysis, an independent
lineage/evidence validator, and AES-BLD-001/AES-SEC-001 inventory parity.

## Related Records

- ADR-0016
- ADR-0026
- ADR-0037
- ADR-0038
- ADR-0039
- ADR-0040
- ADR-0041
- EVO-002
- EVO-HRA-013
- Issues #38, #57, #60, #61, #63, #64, #65, #66, and #83
