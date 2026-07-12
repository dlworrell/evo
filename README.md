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

Project Zero / initial repository bootstrap.
