# EVO-003: Product Command Contract

Status: Implemented for EVO 0.43.0; installed executable delivery remains #93

Version: 0.43.0

Owner: EVO

Governing ADRs: ADR-0016, ADR-0026, ADR-0044, ADR-0045

## Purpose

EVO-003 defines the versioned executable-facing contract for the source
optimizer operations `analyze`, `evolve`, `replay`, and `report`. It sits above
the private source-optimizer foundation in EVO-002 and below the installed
standalone executable in issue #93.

The 0.43.0 implementation is deliberately declarative. A command request is
validated into an immutable execution plan. The planner launches no process,
executes no target-controlled command, and mutates no repository. The installed
executor in #93 may execute only a plan that passed this contract and only
through production providers delivered by #114.

## Registry

The command registry is complete and ordered:

1. `analyze` — schema `catalyst.evo.command.analyze.v1`;
2. `evolve` — schema `catalyst.evo.command.evolve.v1`;
3. `replay` — schema `catalyst.evo.command.replay.v1`; and
4. `report` — schema `catalyst.evo.command.report.v1`.

Each registry entry has a stable help synopsis. Unknown commands are not
aliases and do not dispatch to another operation.

## Common Path Contract

Paths are explicit command inputs. No command may infer a hidden repository,
working directory, cache, service, credential, or network location as
execution authority.

- `manifest` identifies the optimization manifest when required.
- `input` identifies the user-authorized project input when required.
- `evidence` identifies retained canonical evidence when required.
- `checkpoint` identifies an explicit product checkpoint for supported resume
  operations.
- `output` identifies the requested output location and is always required.

The 0.43 planner performs lexical non-empty/control-character validation and
records path roles. #93 owns filesystem resolution, canonicalization, symlink
checks, permissions, safe creation, and clean-prefix behavior before any path is
opened. Relative-path resolution must be documented and independent of hidden
source/build-tree state.

Existing successful output is rejected by command-contract v1. Output
publication is atomic. The input repository is read-only and no command may
commit, push, merge, deploy, or overwrite it.

## Operation Contracts

### `analyze`

Synopsis:

`analyze --manifest <file> --input <project> --output <directory>`

Required inputs:

- parsed valid optimization manifest and manifest path;
- input project path;
- output directory;
- provider policy `catalyst.evo.provider-policy.v1`;
- production analysis provider; and
- production execution provider.

The execution provider is required because baseline configure/build/correctness
commands are target-controlled work and may not run through a weaker host path.
The analysis provider consumes captured compilation evidence. Analysis does not
permit checkpoint/resume in command-contract v1.

### `evolve`

Synopsis:

`evolve --manifest <file> --input <project> --evidence <analysis> --output <directory> [--checkpoint <file>]`

Required inputs:

- parsed valid optimization manifest and manifest path;
- input project path;
- retained analysis/baseline evidence path;
- output directory;
- provider policy `catalyst.evo.provider-policy.v1`; and
- all four production-provider roles.

Resume is optional. If requested, the checkpoint path is mandatory and the
executor must validate all checkpoint/product/provider identities before
external execution.

### `replay`

Synopsis:

`replay --manifest <file> --input <project> --evidence <recorded> --output <directory> [--checkpoint <file>]`

Replay accepts only recorded identities and explicitly declared external
inputs. The request must assert that the retained identity set is complete and
that external inputs are declared before a plan can authorize execution. This
planner assertion is not sufficient by itself: #93 must independently compare
provider identity/version, capability-policy identity, baseline, analysis,
catalogue, toolchain, target, workload, manifest, checkpoint, and artifact
schema authority before launching target work.

Replay requires all four production-provider roles and permits an explicit
checkpoint path. A mismatch is a stable replay failure, not a request to
substitute the nearest available provider.

### `report`

Synopsis:

`report --evidence <recorded> --output <path>`

Report consumes canonical evidence and emits a human-readable and/or requested
machine projection. It requires no manifest, project input, production
provider, network access, or checkpoint. It cannot authorize target execution.

Canonical machine-readable evidence remains authority. A report is derived and
must be regenerable from that evidence; it is not a second independent record.

## Production Provider Contract

The command capability vocabulary is numerically identical to ADR-0044/#114:

| Bit | Capability |
|---:|---|
| 0 | Clang AST |
| 1 | compilation database |
| 2 | direct argv |
| 3 | CPU limit |
| 4 | address-space limit |
| 5 | process limit |
| 6 | storage limit |
| 7 | captured-output limit |
| 8 | wall timeout |
| 9 | filesystem isolation |
| 10 | network isolation |
| 11 | descendant cleanup |
| 12 | asynchronous start/poll/cancel/join |
| 13 | measurement |

Required provider identities are fixed at implementation version 1:

- analysis: `catalyst.evo.provider.clang-analysis.v1`;
- transformation AST: `catalyst.evo.provider.clang-ast.v1`;
- execution/measurement: `catalyst.evo.provider.linux-bwrap.v1`;
- local evaluation: `catalyst.evo.provider.local-evaluation.v1`.

A required provider must match the exact identity and implementation version,
report available, and contain every required capability bit. Any failure leaves
external execution unauthorized. There is no fake-provider or unsandboxed
fallback in the standalone command contract.

`analyze` requires analysis + execution. `evolve` and `replay` require all four.
`report` requires none.

## Manifest Preflight

Commands requiring a manifest receive an already parsed
`evo_project_manifest_t`. Before a plan is valid, the command boundary requires
at least the manifest schema, manifest identity, source identity, build
frontend, language, nonzero path/evidence limits, nonzero command timeout, and
nonzero process budget.

This is command-level preflight, not a substitute for the complete strict
manifest parser in EVO-002. #93 must parse and validate the complete manifest
before invoking the planner and before expensive provider probing/execution.

## Checkpoint and Resume

Only `evolve` and `replay` permit resume in v1. A resume request without an
explicit checkpoint path rejects. `analyze` and `report` reject checkpoint
execution state.

The provider policy and provider identities are replay/checkpoint authority.
#114 binds concrete provider identity/version/capability policy into the product
checkpoint; #93 must compare the requested plan with that retained record
before external execution.

## Output, Streams, and Authority

Every valid plan fixes these invariants:

- input repository read-only: true;
- repository mutation permitted: false;
- atomic output publication: true;
- existing successful output rejected: true;
- implicit network access: false;
- machine evidence authoritative: true;
- human summary is projection: true;
- machine output uses stdout only when requested; and
- diagnostics/progress use stderr.

The command planner itself writes no stream. These are obligations for the #93
executor.

## Exit Status Registry

The stable exit registry is:

- `0` success;
- `1` internal failure;
- `2` usage error;
- `3` configuration/validated-input rejection;
- `10` ingestion failure;
- `11` baseline build failure;
- `12` correctness failure;
- `13` benchmark ineligible;
- `20` analysis failure;
- `21` materialization failure;
- `22` candidate failure;
- `23` no eligible champion;
- `24` replay mismatch;
- `70` resource failure; and
- `130` interrupted.

All four commands have normative success, invalid-input, resource-failure, and
interruption mapping tests. Additional stage-specific statuses remain available
to the operations that can encounter those stages.

## Interrupt and Resource Contract

The planner does not install signal handlers. The stable terminal vocabulary
exists now so #93 can map SIGINT/SIGTERM or platform equivalents without
changing command semantics.

On interruption, the installed executor must stop new admissions, cancel and
join owned workers, preserve the input/baseline, remove incomplete output, and
only then return the interrupted status. Resource exhaustion follows the same
cleanup-before-terminal-publication rule and returns the resource status rather
than an internal error.

## Platform and Toolchain Boundary

Command-contract 0.43.0 is platform-neutral C17 and compiles through both CMake
and GNU Autotools. The first production execution provider in #114 is Linux
Bubblewrap and may be unavailable on other hosts. A command requiring an
unavailable provider rejects; platform portability never authorizes a weaker
execution path.

External Clang/LLVM, CMake, Autotools, compiler, linker, test, and workload tools
remain declared dependencies. EVO does not silently download them or enable
network access.

## Human-Readable Abstraction Contract

The registry and provider-requirement table use fixed arrays and direct scans.
They are exact authority and introduce no accelerator. EVO-HRA-016 records the
issue-specific ADR-0026 assessment.

The stable human-readable projection consists of this operation table, the
provider matrix, and the exit-status registry. A future accelerated dispatcher,
asset cache, or provider index must add an exact fallback and deterministic
projection under ADR-0026.

## Handoff

Issue #67 fixes the command/configuration boundary. It does not install a
binary and does not claim that the #114 production implementations have landed.

After #67 lands:

1. #114 can reconcile its built-in provider registry directly against the
   identities/capabilities above and close the concrete-provider milestone.
2. #93 installs the canonical executable, parses CLI arguments into these
   requests, resolves real #114 provider records, executes valid plans from an
   unrelated working directory, implements signals/streams/path safety, and
   adds staged-install tests.
3. #69 performs the retained end-to-end proof through that installed executable
   with no fake/private provider substitution.

The governed dependency remains `#67 -> #114 -> #93 -> #68 -> #69 -> #56`.
