# ADR-0045: Declarative Product Command Contract

Status: Accepted

Date: 2026-08-17

## Context

EVO 0.42.0 has a complete private source-optimizer foundation through bounded
external-process orchestration, but it has no executable-facing contract for
`analyze`, `evolve`, `replay`, or `report`. The standalone executable belongs
to issue #93 and the concrete production providers belong to issue #114.
Issue #67 must therefore establish a product interface that both later
boundaries can consume without pretending that a private test callback is an
installed product.

The command layer also sits on a trust boundary. Manifests, paths, checkpoints,
evidence, provider metadata, and target repositories are untrusted inputs.
Provider selection must be resolved before target-controlled work starts, and
a command must not silently substitute a weaker provider, enable network
access, overwrite the input repository, or turn a human-readable summary into
independent authority.

## Decision

EVO 0.43.0 introduces a **declarative command registry and execution-plan
contract**. It does not install the final CLI and it does not execute target
code itself.

1. The fixed command registry contains exactly four versioned operations in
   stable order: `analyze`, `evolve`, `replay`, and `report`.
2. Every operation has a stable request-schema identity, help synopsis, path
   roles, checkpoint policy, provider requirements, and terminal/exit-status
   mapping.
3. `evo_project_command_plan_build` validates the already parsed manifest,
   command paths, output policy, replay identity completeness, checkpoint
   policy, provider-policy identity, exact provider identity/version, provider
   availability, and required capability mask. Only after all required checks
   pass may a plan state that external execution is permitted.
4. The command layer contains no execution callback, function pointer, shell
   command, process launch, repository mutation, or provider fallback. Issue
   #93 will execute a valid plan using the concrete provider registry from
   #114.
5. Provider selection uses the ADR-0044 identities and implementation version
   1:
   - `catalyst.evo.provider.clang-analysis.v1`;
   - `catalyst.evo.provider.clang-ast.v1`;
   - `catalyst.evo.provider.linux-bwrap.v1`; and
   - `catalyst.evo.provider.local-evaluation.v1`.
6. The provider-policy identity is
   `catalyst.evo.provider-policy.v1`. Capability bits intentionally match the
   #114 production-provider registry. An unavailable provider, wrong identity,
   wrong implementation version, or missing required capability rejects before
   external execution.
7. `analyze` requires the analysis and execution providers. Baseline build and
   correctness work therefore cannot bypass the same fail-closed execution
   policy used by later candidate work.
8. `evolve` and `replay` require all four production-provider roles because
   they may analyze, inspect transformation ASTs, execute/measure candidates,
   and perform bounded local evaluation.
9. `report` consumes retained evidence only. It requires no production provider
   and does not authorize target execution.
10. Replay requires an explicit assertion that the recorded identity set is
    complete and that every external input is declared. The executor in #93
    must independently reconcile the retained provider, capability-policy,
    toolchain, baseline, catalogue, workload, checkpoint, and artifact
    identities before external execution.
11. Resume is allowed only for `evolve` and `replay`, and an explicit checkpoint
    path is mandatory when resume is requested. `analyze` and `report` reject a
    checkpoint/resume request.
12. The input repository is read-only authority. A successful plan always
    records that repository mutation is forbidden, output publication is
    atomic, existing successful output is rejected in command-contract v1,
    and network access is never implicit.
13. Canonical machine-readable evidence remains authority. Human-readable
    reports are projections. Machine output belongs on standard output only
    when explicitly requested by #93; diagnostics and progress belong on
    standard error.
14. Stable exit statuses distinguish usage/configuration, ingestion, baseline
    build, correctness, benchmark ineligibility, analysis, materialization,
    candidate failure, no eligible champion, replay mismatch, resource failure,
    interruption, and internal failure.
15. Interrupt and resource terminal classes are command-contract facts even
    before #93 installs signal handlers. #93 must map supported platform
    interruption into the stable `interrupted` exit status only after owned
    workers have been stopped/joined and incomplete output has been cleaned.

## Command/Provider Matrix

| Operation | Manifest | Input project | Evidence | Checkpoint | Required production providers |
|---|---:|---:|---:|---:|---|
| `analyze` | yes | yes | no | no | analysis, execution |
| `evolve` | yes | yes | yes | optional | analysis, AST, execution, evaluation |
| `replay` | yes | yes | yes | optional | analysis, AST, execution, evaluation |
| `report` | no | no | yes | no | none |

The matrix is executable authority in `project_command.c`; this ADR explains
rather than duplicates that authority.

## Exit Status Contract

| Exit | Meaning |
|---:|---|
| 0 | success |
| 1 | internal failure |
| 2 | usage error |
| 3 | configuration/validated-input rejection |
| 10 | ingestion failure |
| 11 | baseline build failure |
| 12 | correctness failure |
| 13 | benchmark ineligible |
| 20 | analysis failure |
| 21 | materialization failure |
| 22 | candidate failure |
| 23 | no eligible champion |
| 24 | replay mismatch |
| 70 | resource failure |
| 130 | interrupted |

Issue #93 may add parser-specific detail, but it may not reuse one of these
values for a different terminal class without a command-contract version
change.

## Human-Readable Abstraction Assessment

The command registry and provider-requirement table are small fixed arrays with
direct scans. They are their own exact reference representations. No hash
index, cache, compressed registry, probabilistic filter, or generated dispatch
structure is authority, so ADR-0026 differential accelerator testing is not
applicable to this change.

The audit projection is the stable command descriptor/provider matrix exposed
by EVO-003 and the command tests. If a later executable introduces an
accelerated dispatch table or asset cache, #93 must provide the corresponding
exact reference and human-readable projection.

## Consequences

- #93 receives a complete executable-facing contract instead of reverse
  engineering private foundation APIs.
- #114 can implement one provider registry that matches the command capability
  vocabulary exactly.
- Provider failure is observable before target-controlled execution is
  authorized.
- `report` remains a pure evidence projection and can operate where production
  execution providers are unavailable.
- The product source-optimizer version advances to 0.43.0 without changing the
  separately versioned installed C17 core ABI.

## Related Records

- ADR-0016
- ADR-0026
- ADR-0043
- ADR-0044
- EVO-002
- EVO-003
- Issues #38, #57, #67, #69, #93, and #114
