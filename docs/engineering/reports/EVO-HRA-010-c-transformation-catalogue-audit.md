# EVO-HRA-010: C Transformation Catalogue Human-Readable Abstraction Audit

Date: 2026-08-15

Audited implementation: EVO 0.37.0

Governing records: ADR-0026, ADR-0038, EVO-002, issue #61

## Inventory

The transformation-catalogue addition consists of four private production
units and five private headers. It introduces no installed API or executable.

| Domain authority | Exact representation | Stable audit projection |
|---|---|---|
| Transformation registry | Three identity-ordered static recipe entries and matching capability records | Complete version-1 catalogue JSON and derived Markdown table |
| Dispatch | Direct identity/version comparison followed by one explicit implementation branch | Transformation identity/version and accepted AST forms in registry/application evidence |
| AST evidence | One normalized provider record checked against live recipe and immutable bytes | AST form, operator, controlling context, provider version, and Clang identity |
| Source decision | One zero-based half-open before range and exact owned replacement, or explicit no-change | Complete application JSON and Markdown before/replacement fields |
| Semantics | Explicit ordered assumption and validation-obligation arrays | Complete arrays in both registry and application projections |
| Unsupported cases | Direct flags, source-byte scan, and stable rejection status | Registry `unsupported` policy and stable status names |

The audit found no transformation index, AST cache, hash table, bitmap,
compressed range map, membership filter, probabilistic authority, retained
Clang pointer, address-derived identity, hidden formatter, or raw-source
genome. Catalogue validation, recipe lookup, file lookup, AST-form validation,
and dispatch use bounded direct scans or explicit branches.

## Exact Authority and Projection Completeness

- Static catalogue and capability arrays identify every supported operation,
  version, parameter schema, AST form, formatting/idempotence policy, semantic
  assumption, validation obligation, and unsupported category.
- The recipe supplies the stable transformation and source-location identity;
  the baseline supplies exact immutable bytes; the provider supplies only
  normalized language facts.
- EVO independently resolves the recipe's line/column range and requires the
  provider target range to match it exactly.
- An accepted application retains the complete before range/text and complete
  replacement text, not a summary or token hash.
- Registry and application JSON explicitly declare complete projection and
  absence of probabilistic authority. Markdown is derived from the same exact
  model and cannot authorize a separate outcome.
- `already-satisfied` is an explicit complete result, not suppression by a
  cache or duplicate filter.

## Freshness, Failure, and Fallback

- Baseline mode, size, and byte evidence are verified before source use, after
  provider execution, and after evidence construction.
- Analysis, recipe, and catalogue owner/view identity and diagnostic labels are
  checked against their live committed owners. Substitution or incomplete
  projection fails closed.
- Provider failure, malformed or mismatched ranges, unsupported macros,
  comments, directives, extensions, alias assumptions, ambiguous targets,
  semantic precondition failure, and resource exhaustion publish no partial
  application.
- There is no accelerated path needing fallback. Static arrays, direct scans,
  and exact bytes are already the reference path.
- Application never writes source or creates a candidate, so failure cannot
  leave a partially materialized tree.

## Fingerprint Boundary

FNV-1a labels cover source slices, replacements, canonical application fields,
and earlier authority records for deterministic diagnostics. They are not
authentication, provenance, collision-resistant content addressing, or sole
acceptance authority. Exact bytes, live owners, and complete ranges remain
required even when a diagnostic label matches.

## Differential and Independent Evidence

No accelerator requires a separate fast/reference differential algorithm. The
normative test nevertheless repeats every successful application and requires
identical fingerprints, JSON, and Markdown. It composes only the three emitted
half-open edits and requires exact equality with the retained after fixture.
It also requires runtime catalogue and assignment-application JSON to match
their retained canonical goldens byte-for-byte. Already-satisfied source
produces explicit no-change applications for all three transformations.

The independent Python validator does not call the C implementation. It checks
the two public evidence schemas and retained catalogue/application goldens,
recomputes before and replacement fingerprints, reconstructs the complete
application fingerprint field sequence, and independently applies the three
declared fixture ranges. The dedicated Clang/sanitizer/analyzer and
Autotools/GCC workflow supplies cross-frontend and runtime evidence.

## Result

EVO 0.37.0's AST-aware C transformation catalogue conforms to the
Human-Readable Abstraction Rule. Stable static arrays and direct dispatch are
exact authority; every accepted decision projects one complete source edit or
explicit no-change result; and no accelerated or probabilistic structure can
hide or commit a transformation. This finding does not pre-approve candidate
materialization, overlap resolution, candidate caches, recipe operators,
external-process orchestration, artifact storage, or the standalone
executable. Each remains subject to ADR-0026.
