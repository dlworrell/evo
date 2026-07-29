# EVO-001: Evolutionary Optimization Library Contract

Status: Baseline
Version: 0.1.0
Owner: EVO

## Purpose

EVO provides a reusable C17 interface for deterministic, bounded evolutionary
optimization. Consumers define genomes and problem-specific operations through
callbacks; EVO owns orchestration, reproducibility, and evidence without
embedding consumer policy in the library.

## Public Interface

The public ABI is declared in `include/catalyst/evo/evo.h`.

### Problem definition

`evo_problem_t` declares:

- the byte size of one genome;
- initialization, mutation, crossover, and evaluation callbacks;
- an optional validity callback; and
- an opaque consumer context passed to callbacks.

The consumer owns callback code and context lifetime. Callback behavior must be
deterministic for a fixed input, context, and random stream unless the consumer
explicitly records additional sources of variation.

### Run configuration

`evo_config_t` records population size, generation limit, tournament size,
crossover rate, mutation rate, and the random seed. A reproducible run must
retain the complete configuration and the versioned inputs used for fitness
evaluation.

### Fitness

`evo_fitness_t` separates correctness, performance, memory use, reliability,
maintainability, constraint penalty, and total fitness. Correctness and
consumer-defined validity are hard acceptance boundaries; an optimization
result must not bypass them merely because another score improves.

### Result ownership

On success, EVO owns the allocation stored in `evo_result_t.best_genome` and
transfers that allocation to the caller. The caller releases it with
`evo_result_destroy`. Destruction is null-safe and clears `best_genome`.

## Current 0.1.0 Conformance Boundary

The current implementation is an API and build scaffold:

- `evo_run` rejects null required objects, a zero genome size, or a zero
  population size with `-1`;
- allocation failure returns `-2`;
- success allocates a zero-initialized result genome, records the configured
  seed, records zero completed generations, and returns `0`; and
- selection, crossover, mutation, evaluation, diversity, checkpointing, and
  generation iteration are not yet implemented.

Consumers must not treat the current successful return as evidence that an
optimization search was performed. Implementing the first complete search loop
requires an updated specification, tests for callback order and failures, and
versioned result semantics.

## Safety and Failure Requirements

- Validate every public pointer and size before use.
- Reject size arithmetic that can overflow before allocation or copying.
- Do not invoke a callback whose contract has not been validated.
- Preserve the caller's ability to destroy a partially initialized result.
- Do not return an invalid genome as a successful optimum.
- Treat checkpoint and serialized state as untrusted input when those
  facilities are added.
- Record the seed and stopping reason for every completed run.

## Verification

The baseline verification set is:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

CI additionally covers GCC, Clang, macOS Clang, formatting, static analysis,
AddressSanitizer, and UndefinedBehaviorSanitizer.

## Related Records

- `docs/architecture.md`
- `docs/algorithms.md`
- `docs/benchmarks.md`
- `docs/adr/ADR-0001-library-boundary-and-build-system.md`
- `docs/engineering/AES-DEV-001-development-principles.md`
- `docs/engineering/SECURE-C-CXX.md`
