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
