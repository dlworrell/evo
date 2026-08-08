# EVO-HRA-005: Core Benchmark Evidence Human-Readable Abstraction Audit

Status: Complete
Date: 2026-08-08
Repository: `dlworrell/evo`
Audited implementation: EVO 0.32.0
Governing decisions: ADR-0026 and ADR-0033
Tracking issue: #54

## Question

Can a benchmark aggregate, compact locator, retained report, timing sample,
or execution-mode shortcut become opaque authority over core correctness,
search quality, or a performance claim?

## Classification

`EVO-CORE-001` introduces no accelerated core structure. Its byte genomes,
results, generation traces, and timing samples are explicit bounded records.
There is no result cache, benchmark index, compressed sample set, membership
filter, probabilistic decision, or hidden baseline store.

The lower median and FNV-1a record locator are derived convenience values. They
do not authorize correctness, equivalence, ranking, or publication. Runtime and
resident-memory measurements are explicitly non-deterministic reporting
evidence and cannot override a failed oracle or result comparison.

## Canonical Ordered Projection

The JSON artifact is both machine-readable authority and an explicit
human-auditable projection:

| Logical fact | Canonical JSON scope | Authority rule |
|---|---|---|
| Workload | Named representation, fitness, population, transition limit, validity, and stopping policy | Recorded scalar configuration |
| Environment | EVO version/commit, platform, architecture, compiler, linker, C standard, and clock units | Exact artifact context; no cross-machine equality claim |
| Mode | Fixed serial/parallel and allocate/recycle cases | Only worker count and recycling disposition may differ |
| Oracle | Explicit genome, fitness, termination, and per-generation best/diversity vectors | Direct field comparison, never a digest |
| Search | Complete generation-zero-through-limit trace | Stable generation order with no omitted window |
| Measurement | Every case/seed/repetition wall and CPU sample | Explicit raw samples; aggregates are derived |
| Memory | Exact requested-byte model and platform-native process RSS | Model scope/exclusions stated; RSS reporting-only |
| Summary | Markdown regenerated from validated JSON | Projection only; failure to reconcile is a gate |

Cases are ordered serial-allocate, serial-recycle, parallel-allocate, then
parallel-recycle. Every case has the same stable seed order. Generation rows
are contiguous from zero through 12, and repetition rows are contiguous from
zero through the tier's declared count. The artifact is complete and requires
no page, index, continuation token, or reconstruction guess.

## Correctness and Equivalence

Three small seeds have explicit baked oracles. The comparison reads each
expected field directly: final genome and total, termination/count, and all 13
global-best, generation-best, and scaled-diversity rows. The informational
locator does not participate.

For every seed, serial non-recycled execution is the live reference. Warmups,
measured repetitions, serial recycling, four-worker allocation, and four-worker
recycling compare the complete semantic result, final statistics, and trace
against that reference. Any difference makes the executable, validator,
native test, and workflow fail before timing is summarized.

This retains the exact reference paths established by EVO-HRA-003 and
EVO-HRA-004. Recycling may reduce repeated owner allocation and parallelism may
overlap evaluator calls, but neither changes search authority.

## Aggregation and Record-Locator Boundary

The artifact retains every raw timing sample. The Markdown generator parses
those arrays and computes min, lower median, and max without modifying the
JSON. No outlier is removed. The summary names sample counts, requested-memory
model, initial/final search quality, termination, and the scoped record locator.

FNV-1a covers only the case, seed, best-genome, and raw timing fields declared
by its scope and exists for navigation only. The JSON labels it
`authoritative: false`; the validator requires both declarations and recomputes
the value from those explicit fields. A matching FNV value cannot prove
artifact integrity, schema validity, commit identity, oracle success, mode
equivalence, or acceptable performance.

Timing has no pass threshold. A slow or noisy correct run remains correct, and
a fast mismatching run fails. Platform-specific RSS cannot become a portable
memory claim. Any later performance conclusion must use the retained raw
samples and state an environment-specific statistical tolerance.

## Failure and Reconstruction

- A malformed or incomplete JSON document prevents summary generation.
- A missing/reordered case, seed, generation, or sample fails validation.
- A false oracle, replay, serial-equivalence, or overall correctness field
  fails validation.
- A runtime threshold or cross-machine-equivalence claim set true fails
  validation.
- A record locator marked authoritative or an accelerated authority claim fails
  validation.
- Markdown is always overwritten from the validated JSON; it cannot drift and
  remain accepted as an independent report.

The artifact driver invokes the locally built benchmark through an explicit
argument vector, never a shell command. The smoke and extended timeouts are
fixed, and the C executable owns no filesystem output path. This keeps
orchestration policy outside core search and prevents report construction from
silently changing the benchmark command.

The benchmark makes no fallback to a prior result, cached trace, summary table,
or digest. Re-execution from the recorded commit, policy, seed, build frontend,
and environment is the reconstruction procedure.

## Result

EVO 0.32.0 core benchmark evidence conforms to the Human-Readable Abstraction
Rule. Explicit ordered JSON remains complete authority, the human-readable
summary is regenerated from it, and every aggregate or compact value is
strictly derived and non-authoritative.

This finding does not pre-approve a benchmark database, searchable index,
compressed time series, remote result cache, probabilistic regression detector,
or source-optimizer performance-fitness store. Each would require source
identity, freshness/invalidation, complete or windowed projection, exact
fallback, resource bounds, and differential evidence under ADR-0026.
