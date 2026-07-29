# Repository Lifecycle Contract

Status: Draft implementation contract

## Purpose

Every Catalyst repository shall expose stable automation entry points for repository preparation, verification, documentation and reporting, optimization experiments, and release preparation.

The canonical dependency order is:

```text
Catylist -> AES -> AEMS -> P0 -> repo_templates -> EDT / EVO / EWT -> project repositories
```

## Required workflow entry points

| Workflow | Responsibility | Mutation policy |
|---|---|---|
| `p0.yml` | Inspect, plan, remediate, verify, or assess readiness | Remediation only through a branch and pull request |
| `verify.yml` | Execute repository verification and preserve evidence | Read-only |
| `documentation.yml` | Inventory documentation and produce a report package for EDT | Read-only |
| `compliance.yml` | Verify baseline repository and AES-adoption obligations | Read-only |
| `optimization.yml` | Define and record EVO experiments and candidate measurements | Read-only; no production mutation |
| `release.yml` | Prepare and validate a release evidence package | Read-only unless a later approved release implementation is installed |

Each workflow shall:

1. support `workflow_dispatch` for mobile operation;
2. publish a useful GitHub step summary;
3. produce machine-readable JSON and human-readable Markdown evidence;
4. upload its evidence even when validation fails;
5. avoid direct writes to the default branch;
6. use deterministic inputs and identify the commit under evaluation.

## Local interfaces

Repositories should expose equivalent local commands through scripts or task runners. GitHub Actions and local execution must call the same underlying implementation rather than maintaining separate policy logic.

## Extension boundary

The template workflows establish stable interfaces. Domain implementations may replace their internal commands while preserving workflow names, inputs, evidence locations, and safety guarantees.
