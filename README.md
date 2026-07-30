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

## Status

EVO 0.3.0 is an API and memory-lifecycle scaffold. `evo_run` currently
validates its required inputs and caller-provided genome budget, allocates one
zero-initialized result genome, and records the requested seed. Population
initialization and the evolutionary operators remain future work.

Version 0.3.0 adds the independently tested private population-storage
foundation: checked `population_size * genome_size` arithmetic, a
caller-provided total slab budget, contiguous zero-initialized storage, bounded
non-owning genome views, and fully resetting destruction. The subsystem is not
yet invoked by `evo_run`, so the library does not allocate and discard a fake
population or claim that a search occurred.

## Result Lifecycle

Callers must zero-initialize an `evo_result_t` before first use. A successful
run transfers exclusive ownership of `best_genome` to that result. Reusing an
active result is rejected; `evo_result_destroy` releases the allocation,
resets the full result to zero, and makes it immediately reusable.

`max_genome_bytes` is a required caller-provided per-genome policy bound. It is
not a total population budget. `max_population_bytes` separately bounds the
private population genome slab. Neither field is an arbitrary compiled-in cap.

The public status values remain:

- `EVO_SUCCESS`
- `EVO_ERROR_INVALID_ARGUMENT`
- `EVO_ERROR_OUT_OF_MEMORY`
- `EVO_ERROR_RESULT_ACTIVE`
- `EVO_ERROR_RESOURCE_LIMIT`

See `docs/specs/EVO-001-library-contract.md` for the complete API, ownership,
failure-state, alias, compatibility, and secure-erasure boundaries.

The next implementation boundary is deterministic random-number generation,
followed by generation-zero initialization, validation, and evaluation.

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
