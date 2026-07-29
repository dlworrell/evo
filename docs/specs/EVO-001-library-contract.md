# EVO-001: Evolutionary Optimization Library Contract

Status: Baseline
Version: 0.2.0
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
crossover rate, mutation rate, random seed, and `max_genome_bytes`.

`max_genome_bytes` is trusted caller policy for the largest individual genome
allocation accepted by `evo_run`. It avoids a platform-specific hard-coded
limit. It does not represent the eventual total population working-set budget.
When population storage is implemented, its specification must add checked
size arithmetic and an explicit total-memory policy before allocating
`population_size * genome_size` bytes or additional generation buffers.

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

`evo_run` returns:

| Status | Meaning |
|---|---|
| `EVO_SUCCESS` | The scaffold completed and transferred ownership of a genome allocation. |
| `EVO_ERROR_INVALID_ARGUMENT` | A required pointer argument is null. |
| `EVO_ERROR_OUT_OF_MEMORY` | The system allocator returned null. |
| `EVO_ERROR_RESULT_ACTIVE` | The result already owns a genome and is preserved unchanged. |
| `EVO_ERROR_RESOURCE_LIMIT` | A required size is zero or the genome exceeds caller policy. |

### Fitness placeholder

The 0.2.0 scaffold initializes all seven `evo_fitness_t` fields to zero. This
is a deterministic "not yet evaluated" placeholder, not evidence of a valid
zero-valued fitness or completed optimization run. A later implementation must
introduce explicit evaluated-result semantics before returning an optimum.

## API Compatibility

Version 0.2.0 appends `max_genome_bytes` to `evo_config_t` and changes
`evo_run` from a raw `int` result to `evo_status_t`. Existing member offsets are
preserved, but `sizeof(evo_config_t)` and its array stride change. Consumers
must rebuild against the 0.2.0 header and set a nonzero genome budget.

## Current 0.2.0 Conformance Boundary

The current implementation remains an API and lifecycle scaffold:

- validation enforces required pointers, an inactive result, nonzero size
  policy, and the caller-provided genome bound;
- success allocates one zero-initialized result genome and records the seed;
- allocation failure returns a deterministic empty result;
- result destruction is null-safe, repeatable, and restores the empty state;
  and
- population arrays, selection, crossover, mutation, evaluation, diversity,
  checkpointing, and generation iteration are not implemented.

Consumers must not treat `EVO_SUCCESS` in the current scaffold as evidence that
an optimization search was performed.

## Verification

The baseline verification set is:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The portable lifecycle test runs across supported platforms. A separate
Linux-only static-link test uses the GNU-compatible `--wrap=calloc` linker
facility to prove the allocation-failure state deterministically. CI
additionally covers GCC, Clang, macOS Clang, formatting, static analysis,
AddressSanitizer, and UndefinedBehaviorSanitizer.

## Related Records

- `docs/adr/ADR-0001-library-boundary-and-build-system.md`
- `docs/architecture.md`
- `docs/algorithms.md`
- `docs/benchmarks.md`
- `docs/engineering/AES-DEV-001-development-principles.md`
- `docs/engineering/SECURE-C-CXX.md`
- `docs/engineering/AES-SEC-001-review-dispositions.json`
- `https://github.com/dlworrell/evo/issues/4`
- `https://github.com/dlworrell/AEMS/issues/18`
