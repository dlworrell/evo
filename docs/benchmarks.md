# Benchmark and Evidence Requirements

EVO distinguishes benchmarks of the evolutionary core from measurements used
to compare an evolved C source candidate with its target-project baseline.
Neither category may substitute performance evidence for correctness.

## EVO Core Experiments

Every core experiment records:

- EVO version and commit
- algorithm and operator configuration
- genome representation
- fitness definition and component weights
- constraints and penalties
- random seed
- population size
- generation limit and stopping condition
- dataset or simulator version
- hardware and software environment
- best genome and full fitness breakdown
- generation statistics
- elapsed time and peak memory

Core benchmarks distinguish correctness, solution quality, convergence rate,
diversity, runtime, and memory consumption. They do not prove that EVO has
analyzed or improved a C codebase.

## Source-Optimization Experiments

Every source-optimization experiment additionally records:

- immutable baseline source and dependency identities
- optimization-manifest and transformation-catalogue versions
- exact baseline and candidate patches or tree identities
- compiler, linker, build frontend, target, and binary identities
- declared tests, workloads, inputs, and target hardware
- correctness, sanitizer, analyzer, ABI, security, and governance results
- warmup, repetition, ordering, timeout, aggregation, outlier, and variance
  policy
- raw baseline and candidate measurements plus derived statistics
- transformation recipe, champion lineage, rejected-finalist evidence, and
  termination reason
- complete fitness derivation and configured improvement threshold
- artifact-schema and replay-procedure versions

Correctness gates execute before performance fitness. A candidate failing a
required fast gate receives no performance score, and no candidate can be
published as the champion until it passes every required finalist gate.

Baseline and candidate measurements must run under comparable recorded
conditions. Performance results are reproducible only within the documented
platform and statistical tolerances; timing noise is never represented as
bit-for-bit deterministic evidence.

## Claim Boundary

A benchmark result without its seed, fitness definition, baseline, workload,
target, toolchain, and measurement policy is not reproducible engineering
evidence. Reports identify the result as the highest-ranked verified candidate
found within the recorded bounded search contract. They do not claim global
program optimality, universal semantic equivalence, or cross-machine
performance equivalence.

CI contains bounded correctness and smoke tiers. Extended performance runs are
explicitly dispatched and retained as versioned evidence rather than enforced
through unstable microsecond gates.
