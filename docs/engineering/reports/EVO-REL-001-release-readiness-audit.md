# EVO-REL-001: Release Readiness Evidence Reconciliation

Status: proposed by issue #115 remediation  
Scope: `.github/workflows/release.yml` and exact-SHA gate authority

## Finding Reconciled

The prior Release Readiness workflow emitted `verification: passed` after only
the reusable repository verification job. That statement did not prove the
repository's AES-BLD-001, AES-SEC-001, sanitizer, code-quality, compliance,
documentation, consumer-parity, benchmark, or source-optimizer stage controls
for the same commit.

## Corrected Authority

`scripts/release_readiness.py` defines an explicit release-type gate catalog
and derives readiness from GitHub Actions workflow runs whose `head_sha` equals
the requested release commit. Required candidate/production gates cover the
repository/AES authorities, reusable-core release evidence, and the implemented
0.34-0.42 source-optimizer stage workflows. Documentation-only readiness uses
the narrower AES-SEC-001, repository-compliance, and documentation set.

The retained v2 manifest records workflow definition blob identity, workflow
and run IDs, run attempt, event, raw status/conclusion, exact head SHA,
timestamps, check-suite identity, evidence URL, and resolved reusable-workflow
SHAs when GitHub supplies them.

Missing, skipped, incomplete, wrong-commit, failed, cancelled, timed-out, or
otherwise unverifiable required evidence fails readiness. Only a completed
successful required run can contribute `passed`.

## Main-Branch Coverage

Required release gates must produce evidence for every `main` commit. The
remediation therefore removes historical branch/path restrictions from the
`push` triggers of Project Assurance, Project Measurement, Project Search, and
Project Orchestration, and adds unconditional main-push coverage to Repository
Compliance and Documentation Report. PR path filters remain available where
already defined.

## Read-Only Boundary

Release Readiness uses only `contents: read` and `actions: read`. It does not
start missing workflows, create tags, publish releases, upload packages, deploy
artifacts, or mutate downstream repositories.

## Human-Readable Abstraction

The release manifest and Markdown table are audit projections. Exact workflow
runs and workflow-definition identities remain authority. The projection may
not convert missing or failed evidence into success and may not hide a newer
failed run behind an older successful run for the same workflow and commit.

## Validation

The change series includes unit coverage for release-type gate membership,
latest-run authority, missing/skipped distinction, wrong-SHA rejection,
incomplete-run rejection, Project Zero non-applicability, reusable-workflow
identity retention, and required-gate summary behavior. Hosted repository and
AES/AEMS checks provide merge-time validation.
