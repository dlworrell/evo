# Catylist #27: verified Project Zero remediation commits

Observed defect at EVO `8fa3b5fdd2db55050f28f3b0189d9994f61761c4`:
`.github/workflows/project-zero.yml` creates a local unsigned bot commit and
pushes a remediation branch. Such a PR cannot satisfy ADR-0006 verified-commit
protection. This workflow is retained for manual reassessment; ordinary EVO
onboarding remains complete and is not rerun by this change.

The workflow now uses the canonical `tools/project_zero_commit.py` helper
from `dlworrell/repo_templates@f9e3f20efdf47daf7536759c3028acda38496303`
(project_zero template 0.2.0). The copied helper and its 15 tests are unchanged.
GitHub's `createCommitOnBranch` creates the verified remediation commit. The
helper rejects the base/default branch and `pull_request_target`, checks the
base against the inspected checkout, uses `expectedHeadOid`, and handles no-op
remediation without opening an empty PR. Checkout credentials are not retained.
No signing keys, new credentials or additional permissions are introduced.

The self-test runs the helper tests and rejects a regression to local commit/
push commands or a privileged PR-head trigger. The template catalog includes
the helper so later copies retain this behavior. The workflow inputs, scope of
safe remediation and manual-only entry points are unchanged. No accelerated or
compressed structure is introduced; ADR-0026's additional projection contract
is not applicable.

Validation: all 15 canonical helper tests pass locally, including branch/CAS
refusals, exact payload construction, additions/deletions and no-op behavior;
Python compilation and whitespace checks pass. Workflow self-tests validate the
integration. No live remediation mutation is invoked merely to manufacture
rollout evidence. AI-assisted implementation and self-review; no independent
human review or wider P0/production certification is claimed.
