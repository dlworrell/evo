# Repository Lifecycle Contract

Status: Adopted implementation contract

## Purpose

Every Catalyst repository that begins outside Catylist oversight shall use
Project Zero once for repository preparation and Engineering Ready review.
After certification, ordinary verification, documentation, reporting,
optimization experiments, and release preparation use their domain workflows
without rerunning Project Zero.

For repositories that require Project Zero onboarding, the initial dependency
order is:

```text
Catylist -> AES -> AEMS -> P0 -> repo_templates -> EDT / EVO / EWT -> project repositories
```

## Required workflow entry points

| Workflow | Responsibility | Mutation policy |
|---|---|---|
| `p0.yml` | Manually inspect, remediate, or reassess an onboarding baseline before certification or on explicit authority request | Remediation only through a branch and pull request |
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

## Post-certification boundary

The separately reviewed certification record is the durable evidence that
Project Zero completed. Once the AES manifest declares `ENGINEERING_READY`:

- ordinary pull requests, repository verification, compliance, and release
  readiness do not invoke Project Zero;
- Catylist, AES, AEMS, repository contracts, build parity, tests, security
  review, analyzers, and sanitizers govern ongoing work;
- the Project Zero dispatcher and engine may remain as manual historical and
  reassessment tooling; and
- Project Zero runs again only when Catylist or AEMS explicitly invalidates the
  retained certification or requests a new onboarding assessment.

A new feature, release, or product-scope change does not by itself reset an
Engineering Ready repository to Project Zero.

## Local interfaces

Repositories should expose equivalent local commands through scripts or task runners. GitHub Actions and local execution must call the same underlying implementation rather than maintaining separate policy logic.

## Extension boundary

The template workflows establish stable interfaces. Domain implementations may replace their internal commands while preserving workflow names, inputs, evidence locations, and safety guarantees.
