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

## Execution Flow

1. Initialize a population.
2. Validate and evaluate each genome.
3. Select parents.
4. Apply crossover and mutation.
5. Preserve elites and diversity.
6. Record statistics and evidence.
7. Stop on convergence, stagnation, generation limit, or an application-defined condition.

## Correctness Boundary

Candidate correctness is a hard gate. Invalid candidates are rejected or heavily penalized before performance optimization is considered.
