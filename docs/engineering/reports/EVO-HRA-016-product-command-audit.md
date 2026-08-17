# EVO-HRA-016: Product Command Contract Audit

## Scope

Issue #67 / EVO 0.43.0 executable-facing `analyze`, `evolve`, `replay`, and
`report` contract.

## Reference Authority

The command registry and provider-requirement registry are fixed ordered arrays
in `src/project_command.c`. Direct indexed access and direct string comparison
are the exact reference semantics. The command plan retains the logical command
name/schema, path roles, exact provider policy, exact provider identities and
versions, required capability masks, checkpoint mode, and security/output
invariants.

## Accelerated Structures

None.

This change introduces no hash table, cache, compressed index, membership
filter, generated dispatcher, probabilistic precheck, or other accelerated
authority. Runtime process/provider implementations are outside this issue and
are governed by #114.

## Human-Readable Projection

`docs/specs/EVO-003-product-command-contract.md` exposes, in stable domain
terms:

- the four-command registry and schema identities;
- path and checkpoint roles;
- the command/provider matrix;
- the provider capability bit vocabulary;
- the stable exit-status registry;
- stream/output/repository-mutation invariants; and
- the #114/#93/#69 handoff.

The projection is complete for the 0.43.0 command contract. It contains no
independent acceptance authority; executable behavior is tested against the C
registry and planner.

## Differential Requirement

Not applicable for this change because no accelerated representation exists.
Any later accelerated command/provider lookup or runtime-asset cache must add an
explicit reference path, deterministic audit projection, invalidation/corrupt
behavior, resource bounds, and differential equivalence testing under
ADR-0026.

## Fail-Closed Evidence

`tests/project_command_test.c` verifies that wrong provider identity, wrong
implementation version, unavailable providers, missing capability bits, and a
wrong provider-policy identity reject before `execution_permitted` can become
true. Replay rejects incomplete recorded identity/external-input declarations.
Existing-output overwrite requests and invalid checkpoint/resume combinations
also reject.

## Conclusion

EVO 0.43.0 satisfies the issue-specific Human-Readable Abstraction requirement
without introducing an accelerated authority. EVO-003 is a deterministic
projection of the command/provider/exit registries; canonical machine evidence
remains authority for later execution and reporting.
