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

### Maintained baseline: EVO-CORE-001

Version 0.32.0 defines `catalyst.evo-core-benchmark.v1` and the deterministic
`byte-onemax-v1` workload. It records complete explicit policy and runs the
same seed set through serial/parallel evaluation and explicit/recycled storage.
Only worker count and recycling disposition differ.

Correctness is resolved before measurement:

- seeds `0`, `42`, and `UINT64_MAX` compare directly with fixed final-genome,
  fitness, termination, convergence, and diversity vectors;
- every warmup and measured repetition must reproduce its case/seed reference;
- every case must reproduce the complete serial/non-recycled semantic result;
  and
- a mismatch fails the executable and artifact validator regardless of timing.

The canonical JSON retains every case, seed, full generation trace, final
result/statistics record, and raw measurement repetition in stable order. Its
scoped FNV-1a value is a non-authoritative navigation locator. The
Markdown summary is generated only by parsing and validating the JSON; it is
never an independent result source.

The smoke tier uses three seeds, one warmup, and three measured repetitions per
case/seed. It is the bounded CI tier. The extended tier uses 16 seeds, three
warmups, and 15 repetitions and must be dispatched explicitly. Both CMake and
GNU Autotools expose `benchmark-smoke` and `benchmark-extended`; exact commands
and artifact paths are in `benchmarks/README.md`.

Wall nanoseconds, CPU-clock ticks, and platform-native process RSS are
reporting-only. The derived report retains min, lower median, and max without
outlier removal. EVO enforces no microsecond regression threshold and makes no
cross-machine equivalence claim. The exact requested-memory model covers EVO
population genomes, candidate-evaluation records, result-genome storage, and
worker scratch; allocator metadata, thread stacks, caller context, and process
runtime storage are explicitly excluded.

ADR-0033 fixes the benchmark, schema, tier, tolerance, and failure contract.
EVO-HRA-005 audits the ordered evidence and derived projection under ADR-0026.

## Reference Adapter Evidence

Version 0.33.0 defines `catalyst.evo-reference-adapters.v1` and
`EVO-ADAPTERS-001`. It is deterministic integration evidence rather than a
runtime benchmark: no timing or performance threshold is measured or claimed.

The artifact contains four fixed installed-consumer records in stable order.
Every record includes its complete fixture, search configuration, hard
constraint, soft penalty, limitation, final result, and generation trace. The
repository-scoring record adds all candidates from a generation-two checkpoint
and exact resume comparisons. The scheduler record adds every candidate's
logical worker, dispatch wave, final disposition, and commit evidence for each
generation.

Each program runs twice and must match directly. A bounded no-shell validator
checks all stable fields, constructs the combined object, and requires exact
equality to the reviewed complete golden JSON before writing the canonical
artifact or its Markdown projection. No digest, aggregate, cache, compressed
index, or probabilistic precheck can substitute for an explicit record.

Independent workflow jobs build the adapters against fresh CMake and Autotools
staged installations. ADR-0034 fixes execution, ownership, failure, product,
and evidence boundaries; EVO-HRA-006 audits the stable registry under ADR-0026.

## Source-Optimization Experiments

Version 0.34.0 baseline capture records benchmark eligibility but performs no
candidate comparison and claims no optimization. Its canonical baseline
evidence retains the exact workload registry, declared benchmark command,
provider identity, gate disposition, bounded output metadata, immutable source
registry, and normalized compilation units. A failed correctness gate
suppresses benchmarking; a failed required benchmark yields the distinct
`benchmark-ineligible` state. FNV labels are deterministic diagnostics, not
performance, authentication, or correctness authority.

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
