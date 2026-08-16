#!/usr/bin/env python3
"""Independent structural checks for EVO candidate measurement v1."""

from __future__ import annotations

import json
import pathlib
import re
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
    header = read("src/internal/project_measurement.h")
    model = read("src/project_measurement_model.c")
    runtime = read("src/project_measurement_runtime.c")
    transaction = read("src/project_measurement.c")
    test = read("tests/project_measurement_test.c")
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
    adr = normalized(read("docs/adr/ADR-0041-reproducible-candidate-performance-fitness.md"))
    hra = normalized(read("docs/engineering/reports/EVO-HRA-013-candidate-performance-fitness-audit.md"))
    schema = json.loads(read("docs/schemas/evo-project-measurement-v1.schema.json"))
    manifest = json.loads(read(".aems/aes-bld-001.json"))

    require("EVO_PROJECT_MEASUREMENT_SCHEMA_VERSION 1U" in header, "measurement schema version missing")
    require("evo_project_measurement_provider_fn" in header, "measurement provider boundary missing")
    require("warmup_count" in header and "repetition_count" in header, "sampling policy missing")
    require("max_runtime_range_ppm" in header, "stability policy missing")
    require("comparison_tolerance_ppm" in header and "minimum_improvement_ppm" in header, "comparison policy missing")
    require("fitness_weights" in header, "explicit fitness weights missing")
    require("optimization_flags_identity" in header, "optimization-flags condition identity missing")
    require("performance_eligible" in model, "assurance admission check missing")
    require("runtime-median-deviation" in runtime, "outlier evidence missing")
    require("condition-mismatch" in runtime, "condition mismatch evidence missing")
    require("isfinite(mix_weight_total)" in model, "memory/binary mix overflow guard missing")
    require("EVO_PROJECT_MEASUREMENT_UNSTABLE" in runtime, "unstable classification missing")
    require("correctness_preserved" in runtime, "correctness separation missing")
    require("best verified candidate found within the recorded bounded search contract" in runtime, "bounded result wording missing")
    for field in (
        "order",
        "outlier_policy",
        "outlier_deviation_ns",
        "timeout_ms",
        "workload_weight",
        "peak_memory_mix_weight",
        "binary_size_mix_weight",
    ):
        require(field in runtime, f"canonical policy projection missing: {field}")
    require("Measurement provider" in runtime and "## Workload policy" in runtime, "Markdown condition/policy projection missing")
    require("| Component | Value | Weight | Contribution |" in runtime, "Markdown fitness derivation missing")
    require("config->limits.max_evidence_bytes" in runtime, "evidence buffer is not resource bounded")
    require("evo_measurement_result_fingerprint_double" in transaction, "canonical floating fitness identity missing")
    require("&owner->view.fitness, sizeof(owner->view.fitness)" not in transaction, "raw struct bytes must not define measurement identity")

    for forbidden in ("system(", "popen(", "/bin/sh -c"):
        require(forbidden not in model + runtime + transaction, f"forbidden execution path present: {forbidden}")
    for primitive in ("memcpy(", "memset(", "snprintf("):
        require(
            primitive not in model + runtime + transaction + test,
            f"new measurement dangerous primitive present: {primitive}",
        )

    for case in (
        "FAKE_FASTER",
        "FAKE_EQUAL",
        "FAKE_SLOWER",
        "FAKE_UNSTABLE",
        "FAKE_INCOMPLETE",
        "FAKE_OUTLIER",
        "FAKE_CONDITION_MISMATCH",
        "replay-a",
        "replay-b",
    ):
        require(case in test, f"oracle fixture missing: {case}")

    version_match = re.search(
        r"project\(catalyst_evo VERSION (\d+)\.(\d+)\.(\d+) LANGUAGES C\)",
        cmake,
    )
    require(version_match is not None, "CMake package version missing")
    version_tuple = tuple(int(part) for part in version_match.groups())
    version = ".".join(version_match.groups())
    require(version_tuple >= (0, 40, 0), "package predates measurement boundary")
    require(f"[{version}]" in configure, "CMake/Autotools package versions diverge")

    for source in (
        "src/project_measurement_model.c",
        "src/project_measurement_runtime.c",
        "src/project_measurement.c",
    ):
        require(source in cmake, f"CMake source missing: {source}")
        require(source in automake, f"Automake source missing: {source}")
    require("evo_project_measurement_test" in tests_cmake, "canonical CTest measurement target missing")
    require("tests/evo_project_measurement_test" in automake, "Automake measurement test missing")

    manifest_text = json.dumps(manifest, sort_keys=True)
    for source in (
        "src/project_measurement_model.c",
        "src/project_measurement_runtime.c",
        "src/project_measurement.c",
    ):
        require(source in manifest_text, f"AES-BLD production inventory missing: {source}")
    require("tests/project_measurement_test.c" in manifest_text, "AES-BLD normative test inventory missing")

    require(schema.get("$schema") == "https://json-schema.org/draft/2020-12/schema", "schema draft mismatch")
    required = set(schema.get("required", []))
    for field in (
        "candidate_fingerprint",
        "assurance_fingerprint",
        "condition_fingerprint",
        "workloads",
        "samples",
        "fitness_available",
        "correctness_preserved",
        "fitness",
        "fitness_weights",
        "measurement_fingerprint",
    ):
        require(field in required, f"schema required field missing: {field}")
    condition_required = set(schema["properties"]["condition"]["required"])
    require("optimization_flags_identity" in condition_required, "schema optimization-flags identity missing")
    require(schema["properties"]["correctness_preserved"].get("const") is True, "schema must preserve correctness authority")
    require(schema["properties"]["probabilistic_authority"].get("const") is False, "schema must forbid probabilistic authority")
    workload_required = set(schema["$defs"]["workload"]["required"])
    for field in (
        "order",
        "outlier_policy",
        "outlier_deviation_ns",
        "timeout_ms",
        "workload_weight",
        "peak_memory_mix_weight",
        "binary_size_mix_weight",
    ):
        require(field in workload_required, f"schema workload policy field missing: {field}")
    require("total" not in schema["$defs"]["weights"]["required"], "fitness weights must not invent a total weight")

    require("Status: Accepted" in adr, "ADR-0041 is not accepted")
    require("Raw samples are authority" in adr, "ADR raw-sample authority missing")
    require("Incomplete or unstable measurements" in adr, "ADR unstable/incomplete rule missing")
    require("Audited implementation: EVO 0.40.0" in hra, "HRA implementation status missing")
    require("No accelerator participates in the initial implementation" in hra, "HRA accelerator assessment missing")
    require("Correctness authority remains the assurance result" in hra, "HRA correctness separation missing")

    # The measurement validator is release-forward: later source-optimizer
    # phases must preserve the 0.40 measurement boundary without forcing its
    # historical ADR/HRA to pretend that it was introduced by the later
    # package. Package-level records must remain synchronized to the current
    # version, while measurement-specific authority remains explicitly linked.
    require(f"EVO {version} packages the deterministic" in readme, "README package boundary is stale")
    require("ADR-0041-reproducible-candidate-performance-fitness.md" in readme, "README ADR-0041 link missing")
    require("reproducible baseline-versus-candidate performance measurement" in readme, "README measurement boundary missing")
    require(f"EVO {version} contains" in roadmap, "roadmap package boundary is stale")
    require("Issue #64 implements reproducible candidate performance measurement" in roadmap, "roadmap measurement boundary missing")
    require(f"Version {version} contains" in architecture, "architecture package boundary is stale")
    require("EVO-HRA-013 audits this 0.40.0 boundary" in architecture, "architecture HRA-013 boundary missing")
    require(f"Version {version} implements" in algorithms, "algorithms package boundary is stale")
    require(version in repo_metadata and "source_optimizer" in repo_metadata, "repository package metadata is stale")
    require(f"Version: {version}" in evo001, "EVO-001 package version is stale")
    require(f"Version: {version}" in evo002, "EVO-002 package version is stale")
    require("ADR-0041 and EVO-HRA-013" in evo002, "EVO-002 measurement governance link missing")

    workflow = read(".github/workflows/project-measurement.yml")
    require("validate_project_measurement.py" in workflow, "hosted measurement validator missing")
    require("EVO_ENABLE_SANITIZERS=ON" in workflow, "hosted sanitizer build missing")
    require("autoreconf -fi" in workflow and "make check" in workflow, "hosted Autotools/GNU path missing")

    print("candidate measurement structural validation: PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, KeyError, OSError, json.JSONDecodeError) as exc:
        print(f"candidate measurement structural validation: FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
