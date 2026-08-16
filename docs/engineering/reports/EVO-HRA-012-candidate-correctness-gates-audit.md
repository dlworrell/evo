# EVO-HRA-012: Candidate Correctness Gates Human-Readable Abstraction Audit

Date: 2026-08-16

Audited implementation: EVO 0.39.0 candidate build/correctness-gate boundary

Governing records: ADR-0026, ADR-0040, EVO-002, issue #63

## Inventory

The candidate-assurance boundary consumes one committed candidate from
ADR-0039, one explicit gate policy, one declared build profile, and bounded
execution resources. It emits an ordered gate trace plus committed stage
authority for performance eligibility or champion eligibility.

| Domain authority | Exact representation | Stable audit projection |
|---|---|---|
| Candidate identity | Committed ADR-0039 candidate fingerprint and retained tree | Candidate identity in every gate trace |
| Gate ordering | Direct bounded policy record array | Ordered gate records in JSON/Markdown evidence |
| Command authority | Executable path plus bounded argv array | Executable and argv projection; no shell reconstruction |
| Environment authority | Minimal explicit allow-list | Sanitized environment-policy projection |
| Filesystem authority | Explicit working/writable/toolchain roots | Workspace and path-policy records |
| Network authority | Explicit deny/allow policy | Per-gate network-policy field |
| Resource authority | Timeout, process, output and cleanup bounds | Declared limits plus exact termination result |
| Gate outcome | Exact spawn/exit/signal/status record | Pass/reject record with normalized reason |
| Performance admission | Successful committed fast stage | `performance_eligible` authority flag |
| Champion admission | Successful committed finalist stage with complete release obligations | `champion_eligible` authority flag |

The initial design introduces no result cache, probabilistic membership filter,
compressed trace, build-result index, bloom filter, or alternate acceptance
structure. Direct gate execution and exact process results remain authority.

## Exact Authority and Projection Completeness

Every required gate is represented explicitly and executes in deterministic
policy order. Command policy is not reconstructed from a shell string: the
executable and argv vector are separate bounded values. Environment,
working-directory role, filesystem/network policy, timeout, process limits, and
output budgets are part of the same record.

The audit trace must expose every required gate, including failures. A gate
cannot disappear from the projection because it was cached, unavailable, or
short-circuited. Later gates may be skipped after a failure, but that skip is an
explicit result rather than absence of evidence.

Fast-stage success may authorize performance measurement only for the exact
candidate/policy/profile identities in the committed evidence. Finalist-stage
success is separate authority and must show completion of both declared build
profiles and all configured release gates before champion eligibility is true.

## Failure and Freshness

A candidate assurance result is stale when candidate identity, gate-policy
identity, build-profile identity, required gate set, or declared toolchain
identity no longer matches the execution request. Stale evidence cannot be
promoted to current success authority.

Build failure, test failure, sanitizer finding, timeout, signal termination,
spawn failure, command/path-policy rejection, resource exhaustion, output-budget
exhaustion, unavailable required gate, surviving child process, or incomplete
workspace cleanup all fail closed.

A rejected candidate may retain diagnostic evidence, but the retained trace is
explicitly rejection evidence and carries neither performance nor champion
authority.

## Process Boundary

Candidate-controlled source is treated as untrusted process input. EVO does not
claim a portable in-library OS sandbox: a caller-supplied execution provider owns
process creation and isolation. The exact gate view is argv-only and carries the
declared environment, working-directory, filesystem/network, timeout, process,
memory, storage, and output policy. The provider must attest enforcement and
complete process-group cleanup in its exact outcome; missing enforcement is a
policy rejection, not an implicit pass.

The default network policy is deny. The input repository and immutable baseline
are never authorized as writable execution state. Failure to enforce the declared
filesystem/network boundary, terminate descendants, or prove cleanup is itself a
gate failure and cannot grant performance or champion authority.

## Accelerator Assessment

No accelerator participates in the initial implementation. There is therefore
no fast/reference decision split to validate. Direct ordered gate execution is
the reference algorithm and the human-readable trace is its complete projection.

If a future cache, index, compressed result store, or probabilistic membership
structure is introduced, it must preserve exact candidate, policy, toolchain,
stage, and gate-set semantics; publish deterministic human-readable provenance;
declare completeness/resource bounds/invalidation behavior; and provide an
exact recomputation path. Differential tests must prove equality to direct gate
execution before that structure can affect committed authority.

Probabilistic structures remain prechecks only and can never independently
accept, reject, rank, select, publish, suppress, or terminate a committed
candidate result.

## Result

The EVO 0.39.0 candidate-gate design conforms to the Human-Readable Abstraction
Rule without invoking accelerator-specific requirements. Exact ordered policy
records and process outcomes remain authority, and the ordered gate trace exposes
commands, policy, exact results, rejection reasons, and the stage authority
behind performance and champion eligibility.

This finding does not pre-approve performance measurement, fitness ranking,
champion selection algorithms, persistent result caches, distributed execution,
deployment, or product CLI publication. Those remain later boundaries.
