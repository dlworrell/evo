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
object. The subsystem is independently tested and remains disconnected from
`evo_run` until validity and evaluation semantics are defined.

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
Private population initialization remains disconnected from `evo_run`; it
does not call validity or fitness callbacks and does not represent a completed
search.

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
genome slab owned and unevaluated. Population destruction releases both the
genome slab and evaluation records and resets the complete private object.
Evaluation remains disconnected from `evo_run`.

## Execution Flow

1. Initialize a population.
2. Validate and evaluate each genome.
3. Select parents.
4. Apply crossover and mutation.
5. Preserve elites and diversity.
6. Record statistics and evidence.
7. Stop on convergence, stagnation, generation limit, or an application-defined condition.

Version 0.5.0 implements deterministic byte initialization and the optional
domain initializer portion of step 1, plus generation-zero validation,
evaluation, and best-candidate identification from step 2. The private
subsystem is not yet connected to `evo_run`; no current public success status
indicates that the population execution flow or an optimization search
completed.

## Correctness Boundary

Candidate correctness is a hard gate. Invalid candidates are not evaluated and
cannot win. Fitness callbacks must return finite evidence, and consumer policy
is responsible for producing the scalar `total` used for comparison.
