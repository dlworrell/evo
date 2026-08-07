# ADR-0024: Generalized Deterministic Elite Preservation

Status: Accepted (operator dispatch extended by ADR-0025 and ADR-0027)
Date: 2026-08-03
Decision owner: EVO

## Context

EVO 0.13.0 preserves exactly one stable-best parent in the final slot of an
odd population. That rule makes odd child production complete and replayable,
but it cannot express zero elites for odd populations, one elite for even
populations, or a caller-bounded elite set. Simply subtracting an elite count
from the old pair count is insufficient: the ordinary-offspring prefix may
become odd, valid parents may be fewer than the request, and every slot and RNG
stream still needs one unambiguous owner.

The generalized policy must retain the exact pre-0.24.0 sequence for existing
zero-initialized configurations. It must also reuse fitness-comparison policy
version 1, exclude hard-invalid parents, preserve distinct parent owners, avoid
hidden RNG consumption, and expose enough private evidence to validate a
promoted population later.

## Decision

EVO 0.24.0 appends `elite_count_enabled` and `elite_count` to `evo_config_t`
and publishes `EVO_ELITE_POLICY_VERSION == 1`.

For a positive-limit transition, configuration is interpreted as follows:

- disabled mode requires `elite_count == 0` and requests one elite when
  `population_size` is odd, otherwise zero;
- enabled mode accepts every count from zero through `population_size`;
- the effective count is `min(requested_count, source_valid_count)`;
- the ordinary-offspring count is
  `population_size - effective_count`; and
- a completed source with no valid parent cannot begin another transition.

The cap defines the fewer-valid-than-request case without cloning one parent
more than once. It preserves every distinct valid parent when the request is
larger than the valid set and assigns the remaining population slots to
ordinary offspring.

### Slot ownership

Ordinary offspring occupy the prefix `[0, ordinary_offspring_count)`. Elites
occupy the suffix and are cloned in stable best-to-worst order. Ranking uses the
same comparison authority as evaluation, selection, and global-best
replacement: greater caller-computed total, then lower population index because
all candidates belong to the same source generation. Hard-invalid or
unevaluated records are never rankable.

The ordinary prefix owns
`floor(ordinary_offspring_count / 2)` complete pairs. Pair ordinals, selection
streams, crossover streams, and child-indexed mutation streams keep their
existing version-1 tuple identities.

If the ordinary prefix is odd, singleton-child policy version 1 owns its final
slot:

1. derive the selection stream at index
   `floor(ordinary_offspring_count / 2)`, the next unused pair-selection index;
2. perform one ordinary valid-only tournament;
3. clone the selected parent byte-for-byte into child index
   `ordinary_offspring_count - 1`; and
4. run the standard mutation dispatcher with that child index's mutation
   stream.

The singleton path invokes no crossover and allocates no scratch sibling. Its
mutation dispatcher retains the established one-word probability decision,
including rate endpoints and an absent mutation callback.

After the prefix is complete, elite completion performs a full dry ranking
pass before copying any byte, then clones the stable suffix. Elite ranking and
copying allocate no memory, derive no stream, consume no RNG word, and invoke
no consumer callback.

### Compatibility and evidence

A zero-initialized 0.24.0 configuration leaves the new fields disabled. For an
odd population it therefore produces the same complete-pair prefix, selection,
crossover and mutation streams, child bytes, and final stable-best clone as
0.23.0. The existing odd-tail policy version-1 marker is retained only for this
compatibility form. An even compatibility population retains its all-pair,
zero-elite sequence.

Produced-population evidence records elite policy version, explicit-versus-
compatibility mode, effective elite count, source valid count, and singleton
policy version. Private completion evidence additionally records requested
count, ordinary-offspring count, and best and worst retained parent indexes.
Child-evaluation policy advances to version 4, generation-advancement policy to
version 4, and bounded-run policy to version 6 so those facts remain
reviewable through evaluation, promotion, and run completion.

All object and owned-byte ranges used by singleton or elite completion must be
independent. Library-detectable configuration, lifecycle, provenance, bounds,
ranking, and alias failures occur before callback dispatch or elite copying and
preserve parent state, child state, and caller evidence. Consumer mutation
callbacks retain their existing no-rollback contract after dispatch begins.

ADR-0025 routes the ordinary singleton's parent draw through selection-policy
version 1 in EVO 0.25.0. Tournament mode preserves the sequence specified
above; rank mode uses the same next-unused selection stream. Elite ranking and
copying remain deterministic, RNG-free, and independent of the parent-
selection policy. Selection provenance is added to produced and completion
evidence.

ADR-0027 routes that singleton's mutation through byte-operator policy version
1 in EVO 0.26.0. Consumer mode preserves its exact callback sequence; byte-XOR
mode uses the same child-indexed mutation stream and bypasses the callback.
Elite copies remain operator-free, while completion evidence preserves both
configured operator selectors for lifecycle validation.

ADR-0028 adds effective mutation-rate provenance in EVO 0.27.0. An ordinary
singleton uses that rate; elite copies still invoke no mutation. A one-member
compatibility or explicit-one-elite run has no mutation path, so adaptation is
not applicable and unused mutation/adaptation payload remains uninspected.

## Consequences

- Counts `0`, `1`, `N - 1`, and `N` have one definition for even and odd
  populations.
- Explicit zero is meaningful and may require a singleton for an odd
  population; it is distinct from disabled compatibility mode.
- A request larger than the valid parent set preserves every valid parent once
  and produces ordinary offspring for the remaining slots.
- Stable ties preserve lower parent indexes, and elite suffix order is directly
  reviewable.
- Elite preservation cannot perturb operator streams or callback counts.
- A singleton changes only the slot that no complete pair can own; later pair
  streams do not exist for that prefix.
- Parent genomes and evaluations remain read-only, and successful elite copies
  retain independent child ownership.
- The algorithm uses constant auxiliary storage and `O(E * N)` comparison work
  for `E` effective elites and population size `N`.
- The public configuration layout grows, so consumers must rebuild; no public
  function signature, installed symbol, or allocation budget changes.
- A zero `generation_limit` continues to ignore unused transition policy.

## Alternatives considered

### Clone the stable best repeatedly when valid parents are scarce

Rejected because repeated ownership would make the requested count appear
satisfied while reducing population diversity and obscuring which distinct
parents survived.

### Sort through a temporary index allocation

Rejected because elite preservation does not need another allocation class or
failure point. Repeated stable scans are bounded and keep the commit suffix
allocation-free.

### Give an odd prefix a scratch crossover sibling and discard one output

Rejected because it would require extra storage, invoke crossover for a child
with no retained sibling, and introduce discard semantics absent from the
pair contract.

### Clone the stable best as the ordinary singleton

Rejected because the slot is not an elite. The next selection stream and the
normal mutation stream preserve ordinary offspring semantics and give its RNG
ownership an explicit replay identity.

### Change disabled mode to explicit zero elites

Rejected because existing odd-population callers would receive different
child bytes, callback counts, and RNG scheduling merely by rebuilding.

## Verification

- `tests/elite_test.c` covers explicit counts `0`, `1`, `N - 1`, and `N` for
  even and odd populations; stable ties; invalid-heavy capping; suffix order;
  singleton stream evidence; replay; compatibility bytes; callback neutrality;
  parent preservation; and object and owned-range alias rejection.
- `tests/child_tail_test.c` retains the fixed pre-0.24.0 odd-tail vectors and
  compatibility evidence.
- Child-evaluation, generation-advancement, and bounded-run tests verify
  provenance propagation through completed populations.
- `tests/allocation_failure_test.c` exercises explicit elite mode across the
  bounded public path and locks empty cleanup for every injected allocation
  failure.
- CMake, GNU Autotools, and AES-BLD-001 enumerate the same twenty-two
  production sources and twenty-six normative tests.
- GitHub issue: `https://github.com/dlworrell/evo/issues/46`
