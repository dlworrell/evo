# ADR-0031: Deterministic Population-Storage Recycling

Status: Accepted
Date: 2026-08-08
Decision owner: EVO

## Context

Through EVO 0.29.0, every positive-limit transition allocated a new child
genome slab and candidate-evaluation array. Atomic promotion transferred those
two owners into the active population and immediately released the former
parent owners. The algorithm was bounded per generation, but allocator work
and allocation-failure opportunities grew linearly with the configured
generation limit.

EVO can retain two population owners and alternate their roles. That is an
accelerated lifecycle under ADR-0026: reuse must not become an address-keyed
pool whose live contents, provenance, or cleanup behavior are opaque. The
logical active and reusable owners need stable identities and a complete
human-readable projection independent of allocator addresses.

Recycling must also remain a storage policy rather than a new evolutionary
algorithm. It cannot change initialized bytes, selection, operator streams,
callback order, candidate evaluation, statistics, stopping, the global winner,
or termination. The zero-initialized configuration must preserve the exact
pre-0.30.0 allocate/promote/release path.

Finally, a former active population contains genomes and fitness evidence.
Those bytes must be reset before the owner is exposed for reuse. Enabled
secure-erasure policy must retain its exact range and backend guarantees, while
ordinary mode may claim only deterministic byte zeroing rather than secure
media sanitization.

## Decision

EVO 0.30.0 defines population-recycling policy version 1, population-storage
registry version 1, bounded-run policy version 11, child-evaluation and
generation-advancement policy version 8, and private run-state schema 2.

### Compatibility and public control

`evo_config_t` appends:

- `population_recycling_enabled`;
- synchronous `population_storage_observer`; and
- its caller-owned context.

The zero value disables recycling. Disabled runs use the complete 0.29.0
allocation, promotion, release, callback, and RNG path. The observer is an
audit surface only and cannot stop or modify EVO. When present, it receives one
registry after each committed generation, including generation zero, after the
existing generation observer and application-stop decision and before
checkpoint delivery.

Recycling adds no resource-budget field. Both logical slots have the same
checked `population_size * genome_size` capacity and the same checked
`population_size * sizeof(evo_candidate_evaluation_t)` capacity. The initial
slot uses the existing population and evaluation budgets; the second genome
slot uses `max_child_population_bytes`, and its evaluation owner uses
`max_evaluation_bytes`.

### Fixed two-slot ownership

Enabled execution has exactly two run-local logical slot identities:

| Identity | Generation zero | After first transition |
|---:|---|---|
| `1` | active | active or reusable by generation parity |
| `2` | absent | active or reusable by generation parity |

Slot identity is a small stable integer, never a pointer, allocation address,
hash, process identity, or global-pool key. No owner survives a run and no
storage is shared between runs.

Generation zero constructs slot 1 normally. The first attempted transition
constructs slot 2's genome owner and a detached zeroed evaluation reserve.
Child production writes the genome slab. Evaluation moves the detached reserve
into the provisional and then committed evaluation role without allocating.
Every later transition writes and evaluates directly into the former parent's
reset owners.

A successful positive-length run therefore has five allocation classes:
initial genome, initial evaluations, result genome, second genome, and second
evaluations. The count is independent of the number of later transitions. A
zero-transition run retains the three preexisting generation-zero allocations.
Partial construction and early failure own and release only the allocations
that actually succeeded.

### Atomic promotion and reset

Before commit, advancement validates both completed populations, every owner
range, current generation, mutation provenance, the incoming registry, the
next registry, reset eligibility, and independence of the two population
objects, their owned ranges, registry, and evidence. Rejection changes none of
those objects or bytes.

After preflight, no fallible operation remains. Advancement:

1. moves the evaluated child owners into the active population object;
2. moves the former active owners into the reusable population object;
3. resets the complete former evaluation and genome ranges;
4. clears all candidate, production, statistics, and generation provenance
   from the reusable object while retaining only capacities, owner identity,
   erasure metadata, and detached evaluation reserve;
5. commits the next address-free registry; and
6. commits generation-advancement evidence.

Ordinary reset writes zero to every byte of both exact ranges in ascending
order and records `EVO_POPULATION_STORAGE_RESET_ZERO_BYTES`. This is a logical
reuse reset, not a secure-erasure claim. When secure erasure is enabled, reset
uses the sole reviewed erasure wrapper over each complete range and records
`EVO_POPULATION_STORAGE_RESET_SECURE_ERASE`. Terminal cleanup still erases each
enabled live range immediately before release, so one long-lived slot may have
multiple reset erasures plus its final release erasure.

If evaluation or diversity fails after taking a recycled evaluation reserve,
EVO resets the complete provisional range and returns it to the same reusable
owner. It never frees the reserve on that rollback. The enclosing failed run
then releases both slots through their configured cleanup policy and leaves an
empty public result.

### Human-readable owner registry

`evo_population_storage_registry_t` is the required ADR-0026 projection. It is
complete, fixed-size, allocation-free, and ordered by `owner_identity`.
Top-level fields expose:

- registry and recycling-policy versions;
- enabled state and entry count;
- active and reusable stable identities; and
- secure-erasure policy, selected backend, and enabled state.

Each `evo_population_storage_entry_t` exposes:

- stable owner identity and `EMPTY`, `ACTIVE`, or `REUSABLE` lifecycle;
- represented population generation and source generation;
- exact genome and evaluation capacities;
- active-handoff and reset counts;
- reset-time genome and evaluation erasure counts;
- last reset disposition; and
- explicit genome/evaluation owner-presence flags.

Generation zero has one active entry. Every later committed generation has one
active and one reusable entry. The active identity alternates by generation
parity. Handoff and reset counters are derived from the ordered commit history
and saturate only by the already bounded generation domain; they do not inspect
addresses or allocator behavior. Disabled policy has a canonical zero-entry
registry with both identities zero.

The registry is audit evidence, not a second owner table. The private
population objects retain exact pointer/count authority, and every registry is
reconciled against those owners, the configuration, and the committed
generation before use or delivery. A malformed, stale, aliased, or
unreconcilable registry fails closed; there is no fallback that guesses an
owner from an address.

### Replay neutrality

Recycling performs no RNG operation and adds no seed or substream domain.
Reset invokes no consumer callback. For identical problem, configuration other
than the recycling/audit fields, seed, and context, enabled and disabled runs
must have identical:

- initialization, validity, evaluation, crossover, mutation, stop, and
  generation-observer inputs and order;
- candidate genomes and evaluations at every commit;
- schema-4 generation statistics and adaptive/stopping state;
- global winner, generation count, and termination reason; and
- all algorithm-visible RNG consumption, as demonstrated by callback traces,
  committed genomes, and replay vectors.

Only allocator calls, release timing, reset work, recycler evidence, and
checkpoint configuration/registry fields differ. Repeating either mode must
reproduce its own exact trace.

### Checkpoint and resume amendment

ADR-0030 remains the reference checkpoint decision. EVO 0.30.0 advances the
checkpoint format, checkpoint view, and checkpoint-configuration view to
version 2 and changes the magic to `EVOCKPT2`.

Format 2 appends the recycling enable flag and storage-observer presence to
canonical configuration. Run-state schema 2 persists the complete logical
registry. Population state persists recycling policy, active owner identity,
and enabled disposition. Inspection validates the registry against generation
parity, capacities, population provenance, secure-erasure metadata, and exact
configuration before allocation. The checkpoint view projects the same
registry directly; CRC-32 and the configuration fingerprint remain diagnostics
only.

Resume reconstructs the active population and result through the local
allocators. A source registry is fully validated before allocation. Continued
execution reattaches the restoring build's local erasure backend and
materializes the opposite logical slot before the next visible commit. It does
not replay the restored generation or expose an address. The next checkpoint
and registry are therefore byte/logically identical to an uninterrupted run
on the same build. Terminal resume needs no reusable physical owner.

Format 1 is intentionally not accepted by the format-2 parser. Version failure
is explicit rather than silently inventing a recycler registry or continuation
schema absent from the older bytes.

## Human-Readable Abstraction Assessment

The two-slot lifecycle is an exact allocation accelerator: it avoids repeated
allocation and release but does not replace population or evaluation authority.
ADR-0026 therefore applies.

The required projection is the complete stable owner registry described above.
It exposes lifecycle, provenance, capacities, reset/erasure history, and role
handoff in domain terms. It contains no buckets, compressed words, pointer
values, allocator metadata, probabilistic summary, cache freshness guess, or
partial page. Its fixed two-entry scope needs no pagination.

Differential tests run the explicit pre-0.30.0 allocation path and the
accelerated path under the same seed and compare results, complete statistics,
generation events, consumer callback traces, committed genomes, stopping, and
termination. Separate tests reconcile every projected registry generation
with the physical owner lifecycle, secure reset ranges, checkpoint state, and
resume suffix. The retained EVO-HRA-003 audit records that evidence.

The projection cannot authorize ownership independently. Exact pointer/count
invariants remain canonical, and a registry mismatch rejects before reset or
commit. This satisfies the Human-Readable Abstraction Rule without turning the
audit view into a cache or opaque authority.

## Consequences

- Long positive runs use bounded population allocation count instead of
  allocation count proportional to `generation_limit`.
- Enabled ordinary mode performs deterministic full-range zeroing at every
  handoff; enabled secure mode performs full-range erasure at every handoff and
  again before final release.
- The public configuration, checkpoint views, and installed header grow;
  consumers must rebuild against 0.30.0.
- Checkpoint format 2 is incompatible with format 1 by explicit version.
- The storage observer makes the accelerated lifecycle reviewable without
  exposing addresses or transferring ownership.
- Parallel evaluation remains a separate issue; recycling adds no thread,
  queue, scheduler, or cross-run pool.

## Alternatives considered

### Keep allocating every generation

Rejected as the only implementation because allocation/failure work grows with
the generation limit. It remains the exact disabled reference path and
differential oracle.

### Use a global or variable-size population pool

Rejected because cross-run lifetime, eviction, concurrency, address identity,
and unbounded capacity would enlarge the authority and failure surface. Two
fixed run-local slots are sufficient for the current sequential algorithm.

### Reuse genomes but allocate evaluations every generation

Rejected because allocation remains generation-dependent and ownership
evidence becomes asymmetric. Both fixed-capacity ranges follow one lifecycle.

### Leave former bytes intact until overwritten

Rejected because partial production or evaluation failure could leave stale
genomes or fitness evidence in a reusable role. Complete reset establishes a
canonical empty child and a precise secure-erasure composition.

### Publish allocation addresses as registry identities

Rejected because addresses are nondeterministic, process-specific, sensitive
implementation metadata and would violate replay and ADR-0026 domain
projection requirements.

### Reconstruct registry history silently on resume

Rejected as checkpoint authority. Format 2 persists and validates the complete
logical registry first. Only the restoring build's local backend and absent
physical reusable allocation are reattached after that validation.

## Verification

- `tests/population_recycling_test.c` differentially compares disabled,
  enabled, and replay runs across complete callback traces, committed
  generation events, schema-4 statistics, final results, termination, stable
  registry history, and generation/storage/checkpoint callback order.
- `tests/generation_advancement_test.c` proves registry, population, owner, and
  evidence alias rejection; stale-registry rejection without mutation; exact
  owner-role swapping; complete ordinary reset; and propagated recycler
  evidence.
- `tests/allocation_failure_test.c` proves one- and seven-transition enabled
  runs both use five allocations, fails each construction boundary, and proves
  that a sixth injected allocation cannot occur during later transitions.
- `tests/secure_erasure_test.c` wraps allocation, erasure, and release to prove
  every reset and final release covers the exact owner range, including a
  recycled provisional-evaluation failure.
- `tests/checkpoint_test.c` proves format-2 registry inspection, tamper
  rejection, enabled checkpoint/resume equivalence, and identical resumed
  checkpoint suffixes.
- `tests/checkpoint_fuzz_test.c` continues to exercise every truncation, every
  byte under one-bit mutation, and deterministic arbitrary input against the
  format-2 parser.
- CMake, GNU Autotools, and AES-BLD-001 enumerate the same twenty-five
  production sources and thirty-one normative targets.
- GitHub issue: `https://github.com/dlworrell/evo/issues/52`
