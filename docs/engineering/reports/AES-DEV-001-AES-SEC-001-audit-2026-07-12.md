# EVO AES-DEV-001 and AES-SEC-001 Audit

Date: 2026-07-12
Repository: `dlworrell/evo`
Status: Baseline assessment

## Executive Summary

EVO has adopted local profiles for AES-DEV-001 and AES-SEC-001. The repository has an architecture and algorithm documentation set, a versioned public C API surface, small source modules, tests, CMake build configuration, an accepted initial architecture decision record, and CI workflows for build, test, formatting, static analysis, AddressSanitizer, and UndefinedBehaviorSanitizer.

The repository passes the minimum AES-SEC-001 adoption gate after this change series because the secure C/C++ profile and explicit waiver log exist and no banned unsafe API use was found in the current project-owned C source.

Full compliance is not yet claimed. Component and property tests remain incomplete, and fuzz coverage must be added before external-input parsers or serialization handlers are accepted.

## AES-DEV-001 Matrix

| Requirement | Status | Evidence or action |
|---|---|---|
| Architecture before stable implementation | Pass | `docs/architecture.md`, `docs/theory.md`, `docs/algorithms.md` |
| Documentation updated with behavior | Pass for current scaffold | Documentation and implementation were introduced together |
| Documentation authority declared | Pass | `docs/engineering/AES-DEV-001-development-principles.md` |
| Interfaces versioned | Pass | CMake project version 0.1.0 and manifest versioning |
| Small logical commits | Pass | Repository history uses focused file and workflow commits |
| Tests or test rationale | Partial | Smoke test exists; component and property tests remain future work |
| Observable behavior | Partial | Statistics API and benchmark plan exist; runtime diagnostics are not implemented |
| Recovery and failure behavior | Partial | Documented as a design requirement but not yet implemented |
| Security/trust-boundary review | Pass at scaffold stage | Local secure profile and request templates added |
| ADR for major architecture choices | Pass | `docs/adr/ADR-0001-library-boundary-and-build-system.md` |

## AES-SEC-001 Matrix

| Requirement | Status | Evidence or action |
|---|---|---|
| Local secure profile | Pass | `docs/engineering/SECURE-C-CXX.md` |
| Explicit waiver log | Pass | `docs/engineering/AES-SEC-001-waivers.md` |
| Banned unsafe APIs absent | Pass for current source | Repository search found no banned API use |
| Warning-clean build profile | Pass at configuration level | `-Wall -Wextra -Wpedantic` in CMake |
| Static analysis | Pass at workflow level | `cppcheck` and `clang-tidy` in `.github/workflows/quality.yml` |
| Sanitizers | Pass at workflow level | ASan and UBSan in `.github/workflows/sanitizers.yml` |
| Fuzzing | Not yet applicable | Required when parsers, checkpoint readers, or external-input handlers are implemented |
| Explicit lengths and overflow checks | Pending implementation | Must be demonstrated in code reviews as relevant code is added |
| Unsafe code isolation | Pending implementation | No current unsafe boundary requiring isolation |
| Custom cryptography avoided | Pass | No cryptographic implementation exists |

## Required Follow-Up

1. Add component tests for selection, crossover, mutation, diversity, RNG, and checkpoint behavior.
2. Add fuzz harnesses before checkpoint or other external-input parsing becomes stable.
3. Preserve this audit as the baseline and ratchet enforcement against new work.

## EVO 0.29.0 Checkpoint Amendment (2026-08-08)

This section preserves the scaffold-era assessment above as history and records
the disposition of its parser-specific follow-up when checkpoint format 1 is
implemented by ADR-0030.

| Requirement | 0.29.0 status | Retained evidence |
|---|---|---|
| Fuzzing | Pass for checkpoint parser | `tests/checkpoint_fuzz_test.c` covers every truncation, one bit at every byte, and 2,048 seeded arbitrary inputs; `tests/fuzz/checkpoint_fuzz.c` exposes the same parser to libFuzzer |
| Explicit lengths and overflow checks | Pass for checkpoint boundary | Caller byte budget, fixed header, total length, ordered section offsets/sizes, 64-bit-to-`size_t` conversion, multiplication/addition, record counts, owner byte counts, and re-signed semantic tampering reject before allocation |
| Unsafe-input isolation | Pass for checkpoint boundary | `evo_checkpoint_inspect` validates without allocation, RNG, or callbacks; `evo_resume` allocates only after inspection and exact configuration/identity matching |
| Custom cryptography avoided | Pass | CRC-32 is labeled accidental-corruption detection and FNV-1a is labeled a format fingerprint; neither is authentication, encryption, collision-resistant authority, or a security credential |
| Failure cleanup | Pass | Allocation-failure and secure-erasure tests cover all three restore owners, atomic empty failure, local-backend erasure, and caller-buffer exclusion |

Core checkpoint bytes remain untrusted unless separately authenticated by the
consumer. ADR-0030, EVO-001, and EVO-HRA-002 define the parser, ownership,
security, and explainable-projection boundaries.

## EVO 0.30.0 Population-Recycling Amendment (2026-08-08)

Population recycling reuses only two fixed run-local owners. It introduces no
global pool, address-keyed lookup, concurrent free list, unbounded allocation,
or new raw allocation primitive. The existing checked genome and evaluation
constructors materialize the second slot; later transitions allocate nothing.

| Requirement | 0.30.0 status | Retained evidence |
|---|---|---|
| Exact ownership | Pass | Pointer/count owners remain authority; registry evidence is reconciled and cannot select an owner; advancement rejects object, owner-range, evidence, and registry aliases before mutation |
| Bounded allocation | Pass | `tests/allocation_failure_test.c` proves five allocations for one or seven transitions, cleanup at each allocation failure, and absence of a sixth allocation |
| Reset before reuse | Pass | Ordinary mode zeros both complete ranges; secure mode invokes the reviewed erasure wrapper over both exact ranges before role handoff completes |
| Failure cleanup | Pass | Recycled provisional-evaluation failures reset and return the reserve; enclosing failure releases every acquired owner once through its retained policy |
| Replay neutrality | Pass | `tests/population_recycling_test.c` compares complete callback traces, results, statistics, events, stopping, termination, and algorithm-visible RNG evidence with the explicit allocation path |
| Explainability | Pass | The fixed address-free registry exposes lifecycle, generations, capacities, handoffs, resets, erasures, and owner presence; EVO-HRA-003 retains the ADR-0026 audit |
| Checkpoint trust boundary | Pass | Format 2 validates the complete source registry before allocation, reconstructs local owners, and reattaches only the restoring build's erasure backend |

ADR-0031, EVO-001, and EVO-HRA-003 define the lifecycle, security, replay, and
human-readable projection boundaries. Shared pools, cross-run caches, lazy
reset, and parallel owner access remain outside this assessment.

## EVO 0.31.0 Parallel-Evaluation Amendment (2026-08-08)

Parallel evaluation is disabled by zero initialization. Enabled execution uses
exactly the caller-declared number of run-local POSIX workers and one checked
temporary allocation bounded by an exact public size query. Only a caller-
declared thread-safe evaluator is concurrent; validity and every other callback
remain synchronous.

| Requirement | 0.31.0 status | Retained evidence |
|---|---|---|
| Explicit concurrency contract | Pass | Positive worker count requires the thread-safe evaluator declaration; serial-only callbacks reject before allocation or callbacks |
| Bounded resources | Pass | Worker count is no greater than population size; checked size/alignment arithmetic and the caller scratch budget gate the sole library scheduler allocation |
| Race-free publication | Pass | Release/acquire wave epochs isolate candidate writes; coordinator validation and commit occur only after a complete wave and all workers join; hosted ThreadSanitizer exercises the scheduler |
| Failure cleanup | Pass | Allocation, worker-start, worker-join, non-finite fitness, and later diversity failures attach no record set or generation; every started worker terminates before scratch release |
| Stable authority | Pass | Fitness validates and commits in ascending candidate order; completion timing, native thread identity, and atomic state cannot affect ranking, statistics, stopping, or checkpoint state |
| Explainability | Pass | The complete candidate-ordered schedule projects logical worker, wave, disposition, cancellation, and commit order; EVO-HRA-004 retains the ADR-0026 audit |
| Checkpoint trust boundary | Pass | Format 3 binds deterministic parallel configuration and committed provenance but persists no live thread, queue, atomic, callback timing, or provisional schedule |

ADR-0032, EVO-001, and EVO-HRA-004 define the callback, memory, scheduler,
failure, replay, checkpoint, and human-readable projection boundaries. Dynamic
work stealing, persistent pools, asynchronous callback cancellation, timeouts,
and distributed workers remain outside this assessment.

## EVO 0.32.0 Core-Benchmark Amendment (2026-08-08)

The core benchmark adds a repository-owned C executable and a Python artifact
validator/projection driver. It changes no installed runtime API and introduces
no new library allocation, concurrency, parser, checkpoint, or callback trust
boundary. Benchmark results are evidence about a named build and environment;
they are never executable input to EVO.

| Requirement | 0.32.0 status | Retained evidence |
|---|---|---|
| Dangerous C primitives | Pass | `benchmarks/core_benchmark.c` writes JSON only to standard output and introduces no banned allocation, copy, formatting, process, or file primitive |
| Process invocation | Pass | `benchmarks/validate_core_benchmark.py` invokes the exact build-tree executable with an argument vector, no shell, and fixed smoke/extended timeouts |
| Bounded artifact input | Pass | The driver rejects canonical JSON, schema input, or captured benchmark output larger than 2 MiB before parsing or retention |
| Output authority | Pass | The build system supplies explicit output paths; the C executable has no file authority, and Markdown is written only after canonical JSON validation |
| Correctness authority | Pass | Explicit seed oracles and direct ordered mode equality remain authoritative; timing, RSS, aggregation, and the scoped FNV record locator cannot approve correctness |
| Explainability | Pass | Canonical JSON retains every ordered trace and raw sample; the Markdown projection is derived from validated JSON; EVO-HRA-005 retains the ADR-0026 audit |

ADR-0033 and EVO-HRA-005 define the benchmark's execution, evidence,
measurement, and human-readable projection boundaries. Benchmark artifact
ingestion into the installed library, network publication, shell evaluation,
and performance-threshold enforcement remain outside this assessment.

## EVO 0.33.0 Reference-Adapter Amendment (2026-08-09)

The four reference adapters are external installed-package consumers. They add
no library ABI, private include, network service, filesystem mutation target,
or heavyweight external toolchain. Their deterministic fixture/evidence
boundary is independently staged through CMake and Autotools.

| Requirement | 0.33.0 status | Retained evidence |
|---|---|---|
| Dangerous C primitives | Pass | Adapter snapshots and genomes use explicit bounded byte loops; no banned C allocation, copy, string, formatting-to-buffer, process, or filesystem primitive is introduced |
| Process invocation | Pass | The Python driver invokes four exact program paths with argument vectors, `shell=False`, a 15-second timeout, and captured output |
| Bounded input/output | Pass | Programs use fixed fixture/capture/checkpoint arrays; the driver limits each stdout to 128 KiB and golden/schema/artifact documents to 256 KiB |
| Ownership and cleanup | Pass | Borrowed views are copied only during synchronous callbacks; each zero-initialized result is destroyed once on every path; caller checkpoint arrays are never released by EVO |
| Callback concurrency | Pass | Only scheduler evaluation opts in; it reads an immutable fixture, writes no shared state, and retains the complete post-join logical schedule |
| Output authority | Pass | C programs write stdout only; exact combined golden equality precedes canonical JSON and derived Markdown writes |
| Product boundary | Pass | Every record sets `source_optimizer_claimed: false`; compiler configuration search explicitly parses, transforms, and emits no C source |
| Explainability | Pass | Fixed fixtures, configurations, traces, checkpoint candidates, schedules, limitations, and replay results remain explicit; EVO-HRA-006 retains the ADR-0026 audit |

ADR-0034 and EVO-HRA-006 define the installed boundary, resource policy,
failure behavior, complete registry, and source-optimizer non-claim. Real
repository mutation, compiler execution, operating-system scheduler changes,
FPGA tooling, remote publication, and untrusted fixture ingestion remain
outside this assessment.

## EVO 0.34.0 Project-Ingestion Amendment (2026-08-09)

The private project-ingestion foundation accepts untrusted manifest,
compilation-database, path, filesystem, and execution-provider data. It owns no
process launcher: project commands cross one explicit callback boundary with
network disabled and bounded time, memory, processes, storage, and output.

| Requirement | 0.34.0 status | Retained evidence |
|---|---|---|
| Dangerous C primitives | Reviewed | One private runtime unit contains the only zeroed-allocation, release, and bounded-formatting calls; it rejects invalid allocation dimensions, while callers check every result and retain explicit owner/byte invariants; bounded byte loops replace unchecked copy/string primitives |
| Filesystem authority | Pass | One realpath-authorized source root is read through no-follow traversal; symlinks and special files reject; only a caller-selected output outside that root is reserved and mutated |
| Command boundary | Pass | Exact manifest argv and resource policy are passed to a caller provider; the foundation performs no shell parsing, process spawn, network access, or environment inheritance |
| Input bounds | Pass | Manifest bytes/tokens/depth/strings, roots, paths, files, compilation database, translation units, argv, evidence, command output, time, memory, processes, and storage have manifest and caller limits |
| Atomicity and cleanup | Pass | An incomplete marker identifies the owned transaction; failures remove only that output without following symlinks; source drift, provider corruption, or evidence overflow commits no baseline |
| Identity boundary | Pass | Exact read-only bytes and complete registries are authority; FNV labels are explicitly deterministic, non-authenticating, and non-authoritative |
| Explainability | Pass | `baseline.json` and `baseline.md` enumerate every file, compilation unit, policy list, and gate disposition in stable order; EVO-HRA-007 retains the ADR-0026 audit |

ADR-0035 defines the ownership, path, normalization, provider, failure, and
evidence invariants. The normative test covers missing/duplicate/malformed,
overlapping/out-of-root, oversized, symlinked, ambiguous, corrupt-provider, and
concurrent-source-mutation paths. Provider process isolation, Clang input
handling, candidate execution, authentication across trust boundaries, and the
installed executable remain separately reviewable later milestones.
