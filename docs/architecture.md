# EVO Architecture

## Design Goals

EVO is a reusable C library for bounded engineering optimization. The engine remains independent of repository scoring, operating-system policy, compiler tuning, and FPGA placement; consumers provide problem-specific genome, fitness, validation, mutation, and crossover callbacks.

## Core Modules

- Population management
- Selection
- Crossover
- Mutation
- Diversity and stagnation handling
- Fitness and constraint handling
- Statistics and evidence
- Checkpointing
- Reproducible random-number generation

## Population Storage Boundary

Version 0.3.0 establishes the private population-storage foundation without
claiming that the execution loop exists. The subsystem owns one contiguous
zero-initialized genome slab. It checks `population_size * genome_size` for
`size_t` overflow and enforces both the per-genome and total slab budgets
provided by the caller.

Indexed genome pointers are bounded non-owning views. Population destruction
invalidates every view, releases the slab, and resets the complete private
object. The subsystem remains independently tested; version 0.6.0 invokes it
as the private storage boundary for `evo_run`.

## Deterministic Initialization Boundary

Version 0.4.0 adds private RNG algorithm version 1 and generation-zero
population initialization. One operation-local PCG-XSH-RR stream fills the
complete contiguous population slab using an explicit low-byte-first order.
The configured `uint64_t` seed, including zero, completely determines the raw
population bytes.

After prefill, EVO calls the optional consumer initializer once per genome in
ascending index order. The callback is a bounded deterministic transformation:
it may change only the provided genome, may not change ownership or retain the
view, and may not consult unrecorded entropy.

Successful initialization records the seed, RNG algorithm version, and
lifecycle state. Inactive, already initialized, or inconsistent populations
are rejected unchanged. Population destruction clears this metadata together
with the owned slab.

The random stream is reproducible rather than cryptographically secure.
Private population initialization does not itself call validity or fitness
callbacks and does not represent a completed search. Version 0.6.0 composes it
with the separate evaluation phase inside `evo_run`.

## Validation and Evaluation Boundary

Version 0.5.0 adds private generation-zero validation and evaluation. EVO first
classifies every candidate in ascending index order. A missing validator means
all candidates are valid. It then evaluates only valid candidates, again in
ascending order.

One caller-budgeted private record stores each candidate's validity and fitness
evidence. EVO checks the record-array multiplication for `size_t` overflow and
enforces `max_evaluation_bytes` independently from the genome-slab budget.
Provisional records are attached to the population only after all returned
fitness fields are proven finite.

Validity is a hard correctness gate. Among valid candidates, higher
consumer-computed `fitness.total` wins, and the lower index wins an exact tie.
The other component fields remain evidence rather than library-defined
objectives. An all-invalid population is a completed evaluation state with no
winner.

Resource, allocation, and non-finite-fitness failures leave the initialized
genome slab owned and unevaluated inside the private phase. Population
destruction releases both the genome slab and evaluation records and resets
the complete private object.

## Public Generation-Zero Boundary

Version 0.6.0 makes `evo_run` the owner of one complete private
generation-zero lifecycle:

1. reject an active public result unchanged;
2. construct and deterministically initialize private population storage;
3. validate every candidate and evaluate only valid candidates;
4. identify the stable highest-total winner;
5. transfer an independent copy and complete fitness evidence to the result;
6. release every private allocation before returning.

A missing evaluator is an invalid argument. Completion with no valid candidate
maps to `EVO_ERROR_NO_VALID_CANDIDATE`; all other private failures preserve
their existing status. Every non-active-result failure leaves the public
result empty.

The result copy is a distinct allocation because private population
destruction invalidates every population view. The copy covers exactly the
caller-bounded `genome_size`. `generations_completed` remains zero because no
selection or generation transition occurs.

## Private Selection Boundary

Version 0.7.0 adds a private tournament operator without changing public
generation-zero execution. The operator accepts a completed evaluated
population and an explicitly seeded private RNG stream. It validates storage,
evaluation evidence, validity flags, finite fitness, counts, and stable-best
metadata before advancing the stream.

Each draw uses rejection-sampled bounded indexing and maps a valid-candidate
ordinal to an ascending population index. Sampling is with replacement, higher
`fitness.total` wins, and the lower index wins an exact tie. Invalid candidates
are excluded before sampling rather than sampled and retried.

Selection is read-only over population state, performs no allocation, and
commits its output only after all draws succeed. It does not define stream
derivation or persistence. The future generation-transition owner must make
that sequencing decision before composing selection with crossover and
mutation.

## Private Crossover Boundary

Version 0.8.0 adds a private representation-neutral crossover dispatcher. It
accepts two bounded read-only parent views, two distinct non-overlapping child
views, the configured rate, and an explicitly seeded private RNG.

Each successful pair consumes exactly one 32-bit probability decision. A
selected event invokes the consumer callback exactly once; a non-selected
event or absent callback clones each parent into its corresponding child. All
pointer, genome-policy, rate, and RNG-state validation occurs before stream
consumption or child writes.

The callback owns genome representation semantics and must fully initialize
both children without changing parent bytes, ownership, or retaining any view.
The dispatcher performs no allocation and has no callback rollback path.

This boundary does not select parents or own next-generation storage. It is not
called by `evo_run`. Mutation, child-population ownership, operator stream
sequencing, and the first generation transition remain future orchestration
work.

## Execution Flow

1. Initialize a population.
2. Validate and evaluate each genome.
3. Select parents.
4. Apply crossover and mutation.
5. Preserve elites and diversity.
6. Record statistics and evidence.
7. Stop on convergence, stagnation, generation limit, or an application-defined condition.

Version 0.8.0 publicly implements steps 1 and 2 for generation zero and
transfers the best valid candidate. Step 3 has an independently tested private
tournament operator, and the crossover portion of step 4 has an independently
tested private dispatcher. `evo_run` invokes neither operator. `EVO_SUCCESS`
therefore still does not indicate that steps 3 through 7, a generation
transition, or an optimization search completed.

## Correctness Boundary

Candidate correctness is a hard gate. Invalid candidates are not evaluated and
cannot win. Fitness callbacks must return finite evidence, and consumer policy
is responsible for producing the scalar `total` used for comparison.
