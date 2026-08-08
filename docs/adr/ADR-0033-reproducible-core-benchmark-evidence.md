# ADR-0033: Reproducible Core Benchmark Evidence

Status: Accepted
Date: 2026-08-08
Decision owner: EVO

## Context

EVO's deterministic core has correctness, replay, sanitizer, and build-parity
tests, but through version 0.31.0 it has no maintained benchmark baseline for
search quality, convergence, diversity, runtime, or memory. The historical
EVO-RNG-001 experiment records one seed-schedule study; it is not a repeatable
comparison of current core execution modes.

Performance evidence cannot use the same equality rule as deterministic
algorithm evidence. Wall time and resident memory vary with the operating
system, toolchain, hardware, and concurrent load. Conversely, a noisy timing
sample cannot excuse a different winner, generation trace, or termination
reason. A baseline must separate these evidence classes before measurement.

Benchmark aggregation also falls under ADR-0026. A median, compact locator,
index,
or retained summary may make evidence easier to navigate, but cannot replace
the ordered samples and logical results from which it was produced.

## Decision

EVO 0.32.0 adds benchmark artifact schema
`catalyst.evo-core-benchmark.v1`, benchmark `EVO-CORE-001`, and equivalent
CMake and GNU Autotools entry points. It does not change the public core ABI,
algorithm policy, RNG schedule, or successful-run semantics.

### Deterministic workload and policy

`EVO-CORE-001` uses `byte-onemax-v1`, an explicit 16-byte genome whose fitness
is the number of set bits. Every candidate is hard valid. One run uses:

- population size 32 and 12 completed child transitions;
- stable rank selection with base and step weights both 1;
- uniform-byte crossover at rate 0.75;
- nonzero byte-XOR mutation with initial rate 0.35;
- adaptive mutation in `[0.10, 0.80]`, step 0.10, inclusive diversity
  threshold 0.35, and improvement reset;
- two explicit elites and built-in byte-mismatch diversity; and
- generation-limit termination, with target, stagnation, diversity-floor,
  checkpoint, and secure-erasure controls disabled.

The benchmark compares these four cases in fixed order:

1. serial evaluation with explicit population allocation;
2. serial evaluation with two-slot recycling;
3. four-worker evaluation with explicit population allocation; and
4. four-worker evaluation with two-slot recycling.

Worker count and recycling disposition are the only policy differences.
Seeds, initialization, selection, operators, adaptation, stopping, and all
correctness criteria remain identical.

### Correctness authority

Seeds `0`, `42`, and `UINT64_MAX` have explicit version-1 oracles. Each oracle
contains the expected final genome and fitness plus every generation's global
best, generation-local best, and diversity scaled to one million. The benchmark
compares those fields directly; a compact locator is not accepted as a
substitute.

The first serial/non-recycled result for every configured seed is the
executable reference for mode equivalence. Every warmup, measured repetition,
and other mode must match its complete semantic result: genome, full fitness,
termination, final statistics, and ordered generation trace. A mismatch makes
the benchmark exit unsuccessfully before any timing claim can pass.

### Canonical artifact

The canonical JSON records, in stable case/seed/repetition order:

- schema, benchmark, EVO version, and exact commit identity;
- platform, architecture, compiler, linker, C standard, and clock units;
- the complete workload, policy, seed, tier, warmup, repetition, timer,
  aggregation, and tolerance definitions;
- explicit fixed oracles and their comparison outcomes;
- complete final result and generation-statistics evidence;
- every generation's search-quality, diversity, and mutation-adaptation trace;
- every raw wall-time and CPU-clock sample; and
- exact library-requested heap models plus platform-native process peak RSS.

FNV-1a is included only as a navigation locator over the case, seed, best-
genome, and raw timing fields named in its scope. It is marked
non-authoritative and cannot establish integrity, oracle, mode-equivalence,
schema, provenance, or performance acceptance.

`benchmarks/evo-core-benchmark-v1.schema.json` versions the artifact shape.
`benchmarks/validate_core_benchmark.py` parses the canonical JSON, validates
its required structure, stable ordering, explicit correctness results, raw
sample coverage, and authority declarations, then derives the Markdown
summary. A report that cannot be regenerated from valid JSON is a conformance
failure; Markdown never becomes a second authority.

The C executable writes JSON only to standard output and has no artifact-file
authority. For build targets, the Python driver invokes that exact local
executable with an argument vector and no shell, enforces a tier-specific
timeout, rejects canonical input or output larger than 2 MiB, persists the
bounded output, validates it, and only then writes the Markdown projection.

### Measurement and tolerance policy

The timed region is `evo_run` only. Result destruction, JSON serialization,
validation, and projection generation are outside it. Every raw repetition
records POSIX monotonic wall nanoseconds and ISO C CPU-clock ticks. Reports use
only min, lower median, and max derived from all raw samples in a case.

Runtime and RSS are reporting-only. No microsecond threshold, required speedup,
ranking, or cross-machine equality gate exists. Comparisons are meaningful only
within the artifact's recorded environment and measurement policy. A later
claim based on these samples must state its platform and statistical tolerance
independently.

The exact memory model counts bytes requested by EVO for population genomes,
candidate-evaluation records, the owned best genome, and worker scratch. It
records both peak simultaneously live requests and total requested bytes over
the run. It deliberately excludes allocator metadata, thread stacks, caller
context, and process runtime storage. Platform-native peak RSS covers the
complete benchmark process and is reporting-only because Linux and macOS use
different native units and runtime accounting.

### Tiers and build entry points

The smoke tier uses three seeds, one warmup, and three measured repetitions per
case/seed. It is bounded for pull requests and ordinary `main` builds. The
extended tier uses 16 seeds, three warmups, and 15 measured repetitions and is
available only through an explicit local target or manual workflow dispatch.

CMake targets `benchmark-smoke` and `benchmark-extended` and GNU Make targets
with the same names generate equivalent JSON and Markdown files under
`benchmark-results`. Both invoke the same C executable and validator. The
benchmark executable's default invocation is the smoke correctness test in
both native test inventories.

### Human-Readable Abstraction assessment

The workload uses explicit genome arrays and the existing core reference
representations. It adds no runtime cache, compressed collection, probabilistic
filter, benchmark index, or accelerated decision structure.

Aggregation is derived convenience state. Ordered JSON cases, seeds, traces,
and samples remain complete authority. The Markdown report is generated only
by parsing that JSON and names the scoped record locator as non-authoritative.
EVO-HRA-005 retains the detailed projection and equivalence audit.

## Consequences

- Core correctness and performance evidence now have separate, explicit
  acceptance rules.
- Serial/parallel and allocate/recycle execution can be compared without
  changing workload or search semantics.
- Pull requests gain bounded search-quality and replay evidence without an
  unstable performance gate.
- Extended measurements require an intentional dispatch and retain their raw
  evidence as a workflow artifact.
- The repository benchmark uses one private evaluation-record size to model
  internal heap requests; it is not an installed consumer API.
- Benchmark results do not claim that EVO has ingested, transformed, built, or
  optimized a C project.

## Alternatives considered

### Gate pull requests on elapsed-time thresholds

Rejected because shared-runner timing noise and platform changes would turn
uncontrolled environment state into correctness authority.

### Retain only averages and a final winner

Rejected because reviewers could not inspect sample spread, convergence,
diversity, or mode equivalence, and an aggregate would become opaque authority.

### Use one mode as a performance-only baseline

Rejected because mode comparisons must first prove identical semantic results
under identical seeds and policy.

### Treat the FNV record locator as oracle evidence

Rejected because a compact digest is not an explainable projection and cannot
replace explicit expected fields or direct equality.

### Run the extended tier on every pull request

Rejected because it adds avoidable shared-runner load without strengthening
the fixed correctness oracle beyond the bounded smoke tier.

## Verification

- `benchmarks/core_benchmark.c` checks explicit oracles, warmup replay,
  measured replay, and all four serial/parallel and allocate/recycle modes.
- `benchmarks/evo-core-benchmark-v1.schema.json` defines schema version 1.
- `benchmarks/validate_core_benchmark.py` rejects missing, reordered,
  non-matching, threshold-gated, or authority-drifting evidence and derives the
  Markdown projection from validated JSON.
- CMake and GNU Autotools expose identical smoke and extended targets and run
  the smoke executable in their test inventories.
- `.github/workflows/benchmarks.yml` runs smoke on pull requests and `main`,
  permits an explicit smoke or extended manual dispatch, and retains both
  artifacts.
- EVO-HRA-005 audits the canonical records, derived projection, digest role,
  aggregation, and absence of an opaque accelerator.
