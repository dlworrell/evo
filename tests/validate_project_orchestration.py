#!/usr/bin/env python3
"""Independent structural checks for EVO bounded source orchestration v1."""

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
    header = read("src/internal/project_orchestration.h")
    checkpoint_header = read("src/internal/project_orchestration_checkpoint.h")
    model = read("src/project_orchestration_model.c")
    runtime = read("src/project_orchestration_runtime.c")
    transaction = read("src/project_orchestration.c")
    checkpoint = read("src/project_orchestration_checkpoint.c")
    trace_header = read("src/internal/project_search_orchestration.h")
    trace_runtime = read("src/project_search_orchestration_trace.c")
    test = read("tests/project_orchestration_test.c")
    search_test = read("tests/project_search_test.c")
    checkpoint_test = read("tests/project_orchestration_checkpoint_test.c")
    resume_test = read("tests/project_orchestration_resume_test.c")
    cmake = read("CMakeLists.txt")
    tests_cmake = read("tests/CMakeLists.txt")
    automake = read("Makefile.am")
    adr = normalized(read("docs/adr/ADR-0043-bounded-parallel-source-orchestration.md"))
    hra = normalized(
        read("docs/engineering/reports/EVO-HRA-015-bounded-source-orchestration-audit.md")
    )
    schema = json.loads(read("docs/schemas/evo-project-orchestration-v1.schema.json"))

    require(
        "EVO_PROJECT_ORCHESTRATION_SCHEMA_VERSION 1U" in header,
        "orchestration schema version missing",
    )
    require(
        "EVO_PROJECT_ORCHESTRATION_POLICY_VERSION 1U" in header,
        "orchestration policy version missing",
    )
    for callback in (
        "evo_project_orchestration_provider_start_fn",
        "evo_project_orchestration_provider_poll_fn",
        "evo_project_orchestration_provider_cancel_fn",
        "evo_project_orchestration_provider_join_fn",
    ):
        require(callback in header, f"external provider callback missing: {callback}")
    for field in (
        "external_worker_count",
        "cpu_time_ms",
        "address_space_bytes",
        "descendant_process_count",
        "storage_bytes",
        "output_bytes",
        "wall_timeout_ms",
        "workspace_bytes",
        "require_filesystem_isolation",
        "require_network_isolation",
        "require_descendant_cleanup",
    ):
        require(field in header, f"resource policy field missing: {field}")
    for state in (
        "JOB_ADMITTED",
        "JOB_STARTED",
        "JOB_CANCEL_REQUESTED",
        "JOB_TERMINAL",
        "JOB_JOINED",
        "JOB_STAGED",
        "JOB_COMMITTED",
    ):
        require(state in header, f"job lifecycle state missing: {state}")
    for reason in (
        "TERMINAL_TIMEOUT",
        "TERMINAL_CPU_LIMIT",
        "TERMINAL_MEMORY_LIMIT",
        "TERMINAL_PROCESS_LIMIT",
        "TERMINAL_STORAGE_LIMIT",
        "TERMINAL_OUTPUT_LIMIT",
        "TERMINAL_CANCELED",
        "TERMINAL_CAPABILITY_UNAVAILABLE",
        "TERMINAL_JOIN_FAILED",
        "TERMINAL_CLEANUP_FAILED",
    ):
        require(reason in header, f"stable terminal reason missing: {reason}")

    require(
        "index % config->resources.external_worker_count + 1U" in runtime,
        "deterministic logical worker assignment missing",
    )
    require(
        "index / config->resources.external_worker_count" in runtime,
        "deterministic dispatch wave missing",
    )
    require(
        "first_hard_failure_index" in runtime and "has_hard_failure" in runtime,
        "first hard failure latch missing",
    )
    require(
        "config->provider.cancel" in runtime and "config->provider.join" in runtime,
        "mandatory cancel/join paths missing",
    )
    require(
        "job->commit_ordinal = index" in runtime,
        "stable candidate-order commit missing",
    )
    require(
        "owner->view.generation_committed = true" in runtime,
        "atomic generation commit marker missing",
    )
    require(
        "evo_project_orchestration_capabilities_satisfy" in model
        and "TERMINAL_CAPABILITY_UNAVAILABLE" in runtime,
        "provider enforcement capability gate missing",
    )
    require(
        "config->candidate_count *" in model and "SIZE_MAX /" in model,
        "checked scheduler allocation arithmetic missing",
    )
    require(
        "owner->view.private_owner = owner" in transaction,
        "atomic result publication missing",
    )

    for forbidden in ("system(", "popen(", "/bin/sh -c"):
        require(
            forbidden not in model + runtime + transaction + checkpoint,
            f"forbidden execution path present: {forbidden}",
        )

    require(
        "EVO_PROJECT_ORCHESTRATION_CHECKPOINT_FORMAT_VERSION 1U"
        in checkpoint_header,
        "product checkpoint format version missing",
    )
    require(
        "EVO_PROJECT_ORCHESTRATION_CHECKPOINT_INTEGRITY_FNV1A64 1U"
        in checkpoint_header,
        "product checkpoint integrity identifier missing",
    )
    for identity in (
        "baseline_fingerprint",
        "analysis_fingerprint",
        "catalogue_identity",
        "catalogue_version",
        "recipe_schema_version",
        "search_schema_version",
        "mutation_policy_version",
        "crossover_policy_version",
        "repair_policy_version",
        "search_policy_identity",
        "evaluation_provider_identity",
        "orchestration_policy_identity",
        "toolchain_identity",
        "workload_identity",
        "artifact_schema_identity",
        "random_seed",
        "committed_generation",
        "committed_lineage_fingerprint",
    ):
        require(identity in checkpoint_header, f"checkpoint identity missing: {identity}")
    require(
        "evo_orchestration_checkpoint_integrity" in checkpoint
        and "CHECKSUM_OFFSET" in checkpoint,
        "checkpoint integrity calculation missing",
    )
    require(
        "evo_orchestration_checkpoint_identity_equal" in checkpoint,
        "exact product identity validation missing",
    )
    require(
        "EVO_PROJECT_ORCHESTRATION_CHECKPOINT_ERROR_IDENTITY_MISMATCH"
        in checkpoint,
        "stale dependency rejection missing",
    )
    require(
        "core_checkpoint" in checkpoint_header and "core_checkpoint_size" in checkpoint_header,
        "nested generic core checkpoint boundary missing",
    )

    require(
        "evo_project_search_orchestration_trace_t" in trace_header,
        "persistent orchestration trace view missing",
    )
    require(
        "evo_project_search_orchestration_trace_append" in trace_runtime,
        "persistent orchestration trace append path missing",
    )
    require(
        "trace_rebind_job" in trace_runtime,
        "copied evaluation fingerprint rebinding missing",
    )
    require(
        "persistent worker traces retain every dispatched generation and job"
        in search_test,
        "successful persistent worker-trace oracle missing",
    )
    require(
        "failed search retains complete trustworthy worker schedule evidence"
        in search_test,
        "failed persistent worker-trace oracle missing",
    )

    for fixture in (
        "parallel batch succeeds",
        "runtime completion order is retained diagnostically",
        "worker count does not alter logical candidate outcome",
        "hard failure publishes no partial generation",
        "failure cancels and joins siblings before later starts",
        "candidate rejection remains an exact committed exclusion",
        "required isolation capability fails closed",
    ):
        require(fixture in test, f"scheduler oracle missing: {fixture}")
    for fixture in (
        "same committed boundary replays byte-identical checkpoint evidence",
        "stale toolchain rejects before resume state is published",
        "stale baseline rejects",
        "stale catalogue rejects",
        "stale workload rejects",
        "stale orchestration policy rejects",
        "nested checkpoint corruption fails integrity before resume",
        "unsupported product checkpoint version rejects",
        "truncated product checkpoint rejects",
    ):
        require(fixture in checkpoint_test, f"checkpoint oracle missing: {fixture}")
    for fixture in (
        "provider identity/version mismatch rejects before external execution",
        "provider capability-policy mismatch rejects before external execution",
        "stale toolchain identity rejects before external candidate execution",
        "resumed bounded external evaluation matches uninterrupted execution",
        "resume schedules only post-checkpoint generations",
    ):
        require(fixture in resume_test, f"resume oracle missing: {fixture}")

    for source in (
        "src/project_orchestration_model.c",
        "src/project_orchestration_runtime.c",
        "src/project_orchestration.c",
        "src/project_orchestration_checkpoint.c",
        "src/project_search_orchestration_trace.c",
    ):
        require(source in cmake, f"CMake source missing: {source}")
        require(source in automake, f"Automake source missing: {source}")
    for target in (
        "evo_project_orchestration_test",
        "evo_project_orchestration_checkpoint_test",
        "evo_project_orchestration_resume_test",
    ):
        require(target in tests_cmake, f"canonical CTest target missing: {target}")
        require(f"tests/{target}" in automake, f"Automake target missing: {target}")

    require(
        schema.get("$schema") == "https://json-schema.org/draft/2020-12/schema",
        "orchestration schema draft mismatch",
    )
    required = set(schema.get("required", []))
    for field in (
        "policy_identity",
        "provider_identity",
        "generation",
        "external_worker_count",
        "jobs",
        "generation_committed",
        "cleanup_complete",
        "projection_complete",
        "probabilistic_authority",
    ):
        require(field in required, f"schema required field missing: {field}")
    require(
        schema["properties"]["projection_complete"].get("const") is True,
        "schema must require complete projection",
    )
    require(
        schema["properties"]["probabilistic_authority"].get("const") is False,
        "schema must forbid probabilistic authority",
    )

    require("Status: Accepted" in adr, "ADR-0043 is not accepted")
    require(
        "Runtime completion order is diagnostic only" in adr,
        "ADR stable commit rule missing",
    )
    require(
        "first hard worker/provider failure" in adr,
        "ADR failure propagation rule missing",
    )
    require(
        "Product checkpoint format version 1" in adr,
        "ADR product checkpoint rule missing",
    )
    require(
        "No accelerated representation participates" in hra,
        "HRA-015 accelerator assessment missing",
    )
    require(
        "serial-versus-parallel logical equivalence" in hra,
        "HRA-015 differential claim missing",
    )
    require(
        "uninterrupted-versus-resumed" in hra,
        "HRA-015 resume differential claim missing",
    )

    version_match = re.search(
        r"project\(catalyst_evo VERSION (\d+)\.(\d+)\.(\d+) LANGUAGES C\)",
        cmake,
    )
    require(version_match is not None, "CMake package version missing")
    version_tuple = tuple(int(part) for part in version_match.groups())
    require(version_tuple == (0, 42, 0), "package version is not EVO 0.42.0")

    print("bounded source orchestration structural validation: PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, KeyError, OSError, json.JSONDecodeError) as exc:
        print(f"bounded source orchestration structural validation: FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
