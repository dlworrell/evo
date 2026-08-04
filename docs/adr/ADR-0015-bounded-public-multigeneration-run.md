# ADR-0015: Bounded Public Multi-Generation Run

Status: Accepted (operator dispatch extended by ADR-0025 and ADR-0027)
Date: 2026-08-02
Decision owner: EVO

## Context

EVO 0.15.0 can construct and evaluate generation zero, produce and evaluate a
complete child population through deterministic versioned operators, and move
that child atomically into the parent handle. Those boundaries are separately
tested, but public `evo_run` still stops at generation zero.

The first public loop needs an exact generation-limit meaning, deterministic
global-winner behavior, an extinction rule, and a failure boundary. Folding in
convergence, callbacks for external stopping, recycling, checkpointing, or
parallelism would make that initial contract unnecessarily ambiguous.

## Decision

EVO 0.16.0 adds private bounded-run policy version 1 and composes it inside
public `evo_run`.

`config->generation_limit` counts completed child transitions after generation
zero. A value of zero preserves the established generation-zero-only behavior
and does not require unused child or operator policy. A positive value requires
the child-population budget before any allocation or consumer callback. For a
population larger than one, it also requires valid tournament, crossover, and
mutation policy. A one-member population uses the odd-tail stable-best clone
directly, so unused pair policy is not required.

Generation zero remains the required source of the initial valid winner. EVO
allocates one independent result genome after generation-zero evaluation and
does not allocate another result genome during the loop. For source generation
`g`, EVO:

1. creates one independent child slab;
2. produces every complete pair in ascending order;
3. completes an odd tail when required;
4. evaluates the complete child;
5. identifies a strict global-best improvement;
6. atomically promotes the child to completed generation `g + 1`; and
7. only after promotion, overwrites the result bytes and fitness for a strict
   improvement.

Global ordering uses `fitness.total`. Exact total-fitness ties across
generations retain the earlier winner. Within one population, the existing
lower-index exact-tie rule remains unchanged.

An evaluated all-invalid child is promoted because it is complete ownership
evidence. The completed-transition count is incremented and the public loop
then stops successfully, retaining the earlier valid global winner. Generation
zero being all-invalid remains `EVO_ERROR_NO_VALID_CANDIDATE` because no valid
public result exists.

Every failure after generation zero destroys the current child, current parent,
and public result allocation. Public failure therefore remains empty and does
not expose partial progress. Bounded-run evidence is private and versioned.

ADR-0020 advances bounded-run policy to version 2 for application stopping.
ADR-0021 advances it to version 3 in EVO 0.21.0, centralizes global ordering
under fitness-comparison policy version 1, and records the best generation and
population index. The strict-improvement and earlier-tie results remain
logically unchanged.

ADR-0022 advances bounded-run policy to version 4 in EVO 0.22.0 and records
diversity policy and metric provenance. Pair measurement consumes no operator
RNG and does not change strict improvement, selection, or termination.

ADR-0023 advances bounded-run policy to version 5 in EVO 0.23.0. ADR-0024 then
advances it to version 6 in EVO 0.24.0 and replaces the fixed pair/odd-tail
composition with an ordinary-offspring prefix, optional singleton, and stable
elite suffix. Disabled elite configuration preserves the version-1 sequence.

ADR-0025 advances bounded-run policy to version 7 in EVO 0.25.0 and composes
selection-policy version 1. Tournament mode preserves every prior parent draw;
rank mode interprets the same pair-local selection streams. Child-evaluation
and generation-advancement policies advance to version 5 so the configured
selection identity survives completion.

ADR-0027 advances bounded-run policy to version 8 in EVO 0.26.0 and composes
byte-operator policy version 1. Zero-valued consumer modes preserve prior
callback and RNG behavior; explicit byte modes interpret the same crossover
and mutation streams. Child-evaluation and generation-advancement policies
advance to version 6 so both configured operator identities survive completion.

## Consequences

- Public `EVO_SUCCESS` can now represent zero or more completed transitions;
  `result.generations_completed` is the authoritative count.
- Identical inputs, seed, callback behavior, and configuration replay the same
  callback order, winner, fitness, and completion count.
- The result allocation has stable ownership for the whole successful run.
- The working set is bounded by one parent, one child, evaluation records, and
  one result genome; old parents are released after each promotion.
- A failed later transition discards successful earlier private work rather
  than publishing a partial result.
- No public layout or installed function signature changes, but successful
  behavior changes when `generation_limit` is positive.
- At the 0.16.0 boundary, convergence, stagnation, application stop or observer
  callbacks, a public termination reason, generalized elitism, adaptive
  mutation, population recycling, checkpointing, parallelism, and secure
  erasure remained deferred. ADR-0017 through ADR-0024 resolve the first six;
  the remaining items stay separate.

## Alternatives considered

### Count generation zero in `generation_limit`

Rejected because existing callers use zero for generation-zero execution and
the field name already describes a limit on generation advancement. Counting
child transitions preserves that compatibility and makes the completion count
literal.

### Replace the result with the final generation's best

Rejected because a later generation may regress or become all-invalid. A
global best-so-far gives the public result stable optimization meaning.

### Replace the global winner on an exact tie

Rejected because retaining the earlier winner is deterministic, minimizes
copying, and matches the stable tie policy already used within a population.

### Return partial success after a later failure

Rejected because the current status surface has no partial-completion type or
termination reason. Returning an empty result preserves the established
failure contract.

### Stop before promoting an all-invalid child

Rejected because generation advancement defines the evaluated child as a
complete ownership state. Promotion followed by termination keeps lineage and
`generations_completed` accurate.

## Verification

- `tests/bounded_run_test.c` covers zero-limit compatibility, even, odd, and
  one-member runs, deterministic replay, strict global improvement, exact ties,
  all-invalid termination, pre-callback configuration rejection, active-result
  preservation, and private evidence.
- `tests/allocation_failure_test.c` covers result, child-slab, and child-record
  allocation failures and exact release counts for successful and failed
  bounded runs.
- CMake, GNU Autotools, and AES-BLD-001 enumerate the new production source and
  normative test.
- GitHub issue: `https://github.com/dlworrell/evo/issues/36`
