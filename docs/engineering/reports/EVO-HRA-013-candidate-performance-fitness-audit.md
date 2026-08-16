# EVO-HRA-013: Candidate Performance and Fitness Human-Readable Abstraction Audit

Date: 2026-08-16

Audited design: EVO 0.40.0 candidate measurement and fitness boundary

Governing records: ADR-0026, ADR-0041, EVO-002, issue #64

## Inventory

The measurement boundary consumes one performance-eligible assurance result,
one explicit measurement policy, one exact platform/condition identity, and a
bounded sequence of provider samples. It emits a complete ordered measurement
trace, workload aggregates, comparison status, and finite EVO fitness only when
all required evidence is complete and stable.

| Domain authority | Exact representation | Stable audit projection |
|---|---|---|
| Correctness admission | Committed ADR-0040 assurance with `performance_eligible=true` | Assurance identity and unchanged correctness component |
| Workload policy | Direct bounded workload-policy array | Ordered workload policy records |
| Pair ordering | Deterministic policy plus pair index | Explicit subject, phase, pair index, and sequence index |
| Condition identity | Exact hardware/OS/toolchain/environment/dataset/binary strings | Condition fingerprint plus named identities |
| Raw measurement | Direct bounded sample array | Every warmup/measured sample and provider disposition |
| Exclusion | Declared deterministic median-deviation policy | Per-sample excluded flag and reason |
| Aggregation | Included samples and deterministic median | Baseline/candidate aggregates for each metric |
| Stability | Exact included runtime min/median/max and declared range limit | Stable/unstable status with range evidence |
| Comparison | Aggregate runtime plus tolerance/improvement policy | Faster/equal/slower/incomplete/unstable disposition |
| Fitness | Recorded component values and caller weights | Full component/weight/total derivation |

## Exact Authority and Projection Completeness

Raw provider outcomes are never replaced by a cache hit, statistical summary,
or human-readable report. Warmups remain visible but do not participate in
aggregation. Excluded measured samples remain in the canonical sequence with an
explicit exclusion reason. Median, range, relative improvement, and final
fitness are derived facts and are reproducible from those records.

Correctness authority remains the assurance result. Measurement failure,
variance, regression, or timeout cannot rewrite correctness; those conditions
only affect performance completeness, comparison, and fitness availability.

The audit projection orders workloads by policy order and samples by sequence
index. It exposes all records in the transaction, so no sample can disappear
because it was excluded, failed, or inconvenient.

## Comparable Conditions

Every provider outcome must match the transaction condition fingerprint.
Hardware, operating system, compiler/linker, environment, dataset, baseline
binary, candidate binary, baseline identity, and candidate identity are bound
into evidence. A mismatch makes the measurement incomplete rather than silently
comparing unlike runs.

The contract is intentionally platform-local. It records enough provenance to
review and replay one comparison but does not claim that two different machines
must produce equivalent timing.

## Fitness Derivation

No default consumer objective is hidden in the implementation. Workload weights,
peak-memory versus binary-size mix, and EVO fitness-component weights are all
explicit finite policy. The transaction records both the component values and
the declared weights, and the reported `total` is recomputed from those values.
An incomplete or unstable measurement has no finite ranking authority even
though its raw evidence remains available.

## Accelerator Assessment

No accelerator participates in the initial implementation. There is no result
cache, probabilistic membership structure, compressed sample store, approximate
quantile sketch, or alternate acceptance/ranking index. Direct bounded arrays,
exact sample dispositions, and deterministic scans remain authority.

If a future cache, sketch, index, compressed store, or probabilistic precheck is
introduced, it must identify the exact sample/policy/condition source, version,
completeness, resource bounds, invalidation/corruption behavior, deterministic
human-readable projection, and exact fallback or recomputation path.
Differential tests must prove equivalence to direct execution before it can
affect committed ranking authority. Probabilistic structures remain prechecks
only and can never independently accept, reject, rank, select, publish,
suppress, or terminate a committed result.

## Result

The EVO 0.40.0 design conforms to the Human-Readable Abstraction Rule using
explicit raw samples as authority. The complete audit view retains sample order,
exclusions, aggregates, tolerances, stability, comparison, and fitness
derivation without opaque accelerated state.