# ADR-0040: Isolated Candidate Build and Correctness Gates

Status: Accepted

Date: 2026-08-16

## Context

ADR-0039 materializes one deterministic candidate source tree and patch without
compiling, testing, benchmarking, ranking, or publishing that candidate as a
winner. Issue #63 introduces the next trust boundary: candidate-generated code
may receive performance fitness only after declared fast build/correctness
obligations succeed, and no candidate may become the published champion until
all required finalist gates succeed.

This boundary executes untrusted candidate-controlled source through compilers,
linkers, test binaries, fuzzers, analyzers, and other configured tools. It must
therefore treat command construction, process creation, filesystem access,
network access, environment inheritance, time, process count, output volume,
and cleanup as explicit policy rather than ambient host behavior.

Correctness-gate evidence is authority for admission to later performance
measurement. A timeout, signal, malformed command, path escape, sanitizer
finding, failed test, resource exhaustion, or incomplete gate trace must fail
closed. A fast-gate success may admit a candidate to performance evaluation but
is never sufficient by itself to publish a champion.

## Decision

EVO 0.39.0 adds a private version-1 candidate assurance transaction to the
uninstalled source-optimizer foundation.

1. Assurance consumes one committed candidate result from ADR-0039, one
   explicit gate policy, one declared build profile, and nonzero resource
   limits. It never accepts an arbitrary source directory as equivalent
   authority.
2. The candidate identity, baseline identity, recipe identity, retained patch,
   and candidate tree are verified before execution. A discard-only candidate
   is ineligible for process execution because no retained candidate tree is
   available.
3. Gate policy is data, not shell text. Commands are represented as executable
   path plus bounded argv entries. EVO rejects direct shell executables and never
   reconstructs a shell command from policy or candidate bytes.
4. Portable process creation and OS sandboxing are owned by a caller-supplied
   execution provider. EVO passes the exact bounded gate view and requires the
   provider outcome to attest filesystem-policy enforcement, network-policy
   enforcement, complete process-group cleanup, and unchanged source/baseline
   authority before that outcome can become committed gate evidence.
5. The provider must execute candidate-controlled tools only in the committed
   candidate workspace under the declared working-directory, environment,
   network, timeout, process, memory, storage, output, and cleanup policy. The
   input repository and immutable baseline are never writable execution state.
6. The default network policy is deny. A gate that requires network access must
   declare it explicitly and is not part of the default fast or finalist
   profiles.
7. Process execution is resource bounded. Timeout, process-count exhaustion,
   output-budget exhaustion, signal termination, spawn failure, or cleanup
   failure rejects the current gate and therefore the candidate stage.
8. Gate records execute in deterministic policy order. Every record emits
   executable identity, argv projection, sanitized environment projection,
   working-directory role, resource policy, start/end status, exit or signal
   result, bounded stdout/stderr diagnostics, and normalized rejection reason.
9. Fast-candidate gates are the minimum obligations required before performance
   fitness may be recorded. A candidate that fails any required fast gate is
   rejected and receives no performance fitness.
10. Finalist gates are a strict superset or explicitly declared complete release
    profile. A candidate that fails any required finalist gate cannot become the
    champion even if it previously received performance fitness.
11. The declared CMake/Clang/LLVM profile and an independent Autotools/GNU
    profile are both first-class release authorities. The selected champion must
    pass both declared build profiles and every configured release gate.
12. Configured correctness authorities may include unit/integration tests,
    differential tests, fuzzing, sanitizers, static analyzers, ABI checks, and
    AES/AEMS governance. A disabled or unavailable configured required gate is a
    failure, not an implicit success.
13. Gate output is evidence, not a substitute for exit/status authority. Text
    parsing may enrich diagnostics but cannot convert a failed process into a
    passing gate.
14. Assurance publication is transactional. Partial traces, incomplete
    workspaces, or failed cleanup never publish a committed assurance identity.
    Completed rejection evidence may be retained, but it is explicitly marked
    rejected and cannot be consumed as success authority.
15. Later fitness/ranking code may consume only committed fast-gate success.
    Later champion publication may consume only committed finalist success for
    the exact same candidate identity and required policy/profile identities.

## Gate Stages

### Fast candidate stage

The fast stage is intentionally bounded and reproducible. Its default declared
obligations are:

- configure/build using the primary CMake/Clang profile;
- execute the candidate's declared focused unit/integration test set;
- execute configured sanitizer checks that are part of the fast profile;
- run required structural/static checks needed to reject malformed candidates;
- preserve exact candidate identity and immutable-input proofs.

Failure at this stage terminates candidate evaluation before benchmarking.

### Full finalist stage

The finalist stage revalidates the exact candidate identity and executes every
configured release obligation, including the independent Autotools/GNU path and
any configured differential, fuzz, sanitizer, analyzer, ABI, and AES/AEMS
gates. No fast-stage cache entry independently authorizes finalist publication.

## Process and Filesystem Rules

The assurance workspace is created outside the input repository and immutable
baseline. Candidate source is copied or bound into a private execution tree
according to explicit policy. Writable build/output directories are separate
from immutable candidate input where supported by the frontend.

Executable paths and working directories must resolve within allowed toolchain
or assurance roots. Relative traversal, symbolic-link escape, command injection,
unbounded descriptor inheritance, ambient credential directories, and
candidate-selected host paths reject before spawn.

On timeout or failure, EVO terminates the process group deterministically,
waits for child cleanup within a bounded grace period, closes captured streams,
and removes private workspace state according to policy. Surviving child
processes or failed workspace cleanup are assurance failures.

## Evidence and Identity

A committed assurance result includes a versioned identity over:

- candidate fingerprint;
- gate-policy fingerprint;
- build-profile identity;
- ordered gate definitions;
- exact normalized gate outcomes;
- toolchain identity/version evidence;
- bounded stdout/stderr fingerprints and retained diagnostic excerpts;
- stage (`fast` or `finalist`);
- explicit `performance_eligible` and `champion_eligible` authority flags.

`performance_eligible:true` is valid only for a committed successful fast stage.
`champion_eligible:true` is valid only for a committed successful finalist stage
that includes both declared build profiles and all required release gates.

## Human-Readable Abstraction Assessment

The initial implementation uses direct ordered gate arrays and exact process
results. No result cache, probabilistic membership filter, compressed gate
trace, build-result index, or alternate acceptance authority participates in
admission, rejection, performance eligibility, or champion eligibility.

The canonical human-readable projection is the ordered candidate gate trace:
command/executable identity, argv, sanitized environment policy, filesystem and
network policy, resource limits, exact result, rejection reason, and the final
stage authority flags. Because direct gate execution is the reference path and
no accelerated representation makes decisions, ADR-0026 accelerator-specific
requirements are not applicable to the initial boundary. EVO-HRA-012 records
this assessment.

Any future cache or index may be a precheck only until exact candidate identity,
policy identity, toolchain identity, freshness, and complete gate evidence are
confirmed. Probabilistic structures may never independently accept, reject,
rank, select, publish, suppress, or terminate a committed result.

## Consequences

- Candidate code cannot receive performance fitness merely because it
  materialized successfully.
- Fast-gate failure becomes a deterministic candidate rejection rather than a
  benchmark-time surprise.
- Champion publication has a stronger authority boundary than ordinary
  performance admission.
- Shell interpretation and ambient host state are excluded from the command
  contract.
- CMake/Clang/LLVM and Autotools/GNU remain independent release authorities.
- Issue #63 still does not own performance measurement, fitness ranking,
  distributed execution, deployment, commit/push behavior, or product CLI
  publication.

## Rejected Alternatives

- Running policy strings through `/bin/sh -c` was rejected because quoting and
  candidate-controlled interpolation would become command authority.
- Benchmarking first and validating only the apparent winner was rejected
  because invalid candidates would consume fitness/ranking authority.
- Treating a fast-gate pass as champion authority was rejected because the
  issue explicitly requires complete finalist release gates.
- Reusing an old success solely by candidate fingerprint was rejected because
  policy, toolchain, configuration, and gate-set identity are part of the
  correctness claim.
- Allowing unavailable required gates to become warnings was rejected because
  a partial release profile could be mistaken for complete assurance.
- Allowing candidate processes to write the repository or immutable baseline
  was rejected because it destroys replay and provenance authority.

## Verification

The normative assurance target must cover successful fast and finalist traces,
build failure, test failure, sanitizer finding, timeout, signal termination,
command-injection attempts, path escape, process/resource exhaustion, output
budget exhaustion, unavailable required gate, immutable-input verification,
process-group cleanup, and deterministic replay of rejection evidence.

Tests must prove that fast-gate failure yields `performance_eligible:false`,
that finalist failure yields `champion_eligible:false`, and that only a
candidate passing both declared build profiles and every configured release gate
can obtain champion authority.

Hosted validation must exercise the implementation through CMake/Clang/LLVM and
Autotools/GNU, run ASan/UBSan and static analysis over the assurance runtime,
validate the versioned evidence schema, and preserve AES-DEV-001,
AES-SEC-001, and AES-BLD-001 governance.

## Related Records

- ADR-0016
- ADR-0026
- ADR-0035
- ADR-0039
- EVO-002
- EVO-HRA-011
- EVO-HRA-012
- Issues #38, #57, #62, #63, #67, #83, and #93
