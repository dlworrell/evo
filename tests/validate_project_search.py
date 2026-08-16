#!/usr/bin/env python3
"""Independent structural checks for EVO structured recipe search v1."""

from __future__ import annotations

import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def normalized(text: str) -> str:
    return " ".join(text.split())


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    header = read("src/internal/project_search.h")
    internal = read("src/internal/project_search_internal.h")
    owner = read("src/internal/project_search_owner.h")
    model = read("src/project_search_model.c")
    runtime = read("src/project_search_runtime.c")
    transaction = read("src/project_search.c")
    test = read("tests/project_search_test.c")
    cmake = read("CMakeLists.txt")
    tests_cmake = read("tests/CMakeLists.txt")
    automake = read("Makefile.am")
    adr = normalized(read("docs/adr/ADR-0042-structured-recipe-evolution.md"))
    hra = normalized(
        read("docs/engineering/reports/EVO-HRA-014-structured-recipe-evolution-audit.md")
    )
    schema = json.loads(read("docs/schemas/evo-project-search-v1.schema.json"))

    require(
        "EVO_PROJECT_SEARCH_SCHEMA_VERSION 1U" in header,
        "structured search schema version missing",
    )
    for operation in ("ADD", "REMOVE", "PARAMETERIZE", "REPLACE", "REORDER"):
        require(
            f"EVO_PROJECT_SEARCH_MUTATION_{operation}" in header,
            f"structured mutation flag missing: {operation.lower()}",
        )
    require(
        "evo_project_search_evaluation_provider_fn" in header,
        "evaluation provider boundary missing",
    )
    require(
        "evo_project_search_lineage_record" in header,
        "lineage result model missing",
    )
    require("max_repair_passes" in header, "bounded repair policy missing")
    require("evo_project_search_owner" in owner, "private search owner missing")
    require("evo_project_search_mutable_recipe" in internal, "structured mutable recipe missing")

    for operation in (
        "evo_search_add_operation",
        "evo_search_remove_operation",
        "evo_search_parameterize_operation",
        "evo_search_replace_operation",
        "evo_search_reorder_operation",
        "evo_search_crossover_recipes",
    ):
        require(operation in runtime, f"structured operator missing: {operation}")
    require("evo_search_repair" in runtime, "deterministic repair boundary missing")
    require(
        "record->parameter_count == 0U" in runtime,
        "zero-parameter recipe proposals must preserve a null parameter view",
    )
    require(
        "raw_source_bytes\\\":false" in runtime,
        "canonical evidence does not forbid raw source bytes",
    )
    require(
        "best verified candidate found within the recorded bounded search contract" in runtime,
        "bounded-result claim language missing",
    )
    require(
        "probabilistic_authority\\\":false" in runtime,
        "evidence must forbid probabilistic ranking authority",
    )

    require("evo_run(&problem" in transaction, "deterministic EVO core is not used")
    require(
        "EVO_CROSSOVER_CONSUMER" in transaction
        and "EVO_MUTATION_CONSUMER" in transaction,
        "consumer structured operators are not wired to EVO core",
    )
    require(
        "EVO_EVALUATION_CALLBACK_SERIAL" in transaction,
        "0.41 evaluation boundary must remain serial",
    )
    require(
        "correctness_preserved" in transaction
        and "performance_eligible" in transaction
        and "fitness_available" in transaction,
        "fitness admission does not preserve correctness/assurance/measurement authority",
    )
    require(
        "record->generation" in transaction and "record->population_index" in transaction,
        "stable generation/population lineage mapping missing",
    )

    implementation = model + runtime + transaction + test
    for forbidden in ("system(", "popen(", "/bin/sh -c"):
        require(forbidden not in implementation, f"forbidden execution path present: {forbidden}")
    for forbidden in ("strcpy(", "strcat(", "sprintf(", "gets("):
        require(forbidden not in implementation, f"dangerous primitive present: {forbidden}")

    for oracle in (
        "MUTATION_ADD",
        "MUTATION_REMOVE",
        "MUTATION_PARAMETERIZE",
        "MUTATION_REPLACE",
        "MUTATION_REORDER",
        "evo_search_crossover_recipes",
        "fixed seed replays population lineage and winner exactly",
        "strict verified fitness improvement",
        "exact ties preserve earlier generation and lower stable index",
    ):
        require(oracle in test, f"normative structured-search oracle missing: {oracle}")
    require(
        'strstr(first.canonical_json, "unit_value") == NULL' in test,
        "normative evidence test does not prove source bytes stay absent",
    )

    for source in (
        "src/project_search_model.c",
        "src/project_search_runtime.c",
        "src/project_search.c",
    ):
        require(source in cmake, f"CMake source missing: {source}")
        require(source in automake, f"Automake source missing: {source}")
    require(
        "target_link_libraries(catalyst_evo_project_foundation PUBLIC catalyst_evo)" in cmake,
        "project foundation is not linked to deterministic EVO core",
    )
    require(
        "evo_project_search_test" in tests_cmake,
        "canonical CTest structured-search target missing",
    )
    require(
        "tests/evo_project_search_test" in automake,
        "Automake structured-search test missing",
    )

    require(
        schema.get("$schema") == "https://json-schema.org/draft/2020-12/schema",
        "search evidence schema draft mismatch",
    )
    required = set(schema.get("required", []))
    for field in (
        "baseline_fingerprint",
        "analysis_fingerprint",
        "policy",
        "lineage",
        "best",
        "projection_complete",
        "probabilistic_authority",
        "raw_source_bytes",
        "search_fingerprint",
    ):
        require(field in required, f"search schema required field missing: {field}")
    require(
        schema["properties"]["projection_complete"].get("const") is True,
        "search schema must require a complete audit projection",
    )
    require(
        schema["properties"]["probabilistic_authority"].get("const") is False,
        "search schema must forbid probabilistic authority",
    )
    require(
        schema["properties"]["raw_source_bytes"].get("const") is False,
        "search schema must forbid raw source bytes",
    )
    lineage_required = set(schema["$defs"]["lineage"]["required"])
    for field in (
        "generation",
        "population_index",
        "operator_kind",
        "recipe_fingerprint",
        "candidate_fingerprint",
        "assurance_fingerprint",
        "measurement_fingerprint",
        "fitness",
        "rejection_reason",
        "winner",
    ):
        require(field in lineage_required, f"lineage schema field missing: {field}")

    require("Status: Accepted" in adr, "ADR-0042 is not accepted")
    require(
        "Raw C source crossover is prohibited" in adr,
        "ADR does not preserve raw-source exclusion",
    )
    require(
        "existing EVO core ordering" in adr,
        "ADR does not preserve exact core tie semantics",
    )
    require(
        "No hash table, bitmap, cache" in hra,
        "HRA-014 does not state the exact-authority implementation",
    )
    require(
        "fitness back to stable recipe, candidate, assurance, and measurement identities" in hra,
        "HRA-014 lineage authority is incomplete",
    )

    print("structured recipe search structural validation: PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, KeyError, OSError, json.JSONDecodeError) as exc:
        print(f"structured recipe search structural validation: FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
