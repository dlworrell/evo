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

The repository is a Project Zero certification candidate. Its repository
baseline, lifecycle entry points, manifest, local standards profiles,
architecture decision, specification, build, tests, quality checks, and
sanitizer checks are present and reviewable.

The implementation remains an early `0.1.0` scaffold. `evo_run` currently
validates its required inputs, allocates a result genome, and records the
requested seed; the evolutionary operators described in the roadmap are not
yet implemented.

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
