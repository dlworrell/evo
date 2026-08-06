# ADR-0027: Reference Byte-Genome Crossover and Mutation Operators

Status: Accepted
Date: 2026-08-04
Decision owner: EVO

## Context

EVO has exposed representation-neutral consumer crossover and mutation
callbacks since the first deterministic generation pipeline. That boundary is
necessary for arbitrary domains, but it leaves every consumer to reimplement
even the simplest bounded byte-genome operators and their replay rules.

The 1.0 core roadmap requires reference one-point, two-point, and uniform
crossover plus deterministic byte mutation. These helpers must be selected
explicitly: neither a missing callback nor the size of a genome proves that the
consumer intends byte semantics. Adding them also must not change the exact
callback, clone, no-op, or RNG behavior of zero-initialized pre-0.26.0
configurations.

The operators are EVO Core utilities. They are not permission to splice or
mutate C source text. Source evolution remains an AST-aware transformation-
recipe boundary governed by ADR-0016 and EVO-002.

## Decision

EVO 0.26.0 publishes byte-operator policy version 1 and appends explicit
crossover and mutation selectors to `evo_config_t`:

```c
typedef enum evo_crossover_operator {
    EVO_CROSSOVER_CONSUMER = 0,
    EVO_CROSSOVER_BYTE_ONE_POINT = 1,
    EVO_CROSSOVER_BYTE_TWO_POINT = 2,
    EVO_CROSSOVER_BYTE_UNIFORM = 3
} evo_crossover_operator_t;

typedef enum evo_mutation_operator {
    EVO_MUTATION_CONSUMER = 0,
    EVO_MUTATION_BYTE_XOR = 1
} evo_mutation_operator_t;
```

The zero-valued selectors are the compatibility defaults. Consumer crossover
still invokes the callback once when its existing probability gate selects the
event and otherwise clones corresponding parents. A missing crossover callback
still selects the same clone path. Consumer mutation still invokes the callback
once when selected, forwards the configured rate unchanged, and otherwise
leaves the genome untouched. Built-in modes take precedence over non-null
callbacks and never invoke them.

Every active selector must be a defined enum. The byte genome remains the
complete caller-bounded `problem->genome_size` span, which must be nonzero and
no larger than `max_genome_bytes`. Crossover parents are read-only. Both child
spans must be distinct, non-overlapping, and disjoint from both parent spans.
Every successful crossover completely initializes both children. The helpers
allocate no memory and add no resource budget.

### One-point crossover

For a selected event with byte length `n > 1`, EVO samples one unbiased
internal boundary `cut` in `[1, n - 1]`. Child A receives parent A bytes
`[0, cut)` and parent B bytes `[cut, n)`; child B receives the complementary
ranges. For `n == 1`, no internal boundary exists, so corresponding parents are
cloned and no cut draw is consumed.

### Two-point crossover

For a selected event with byte length `n`, EVO samples two distinct boundaries
uniformly without replacement from `[0, n]`, sorts them as `lower < upper`, and
swaps the half-open byte range `[lower, upper)`. Bytes outside that range retain
their corresponding parent. Boundary endpoints are deliberate: the policy can
swap a prefix, suffix, or the complete genome and is defined for `n == 1`.

The implementation first samples one of `n + 1` boundaries, then samples one
rank among the remaining `n` boundaries and maps that rank around the first.
`n == SIZE_MAX` is rejected because the first bound cannot be represented.

### Uniform crossover

For a selected event, EVO consumes one 32-bit mask for every group of up to 32
bytes. The least-significant available bit controls the next ascending byte
offset: zero retains corresponding parents and one swaps them. Child B is
always complementary to child A. Unused high bits in the final word are
discarded.

### Byte-XOR mutation

For a selected mutation event over `n` bytes, EVO samples one unbiased byte
index in `[0, n - 1]`, then one unbiased value in `[1, 255]`, and XORs exactly
that byte with the nonzero value. The selected event therefore changes exactly
one byte, including when `n == 1`, without writing outside the genome. The
configured mutation rate remains the per-genome selection probability in this
policy; adaptive intensity is separate roadmap work.

### Fixed RNG-consumption schedule

Every valid crossover pair and mutation attempt first consumes the existing
one-word probability event, including rate zero and rate one. If the event is
not selected, no operator-specific draw follows.

Selected built-in draws occur in this exact order:

| Operator | Draws after the probability word |
|---|---|
| One-point, `n == 1` | none |
| One-point, `n > 1` | bounded sample with bound `n - 1`, then add one |
| Two-point | bounded sample with bound `n + 1`, then bounded sample with bound `n` |
| Uniform | `ceil(n / 32)` successive 32-bit mask words |
| Byte-XOR mutation | bounded sample with bound `n`, then bounded sample with bound `255` and add one |

Each bounded sample retains RNG algorithm version 1's existing unbiased rule:
two 32-bit words form one little-word-order 64-bit sample, rejected modulo-bias
prefixes consume another two-word sample, and only an accepted value is reduced
by the bound. Rejection in one pair or child cannot shift any later operator
because the existing pair- and child-indexed domain streams remain unchanged.

### Provenance and lifecycle composition

Produced populations and pair, singleton, elite, child-evaluation, generation-
advancement, and bounded-run evidence record
`EVO_BYTE_OPERATOR_POLICY_VERSION == 1` plus both configured selectors. A
completed produced population is rejected when that provenance does not match
the active configuration.

Child-evaluation policy advances to version 6, generation-advancement policy to
version 6, and bounded-run policy to version 8. This changes private evidence
schemas only. Supported public function signatures and symbols do not change;
the implementation adds validator symbols declared only by non-installed
headers.

## Human-Readable Abstraction Assessment

This decision introduces no compressed, cached, indexed, probabilistic, or
otherwise accelerated structure. The bounded byte arrays are both the exact
reference representation and the bytes on which authority operates. Cut
boundaries, half-open ranges, uniform masks, selected mutation index, nonzero
XOR value, and RNG order are defined directly above and locked by golden-vector
tests. Operator selectors and policy version are propagated as explicit
evidence.

ADR-0026 therefore requires no separate accelerator projection API for this
change. There is no hidden table, bitmap, cache, filter, or index to project or
reconcile. If a later implementation accelerates these operators or retains
their decisions in a compact structure, that change must define an exact
fallback plus a deterministic audit projection of selector, parameters,
boundaries or masks, affected byte ranges, source identity, and completeness,
with differential equivalence to these reference semantics.

## Consequences

- Zero-initialized callers retain consumer dispatch and exact pre-0.26.0 replay.
- Consumers can opt into portable byte operators without supplying callbacks.
- Operator representation is explicit and cannot be inferred accidentally.
- Every selected crossover writes both complete child spans; selected byte-XOR
  mutation always changes exactly one in-bounds byte.
- Built-in modes are allocation-free and preserve existing resource budgets and
  operator-domain isolation.
- Appending selectors changes `sizeof(evo_config_t)`, so consumers must rebuild.
- The helpers are intentionally byte-oriented and do not understand typed
  fields, encodings, graph structure, checksums, domain validity, or repair.
- Raw C source text remains outside this operator boundary.

## Alternatives considered

### Infer byte mode when a callback is absent

Rejected because absence historically means clone/no-op compatibility and says
nothing about representation. Inference would silently change existing runs.

### Replace the consumer callback path

Rejected because domain-specific crossover, mutation, and repair remain core
extension points. Explicit zero-valued dispatch preserves them.

### Permit equal two-point boundaries

Rejected because a selected event could become an accidental empty-range copy.
Distinct boundaries give every selected two-point event a nonempty swapped
range while still allowing all endpoint cases.

### Draw one random word per uniform byte

Rejected because consuming mask bits in stable ascending order is simpler,
faster, and gives a compact fixed schedule without adding stored state.

### Mutate an independently selected bit

Rejected for the initial reference policy because a nonzero byte mask exercises
all byte bits while guaranteeing one changed byte with two bounded samples. Bit-
level, integer, floating-point, permutation, and adaptive policies remain
separate versioned decisions.

## Verification

- `tests/crossover_test.c` covers invalid selectors, full-span alias rejection,
  one-byte and equal-parent behavior, every internal one-point cut, every
  distinct two-point boundary pair, uniform masks crossing a 32-byte boundary,
  rate-zero clones, callback bypass, golden children, exact post-operation RNG
  state, guard bytes, and replay.
- `tests/mutation_test.c` covers invalid selectors, one-byte and multi-byte
  genomes, every byte position, nonzero XOR behavior, rate-zero no-op, callback
  bypass, golden bytes, exact post-operation RNG state, guard bytes, and replay.
- Child-pair and lifecycle tests prove built-ins compose through existing
  domain-separated streams, bypass consumer callbacks, replay identically, and
  propagate selector/version evidence through evaluation and promotion.
- Strict C17 warning builds, sanitizers, analyzers, CMake/Clang/LLVM, and
  Autotools/GNU exercise the unchanged twenty-two-source and twenty-six-test
  inventories.
- GitHub issue: `https://github.com/dlworrell/evo/issues/48`
