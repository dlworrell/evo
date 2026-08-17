#!/usr/bin/env python3
from pathlib import Path


def read(path):
    return Path(path).read_text(encoding="utf-8")


def write(path, text):
    Path(path).write_text(text, encoding="utf-8")


def replace_exact(path, old, new, expected=1):
    text = read(path)
    count = text.count(old)
    if count != expected:
        raise SystemExit(
            f"{path}: expected {expected} occurrence(s), found {count}: {old!r}"
        )
    write(path, text.replace(old, new, expected))


def replace_section(path, start, end, replacement):
    text = read(path)
    if text.count(start) != 1:
        raise SystemExit(f"{path}: start marker is not unique: {start!r}")
    start_index = text.index(start)
    end_index = text.index(end, start_index + len(start))
    write(path, text[:start_index] + replacement + text[end_index:])


# README package authority and product boundary.
replace_exact(
    "README.md",
    "- `docs/adr/ADR-0043-bounded-parallel-source-orchestration.md`\n"
    "- `docs/specs/EVO-001-library-contract.md`\n"
    "- `docs/specs/EVO-002-source-optimizer-contract.md`",
    "- `docs/adr/ADR-0043-bounded-parallel-source-orchestration.md`\n"
    "- `docs/adr/ADR-0045-product-command-contract.md`\n"
    "- `docs/specs/EVO-001-library-contract.md`\n"
    "- `docs/specs/EVO-002-source-optimizer-contract.md`\n"
    "- `docs/specs/EVO-002A-product-command-provider-authority.md`\n"
    "- `docs/specs/EVO-003-product-command-contract.md`",
)
replace_exact(
    "README.md",
    "EVO-HRA-015 retains this orchestration-specific audit.\n\n## Roadmap Scope",
    "EVO-HRA-015 retains this orchestration-specific audit.\n"
    "ADR-0045 assesses the 0.43.0 product command contract: fixed ordered command\n"
    "and provider-requirement registries plus direct validation are exact authority;\n"
    "no cache, generated dispatcher, compressed lookup, or probabilistic precheck\n"
    "participates. EVO-003 is the complete executable-facing audit projection and\n"
    "EVO-HRA-016 retains this command-specific assessment.\n\n## Roadmap Scope",
)
replace_exact(
    "README.md",
    "- Implemented bounded external-process orchestration with stable result-commit\n"
    "  order, persistent worker traces, resource/capability policy, product\n"
    "  checkpoint identity, and deterministic resume\n"
    "- Reviewable optimized patches and machine-readable evidence bundles",
    "- Implemented bounded external-process orchestration with stable result-commit\n"
    "  order, persistent worker traces, resource/capability policy, product\n"
    "  checkpoint identity, and deterministic resume\n"
    "- Implemented versioned `analyze`, `evolve`, `replay`, and `report` command\n"
    "  contract with stable production-provider identity/capability selection,\n"
    "  replay/checkpoint binding, fail-closed admission, and read-only input policy\n"
    "- Reviewable optimized patches and machine-readable evidence bundles",
)
replace_exact(
    "README.md",
    "Version 0.42.0 adds bounded external-process source orchestration, deterministic\n"
    "candidate/workspace/logical-worker assignment, stable result commit independent\n"
    "of completion order, persistent exact worker traces, fail-closed cleanup, and\n"
    "product checkpoint identity with uninterrupted-versus-resumed differential\n"
    "evidence. The installed executable remains dependency-ordered roadmap work.",
    "Version 0.42.0 adds bounded external-process source orchestration, deterministic\n"
    "candidate/workspace/logical-worker assignment, stable result commit independent\n"
    "of completion order, persistent exact worker traces, fail-closed cleanup, and\n"
    "product checkpoint identity with uninterrupted-versus-resumed differential\n"
    "evidence. Version 0.43.0 adds the executable-facing product command contract:\n"
    "versioned analyze/evolve/replay/report schemas, stable exit classes, explicit\n"
    "path and stream roles, exact production-provider identity/version/capability\n"
    "admission, and replay/checkpoint binding before external execution. The\n"
    "installed executable remains dependency-ordered roadmap work after #114.",
)
replace_exact(
    "README.md",
    "installed-core sources, 35 private source-foundation sources, and 44\n"
    "normative tests.",
    "installed-core sources, 36 private source-foundation sources, and 45\n"
    "normative tests.",
)
replace_section(
    "README.md",
    "## Status\n\n",
    "EVO 0.16.0 composes",
    "## Status\n\n"
    "**Current implementation boundary:** EVO 0.43.0 packages the deterministic\n"
    "evolutionary-search core plus the private source-optimizer foundations for\n"
    "strict C-project ingestion, immutable baselines, normalized Clang/LLVM\n"
    "analysis, canonical transformation recipes, three AST-aware C\n"
    "transformations, isolated candidate materialization, candidate assurance,\n"
    "reproducible candidate performance measurement, structured recipe search,\n"
    "bounded external-process source orchestration, and the executable-facing\n"
    "product command contract.\n\n"
    "The command layer exposes the versioned `analyze`, `evolve`, `replay`, and\n"
    "`report` contract as deterministic execution plans. It binds stable production\n"
    "provider identities, implementation versions, capability-policy identity,\n"
    "explicit path/stream roles, checkpoint/replay requirements, and fail-closed\n"
    "security/output invariants before external execution may be authorized. The\n"
    "planner itself launches no target process and never mutates the input\n"
    "repository. Concrete production-provider implementations remain #114 and the\n"
    "installed standalone executable remains #93; optimized artifact publication\n"
    "(#68), end-to-end proof (#69), and final stabilization (#56) follow those\n"
    "dependency-ordered boundaries.\n\n",
)

# Architecture current package and command boundary.
replace_exact(
    "docs/architecture.md",
    "before resume.\n\nSource genomes never",
    "before resume.\n"
    "Version 0.43.0 adds the executable-facing product command contract as a fixed\n"
    "ordered registry plus deterministic execution-plan validation. The command\n"
    "layer binds explicit path roles, checkpoint/replay semantics, stable exit\n"
    "classes, and the exact production provider identity/version/capability policy\n"
    "that #114 must satisfy before #93 may execute target-project work. It launches\n"
    "no target process itself and preserves the immutable input boundary.\n\n"
    "Source genomes never",
)
replace_section(
    "docs/architecture.md",
    "## Current Conformance Boundary\n\n",
    "The 0.37.0 package's core",
    "## Current Conformance Boundary\n\n"
    "Version 0.43.0 contains the evolutionary-search core, its bounded reference\n"
    "consumers, private project ingestion, normalized analysis and hotspot model,\n"
    "canonical transformation recipes, the initial AST-aware C transformation\n"
    "catalogue, isolated candidate materialization, candidate assurance,\n"
    "reproducible candidate measurement with finite fitness mapping, deterministic\n"
    "structured recipe search, bounded external-process source orchestration, and\n"
    "the executable-facing product command contract.\n\n"
    "The 0.43 command layer is declarative authority: it validates the complete\n"
    "operation request and the exact production-provider selection required for\n"
    "external execution, but does not itself provide a sandbox or run target code.\n"
    "The concrete providers remain issue #114 and the installed standalone\n"
    "application remains issue #93. Product patch/evidence artifacts remain #68.\n\n",
)
replace_exact(
    "docs/architecture.md",
    "EVO-HRA-015 audits this 0.42.0 boundary. The current implementation\n"
    "therefore has no opaque accelerated authority requiring remediation.",
    "EVO-HRA-015 audits this 0.42.0 boundary. The 0.43.0 command registry\n"
    "and provider requirement registry are fixed ordered arrays with direct scans;\n"
    "EVO-HRA-016 audits that command boundary and confirms that no accelerated or\n"
    "probabilistic authority participates. The current implementation therefore has\n"
    "no opaque accelerated authority requiring remediation.",
)

# Algorithms package introduction.
replace_section(
    "docs/algorithms.md",
    "This document distinguishes algorithms implemented by the reusable\n",
    "## EVO Core Initial Release",
    "This document distinguishes algorithms implemented by the reusable\n"
    "`catalyst_evo` core from the structured program transformations and evaluation\n"
    "algorithm required by the EVO 1.0 source optimizer. Version 0.43.0 implements\n"
    "the core plus project ingestion, immutable-baseline preparation, normalized\n"
    "Clang/LLVM analysis and hotspot ranking, canonical transformation recipes,\n"
    "three AST-aware source-transformation applications, deterministic isolated\n"
    "candidate materialization, exact candidate build/correctness assurance,\n"
    "reproducible baseline-versus-candidate performance measurement, structured\n"
    "recipe evolution, bounded external-process source orchestration, and the\n"
    "versioned `analyze`, `evolve`, `replay`, and `report` product command contract.\n\n"
    "The search algorithm initializes canonical recipes from live\n"
    "opportunities/catalogue authority, mutates only whole records or typed\n"
    "parameters, crosses whole records rather than source bytes, applies only\n"
    "versioned bounded deterministic repair, rejects invalid recipes before\n"
    "materialization, and records every operator event plus complete downstream\n"
    "recipe → candidate → assurance → measurement → fitness lineage. Fixed seeds\n"
    "replay population/search identity; strict improvements replace the winner and\n"
    "exact ties retain the earlier stable candidate. External evaluation batches\n"
    "may finish out of order, but stable candidate-order commit, exact worker traces,\n"
    "fail-closed cleanup, and product checkpoint identity keep logical search and\n"
    "resume deterministic.\n\n"
    "The 0.43 command planner adds no search heuristic. It deterministically maps a\n"
    "validated request to an execution plan, provider requirements, path/stream\n"
    "roles, checkpoint/replay policy, and stable exit semantics. Concrete production\n"
    "provider execution is #114 and installed CLI dispatch is #93.\n\n",
)

# EVO-001 remains the same installed-core contract while package metadata advances.
replace_exact(
    "docs/specs/EVO-001-library-contract.md", "Version: 0.42.0", "Version: 0.43.0"
)
replace_section(
    "docs/specs/EVO-001-library-contract.md",
    "## Scope Boundary\n\n",
    "## Purpose",
    "## Scope Boundary\n\n"
    "This specification governs the reusable deterministic C17 evolutionary-search\n"
    "core packaged through version 0.43.0. Version 0.43.0 changes no installed core\n"
    "semantics; its new implementation is confined to the private source-optimizer\n"
    "product-command contract. The installed core compatibility header remains\n"
    "0.37.0. This contract does not define C-project ingestion, Clang/LLVM analysis,\n"
    "structured source transformations, isolated candidate builds,\n"
    "baseline-versus-candidate measurement, optimized patches, or product-level\n"
    "replay artifacts.\n\n"
    "Those source-to-source product responsibilities are defined separately by\n"
    "`EVO-002-source-optimizer-contract.md`, its product-provider addendum EVO-002A,\n"
    "and the executable-facing EVO-003 command contract. An EVO-001 conforming run\n"
    "or compiler-option adapter alone must not be described as an optimized C\n"
    "codebase.\n\n",
)

# EVO-002 advances and explicitly delegates executable-facing provider authority.
replace_exact(
    "docs/specs/EVO-002-source-optimizer-contract.md",
    "Status: Implemented through the 0.42.0 bounded source-orchestration boundary; draft 1.0 target",
    "Status: Implemented through the 0.43.0 product-command contract boundary; draft 1.0 target",
)
replace_exact(
    "docs/specs/EVO-002-source-optimizer-contract.md",
    "Version: 0.42.0",
    "Version: 0.43.0",
)
replace_exact(
    "docs/specs/EVO-002-source-optimizer-contract.md",
    "Governing ADRs: ADR-0016, ADR-0026, ADR-0035, ADR-0036, ADR-0037, ADR-0038, ADR-0039, ADR-0040, ADR-0041, ADR-0042, and ADR-0043",
    "Governing ADRs: ADR-0016, ADR-0026, ADR-0035, ADR-0036, ADR-0037, ADR-0038, ADR-0039, ADR-0040, ADR-0041, ADR-0042, ADR-0043, and ADR-0045\n"
    "Command/provider addendum: `EVO-002A-product-command-provider-authority.md`\n"
    "Executable-facing command contract: `EVO-003-product-command-contract.md`",
)
replace_section(
    "docs/specs/EVO-002-source-optimizer-contract.md",
    "## Purpose\n\n",
    "## Claim Boundary",
    "## Purpose\n\n"
    "EVO ingests a buildable C codebase and a declared optimization contract,\n"
    "analyzes its source and measured behavior, evolves structured source-level\n"
    "alternatives, compiles and validates isolated candidates, and emits the\n"
    "highest-ranked fully verified C source candidate found within the bounded\n"
    "search as a reviewable patch and reproducibility package.\n\n"
    "This specification defines the product layer above the reusable C17\n"
    "evolutionary-search core governed by EVO-001. The complete installed source\n"
    "optimizer is not yet delivered in version 0.43.0. The implemented private\n"
    "foundation includes strict project ingestion and immutable baselines,\n"
    "normalized analysis/hotspot evidence, canonical transformation recipes, the\n"
    "initial AST-aware C transformation catalogue, deterministic isolated candidate\n"
    "materialization, candidate build/correctness assurance, reproducible\n"
    "baseline-versus-candidate measurement with finite fitness mapping,\n"
    "deterministic structured recipe evolution through the reusable EVO core, and\n"
    "bounded external-process source orchestration with stable result commit,\n"
    "cleanup evidence, persistent worker traces, and product checkpoint/resume\n"
    "authority.\n\n"
    "Version 0.43.0 additionally defines the executable-facing `analyze`, `evolve`,\n"
    "`replay`, and `report` contract. EVO-002A and EVO-003 supersede the older\n"
    "caller-supplied callback wording for the standalone product path: commands must\n"
    "select stable production provider identities, implementation versions, and the\n"
    "required capability-policy identity; unavailable or incompatible required\n"
    "providers fail before external candidate execution and may not fall back to a\n"
    "private callback or weaker unsandboxed path. The 0.34–0.42 callback seams\n"
    "remain internal test/embedding mechanisms.\n\n"
    "Concrete production provider implementations remain issue #114 and the\n"
    "installed standalone executable remains issue #93. Artifact publication (#68),\n"
    "end-to-end source proof (#69), and later release boundaries remain 1.0 work.\n\n",
)

# Release-forward historical orchestration validator.
replace_exact(
    "tests/validate_project_orchestration.py",
    '    require(version_tuple == (0, 42, 0), "package version is not EVO 0.42.0")',
    '    require(version_tuple >= (0, 42, 0), "package predates orchestration boundary")',
)

# AES-BLD source/test inventory must include the new private command module.
replace_exact(
    ".aems/aes-bld-001.json",
    '      "src/project_orchestration.c",\n'
    '      "src/project_orchestration_checkpoint.c"\n'
    "    ],",
    '      "src/project_orchestration.c",\n'
    '      "src/project_orchestration_checkpoint.c",\n'
    '      "src/project_command.c"\n'
    "    ],",
)
replace_exact(
    ".aems/aes-bld-001.json",
    '      {\n'
    '        "id": "project-orchestration-resume",\n'
    '        "source": "tests/project_orchestration_resume_test.c",\n'
    '        "cmake": "evo_project_orchestration_resume_test",\n'
    '        "autotools": "tests/evo_project_orchestration_resume_test"\n'
    "      }\n"
    "    ],",
    '      {\n'
    '        "id": "project-orchestration-resume",\n'
    '        "source": "tests/project_orchestration_resume_test.c",\n'
    '        "cmake": "evo_project_orchestration_resume_test",\n'
    '        "autotools": "tests/evo_project_orchestration_resume_test"\n'
    "      },\n"
    "      {\n"
    '        "id": "project-command",\n'
    '        "source": "tests/project_command_test.c",\n'
    '        "cmake": "evo_project_command_test",\n'
    '        "autotools": "tests/evo_project_command_test"\n'
    "      }\n"
    "    ],",
)

# Branch metadata was already advanced earlier; assert it remains exact.
repo = read("repo.yaml")
for marker in (
    "status: implemented-core-0.37.0",
    "status: product-command-contract-implemented-0.43.0",
    "contract: docs/specs/EVO-003-product-command-contract.md",
):
    if marker not in repo:
        raise SystemExit(f"repo.yaml: missing expected marker: {marker}")

print("EVO 0.43.0 package authority synchronization complete")
