#!/usr/bin/env python3
"""Synchronize long-form package authorities for the EVO 0.43.0 boundary.

This is intentionally assertion-heavy: every edit names the exact 0.42.0 text
it supersedes and refuses to write if repository state has drifted. Historical
release sections remain untouched.
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_exact(text: str, old: str, new: str, label: str, count: int = 1) -> str:
    actual = text.count(old)
    if actual != count:
        raise RuntimeError(f"{label}: expected {count} occurrence(s), found {actual}")
    return text.replace(old, new, count)


def update(path: str, edits: list[tuple[str, str, str]]) -> None:
    target = ROOT / path
    original = target.read_text(encoding="utf-8")
    current = original
    for label, old, new in edits:
        current = replace_exact(current, old, new, f"{path}: {label}")
    if current == original:
        raise RuntimeError(f"{path}: no changes produced")
    target.write_text(current, encoding="utf-8")


def main() -> None:
    update(
        "README.md",
        [
            (
                "governing ADRs",
                "- `docs/adr/ADR-0043-bounded-parallel-source-orchestration.md`\n- `docs/specs/EVO-001-library-contract.md`",
                "- `docs/adr/ADR-0043-bounded-parallel-source-orchestration.md`\n- `docs/adr/ADR-0044-built-in-production-providers.md`\n- `docs/adr/ADR-0045-product-command-contract.md`\n- `docs/specs/EVO-001-library-contract.md`",
            ),
            (
                "governing specs",
                "- `docs/specs/EVO-002-source-optimizer-contract.md`\n- `docs/roadmap.md`",
                "- `docs/specs/EVO-002-source-optimizer-contract.md`\n- `docs/specs/EVO-002A-product-command-provider-addendum.md`\n- `docs/specs/EVO-003-product-command-contract.md`\n- `docs/roadmap.md`",
            ),
            (
                "native inventory counts",
                "installed-core sources, 35 private source-foundation sources, and 44\nnormative tests.",
                "installed-core sources, 36 private source-foundation sources, and 45\nnormative tests.",
            ),
            (
                "current package version",
                "**Current implementation boundary:** EVO 0.42.0 packages the deterministic",
                "**Current implementation boundary:** EVO 0.43.0 packages the deterministic",
            ),
            (
                "current command handoff",
                "boundary remains private and uninstalled. Product commands and installed\nexecutable (#67/#93), artifact publication (#68), end-to-end proof (#69), and\nfinal stabilization (#56) remain later dependency-ordered work.",
                "source-optimizer implementation remains private and uninstalled. EVO 0.43.0\nnow fixes the executable-facing `analyze`, `evolve`, `replay`, and `report`\ncommand contract in EVO-003. Concrete production providers (#114), the installed\nexecutable (#93), artifact publication (#68), end-to-end proof (#69), and final\nstabilization (#56) remain later dependency-ordered work.",
            ),
            (
                "repository layout command boundary",
                "Version 0.42.0 adds bounded external-process source orchestration, deterministic\ncandidate/workspace/logical-worker assignment, stable result commit independent\nof completion order, persistent exact worker traces, fail-closed cleanup, and\nproduct checkpoint identity with uninterrupted-versus-resumed differential\nevidence. The installed executable remains dependency-ordered roadmap work.",
                "Version 0.42.0 adds bounded external-process source orchestration, deterministic\ncandidate/workspace/logical-worker assignment, stable result commit independent\nof completion order, persistent exact worker traces, fail-closed cleanup, and\nproduct checkpoint identity with uninterrupted-versus-resumed differential\nevidence. Version 0.43.0 adds the fixed executable-facing command registry,\nrequest schemas, exact production-provider selection policy, replay/checkpoint\npreflight, stable exit-status mapping, and fail-closed execution-plan authority.\nConcrete providers and the installed executable remain dependency-ordered work.",
            ),
            (
                "command HRA",
                "EVO-HRA-015 retains this orchestration-specific audit.\n\n## Roadmap Scope",
                "EVO-HRA-015 retains this orchestration-specific audit. ADR-0045 assesses\n0.43.0 product command planning: fixed ordered command/provider registries and\ndirect scans are exact authority, and no accelerated dispatcher or cache is\nintroduced. EVO-HRA-016 retains this command-specific audit.\n\n## Roadmap Scope",
            ),
            (
                "source optimizer track command entry",
                "- Implemented bounded external-process orchestration with stable result-commit\n  order, persistent worker traces, resource/capability policy, product\n  checkpoint identity, and deterministic resume\n- Reviewable optimized patches and machine-readable evidence bundles",
                "- Implemented bounded external-process orchestration with stable result-commit\n  order, persistent worker traces, resource/capability policy, product\n  checkpoint identity, and deterministic resume\n- Implemented 0.43.0 executable-facing `analyze`, `evolve`, `replay`, and\n  `report` request schemas plus fail-closed execution-plan semantics\n- Reviewable optimized patches and machine-readable evidence bundles",
            ),
        ],
    )

    update(
        "docs/architecture.md",
        [
            (
                "0.43 source-layer boundary",
                "before resume.\n\nSource genomes never contain arbitrary C text for byte-wise mutation or",
                "before resume.\nVersion 0.43.0 adds a private executable-facing command registry and\nexecution-plan boundary for `analyze`, `evolve`, `replay`, and `report`. It\nvalidates explicit path roles, checkpoint/replay policy, stable exit classes,\nand exact production-provider identity/version/capability requirements before\nexternal execution may be authorized; it launches no target process itself.\n\nSource genomes never contain arbitrary C text for byte-wise mutation or",
            ),
            (
                "product orchestration command semantics",
                "The product layer coordinates analyze, evolve, replay, and report operations;\nmaps candidate evidence into finite EVO fitness; binds product checkpoints to\nbaseline, analysis, catalogue, toolchain, workload, and schema identities; and\nemits the selected patch, recipe, lineage, validation, measurements, and\nreplay evidence.",
                "The product layer coordinates `analyze`, `evolve`, `replay`, and `report`\noperations. Version 0.43.0 fixes their request schemas, help/path roles,\nprovider-policy requirements, replay/checkpoint preflight, output/stream\ninvariants, and terminal/exit mapping as a declarative execution plan. Later\ninstalled execution maps candidate evidence into finite EVO fitness, binds\nproduct checkpoints to baseline, analysis, provider, catalogue, toolchain,\nworkload, and schema identities, and emits the selected patch, recipe, lineage,\nvalidation, measurements, and replay evidence.",
            ),
            (
                "current conformance boundary",
                "Version 0.42.0 contains the evolutionary-search core, its bounded reference\nconsumers, private project ingestion, a private normalized analysis and hotspot\nmodel, a private canonical transformation-recipe model, a private initial AST-\naware C transformation catalogue, private isolated candidate materialization,\nprivate candidate assurance, private reproducible candidate measurement with\nfinite fitness mapping, private deterministic structured recipe search, and\nprivate bounded external-process source orchestration. Candidate process\nexecution and target workload sampling remain owned by caller-supplied providers;\nEVO validates declared policy, condition identity, ordered outcomes, cleanup, and\nexact evidence rather than pretending to provide a portable OS sandbox or\nuniversal timing environment. Product commands, the installed standalone\nexecutable, and final optimized-patch artifacts remain planned by issues #67\nthrough #69 and #93. Documentation of those planned boundaries is not an\nimplementation claim.",
                "Version 0.43.0 contains the evolutionary-search core, its bounded reference\nconsumers, private project ingestion, normalized analysis/hotspot evidence,\ncanonical transformation recipes, the initial AST-aware C transformation\ncatalogue, isolated candidate materialization, candidate assurance, reproducible\ncandidate measurement with finite fitness mapping, deterministic structured\nrecipe search, bounded external-process source orchestration, and the private\nexecutable-facing product command/execution-plan contract. Historical 0.34.0-\n0.42.0 callback seams remain private foundation mechanisms; EVO-002A and\nEVO-003 define the 0.43.0 standalone provider-selection authority. Concrete\nproduction providers remain owned by #114 and installed execution by #93.\nOptimized-patch publication and end-to-end proof remain #68/#69 work; the\n0.43.0 command contract itself launches no target process and makes no installed\napplication claim.",
            ),
            (
                "0.43 HRA",
                "EVO-HRA-015 audits this 0.42.0 boundary. The current implementation\ntherefore has no opaque accelerated authority requiring remediation.",
                "EVO-HRA-015 audits this 0.42.0 boundary. The 0.43.0 product command\nregistry and provider-requirement table are likewise fixed ordered arrays with\ndirect scans and no accelerated dispatcher, cache, or probabilistic authority;\nEVO-HRA-016 audits this command boundary. The current implementation therefore\nhas no opaque accelerated authority requiring remediation.",
            ),
        ],
    )

    update(
        "docs/algorithms.md",
        [
            (
                "package version",
                "Version 0.42.0 implements",
                "Version 0.43.0 implements",
            ),
            (
                "next boundary",
                "Product command orchestration is the next boundary.",
                "Version 0.43.0 adds the declarative `analyze`, `evolve`, `replay`, and\n`report` registry plus fail-closed execution-plan validation. Concrete\nproduction providers and installed command execution are the next boundary.",
            ),
        ],
    )

    update(
        "docs/specs/EVO-001-library-contract.md",
        [
            ("spec version", "Version: 0.42.0", "Version: 0.43.0"),
            (
                "scope boundary",
                "core packaged through version 0.42.0. Version 0.42.0 changes no installed\ncore semantics; its new implementation is confined to the private source-\noptimizer bounded external-process orchestration foundation.",
                "core packaged through version 0.43.0. Version 0.43.0 changes no installed\ncore semantics or public 0.37.0 compatibility surface; its new implementation\nis confined to the private source-optimizer executable-facing command and\nexecution-plan foundation.",
            ),
            (
                "product responsibility reference",
                "draft 1.0 target in `EVO-002-source-optimizer-contract.md`.",
                "draft 1.0 target in `EVO-002-source-optimizer-contract.md`, its EVO-002A\nprovider addendum, and `EVO-003-product-command-contract.md`.",
            ),
        ],
    )

    update(
        "docs/specs/EVO-002-source-optimizer-contract.md",
        [
            (
                "status",
                "Status: Implemented through the 0.42.0 bounded source-orchestration boundary; draft 1.0 target",
                "Status: Implemented through the 0.43.0 product command-contract boundary; draft 1.0 target",
            ),
            ("spec version", "Version: 0.42.0", "Version: 0.43.0"),
            (
                "governing ADRs",
                "ADR-0041, ADR-0042, and ADR-0043",
                "ADR-0041, ADR-0042, ADR-0043, ADR-0044, and ADR-0045",
            ),
            (
                "implementation claim",
                "the complete source optimizer is implemented in version 0.42.0. This release",
                "the complete source optimizer is implemented in version 0.43.0. This release",
            ),
            (
                "0.43 command boundary",
                "Assurance, measurement, and candidate execution consume exact results from\ncaller-supplied providers; portable OS sandboxing and target workload execution\nremain provider responsibilities. Product commands, installed application\ndelivery, artifact publication, and later roadmap boundaries remain 1.0 targets.",
                "The 0.34.0-0.42.0 foundation continues to expose private caller-supplied\nprovider seams for deterministic testing and internal composition. Version\n0.43.0 adds the executable-facing `analyze`, `evolve`, `replay`, and `report`\nrequest/execution-plan contract. EVO-002A and EVO-003 govern the standalone\nprovider interpretation: exact production provider identity/version/capability\npolicy must pass before external execution is authorized. Concrete providers,\ninstalled application delivery, artifact publication, and later roadmap\nboundaries remain #114/#93/#68/#69 work.",
            ),
        ],
    )


if __name__ == "__main__":
    main()
