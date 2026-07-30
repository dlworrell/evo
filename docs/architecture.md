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
`evo_run` until initialization, validity, and evaluation semantics are defined.

## Execution Flow

1. Initialize a population.
2. Validate and evaluate each genome.
3. Select parents.
4. Apply crossover and mutation.
5. Preserve elites and diversity.
6. Record statistics and evidence.
7. Stop on convergence, stagnation, generation limit, or an application-defined condition.

Only the storage prerequisite for step 1 exists in version 0.3.0. No current
success status indicates that any execution-flow step has completed.

## Correctness Boundary

Candidate correctness is a hard gate. Invalid candidates are rejected or heavily penalized before performance optimization is considered.
