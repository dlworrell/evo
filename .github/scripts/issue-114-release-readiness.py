#!/usr/bin/env python3
from pathlib import Path
import subprocess

BRANCH = "agent/issue-114-production-providers"


def replace_once(path, old, new):
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one match, found {count}: {old!r}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


replace_once(
    "scripts/release_readiness.py",
    '''    Gate("project-orchestration", "Project Orchestration",
         ".github/workflows/project-orchestration.yml", PRODUCT_RELEASES, "source-optimizer"),
    Gate(
        "project-zero",
''',
    '''    Gate("project-orchestration", "Project Orchestration",
         ".github/workflows/project-orchestration.yml", PRODUCT_RELEASES, "source-optimizer"),
    Gate("production-providers", "Production Providers",
         ".github/workflows/production-providers.yml", PRODUCT_RELEASES, "source-optimizer"),
    Gate("production-provider-async", "Production Provider Async Lifecycle",
         ".github/workflows/production-provider-async.yml", PRODUCT_RELEASES, "source-optimizer"),
    Gate(
        "project-zero",
''',
)

replace_once(
    "tests/test_release_readiness.py",
    '''        self.assertEqual(candidate, production)
        self.assertEqual(len(candidate), 20)
        self.assertEqual(
''',
    '''        self.assertEqual(candidate, production)
        self.assertEqual(len(candidate), 22)
        self.assertTrue(
            {"production-providers", "production-provider-async"}.issubset(candidate)
        )
        self.assertEqual(
''',
)

replace_once(
    "docs/engineering/EVO-release-readiness.md",
    '''| Project orchestration | `.github/workflows/project-orchestration.yml` | required | required | not applicable |
| Project Zero | `.github/workflows/project-zero.yml` | not applicable | not applicable | not applicable |
''',
    '''| Project orchestration | `.github/workflows/project-orchestration.yml` | required | required | not applicable |
| Production providers | `.github/workflows/production-providers.yml` | required | required | not applicable |
| Production provider async lifecycle | `.github/workflows/production-provider-async.yml` | required | required | not applicable |
| Project Zero | `.github/workflows/project-zero.yml` | not applicable | not applicable | not applicable |
''',
)
replace_once(
    "docs/engineering/EVO-release-readiness.md",
    '''This rule applies to repository compliance, documentation, project assurance,
project measurement, project search, and project orchestration as well as the
already-unconditional main-branch gates.
''',
    '''This rule applies to repository compliance, documentation, project assurance,
project measurement, project search, project orchestration, production providers,
and the production-provider async lifecycle as well as the already-unconditional
main-branch gates.
''',
)

replace_once(
    "docs/engineering/reports/EVO-REL-001-release-readiness-audit.md",
    '''repository/AES authorities, reusable-core release evidence, and the implemented
0.34-0.42 source-optimizer stage workflows. Documentation-only readiness uses
the narrower AES-SEC-001, repository-compliance, and documentation set.
''',
    '''repository/AES authorities, reusable-core release evidence, and the implemented
0.34-0.43 source-optimizer stage workflows, including the production-provider and
production-provider async-lifecycle authorities. Documentation-only readiness
uses the narrower AES-SEC-001, repository-compliance, and documentation set.
''',
)
replace_once(
    "docs/engineering/reports/EVO-REL-001-release-readiness-audit.md",
    '''Required release gates must produce evidence for every `main` commit. The
remediation therefore removes historical branch/path restrictions from the
`push` triggers of Project Assurance, Project Measurement, Project Search, and
Project Orchestration, and adds unconditional main-push coverage to Repository
Compliance and Documentation Report. PR path filters remain available where
already defined.
''',
    '''Required release gates must produce evidence for every `main` commit. The
remediation therefore removes historical branch/path restrictions from the
`push` triggers of Project Assurance, Project Measurement, Project Search, and
Project Orchestration, adds unconditional main-push coverage to Repository
Compliance and Documentation Report, and requires the production-provider and
async-lifecycle workflows to retain unconditional `main` coverage. PR path
filters remain available where already defined.
''',
)

subprocess.run(["python3", "-m", "unittest", "tests.test_release_readiness"], check=True)
subprocess.run(["git", "diff", "--check"], check=True)

for helper in (
    Path(".github/workflows/issue-114-release-readiness.yml"),
    Path(".github/scripts/issue-114-release-readiness.py"),
):
    helper.unlink()
subprocess.run(["git", "config", "user.name", "github-actions[bot]"], check=True)
subprocess.run(
    ["git", "config", "user.email", "41898282+github-actions[bot]@users.noreply.github.com"],
    check=True,
)
subprocess.run(["git", "add", "-A"], check=True)
subprocess.run(["git", "diff", "--cached", "--check"], check=True)
subprocess.run(
    ["git", "commit", "-m", "Require production provider release gates"],
    check=True,
)
subprocess.run(["git", "push", "origin", f"HEAD:{BRANCH}"], check=True)
