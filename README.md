# Catalyst EVO

Catalyst evolutionary optimization library for engineering search, tuning, and design-space exploration.

EVO provides deterministic, reproducible evolutionary optimization algorithms for engineering applications such as repository assessment, compiler optimization, operating-system policy tuning, FPGA design-space exploration, and automated software engineering.

## Mission

Develop a reusable, evidence-driven optimization framework for Catalyst that replaces ad hoc parameter tuning with reproducible evolutionary search.

## Initial Scope

- Genetic algorithms
- Tournament and rank-based selection
- One-point, two-point, and uniform crossover
- Mutation and adaptive mutation
- Diversity measurement and stagnation handling
- Constraint and penalty handling
- Checkpointing and reproducible random-number generation
- Parallel fitness evaluation
- Benchmarking and engineering evidence

## Safety Boundary

EVO may optimize bounded configurations and design choices, but evolved candidates must not bypass correctness tests, safety constraints, or validation gates.

## Repository Layout

- `include/catalyst/evo/` — public C API
- `src/` — implementation
- `tests/` — unit and integration tests
- `examples/` — application examples
- `benchmarks/` — performance and algorithm benchmarks
- `docs/` — architecture, theory, algorithms, and evidence guidance

## Authoritative Native Builds

EVO treats CMake and GNU Autotools as independent, equivalent build
frontends. The checked-in AES-BLD-001 profiles bind:

- CMake + Clang 18 to LLVM archive/inspection tools and LLD
- CMake + GCC 13 to GNU Binutils and the BFD linker
- Autotools to both GCC/GNU tools and Clang/LLVM tools

The canonical local entry points are:

```sh
cmake --preset aes-clang
cmake --build --preset aes-clang
ctest --preset aes-clang

autoreconf -fvi
mkdir -p build/autotools-gcc
cd build/autotools-gcc
CC=gcc ../../configure
make
make check
```

`CMakePresets.json`, `configure.ac`, and `Makefile.am` enumerate the same C17
library sources and normative tests. CI also compares staged install
manifests, public symbols, package metadata, and a downstream consumer built
against each installed result. See
`docs/engineering/AES-BLD-001-toolchain-profile.md`.

## Status

EVO 0.8.0 retains the complete public generation-zero boundary and adds a
private deterministic crossover-dispatch operator after the independently
tested tournament-selection boundary. `evo_run` still constructs and
deterministically initializes a private population, validates every candidate,
evaluates only valid candidates, selects the stable generation-zero winner,
and transfers an independent genome copy plus its complete fitness evidence to
the public result.

Version 0.3.0 added the independently tested private population-storage
foundation: checked `population_size * genome_size` arithmetic, a
caller-provided total slab budget, contiguous zero-initialized storage, bounded
non-owning genome views, and fully resetting destruction. That boundary became
the storage foundation for public generation-zero execution in version 0.6.0.

Version 0.4.0 adds private deterministic RNG algorithm version 1 and
generation-zero population initialization. A configured seed deterministically
prefills the complete population slab using an explicit cross-platform byte
order. EVO then calls the optional consumer initializer once per genome in
ascending order. Successful initialization records the seed and algorithm
version; lifecycle rejection preserves existing storage unchanged.

Consumer initializers must be bounded deterministic transformations of their
prefilled genome and context. The RNG is not cryptographically secure.
Population initialization remains private and does not itself validate,
evaluate, select, mutate, cross over, or iterate candidates.

Version 0.5.0 adds a private generation-zero validation and evaluation phase.
An optional validator is applied to every candidate in ascending order, and
only valid candidates are evaluated. Every returned fitness field must be
finite. Higher consumer-computed `fitness.total` wins, with the lower
population index breaking exact ties. An all-invalid population is a completed
evaluation state without a winner.

Evaluation records have a separate caller-provided byte budget and remain
private. Resource, allocation, or non-finite-fitness failure preserves the
initialized genome slab as unevaluated within the private phase.

Version 0.6.0 connects those private lifecycle stages to `evo_run`. A missing
evaluator is rejected before allocation. Every non-active-result failure
returns an empty result after releasing private storage. A completed
all-invalid population returns `EVO_ERROR_NO_VALID_CANDIDATE`; it is distinct
from an internal state, resource, allocation, or callback-output failure.

`EVO_SUCCESS` now means a valid generation-zero winner was produced. It does
not mean that selection, crossover, mutation, a generation transition, or an
optimization search occurred. `generations_completed` therefore remains zero.

Version 0.7.0 adds unbiased bounded-index sampling over the existing private
PCG stream and tournament selection with replacement from valid evaluated
candidates. Higher `fitness.total` wins each tournament, and exact ties select
the lower population index. The operator validates the completed population
before consuming RNG state, performs no allocation, and preserves its output
on failure.

Selection accepts an explicitly seeded private RNG stream. It does not define a
new seed schedule, change RNG algorithm version 1, or connect selection to
`evo_run`. Generation orchestration will define stream ownership and sequencing
when the first transition is implemented.

Version 0.8.0 adds a private probability gate and crossover pair dispatcher.
The gate quantizes `crossover_rate` to a 32-bit threshold and consumes exactly
one version-1 RNG word for every successful pair, including rates zero and one.
When selected, the existing consumer callback receives two read-only parents
and two distinct writable children. Otherwise, or when the callback is absent,
the parents are cloned into their corresponding children.

The operator remains representation-neutral and allocation-free. It is not
called by `evo_run`, does not select parents, mutate children, own a child
population, or advance a generation.

## Result Lifecycle

Callers must zero-initialize an `evo_result_t` before first use. A successful
run transfers exclusive ownership of `best_genome` to that result. Reusing an
active result is rejected; `evo_result_destroy` releases the allocation,
resets the full result to zero, and makes it immediately reusable.

`max_genome_bytes` is a required caller-provided per-genome policy bound.
`max_population_bytes` separately bounds the private population genome slab,
and `max_evaluation_bytes` bounds private validity and fitness records. None of
these fields is an arbitrary compiled-in cap.

The status values are:

- `EVO_SUCCESS`
- `EVO_ERROR_INVALID_ARGUMENT`
- `EVO_ERROR_OUT_OF_MEMORY`
- `EVO_ERROR_RESULT_ACTIVE`
- `EVO_ERROR_RESOURCE_LIMIT`
- `EVO_ERROR_STATE`
- `EVO_ERROR_EVALUATION`
- `EVO_ERROR_NO_VALID_CANDIDATE`

See `docs/specs/EVO-001-library-contract.md` for the complete API, ownership,
failure-state, alias, compatibility, and secure-erasure boundaries.

The private tournament and crossover-dispatch operators are independently
verified. Mutation, child-population ownership, stream orchestration, and the
first generation transition remain the next execution boundaries; none is
implied by `evo_run` success in version 0.8.0.

## Project Zero

The Project Zero interface is installed from the canonical `repo_templates`
contract:

```sh
bash scripts/project-zero inspect
bash scripts/project-zero verify
python3 ../AEMS/scripts/aems_project_zero.py \
  . \
  --output build/aems/project-zero \
  --format all
```

The local workflow reports whether the repository baseline is certifiable.
AEMS remains authoritative for AES-002 lifecycle state. This repository does
not approve its own transition to `ENGINEERING_READY`.

The stable GitHub Actions entry points are:

- `P0 Repository Lifecycle`
- `Verify Repository`
- `Documentation Report`
- `Repository Compliance`
- `EVO Optimization Experiment`
- `Release Readiness`

See `docs/standards/repository-lifecycle-contract.md` and
`docs/engineering/reports/EVO-P0-001-certification-candidate.md` for the
governing contract and certification boundary.
