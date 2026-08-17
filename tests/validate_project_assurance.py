#!/usr/bin/env python3
"""Independent structural checks for EVO candidate assurance v1."""

from __future__ import annotations

import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def normalized(text: str) -> str:
    return " ".join(text.split())


def main() -> int:
    header = read("src/internal/project_assurance.h")
    model = read("src/project_assurance_model.c")
    runtime = read("src/project_assurance_runtime.c")
    transaction = read("src/project_assurance.c")
    test = read("tests/project_assurance_test.c")
    cmake = read("CMakeLists.txt")
    tests_cmake = read("tests/CMakeLists.txt")
    automake = read("Makefile.am")
    configure = read("configure.ac")
    readme = normalized(read("README.md"))
    roadmap = normalized(read("docs/roadmap.md"))
    architecture = normalized(read("docs/architecture.md"))
    algorithms = normalized(read("docs/algorithms.md"))
    repo_metadata = normalized(read("repo.yaml"))
    evo001 = normalized(read("docs/specs/EVO-001-library-contract.md"))
    evo002 = normalized(read("docs/specs/EVO-002-source-optimizer-contract.md"))
    adr = normalized(read("docs/adr/ADR-0040-isolated-candidate-correctness-gates.md"))
    hra = normalized(
        read("docs/engineering/reports/EVO-HRA-012-candidate-correctness-gates-audit.md")
    )
    manifest = json.loads(read(".aems/aes-bld-001.json"))
    schema = json.loads(read("docs/schemas/evo-project-assurance-v1.schema.json"))

    require("EVO_PROJECT_ASSURANCE_SCHEMA_VERSION 1U" in header, "schema version missing")
    require("evo_project_assurance_runner_fn" in header, "execution-provider boundary missing")
    require("workspace_only_filesystem" in header, "filesystem enforcement evidence missing")
    require("shell_interpretation" in header, "argv-only shell evidence missing")
    require("network_policy_enforced" in header, "network enforcement evidence missing")
    require("performance_eligible" in header and "champion_eligible" in header, "authority flags missing")
    require("required_profile_count" in header, "independent profile authority missing")

    for forbidden in ("system(", "popen(", "/bin/sh -c", "shell_interpretation = true"):
        require(forbidden not in model + runtime + transaction, f"forbidden command path present: {forbidden}")

    require("evo_assurance_shell_basename" in model, "direct shell rejection missing")
    require("LD_PRELOAD" in model and "DYLD_" in model, "dangerous environment rejection missing")
    require("evo_assurance_relative_path_valid" in model, "working-directory path validation missing")
    require("required_profile_count < 2U" in model, "finalist dual-profile requirement missing")
    require("evo_assurance_required_gates_passed" in transaction, "required-gate authority missing")
    require("EVO_PROJECT_ASSURANCE_GATE_CLEANUP_FAILED" in model, "cleanup rejection path missing")
    require("source_modified" in transaction and "snapshot_modified" in transaction, "immutable-input evidence missing")
    require(".evo-assurance-incomplete-v1" in transaction, "transaction marker missing")
    require("assurance.json" in transaction and "assurance.md" in transaction, "evidence publication missing")
    require("row_prefix" in runtime and "row_status" in runtime, "ordered Markdown gate projection missing")

    for case in (
        "FAKE_FAIL",
        "FAKE_TIMEOUT",
        "FAKE_SIGNAL",
        "FAKE_RESOURCE",
        "FAKE_UNAVAILABLE",
        "FAKE_POLICY",
        "FAKE_CLEANUP",
        "FAKE_SOURCE_MUTATION",
        "FAKE_SNAPSHOT_MUTATION",
        "shell-rejected",
        "path-rejected",
        "network-rejected",
        "replay-a",
        "replay-b",
    ):
        require(case in test, f"normative case missing: {case}")

    require("project(catalyst_evo VERSION 0.40.0" in cmake, "CMake package version is not 0.40.0")
    require("[0.40.0]" in configure, "Autotools package version is not 0.40.0")
    for source in ("src/project_assurance_model.c", "src/project_assurance_runtime.c", "src/project_assurance.c"):
        require(source in cmake, f"CMake source missing: {source}")
        require(source in automake, f"Automake source missing: {source}")
    require("evo_project_assurance_test" in tests_cmake, "canonical CTest assurance target missing")
    require("tests/evo_project_assurance_test" in automake, "Automake assurance test missing")

    manifest_text = json.dumps(manifest, sort_keys=True)
    for source in ("src/project_assurance_model.c", "src/project_assurance_runtime.c", "src/project_assurance.c"):
        require(source in manifest_text, f"AES-BLD production inventory missing: {source}")
    require("tests/project_assurance_test.c" in manifest_text, "AES-BLD normative test inventory missing")

    require(schema.get("$schema") == "https://json-schema.org/draft/2020-12/schema", "schema draft mismatch")
    required = set(schema.get("required", []))
    for field in (
        "candidate_fingerprint",
        "policy_fingerprint",
        "execution_provider_identity",
        "gates",
        "performance_eligible",
        "champion_eligible",
        "assurance_fingerprint",
    ):
        require(field in required, f"schema required field missing: {field}")
    gate_props = schema["$defs"]["gate"]["properties"]
    require(gate_props["shell_interpretation"].get("const") is False, "schema must forbid shell interpretation")
    require(gate_props["workspace_only_filesystem"].get("const") is True, "schema must require workspace filesystem")

    require("Status: Accepted" in adr, "ADR-0040 is not accepted")
    require("A gate that requires network access must declare it explicitly" in adr, "ADR network policy missing")
    require("A candidate that fails any required fast gate is rejected" in adr, "ADR fast-gate authority missing")
    require("selected champion must pass both declared build profiles" in adr, "ADR dual-profile finalist rule missing")
    require("caller-supplied execution provider" in adr, "ADR execution-provider responsibility missing")
    require("Audited implementation: EVO 0.39.0" in hra, "HRA implementation status missing")
    require("No accelerator participates in the initial implementation" in hra, "HRA exact-authority assessment missing")
    require("Probabilistic structures remain prechecks only" in hra, "HRA probabilistic boundary missing")
    require("caller-supplied execution provider" in hra, "HRA execution-provider boundary missing")

    require("EVO 0.40.0 packages the deterministic" in readme, "README package boundary is stale")
    require("ADR-0040-isolated-candidate-correctness-gates.md" in readme, "README ADR-0040 link missing")
    require("Issue #65 is the next dependency-ready" in roadmap, "roadmap package boundary is stale")
    require("Version 0.40.0 contains" in architecture, "architecture package boundary is stale")
    require("EVO-HRA-012 audits this 0.39.0 boundary" in architecture, "architecture assurance boundary missing")
    require("Version 0.40.0 implements" in algorithms, "algorithms package boundary is stale")
    require("candidate build/correctness assurance" in algorithms, "algorithms assurance boundary missing")
    require("performance-measurement-implemented-0.40.0" in repo_metadata, "repository package metadata is stale")
    require("Version: 0.40.0" in evo001, "EVO-001 package version is stale")
    require("0.40.0 candidate-measurement boundary" in evo002, "EVO-002 package boundary is stale")
    require("ADR-0040 and EVO-HRA-012" in evo002, "EVO-002 assurance governance link missing")

    workflow = read(".github/workflows/project-assurance.yml")
    require("validate_project_assurance.py" in workflow, "hosted assurance validator missing")
    require("EVO_ENABLE_SANITIZERS=ON" in workflow, "hosted sanitizer build missing")
    require("autoreconf -fi" in workflow and "make check" in workflow, "hosted Autotools/GNU path missing")

    print("candidate assurance structural validation: PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, KeyError, OSError, json.JSONDecodeError) as exc:
        print(f"candidate assurance structural validation: FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
