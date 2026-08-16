# ADR-0038: AST-Aware C Transformation Catalogue

Status: Accepted

Date: 2026-08-15

## Context

ADR-0037 represents one complete source genome as a canonical recipe whose
records identify stable analysis targets and versioned transformations. Issue
#61 must give those identities concrete C17 semantics without treating recipe
text, provider pointers, a source checksum, or a formatter as permission to
rewrite arbitrary source. Issue #62, not this change, owns isolated candidate
materialization.

A useful transformation boundary must reconcile two kinds of evidence. Clang
provides language-aware facts such as declaration identity, scalar type,
unsigned result width, and macro provenance. The immutable baseline remains
the authority for the exact bytes that a later materializer may replace. A
provider result that names a different range, misreports a token, crosses a
comment or directive, or changes the snapshot during analysis must not produce
an accepted edit.

The initial catalogue also needs narrow, reviewable semantic claims. It cannot
claim universal C equivalence, accept compiler extensions by default, or
silently infer alias facts that the normalized provider contract does not
prove.

## Decision

EVO 0.37.0 adds a private version-1 AST-provider contract, an exact built-in C
transformation catalogue, and a non-writing application transaction to the
uninstalled source-optimizer foundation.

1. The built-in catalogue identity is
   `catalyst.evo.c.ast-transformations`, version 1. Its identity-ordered static
   recipe entries and capability records are the exact selection and dispatch
   authority. A generated canonical JSON registry and Markdown audit expose
   the complete same model.
2. Application accepts only an eligible committed immutable baseline, a
   completed exact analysis, a canonical recipe built from the same authority,
   the built-in registry, one recipe-record identity, bounded provider
   identity/version data, explicit resource limits, and one normalized AST
   provider callback.
3. EVO resolves the recipe target to a baseline file, opens each relative path
   component without following symbolic links, reads only the immutable
   snapshot, verifies size, hardened mode, and bytes, and converts the recipe's
   one-based line/column range to an exact zero-based half-open byte range.
4. The provider receives contract version 1, all baseline/analysis/recipe and
   transformation identities, the immutable snapshot path, exact target,
   parameters, source size and diagnostic fingerprint, and explicit
   `network_access:false`. It returns a completed normalized AST record, not a
   retained Clang pointer or source rewrite.
5. EVO requires the returned file and location identity to match the recipe
   and the returned target bytes to equal the independently resolved recipe
   range. Every component range must be nonempty, ordered, contained by that
   target, and appropriate to the declared AST form.
6. Provider status, catalogue authority, and the complete snapshot are checked
   again after the callback. Snapshot mutation at any point returns
   `baseline-changed`; stale or substituted recipe/catalogue authority fails
   closed.
7. The application emits either one exact owned source edit or a deterministic
   `already-satisfied` no-change record. It never writes the snapshot, creates
   a candidate workspace, applies the edit, invokes a compiler, or runs a
   correctness gate.
8. All three transforms reject macro-expansion targets, target comments,
   preprocessor directives, non-C17 bytes/extensions, ambiguous targets,
   volatile accesses where relevant, and provider-declared alias assumptions.
   These outcomes have stable rejection names.
9. Exact before bytes, replacement bytes, half-open before and after ranges,
   separate diagnostic fingerprints, AST form/operator/context, provider and
   Clang identities, catalogue/recipe provenance, formatting policy, semantic
   assumptions, and validation obligations are retained in
   `catalyst.evo-c-transformation-application.v1`.
10. Application and registry FNV-1a labels are deterministic diagnostics. They
    are not authentication, collision-resistant content identity, or
    authorization. Live authority plus exact bytes and ranges remain required.

## Initial Transformations

### Assignment to compound assignment

`catalyst.evo.c.assignment-to-compound`, implementation version 1, accepts the
required `operator` parameter with `add`, `subtract`, `multiply`,
`bitwise-and`, `bitwise-or`, or `bitwise-xor`.

The accepted AST is a plain nonvolatile identifier assignment of the form
`x = x op operand`. Both identifier ranges must contain identical basic C
identifier-token bytes and resolve to the same declaration, and the source
bytes between normalized ranges must contain exactly `=` and the selected
operator after insignificant whitespace/parentheses are removed. The provider
must establish the compatible C type semantics. The replacement is formatted
deterministically as `x op= operand`. A matching compound-assignment AST
returns no change.

Plain identifiers exclude member, subscript, dereference, call, increment, and
other side-effecting lvalues for which the single-evaluation rule would require
additional proof.

### Double-negation condition

`catalyst.evo.c.double-negation-condition`, implementation version 1, accepts
the required `context` parameter with `if`, `while`, `do-while`, or `for`.

The accepted AST is a scalar controlling expression whose exact target prefix
contains two `!` tokens and only whitespace before the operand. The operand
ends at the target boundary. The replacement preserves the operand bytes
exactly. A scalar-condition AST whose operand is the complete target returns
no change. This transformation claims preservation of C truth testing, not
preservation of an integer materialization of `!!operand` outside a controlling
expression.

### Unsigned multiply by a power of two

`catalyst.evo.c.unsigned-multiply-to-shift`, implementation version 1, accepts
the required integer `maximum-shift` parameter in `1..63`.

The accepted AST is an unsigned multiplication by a decimal C17 integer
constant whose independently parsed source bytes equal the provider value and
whose value is a power of two. The provider must establish that the
multiplication result is unsigned, its width is in `1..64`, and the shift
result type matches the original result type. The derived shift must be
nonzero, less than the result width, and no greater than `maximum-shift`.
The replacement is the parenthesized form `(primary << N)`. A matching
left-shift AST returns no change.

Hexadecimal, octal, digit-separated, floating, macro-produced, extension, or
source/provider-disagreeing constants are outside version 1 and reject rather
than being normalized speculatively.

## Formatting and Comment Boundary

Formatting is transformation-local and deterministic:

- compound assignments use one ASCII space around the compound operator;
- conditions preserve the operand spelling byte-for-byte; and
- shifts use one parenthesized ASCII form with spaces around `<<`.

No whole-file formatter participates in application authority. A comment
inside the declared target rejects because removing or moving it would violate
the exact-range review boundary. Comments outside the target remain untouched.
Preprocessor bytes and non-ASCII/invalid control bytes inside a target reject.

## Semantic and Validation Boundary

Every capability publishes explicit semantic assumptions and the same minimum
validation obligations: baseline build, baseline correctness, C17 syntax, and
sanitizers. These declarations do not prove program equivalence. They specify
what the provider must establish and what later candidate assurance must run
before an edit may contribute fitness or publication evidence.

The catalogue is intentionally not exhaustive. Version 1 does not support
arbitrary natural-language rewrites, raw-source crossover, universal alias
reasoning, compiler extensions, generated macro spelling, exhaustive C
transformation search, or a promise of global optimality.

## Ownership and Resource Rules

Provider and authority views are borrowed only for the application call. One
private owner deep-copies every retained identity, target string, policy,
assumption, obligation, exact before byte, replacement byte, JSON byte, and
Markdown byte. Destruction is null-safe and resets the complete public view.
Any failure releases partial ownership and leaves the caller result inactive;
an active result cannot be overwritten.

Source, string, path, replacement, registry JSON, application JSON, Markdown,
and combined evidence bytes have separate nonzero limits. Source size must also
fit the immutable manifest's file budget, and application evidence must fit the
lower of the caller total limit and manifest evidence budget. Arithmetic is
checked before allocation or range composition.

## Human-Readable Abstraction Assessment

No cache, lookup index, hash table, membership filter, compressed AST,
probabilistic structure, or retained compiler object is introduced. Stable
capability arrays and direct bounded scans/dispatch are the exact reference
implementation. The registry JSON completely enumerates every transformation,
parameter, AST form, rejection capability, semantic assumption, and validation
obligation. Markdown derives from the same static authority.

Each accepted application exposes one exact source range and replacement as
its reference form, plus complete canonical JSON and derived Markdown. No
projection suppresses a target, edit, assumption, or decision. EVO-HRA-010
retains the change-specific ADR-0026 assessment.

## Consequences

- Recipes now name concrete, stable, reviewable C17 transformation semantics.
- A later materializer can consume an exact edit without reinterpreting AST
  intent, while still revalidating baseline and application authority.
- Unsupported syntax and unproved semantics produce explicit rejection rather
  than an unsafe best effort.
- Deterministic no-change results make reapplication observable and idempotent.
- Issue #62 still owns isolated source-tree copying, edit composition,
  overlap policy, atomic candidate publication, and candidate identity.
- EVO 0.37.0 remains a private foundation and not an installed optimizer
  executable. Issues #67 and #93 retain the product-command and binary gates.

## Rejected Alternatives

- Arbitrary natural-language source rewriting was rejected because it has no
  stable versioned semantics, exact AST applicability proof, or bounded review
  range.
- Trusting provider byte ranges or numeric values without checking the
  immutable source was rejected because provider evidence would replace
  baseline authority.
- Applying edits directly to the baseline was rejected because issue #58
  defines it as immutable and issue #62 requires isolated materialization.
- Running a general formatter was rejected because unrelated changes would
  escape the declared transformation ranges.
- Supporting macro expansions by editing spelling heuristically was rejected
  because one expansion may not identify one safe writable source range.
- Hash or cache lookup authority was rejected because three static entries and
  direct dispatch are simpler, exact, and auditable.
- Claiming general semantic equivalence was rejected because correctness is
  bounded by declared assumptions and later recorded validation gates.

## Verification

The normative C target constructs a read-only immutable baseline and a live
analysis/recipe chain, then applies all three transformations through a
normalized fixture provider. For every transform it covers positive output,
the exact replacement-byte limit and one-byte-under rejection, a failed
semantic precondition, malformed range evidence, deterministic replay, and
already-satisfied idempotence. It composes only the three declared half-open
edits and requires byte equality with the retained after fixture. Runtime
catalogue and assignment-application JSON must also match their retained
canonical goldens byte-for-byte.

Additional cases cover provider failure, macro, comment, preprocessor,
extension, alias-assumption, ambiguous-target and literal-disagreement
rejection; stale/ineligible authorities; source and evidence budgets; active
results; provider request fields; immutable source mode/bytes; and mutation of
the snapshot during callback execution. The dedicated workflow compiles both
C fixtures as strict C17, runs the target under Clang sanitizers and
Autotools/GCC, and runs Clang static analysis over every implementation unit.
An independent Python validator checks both schemas, catalogue/application
goldens, exact field sets and stable order, capability bindings, source and
replacement fingerprints, the complete application fingerprint vector, and
composition of the exact ranges to the after fixture.

## Related Records

- ADR-0016
- ADR-0026
- ADR-0035
- ADR-0036
- ADR-0037
- EVO-002
- EVO-HRA-010
- Issues #38, #57, #61, #62, #63, #67, #83, and #93
