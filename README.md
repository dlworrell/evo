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

EVO 0.15.0 retains the complete public generation-zero boundary and adds a
private atomic ownership transition from an evaluated child population to the
next completed generation.
`evo_run` still constructs and
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

Selection accepts an explicitly seeded private RNG stream. Version 0.11.0 adds
the separate owner that derives a selection stream for each complete pair; the
selection operator itself remains unchanged and disconnected from `evo_run`.

Version 0.8.0 adds a private probability gate and crossover pair dispatcher.
The gate quantizes `crossover_rate` to a 32-bit threshold and consumes exactly
one version-1 RNG word for every successful pair, including rates zero and one.
When selected, the existing consumer callback receives two read-only parents
and two distinct writable children. Otherwise, or when the callback is absent,
the parents are cloned into their corresponding children.

The operator remains representation-neutral and allocation-free. It is not
called by `evo_run`, does not select parents, mutate children, own a child
population, or advance a generation.

Version 0.9.0 adds a private fixed-rate mutation dispatcher over one bounded
writable genome. Every valid attempt consumes exactly one version-1 RNG word,
including rates zero and one and an absent callback. A selected event invokes
the existing consumer callback exactly once; otherwise the genome remains
unchanged.

The configured `mutation_rate` is the engine-owned per-genome event
probability and is forwarded unchanged as the callback's
representation-specific mutation intensity. Callback code must be
deterministic for fixed bytes, rate, and context and may not consult
unrecorded entropy. The in-place callback has no failure or rollback channel.
This private operator does not allocate storage, implement representation-
specific helpers or adaptive schedules, or advance a generation.

Version 0.10.0 adds an independently owned, zero-initialized child-population
slab. A completed parent population is validated through the same invariant
authority used by tournament selection before a separate child allocation is
made. Parent genomes, evaluations, and lifecycle evidence remain read-only.

The appended `max_child_population_bytes` field is a caller-controlled policy
for exactly one child genome slab. Child storage matches the configured parent
dimensions, begins without initialization or evaluation evidence, and may be
destroyed independently. This boundary does not select parents, invoke genetic
operators, mark children complete, swap populations, or advance a generation.

Version 0.11.0 promotes the plain tuple-mixed seed schedule measured in the
EVO-RNG-001 research into production as operator seed-schedule version 1.
Selection, crossover, and mutation streams are independently addressable by
master seed, source generation, pair or child index, and operation domain. RNG
algorithm version 1 and every generation-zero initialization vector remain
unchanged.

The private parent-pair planner maps each complete pair ordinal to consecutive
child slots and performs two deterministic tournaments with replacement using
that pair's selection-domain stream. It commits no output until both parents
are selected, leaves the completed parent read-only, and writes no child bytes.
Only `population_size / 2` complete pairs are planned; an odd trailing slot is
reserved for a later singleton or elitism policy.

Version 0.12.0 composes the accepted private boundaries for the next complete
pair. It requires ascending pair order, derives a pair-indexed crossover stream
and one mutation stream per child index, preflights every expected library
rejection, then dispatches crossover and both mutations into the separately
owned child slab.

Successful production records the contiguous child count, source generation,
operator seed-schedule version, and pair evidence. Repeated, skipped, or
mismatched-generation pairs reject before callbacks and preserve child state.
Parents remain read-only, child evaluation evidence remains absent, and an odd
trailing child remains untouched. Consumer callbacks still have no failure or
rollback channel.

Version 0.13.0 completes the trailing slot of an odd child slab by cloning the
completed parent's stable best valid genome byte-for-byte. Policy version 1
requires every complete pair first, consumes no RNG state, invokes no consumer
callback, and records full production count, source generation, operator
schedule version, and odd-tail policy version. A one-member population is
completed directly from its sole valid parent.

In version 0.13.0, full production remained distinct from completed-population
evidence: the child was still unevaluated and without validity, fitness, or
best-candidate records.

Version 0.14.0 evaluates a fully produced even or odd child slab through the
same provisional-record engine used for generation zero. Policy version 1
validates every candidate in ascending order, evaluates only valid candidates
in ascending order, rejects every non-finite fitness field, and retains the
lower index on exact total-fitness ties. Evaluation consumes no RNG state and
does not modify genome bytes.

Success preserves child production provenance while committing validity,
fitness, valid-count, and stable-best evidence. All-invalid children are
completed evaluated populations without a best candidate. The shared
completed-population validator accepts either generation-zero or evaluated-
child provenance, so an evaluated child can authorize the next independent
child slab.

Version 0.15.0 adds generation-advancement policy version 1. After validating
both completed populations, their generation lineage, every owned byte range,
and `uint64_t` increment safety, the operation moves the evaluated child into
the parent handle without allocation or copying. The child handle is reset to
zero, and the former parent is released only after ownership transfer.

Every fallible check precedes that no-fail commit suffix. Rejection preserves
both populations and caller evidence; success preserves child genome bytes,
evaluation records, stable-best evidence, and production provenance exactly.
The boundary consumes no RNG state and invokes no callback. Completed all-
invalid children remain promotable because extinction and stopping policy are
separate decisions. Public `evo_run`, generation-limit handling, population
recycling, and multi-generation iteration remain later milestones.

## Result Lifecycle

Callers must zero-initialize an `evo_result_t` before first use. A successful
run transfers exclusive ownership of `best_genome` to that result. Reusing an
active result is rejected; `evo_result_destroy` releases the allocation,
resets the full result to zero, and makes it immediately reusable.

`max_genome_bytes` is a required caller-provided per-genome policy bound.
`max_population_bytes` separately bounds the private population genome slab,
`max_evaluation_bytes` bounds private validity and fitness records, and
`max_child_population_bytes` bounds one separately owned child slab. None of
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

The private tournament, crossover-dispatch, mutation-dispatch, child-population
ownership, operator-stream, complete-pair-planning, sequential complete-pair
production, odd-tail elite-clone, and produced-child evaluation boundaries are
independently verified. The first private generation ownership transition is
also verified. Generalized elitism, adaptive mutation, stopping policy,
population recycling, and public multi-generation execution remain later
boundaries; none is implied by `evo_run` success in version 0.15.0.

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
