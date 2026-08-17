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
    "docs/adr/ADR-0044-built-in-production-providers.md",
    '''EVO 0.42.0 has deterministic source-optimizer contracts for project analysis,
AST-aware transformation, baseline and candidate execution, performance
measurement, structured search, and bounded external-process orchestration.
Those contracts intentionally accept caller-supplied callbacks. That was the
correct boundary while the source optimizer remained an uninstalled foundation,
but it is not a sufficient product architecture for the standalone executable
owned by issue #93. An installed EVO binary cannot require a consumer to write a
custom embedding application merely to provide Clang analysis, execute declared
build/test commands, benchmark a candidate, or satisfy the asynchronous
start/poll/cancel/join contract.

The existing callbacks are still useful internal seams and test oracles. They
must not, however, become the only implementation of product-critical work.
EVO also must not freeze a public third-party provider ABI before the product
command and configuration contracts in issue #67 stabilize.

Issue #114 therefore needs a concrete provider delivery model that preserves the
0.42 contracts, supplies a supported production path, binds provider identity
into replay/checkpoint authority, and fails closed on platforms where required
isolation cannot be provided.
''',
    '''EVO 0.43.0 now layers the landed EVO-003/ADR-0045 product command contract
on top of the deterministic 0.42 source-optimizer foundation for project
analysis, AST-aware transformation, baseline and candidate execution,
performance measurement, structured search, and bounded external-process
orchestration. The historical foundation contracts intentionally accept
caller-supplied callbacks. That remains appropriate for private embedding and
test seams, but it is not a sufficient product architecture for the standalone
executable owned by issue #93. An installed EVO binary cannot require a
consumer to write a custom embedding application merely to provide Clang
analysis, execute declared build/test commands, benchmark a candidate, or
satisfy the asynchronous start/poll/cancel/join contract.

The existing callbacks remain useful internal seams and test oracles. They do
not become standalone product authority. Issue #67 is complete: EVO-003 fixes
the executable-facing provider identities, versions, capability policy, and
fail-closed selection semantics that this ADR's implementations must satisfy.
That command contract still does not define or freeze a public third-party
provider plug-in ABI.

Issue #114 therefore supplies the concrete provider delivery model that
preserves the 0.42 foundation contracts while satisfying the landed 0.43
product command contract, binds provider identity into replay/checkpoint
authority, and fails closed on platforms where required isolation cannot be
provided.
''',
)

replace_once(
    "docs/adr/ADR-0044-built-in-production-providers.md",
    '''10. Issue #67 owns the versioned analyze/evolve/replay/report command contracts
    and selects only provider identities from this production registry. Issue
    #114 owns the provider implementations and registry. Issue #93 owns
    installation and proves that the installed executable can resolve and use
    the supported providers from an unrelated working directory. The dependency
    order is therefore `#67 -> #114 -> #93`; #114 does not install the final CLI.
''',
    '''10. Issue #67 is complete and EVO-003 now owns the landed versioned
    analyze/evolve/replay/report command contract. It selects only provider
    identities from this production registry. Issue #114 owns the provider
    implementations and registry. Issue #93 owns installation and proves that
    the installed executable can resolve and use the supported providers from an
    unrelated working directory. The dependency order remains
    `#67 -> #114 -> #93`; #114 does not install the final CLI.
''',
)

replace_once(
    "docs/adr/ADR-0044-built-in-production-providers.md",
    '''- **Freeze a public C plug-in ABI now** was rejected because #67 has not yet
  fixed the complete product command/configuration surface or long-term provider
  compatibility policy.
''',
    '''- **Freeze a public C plug-in ABI now** remains rejected because EVO-003 fixes
  the built-in standalone provider selection contract, not a third-party plug-in
  compatibility, trust, lifecycle, or security ABI. Any public provider ABI is a
  separate future compatibility decision.
''',
)

replace_once(
    "docs/adr/ADR-0044-built-in-production-providers.md",
    '''Hosted validation must retain the existing fake-provider unit tests and add a
separate Linux real-provider job using supported Clang/Bubblewrap packages. Both
CMake/Clang and Autotools/GNU builds must compile the production provider code.
''',
    '''Hosted validation retains the existing fake-provider unit tests and adds a
separate Linux real-provider job using supported Clang/Bubblewrap packages. Both
CMake/Clang and Autotools/GNU builds compile the production provider code. The
Production Providers and Production Provider Async Lifecycle workflows are also
required candidate/production Release Readiness authorities for the exact
release commit.
''',
)

subprocess.run(["git", "diff", "--check"], check=True)
for helper in (
    Path(".github/workflows/issue-114-records.yml"),
    Path(".github/scripts/issue-114-records.py"),
):
    helper.unlink()
subprocess.run(["git", "config", "user.name", "github-actions[bot]"], check=True)
subprocess.run(
    ["git", "config", "user.email", "41898282+github-actions[bot]@users.noreply.github.com"],
    check=True,
)
subprocess.run(["git", "add", "-A"], check=True)
subprocess.run(["git", "diff", "--cached", "--check"], check=True)
subprocess.run(["git", "commit", "-m", "Reconcile provider ADR with v0.43 commands"], check=True)
subprocess.run(["git", "push", "origin", f"HEAD:{BRANCH}"], check=True)
