# EVO-001: Evolutionary Optimization Library Contract

Status: Baseline
Version: 0.5.0
Owner: EVO

## Purpose

EVO provides a reusable C17 interface for deterministic, bounded evolutionary
optimization. Consumers define genomes and problem-specific operations through
callbacks; EVO owns orchestration, reproducibility, and evidence without
embedding consumer policy in the library.

## Public Interface

The public API is declared in `include/catalyst/evo/evo.h`.

### Problem definition

`evo_problem_t` declares:

- the byte size of one genome;
- initialization, mutation, crossover, and evaluation callbacks;
- an optional validity callback; and
- an opaque consumer context passed to callbacks.

The consumer owns callback code and context lifetime. Callback behavior must be
deterministic for a fixed input, context, and random stream unless the consumer
records an additional source of variation.

### Run configuration

`evo_config_t` records population size, generation limit, tournament size,
crossover rate, mutation rate, random seed, `max_genome_bytes`,
`max_population_bytes`, and `max_evaluation_bytes`.

`max_genome_bytes` is trusted caller policy for the largest individual genome
allocation accepted by `evo_run`. It avoids a platform-specific hard-coded
limit.

`max_population_bytes` is trusted caller policy for the contiguous genome slab
owned by the internal population subsystem. Before allocation, EVO proves that
`population_size * genome_size` is representable as `size_t` and no greater
than this budget. The v0.3.0 field bounds the complete storage allocation made
by that subsystem. It does not silently authorize future fitness arrays,
second-generation buffers, checkpoint state, or a total run working set; each
additional allocation class requires an updated specification and explicit
policy.

`max_evaluation_bytes` independently bounds the private candidate-evaluation
record array. EVO checks
`population_size * sizeof(evo_candidate_evaluation_t)` for `size_t` overflow
before allocation. The field does not authorize future generation buffers,
operator scratch space, checkpoint state, or parallel-worker storage.

### Internal population storage

Version 0.3.0 adds a private, independently verified population-storage
subsystem. It is not part of the installed public API and is not yet invoked by
`evo_run`.

The internal lifecycle contract is:

1. A population object is zero-initialized before its first construction.
2. Construction rejects an active population without modifying it.
3. Null input, invalid size policy, arithmetic overflow, budget excess, and
   allocation failure leave an inactive population in the empty zero state.
4. Successful construction owns one contiguous, zero-initialized genome slab
   of exactly `population_size * genome_size` bytes.
5. Indexed genome access returns a bounded, non-owning view. Out-of-range
   access returns null, and no view may outlive the population.
6. Destruction releases the slab and resets every population field to zero.
   It is null-safe and repeatable for initialized objects.

Population destruction does not securely erase the genome slab. The same
secret-material restriction defined for `evo_result_destroy` applies to
population storage and every non-owning genome view.

### Deterministic population initialization

Version 0.4.0 defines private RNG algorithm version 1 and a generation-zero
population initializer. The normative algorithm decision is recorded in
`docs/adr/ADR-0002-deterministic-rng-and-population-initialization.md`.

RNG version 1 uses PCG-XSH-RR with 64-bit state, 32-bit output, a fixed odd
stream increment, and explicit least-significant-byte-first output. Unsigned
fixed-width wraparound is intentional. Every `uint64_t` seed, including zero,
is valid. No global RNG state, clock, process identity, platform entropy, or
native byte-order conversion participates in the stream.

The initialization lifecycle is:

1. The population must be active, uninitialized, and structurally consistent
   with the supplied problem and configuration.
2. EVO seeds one operation-local RNG from `config->random_seed`.
3. EVO fills the complete contiguous slab from one continuous stream.
4. If `problem->initialize` is non-null, EVO calls it once per genome in
   ascending index order.
5. Each callback receives deterministic prefilled bytes as a bounded,
   non-owning view. It must be deterministic for fixed bytes and context,
   remain within the genome, preserve ownership, and not retain the view.
6. After all callbacks return, the population records the seed, RNG algorithm
   version, and initialized state.
7. Null arguments return `EVO_ERROR_INVALID_ARGUMENT`. Inactive, previously
   initialized, policy-inconsistent, or metadata-inconsistent populations
   return `EVO_ERROR_STATE` without modification.
8. Population destruction clears the initialization metadata together with
   the owned storage.

The RNG is not cryptographically secure and is not approved for secrets, keys,
nonces, authentication, or adversarial unpredictability.

Population initialization does not call `is_valid` or `evaluate`. The private
subsystem remains disconnected from `evo_run` so the public scaffold does not
allocate and discard a population or claim that initialization, validation,
evaluation, or search completed.

### Generation-zero validation and evaluation

Version 0.5.0 adds a private evaluation phase after successful population
initialization. The normative decision is recorded in
`docs/adr/ADR-0004-generation-zero-validation-and-evaluation.md`.

The evaluation lifecycle is:

1. The population must be active, initialized, unevaluated, and structurally
   consistent with the supplied problem and configuration.
2. The evaluator callback is required. A missing evaluator returns
   `EVO_ERROR_INVALID_ARGUMENT`.
3. EVO proves the evaluation-record byte count is representable and no greater
   than `config->max_evaluation_bytes`.
4. If `problem->is_valid` is null, every candidate is valid. Otherwise EVO
   calls it exactly once for every genome in ascending index order.
5. EVO calls `problem->evaluate` exactly once for each valid genome in
   ascending index order. Invalid candidates are never evaluated.
6. Every field in the returned `evo_fitness_t` must be finite. NaN or infinity
   returns `EVO_ERROR_EVALUATION`, releases provisional records, and preserves
   the initialized population as unevaluated.
7. Validity is a hard gate. Higher consumer-computed `fitness.total` wins, and
   the lower population index wins an exact tie. EVO records but does not
   independently rank the other fitness components.
8. A completed all-invalid population returns `EVO_SUCCESS`, records zero
   valid candidates, and has no best-candidate index.
9. Repeated evaluation is rejected with `EVO_ERROR_STATE` without modifying
   the completed records.
10. Population destruction releases the evaluation records together with the
    genome slab and resets all lifecycle, count, and winner metadata.

Callbacks receive bounded, non-owning, read-only genome views. They must not
change storage ownership or retain a view. EVO can roll back only its private
provisional records; side effects in consumer context remain consumer-owned.

### Result lifecycle

The caller must zero-initialize `evo_result_t` before its first use:

```c
evo_result_t result = {0};
```

The lifecycle contract is:

1. `evo_run` rejects a result whose `best_genome` is non-null and preserves the
   active result unchanged.
2. Null input, invalid resource policy, and allocation failure leave a
   non-null, inactive result in the empty zero state.
3. On success, the result exclusively owns `best_genome`.
4. Callers may use bounded, non-owning aliases to read or write genome bytes
   while the result remains alive. An alias may not free or reallocate the
   storage and must not survive result destruction.
5. `evo_result_destroy` releases the owned allocation and resets every result
   field to zero. Destruction is null-safe and repeatable for initialized
   result objects.
6. A destroyed result may be passed to `evo_run` again immediately.

`evo_result_destroy` does not securely erase genome bytes. Consumers must not
place secret or cryptographic material in genomes without a separately
reviewed erasure boundary.

### Status values

`evo_status_t` defines:

| Status | Meaning |
|---|---|
| `EVO_SUCCESS` | The scaffold completed and transferred ownership of a genome allocation. |
| `EVO_ERROR_INVALID_ARGUMENT` | A required pointer argument is null. |
| `EVO_ERROR_OUT_OF_MEMORY` | The system allocator returned null. |
| `EVO_ERROR_RESULT_ACTIVE` | The result already owns a genome and is preserved unchanged. |
| `EVO_ERROR_RESOURCE_LIMIT` | A required size is zero, arithmetic overflows, or a caller budget is exceeded. |
| `EVO_ERROR_STATE` | A private lifecycle operation received inactive, initialized, or inconsistent state. |
| `EVO_ERROR_EVALUATION` | A fitness callback returned a non-finite component. |

### Fitness placeholder

The public scaffold initializes all seven `evo_fitness_t` fields to zero. This
is a deterministic "not yet evaluated" placeholder, not evidence of a valid
zero-valued fitness or completed optimization run. A later implementation must
introduce explicit evaluated-result semantics before returning an optimum.

## API Compatibility

Version 0.2.0 appended `max_genome_bytes` to `evo_config_t` and changed
`evo_run` from a raw `int` result to `evo_status_t`. Version 0.3.0 appended
`max_population_bytes`, and version 0.4.0 appended one status enumerator.
Version 0.5.0 appends `max_evaluation_bytes` and one additional status value.
Existing member offsets remain preserved, but `sizeof(evo_config_t)` and its
array stride change again. Consumers must rebuild against the 0.5.0 header.
The current public scaffold still requires only a nonzero per-genome budget;
population-backed execution will additionally require population-storage and
evaluation-record budgets when it is integrated.

## Current 0.5.0 Conformance Boundary

The current implementation remains an API and lifecycle scaffold with a
verified private population-storage foundation:

- validation enforces required pointers, an inactive result, nonzero size
  policy, and the caller-provided genome bound;
- success allocates one zero-initialized result genome and records the seed;
- allocation failure returns a deterministic empty result;
- result destruction is null-safe, repeatable, and restores the empty state;
- private population construction checks size arithmetic and both caller
  budgets before allocating a contiguous zero-initialized slab;
- private population views are bounds-checked and non-owning;
- private population destruction is null-safe, repeatable, and fully
  resetting;
- private RNG output and byte order are locked by fixed vectors;
- private population initialization is seed-reproducible and records its RNG
  algorithm version;
- optional initializers run exactly once in ascending genome order; and
- private validation and evaluation run in deterministic ascending passes;
- invalid candidates are not evaluated;
- finite consumer totals select a stable generation-zero winner;
- all-invalid populations complete without a winner; and
- selection, crossover, mutation, diversity, checkpointing, public population
  integration, and generation iteration are not implemented.

Consumers must not treat `EVO_SUCCESS` in the current scaffold as evidence that
an optimization search was performed. The internal storage subsystem is not
evidence that a population has been validated, evaluated, or searched.

## Verification

The baseline verification set is:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The portable result-lifecycle, population-storage, RNG-vector,
population-initialization, and population-evaluation tests run across supported
platforms. A separate Linux-only static-link test uses the GNU-compatible
`--wrap=calloc` linker facility to prove result, population, and evaluation
allocation-failure states deterministically.

## Related Records

- `docs/adr/ADR-0001-library-boundary-and-build-system.md`
- `docs/adr/ADR-0002-deterministic-rng-and-population-initialization.md`
- `docs/adr/ADR-0004-generation-zero-validation-and-evaluation.md`
- `docs/architecture.md`
- `docs/algorithms.md`
- `docs/benchmarks.md`
- `docs/engineering/AES-DEV-001-development-principles.md`
- `docs/engineering/SECURE-C-CXX.md`
- `docs/engineering/AES-SEC-001-review-dispositions.json`
- `https://github.com/dlworrell/evo/issues/4`
- `https://github.com/dlworrell/evo/issues/6`
- `https://github.com/dlworrell/evo/issues/8`
- `https://github.com/dlworrell/evo/issues/12`
- `https://github.com/dlworrell/AEMS/issues/18`
