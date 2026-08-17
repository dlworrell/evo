# EVO-HRA-014: Structured Recipe Evolution Human-Readable Abstraction Audit

Date: 2026-08-16

Audited design: EVO 0.41.0 structured source-recipe evolutionary search

Governing records: ADR-0026, ADR-0042, EVO-002, issue #65

## Inventory

The 0.41.0 boundary connects canonical project-recipe genomes to the existing
EVO core through private consumer initialization, mutation, crossover,
validity, and evaluation adapters. It retains an ordered lineage record for
every generation/population position and maps accepted fitness back to stable
recipe, candidate, assurance, and measurement identities.

The logical authority is explicit:

| Domain authority | Exact representation | Audit projection |
|---|---|---|
| Search seed/configuration | Versioned search policy plus EVO core configuration | Search header and policy fields |
| Genome authority | Complete fixed project-recipe v1 envelope | Recipe fingerprint and canonical recipe evidence |
| Population order | EVO generation and stable population index | Generation/population ordered lineage |
| Mutation | Whole-record operator decision and typed parameter edit | Operator kind, selector ordinal, before/after recipe identities |
| Crossover | Whole parent recipe records and deterministic cuts | Parent identities, cuts, inherited records, child identity |
| Repair | Explicit version-1 repair actions and exact recipe rebuild | Ordered repair actions and rejection reason |
| Validity | Successful live-authority recipe reconstruction | Admission/rejection state |
| Evaluation | Stable candidate/assurance/measurement mapping and finite fitness | Complete evaluation mapping |
| Winner selection | Existing EVO core fitness and exact tie semantics | Winning generation/population lineage |

No hash table, bitmap, cache, compact dependency index, probabilistic filter,
compressed population, learned predictor, or alternate rank authority
participates in this design.

## Exact Authority

Every source-search genome presented as valid must decode and rebuild through
the current project-recipe authority. Raw initialization bytes are selector
input only and are fully replaced before validity. Mutation and crossover never
interpret source text and never apply arbitrary byte operators to a committed
recipe envelope.

Recipe validation remains exact against the live baseline, analysis, and
catalogue. Stale target, unknown transformation, invalid parameter, ambiguous
or cyclic dependency, unresolved conflict, or resource failure cannot be hidden
by the search layer. A deterministic repair action is permitted only when the
versioned repair policy explicitly defines it and the repaired proposal still
passes the ordinary recipe transaction.

The source-search adapter does not infer correctness from fitness. An accepted
evaluation retains separate candidate, assurance, and measurement identities
and requires explicit assurance/performance-admission and measurement-fitness
state before the core receives finite fitness.

## Population and Lineage Projection

The canonical search evidence enumerates every retained lineage record in
stable `(generation, population_index)` order. Each record exposes:

- recipe/genome identity;
- parent generation/index identities where applicable;
- initialization, mutation, crossover, or elite lineage kind;
- deterministic operator selector ordinal and policy version;
- complete repair action/rejection state;
- candidate, assurance, and measurement identities when evaluated; and
- complete fitness components when fitness is admitted.

Exact ties require no source-layer summary or index. The existing core's earlier
generation/lower-index semantics remain the sole tie authority. The winning
lineage can therefore be reconstructed by following explicit parent links from
the final winner back through the ordered records.

## Compatibility, Deduplication, and Repair

Version 1 uses bounded direct scans over explicit transformation records.
Canonical deduplication compares complete logical transformation keys.
Dependency and conflict handling scans live catalogue references and selected
proposal records directly. No lookup result can become stale independently of
the explicit arrays because no lookup cache exists.

Repair actions are themselves ordered evidence. A dropped conflict, inserted
unique dependency, normalized record identity, or budget trim is visible as a
stable action rather than being encoded only in the final recipe. If a legal
repair is not defined, the candidate is rejected and the exact reason remains
in lineage.

## Deterministic Selector Boundary

Operator choices use a versioned deterministic selector domain bound to the run
seed, operator callback ordinal, parent recipe identity, mutation intensity,
and policy version. Any compact numeric selector value is a deterministic
choice input only; it is not provenance or correctness authority. The complete
domain fields and selected operation are retained in evidence.

A selector collision can at most choose the same legal structured operation for
two different domains. It cannot make an invalid recipe valid, bypass the
recipe builder, alter correctness admission, or substitute for exact tie/winner
logic.

## Failure, Freshness, and Fallback

Baseline, analysis, and catalogue identity are checked through the existing
recipe transaction for every initialized, mutated, or crossed child. A failed
structured operation publishes no partially valid genome. A rejected child is
represented as invalid to the core and cannot reach source candidate
compilation.

There is no accelerated path requiring a separate exact fallback. Direct recipe
records, direct repair scans, the existing recipe transaction, and the existing
EVO core are already the reference path.

## Canonical and Human-Readable Evidence

Canonical JSON retains the complete search policy, source-authority identities,
ordered lineage, operator/repair decisions, evaluation mappings, core result,
termination, and search fingerprint. Markdown is generated from the same owner
and projects the same complete domain in readable generation/population order.

The Markdown view is not an independent ranking source. If canonical evidence
or the complete projection cannot be generated within the caller's declared
budget, the transaction fails closed rather than publishing a winner without
reviewable lineage.

## Differential and Independent Evidence

ADR-0026 accelerator differential testing is not triggered because the initial
implementation has no accelerated substitute for recipe, dependency,
compatibility, lineage, or ranking authority.

Independent validation instead checks that:

- every committed lineage recipe is a canonical project-recipe genome;
- raw C source bytes are absent from operators and evidence authority;
- every operator works on complete records or typed parameters;
- all repair actions precede successful recipe reconstruction;
- rejected recipes have no evaluation mapping;
- every evaluated recipe maps to candidate, assurance, and measurement
  identities before fitness;
- exact ties preserve generation/population ordering; and
- the final winner is reachable through the explicit lineage graph.

Hosted CMake/Clang, Autotools/GNU, sanitizer/static-analysis, macOS portability,
and AES-BLD-001/AES-SEC-001 validation remain independent implementation
evidence.

## Result

The EVO 0.41.0 structured-recipe evolution design conforms to the
Human-Readable Abstraction Rule without introducing an accelerated
representation. Exact recipe records, live-authority rebuilds, explicit lineage,
and the existing EVO core remain authority. Every source-search acceptance,
rejection, fitness mapping, tie, and winner is reconstructable from the complete
ordered projection.

This audit does not pre-approve external-process parallel scheduling,
completion-order buffering, product checkpoint indexes, persistent artifact
stores, distributed workers, installed commands, or automatic source
publication. Those remain later ADR-0026 boundaries, including issue #66.
