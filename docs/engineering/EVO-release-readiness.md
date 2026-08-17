# EVO Release Readiness Authority

Status: Adopted for the current pre-1.0 release-readiness workflow  
Owner: EVO  
Workflow: `.github/workflows/release.yml`  
Collector: `scripts/release_readiness.py`

## Purpose

Release Readiness is an evidence aggregation boundary. It does not build an
independent claim from a generic smoke test and it does not create a tag,
publish a GitHub release, deploy software, or mutate a downstream repository.

A readiness result is valid only for the exact commit recorded in the retained
manifest. `verification: passed` means that every gate required by the selected
release type has a completed successful GitHub Actions workflow run whose
`head_sha` is exactly that commit and whose workflow definition is identifiable
at that commit.

The workflow-run records remain authority. `manifest.json`, `report.md`, and
the GitHub step summary are deterministic audit projections of that authority;
they do not become a second source of truth.

## Release Types

Candidate and production currently use the same verification matrix. This is
intentional while EVO remains pre-1.0: production classification does not yet
add tagging or publication behavior. Issue #56 owns final 1.0 stabilization and
may add separately reviewed release/publishing boundaries later.

Documentation-only readiness is deliberately narrower because it does not
claim that an executable/library release is ready. It still requires the
security-governance, repository-compliance, and documentation authorities for
the exact commit.

## Gate Matrix

| Gate | Workflow | Candidate | Production | Documentation |
|---|---|:---:|:---:|:---:|
| Repository verification | `.github/workflows/verify.yml` | required | required | not applicable |
| Cross-platform CI | `.github/workflows/ci.yml` | required | required | not applicable |
| AES-BLD-001 native build parity | `.github/workflows/aes-bld-001.yml` | required | required | not applicable |
| AES-SEC-001 governance | `.github/workflows/aes-sec-001-governance.yml` | required | required | required |
| Sanitizers | `.github/workflows/sanitizers.yml` | required | required | not applicable |
| Code quality/static analysis | `.github/workflows/quality.yml` | required | required | not applicable |
| Repository compliance | `.github/workflows/compliance.yml` | required | required | required |
| Documentation report | `.github/workflows/documentation.yml` | required | required | required |
| Core benchmark evidence | `.github/workflows/benchmarks.yml` | required | required | not applicable |
| Reference adapters | `.github/workflows/reference-adapters.yml` | required | required | not applicable |
| Installed core version parity | `.github/workflows/version-parity.yml` | required | required | not applicable |
| Project ingestion | `.github/workflows/project-ingestion.yml` | required | required | not applicable |
| Project analysis | `.github/workflows/project-analysis.yml` | required | required | not applicable |
| Project recipe | `.github/workflows/project-recipe.yml` | required | required | not applicable |
| Project transformation | `.github/workflows/project-transformation.yml` | required | required | not applicable |
| Project candidate | `.github/workflows/project-candidate.yml` | required | required | not applicable |
| Project assurance | `.github/workflows/project-assurance.yml` | required | required | not applicable |
| Project measurement | `.github/workflows/project-measurement.yml` | required | required | not applicable |
| Project search | `.github/workflows/project-search.yml` | required | required | not applicable |
| Project orchestration | `.github/workflows/project-orchestration.yml` | required | required | not applicable |
| Project Zero | `.github/workflows/project-zero.yml` | not applicable | not applicable | not applicable |

Project Zero is explicitly not a routine release gate. EVO-P0-002 is retained
onboarding evidence after the Engineering Ready transition; normal release
readiness remains governed by current AES/AEMS, build, security, quality,
repository, and product-stage controls.

`EVO Optimization Experiment` is not a release gate. It is an explicitly
requested experiment surface and may execute an optional candidate measurement;
its existence or outcome does not authorize a release-readiness pass.

## Exact-Commit Authority

The collector queries GitHub Actions workflow runs using the release
`GITHUB_SHA` as `head_sha`. For each gate it matches the exact workflow path.
If more than one run exists for the same workflow and commit, the newest run is
selected deterministically by workflow run number, run attempt, and run ID.

This rule is intentionally conservative: a newer failed rerun cannot be hidden
by an older successful run for the same commit.

The checked-out repository must also have `HEAD` equal to the requested release
commit. The collector resolves each workflow file's Git blob SHA from that
commit. A required gate therefore retains both:

- the workflow definition identity at the release commit; and
- the GitHub Actions run identity and result for that exact commit.

For workflows that call reusable workflows, GitHub's resolved
`referenced_workflows` records are copied into the manifest, including the
resolved upstream workflow SHA. This preserves the concrete AEMS revision used
by AES-BLD-001/AES-SEC-001 even when the EVO workflow source references an
upstream branch name.

## Gate Status Semantics

Every catalogued gate receives one of these readiness statuses:

- `passed`: the gate is required and its authoritative run completed with
  `conclusion: success` for the exact release commit;
- `not-applicable`: the gate is defined but is not required for the selected
  release type;
- `skipped`: the required authoritative workflow run concluded `skipped`;
- `missing`: no exact-commit run exists for a required workflow, or its workflow
  identity cannot be established when no run exists; or
- `failed`: the required run failed, was cancelled/timed out/stale, is still in
  progress, refers to another commit, or otherwise cannot be verified as a
  completed success.

Only `passed` satisfies a required gate. A readiness manifest can contain
`verification: passed` only when `required == required_passed` across the
complete selected matrix.

API/collector errors are themselves fail-closed. The workflow retains a failed
manifest when possible and exits nonzero.

## Main-Branch Evidence Coverage

A required candidate/production gate must be able to produce evidence for every
`main` commit. PR path filters may remain in place to reduce review-time work,
but the corresponding `push` authority for a required release gate must not be
restricted to a historical development branch or to a subset of paths.

This rule applies to repository compliance, documentation, project assurance,
project measurement, project search, and project orchestration as well as the
already-unconditional main-branch gates.

A release dispatch performed before the required main-branch workflows have
completed will fail readiness. The correct operation is to wait for the same
commit's normal gate matrix to complete and then dispatch Release Readiness;
the readiness workflow does not acquire write permission merely to start or
repair missing evidence.

## Retained Manifest

`catalyst.release-readiness.v2` records at least:

- repository, exact commit, proposed version, and release type;
- overall verification state and per-status counts;
- every catalogued gate and whether it is required;
- gate status and explanatory reason;
- workflow path and exact Git blob SHA;
- GitHub workflow ID, run ID, run number, run attempt, event, raw status and
  conclusion, exact `head_sha`, timestamps, check-suite identity, and evidence
  URL when a run exists;
- resolved reusable-workflow path/ref/SHA identities reported by GitHub; and
- explicit `tag_created: false` and `release_published: false` fields.

The Markdown report intentionally contains less detail. Reviewers must use the
machine-readable manifest and linked workflow runs for exact evidence.

## Read-Only Boundary

Release Readiness requires only:

- `contents: read`, to check out and identify workflow definitions; and
- `actions: read`, to inspect workflow-run authority.

It does not request `contents: write`, `actions: write`, package publication,
deployment, or release permissions. A later workflow that creates tags,
publishes releases, uploads packages, or deploys artifacts is a separate
security/release boundary and must not inherit a readiness pass as implicit
write authorization.

## Change Control

A new or renamed workflow that becomes release-relevant requires all of the
following in the same reviewed change series:

1. update the gate catalog in `scripts/release_readiness.py`;
2. update the matrix in this document;
3. ensure required candidate/production gates run on every `main` commit;
4. add or update collector tests for release-type/status semantics; and
5. preserve the rule that a human-readable summary is a projection of exact
   workflow-run authority rather than independent authority.

Removing a gate, weakening it from required to not-applicable, changing the
latest-run selection rule, or broadening the release workflow's permissions is
a release-governance change and requires explicit review.
