#!/usr/bin/env python3
"""Run and validate the four installed-consumer reference adapters."""

from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path
from typing import Any


SCHEMA_ID = "catalyst.evo-reference-adapters.v1"
SCHEMA_VERSION = "1.0.0"
EVO_VERSION = "0.37.0"
ADAPTER_IDS = [
    "repository-scoring",
    "compiler-options",
    "scheduler-tuning",
    "fpga-placement",
]
MAX_PROGRAM_OUTPUT_BYTES = 128 * 1024
MAX_DOCUMENT_BYTES = 256 * 1024
PROGRAM_TIMEOUT_SECONDS = 15


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def require_mapping(value: Any, name: str) -> dict[str, Any]:
    require(isinstance(value, dict), f"{name} must be an object")
    return value


def require_list(value: Any, name: str) -> list[Any]:
    require(isinstance(value, list), f"{name} must be an array")
    return value


def require_integer(value: Any, name: str, minimum: int = 0) -> int:
    require(type(value) is int and value >= minimum, f"{name} is invalid")
    return value


def load_bounded_json(path: Path, name: str) -> dict[str, Any]:
    size = path.stat().st_size
    require(size <= MAX_DOCUMENT_BYTES, f"{name} exceeds the byte limit")
    return require_mapping(json.loads(path.read_text(encoding="utf-8")), name)


def run_adapter(path: Path, expected_id: str) -> dict[str, Any]:
    completed = subprocess.run(
        [str(path)],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=PROGRAM_TIMEOUT_SECONDS,
    )
    require(completed.returncode == 0, f"{expected_id} exited unsuccessfully")
    require(not completed.stderr, f"{expected_id} wrote unexpected stderr")
    require(
        len(completed.stdout) <= MAX_PROGRAM_OUTPUT_BYTES,
        f"{expected_id} output exceeds the byte limit",
    )
    try:
        decoded = completed.stdout.decode("utf-8")
        document = require_mapping(json.loads(decoded), expected_id)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError(f"{expected_id} output is not UTF-8 JSON") from error
    validate_adapter(document, expected_id)
    return document


def validate_fitness(value: Any, name: str) -> dict[str, Any]:
    fitness = require_mapping(value, name)
    expected = {
        "correctness",
        "performance",
        "memory_use",
        "reliability",
        "maintainability",
        "constraint_penalty",
        "total",
    }
    require(set(fitness) == expected, f"{name} fields changed")
    for field in expected:
        require(
            type(fitness[field]) in {int, float},
            f"{name}.{field} must be numeric",
        )
    require(fitness["constraint_penalty"] >= 0, f"{name} penalty is negative")
    return fitness


def validate_trace(adapter: dict[str, Any]) -> None:
    trace = require_list(adapter.get("trace"), "trace")
    result = require_mapping(adapter.get("result"), "result")
    generations = require_integer(
        result.get("generations_completed"), "generations completed"
    )
    require(len(trace) == generations + 1, "trace is not complete")
    previous_global: float | int | None = None
    for index, item_value in enumerate(trace):
        item = require_mapping(item_value, f"trace[{index}]")
        require(item.get("generation") == index, "trace order changed")
        valid_count = require_integer(item.get("valid_count"), "valid count")
        invalid_count = require_integer(item.get("invalid_count"), "invalid count")
        require(valid_count + invalid_count == 12, "trace population is incomplete")
        require_integer(item.get("best_index"), "best index")
        require_integer(item.get("diversity_pair_count"), "pair count")
        require_integer(item.get("diversity_work_units"), "work units")
        genome = item.get("global_best_genome")
        require(
            isinstance(genome, str)
            and len(genome) == 8
            and all(character in "0123456789abcdef" for character in genome),
            "trace genome is invalid",
        )
        global_total = item.get("global_best_total")
        require(type(global_total) in {int, float}, "global total is invalid")
        if previous_global is not None:
            require(global_total >= previous_global, "global best regressed")
        previous_global = global_total


def validate_checkpoint(adapter: dict[str, Any]) -> None:
    checkpoint = require_mapping(adapter.get("checkpoint_resume"), "checkpoint")
    require(checkpoint.get("format_version") == 3, "checkpoint format changed")
    require(checkpoint.get("captured_generation") == 2, "resume point changed")
    require_integer(checkpoint.get("serialized_size"), "checkpoint size", 1)
    require_integer(checkpoint.get("integrity_crc32"), "checkpoint integrity")
    require(
        checkpoint.get("candidate_projection_complete") is True,
        "checkpoint projection is incomplete",
    )
    require(checkpoint.get("resumed_result_equal") is True, "resume result differs")
    require(
        checkpoint.get("resumed_trace_suffix_equal") is True,
        "resume trace differs",
    )
    candidates = require_list(checkpoint.get("candidates"), "checkpoint candidates")
    require(len(candidates) == 12, "checkpoint candidate projection is incomplete")
    for index, value in enumerate(candidates):
        candidate = require_mapping(value, f"checkpoint candidate {index}")
        require(candidate.get("population_index") == index, "candidate order changed")
        require(candidate.get("valid") in {True, False}, "candidate validity missing")
        require(candidate.get("evaluated") in {True, False}, "candidate state missing")
        validate_fitness(candidate.get("fitness"), f"candidate {index} fitness")


def validate_schedules(adapter: dict[str, Any]) -> None:
    schedules = require_list(adapter.get("evaluation_schedules"), "schedules")
    trace = require_list(adapter.get("trace"), "trace")
    require(len(schedules) == len(trace), "parallel schedule history is incomplete")
    for generation, value in enumerate(schedules):
        schedule = require_mapping(value, f"schedule {generation}")
        require(schedule.get("generation") == generation, "schedule order changed")
        require(schedule.get("worker_count") == 3, "worker bound changed")
        require_integer(schedule.get("scratch_bytes"), "worker scratch", 1)
        require(schedule.get("outcome") == "committed", "schedule did not commit")
        require(schedule.get("complete") is True, "schedule projection is incomplete")
        assignments = require_list(schedule.get("assignments"), "assignments")
        require(len(assignments) == 12, "candidate assignment projection is incomplete")
        for index, item_value in enumerate(assignments):
            item = require_mapping(item_value, f"assignment {index}")
            require(item.get("population_index") == index, "assignment order changed")
            require_integer(item.get("worker_identity"), "worker identity")
            require_integer(item.get("dispatch_wave"), "dispatch wave")
            require_integer(item.get("commit_order"), "commit order")
            require(
                item.get("disposition") in {"excluded", "completed"},
                "final assignment disposition is invalid",
            )
            require(item.get("committed") in {True, False}, "commit flag missing")


def validate_adapter(adapter: dict[str, Any], expected_id: str) -> None:
    require(adapter.get("schema_version") == SCHEMA_VERSION, "schema version changed")
    require(adapter.get("adapter_id") == expected_id, "adapter order or identity changed")
    require(adapter.get("evo_version") == EVO_VERSION, "EVO version changed")
    require_mapping(adapter.get("fixture"), "fixture")
    require(isinstance(adapter.get("fixture_id"), str), "fixture identity missing")
    require(isinstance(adapter.get("domain"), str), "domain missing")

    configuration = require_mapping(adapter.get("configuration"), "configuration")
    expected_configuration = {
        "genome_size": 4,
        "population_size": 12,
        "generation_limit": 6,
        "selection": "stable-linear-rank-v1",
        "rank_base_weight": 1,
        "rank_step_weight": 1,
        "crossover": "uniform-byte-v1",
        "crossover_rate": 0.75,
        "mutation": "nonzero-xor-byte-v1",
        "mutation_rate": 0.35,
        "elite_count": 1,
        "population_recycling": True,
        "max_genome_bytes": 4,
        "max_population_bytes": 48,
        "max_evaluation_bytes": 4096,
        "max_child_population_bytes": 48,
        "max_diversity_work": 264,
    }
    for field, expected in expected_configuration.items():
        require(configuration.get(field) == expected, f"configuration.{field} changed")
    require_integer(configuration.get("random_seed"), "random seed")
    require_integer(configuration.get("max_checkpoint_bytes"), "checkpoint bound")
    require_integer(
        configuration.get("checkpoint_required_bytes"), "required checkpoint bytes"
    )
    require(
        isinstance(configuration.get("checkpoint_problem_identity"), str),
        "problem identity is missing",
    )
    require(
        isinstance(configuration.get("checkpoint_context_identity"), str),
        "context identity is missing",
    )
    require(
        configuration.get("fitness_target_enabled") is False,
        "fitness target unexpectedly enabled",
    )
    require(
        configuration.get("diversity_floor_enabled") is False,
        "diversity floor unexpectedly enabled",
    )
    require_integer(configuration.get("stagnation_patience"), "stagnation patience")
    require_integer(
        configuration.get("application_stop_generation"),
        "application stop generation",
    )
    require(
        configuration.get("evaluation_callback_thread_safety") == "thread-safe",
        "evaluator thread-safety declaration changed",
    )
    require_integer(configuration.get("evaluation_worker_count"), "worker count")
    require_integer(
        configuration.get("evaluation_worker_scratch_bytes"), "worker scratch"
    )

    authority = require_mapping(adapter.get("authority"), "authority")
    require(authority.get("accelerated_structure") is None, "accelerator appeared")
    require(authority.get("projection_complete") is True, "projection is incomplete")
    require(
        authority.get("probabilistic_authority") is False,
        "probabilistic authority appeared",
    )
    require(
        authority.get("source_optimizer_claimed") is False,
        "adapter claims source optimization",
    )
    require(isinstance(authority.get("limitation"), str), "limitation is missing")

    capabilities = require_mapping(adapter.get("capabilities"), "capabilities")
    require(capabilities.get("deterministic_replay") is True, "replay missing")
    require(capabilities.get("hard_constraints") is True, "constraints missing")
    require(capabilities.get("soft_penalties") is True, "penalties missing")
    replay = require_mapping(adapter.get("replay"), "replay")
    require(replay == {"run_count": 2, "exact": True}, "replay proof changed")

    result = require_mapping(adapter.get("result"), "result")
    validate_fitness(result.get("best_fitness"), "best fitness")
    require(result.get("termination_reason") != "none", "run did not terminate")
    require(result.get("random_seed") == configuration["random_seed"], "seed differs")
    validate_trace(adapter)

    if expected_id == "repository-scoring":
        require(capabilities.get("checkpoint_resume") is True, "resume missing")
        require(configuration["max_checkpoint_bytes"] == 16384, "checkpoint bound changed")
        require(configuration["checkpoint_required_bytes"] > 0, "checkpoint size missing")
        validate_checkpoint(adapter)
    else:
        require(configuration["max_checkpoint_bytes"] == 0, "unexpected checkpoint bound")
        require(configuration["checkpoint_required_bytes"] == 0, "unexpected checkpoint size")
        require("checkpoint_resume" not in adapter, "unexpected checkpoint evidence")
    if expected_id == "compiler-options":
        require(configuration["stagnation_patience"] == 3, "patience changed")
    else:
        require(configuration["stagnation_patience"] == 0, "unexpected patience")
    if expected_id == "fpga-placement":
        require(configuration["application_stop_generation"] == 3, "stop point changed")
    else:
        require(configuration["application_stop_generation"] == 0, "unexpected stop point")
    if expected_id == "scheduler-tuning":
        require(
            capabilities.get("bounded_parallel_evaluation") is True,
            "parallel evaluation missing",
        )
        validate_schedules(adapter)
    else:
        require("evaluation_schedules" not in adapter, "unexpected schedule evidence")


def render_markdown(document: dict[str, Any]) -> str:
    lines = [
        "# EVO Reference Adapter Evidence",
        "",
        "The JSON artifact is the complete authority. This table is a human-readable projection of its explicit records.",
        "",
        "| Adapter | Fixture | Best genome | Total | Generations | Stop | Replay | Resume | Workers |",
        "|---|---|---:|---:|---:|---|---|---|---:|",
    ]
    for adapter_value in require_list(document["adapters"], "adapters"):
        adapter = require_mapping(adapter_value, "adapter")
        result = require_mapping(adapter["result"], "result")
        config = require_mapping(adapter["configuration"], "configuration")
        lines.append(
            "| {adapter} | {fixture} | `{genome}` | {total} | {generations} | {stop} | exact | {resume} | {workers} |".format(
                adapter=adapter["adapter_id"],
                fixture=adapter["fixture_id"],
                genome=result["best_genome"],
                total=result["best_fitness"]["total"],
                generations=result["generations_completed"],
                stop=result["termination_reason"],
                resume="exact" if "checkpoint_resume" in adapter else "n/a",
                workers=config["evaluation_worker_count"],
            )
        )
    lines.extend(
        [
            "",
            "All fixtures, generation traces, checkpoint candidates, and scheduler assignments remain explicit in the JSON artifact; no digest, cache, compressed index, or probabilistic precheck is authoritative.",
            "",
        ]
    )
    return "\n".join(lines)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository", type=Path, required=True)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--scheduler", type=Path, required=True)
    parser.add_argument("--fpga", type=Path, required=True)
    parser.add_argument(
        "--golden",
        type=Path,
        default=Path(__file__).with_name("reference-adapters-v1.golden.json"),
    )
    parser.add_argument("--json", type=Path)
    parser.add_argument("--markdown", type=Path)
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    schema = load_bounded_json(
        Path(__file__).with_name("reference-adapters-v1.schema.json"),
        "schema document",
    )
    properties = require_mapping(schema.get("properties"), "schema properties")
    require(
        require_mapping(properties.get("schema"), "schema identity").get("const")
        == SCHEMA_ID,
        "schema identity changed",
    )
    require(
        require_mapping(properties.get("schema_version"), "schema version").get(
            "const"
        )
        == SCHEMA_VERSION,
        "schema version changed",
    )
    programs = [
        arguments.repository,
        arguments.compiler,
        arguments.scheduler,
        arguments.fpga,
    ]
    adapters = [
        run_adapter(path, expected_id)
        for path, expected_id in zip(programs, ADAPTER_IDS)
    ]
    document = {
        "schema": SCHEMA_ID,
        "schema_version": SCHEMA_VERSION,
        "adapters": adapters,
    }
    golden = load_bounded_json(arguments.golden, "golden document")
    require(document == golden, "adapter evidence differs from the reviewed golden")

    encoded = json.dumps(document, indent=2, sort_keys=True) + "\n"
    require(len(encoded.encode("utf-8")) <= MAX_DOCUMENT_BYTES, "artifact is too large")
    if arguments.json is not None:
        arguments.json.parent.mkdir(parents=True, exist_ok=True)
        arguments.json.write_text(encoded, encoding="utf-8")
    if arguments.markdown is not None:
        arguments.markdown.parent.mkdir(parents=True, exist_ok=True)
        arguments.markdown.write_text(render_markdown(document), encoding="utf-8")
    print("reference adapter evidence validation passed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (
        KeyError,
        OSError,
        subprocess.SubprocessError,
        TypeError,
        UnicodeError,
        ValueError,
    ) as error:
        raise SystemExit(f"reference adapter validation failed: {error}") from error
