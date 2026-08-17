# ADR-0041: Reproducible Candidate Performance Measurement and Fitness

Status: Accepted

Date: 2026-08-16

## Context

ADR-0040 admits a retained candidate to performance work only after its required
fast correctness gates pass. Issue #64 introduces the next boundary: EVO must
compare that admissible candidate with its immutable baseline without allowing
noisy timing evidence to alter correctness authority or silently inventing a
fitness objective.

Performance measurement is inherently noisy and platform-local. EVO therefore
needs a versioned contract for sample order, warmup, repetition, exclusion,
aggregation, stability, tolerance, condition identity, and fitness derivation.
The result may rank candidates inside one recorded bounded search, but it cannot
support cross-machine equivalence or a global-optimality claim.

## Decision

EVO 0.40.0 adds a private version-1 candidate measurement transaction.

1. Measurement consumes one committed candidate assurance result with
   `performance_eligible=true`. Correctness remains the assurance result; timing
   evidence cannot convert a correctness pass to failure or a correctness
   failure to pass.
2. Every workload declares warmup count, measured repetition count, deterministic
   pair ordering, timeout, outlier policy, stability limit, comparison tolerance,
   minimum improvement threshold, and workload weight. EVO never supplies
   consumer weights implicitly.
3. Sample order is deterministic. Each baseline/candidate pair has an explicit
   pair index and sequence index. Alternating order is derived only from the
   declared policy and pair index.
4. A caller-supplied measurement provider executes one requested subject sample
   and returns bounded raw evidence. The provider must attest the exact recorded
   condition fingerprint covering hardware, operating system, compiler/linker,
   optimization flags, environment, dataset, and binary identities.
5. Raw samples are authority. Warmups are retained but never aggregated.
   Measured samples may be excluded only by the declared deterministic policy;
   every exclusion remains visible in canonical evidence.
6. Version 1 supports no exclusion or absolute deviation from the subject median.
   Aggregation is the median of included measured samples. Stability is the
   included runtime range relative to the median and is checked independently
   for baseline and candidate.
7. Missing, timed-out, failed, condition-mismatched, or insufficient samples make
   the measurement incomplete. Excess variance makes it unstable. Incomplete or
   unstable measurements publish evidence but produce no EVO fitness.
8. Complete stable workloads classify the candidate as faster, equal within the
   declared tolerance/threshold policy, or slower. Comparison status is
   performance evidence only.
9. Runtime, peak memory, binary size, reliability, and maintainability are
   recorded separately for baseline and candidate. Runtime contributes the
   `performance` fitness component. Peak-memory and binary-size improvements are
   combined only through an explicit declared mix before entering
   `memory_use`. Reliability and maintainability remain independent components.
10. Fitness weights are explicit finite values supplied by the caller for
    correctness, performance, memory use, reliability, maintainability, and
    constraint penalty. `total` is reconstructed from those recorded components
    and weights; EVO does not reweight them elsewhere.
11. The transaction records canonical JSON and derived Markdown with identities,
    policy, condition fingerprint, every raw sample, exclusions, aggregates,
    stability calculations, comparison, fitness components, weights, and exact
    total derivation.
12. Measurement identity is independent of output path. Replay under identical
    inputs and raw provider outcomes must produce identical canonical evidence
    and fitness.
13. The report wording is bounded: a later winner may be described only as the
    best verified candidate found within the recorded bounded search contract,
    never as a globally optimal program.
14. Publication is transactional. Partial output cannot become committed
    measurement evidence.

## Human-Readable Abstraction Rule

The initial implementation uses explicit bounded workload and raw-sample arrays,
deterministic scans, and deterministic median aggregation. It introduces no
cache, probabilistic filter, compressed result store, or alternate ranking
authority. Canonical raw samples and declared policy remain exact authority; the
Markdown report is a derived complete projection.

A future accelerator must retain exact reference semantics, source identity,
version, resource bounds, invalidation behavior, deterministic projection, and
an exact fallback/recomputation path. Probabilistic structures may precheck work
but may never independently accept, reject, rank, select, publish, suppress, or
terminate a committed result.

## Consequences

Performance ranking becomes reproducible for one recorded environment and
policy without pretending that timing is correctness. Unstable or incomplete
measurements remain reviewable failures rather than arbitrary numeric fitness.
Issue #65 may consume only committed finite fitness from this boundary.