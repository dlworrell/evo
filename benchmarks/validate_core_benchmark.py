#!/usr/bin/env python3
"""Validate EVO-CORE-001 structure and its human-readable projection."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from pathlib import Path
from typing import Any


EXPECTED_CASES = [
    ("serial-allocate", False, False),
    ("serial-recycle", False, True),
    ("parallel-allocate", True, False),
    ("parallel-recycle", True, True),
]
EXPECTED_SEEDS = {
    "smoke": [
        "0000000000000000",
        "000000000000002a",
        "ffffffffffffffff",
    ],
    "extended": [
        "0000000000000000",
        "0000000000000001",
        "0000000000000002",
        "0000000000000003",
        "0000000000000007",
        "000000000000000b",
        "0000000000000011",
        "0000000000000017",
        "000000000000002a",
        "0000000000000063",
        "00000000000000ff",
        "0000000000000400",
        "0000000000010001",
        "0123456789abcdef",
        "fedcba9876543210",
        "ffffffffffffffff",
    ],
}
EXPECTED_TIER_COUNTS = {
    "smoke": (1, 3),
    "extended": (3, 15),
}
MAX_ARTIFACT_BYTES = 2 * 1024 * 1024
UINT64_MAX = (1 << 64) - 1
FNV1A64_OFFSET_BASIS = 14695981039346656037
FNV1A64_PRIME = 1099511628211


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def require_mapping(value: Any, name: str) -> dict[str, Any]:
    require(isinstance(value, dict), f"{name} must be an object")
    return value


def require_list(value: Any, name: str) -> list[Any]:
    require(isinstance(value, list), f"{name} must be an array")
    return value


def load_bounded_json(path: Path, name: str) -> dict[str, Any]:
    size = path.stat().st_size
    require(size <= MAX_ARTIFACT_BYTES, f"{name} exceeds the byte limit")
    return require_mapping(json.loads(path.read_text()), name)


def require_integer(value: Any, name: str, minimum: int = 0) -> int:
    require(type(value) is int and value >= minimum, f"{name} is invalid")
    return value


def fnv_byte(digest: int, value: int) -> int:
    return ((digest ^ value) * FNV1A64_PRIME) & UINT64_MAX


def fnv_uint64(digest: int, value: int) -> int:
    require_integer(value, "record-locator integer")
    require(value <= UINT64_MAX, "record-locator integer exceeds uint64")
    for _ in range(8):
        digest = fnv_byte(digest, value & 0xFF)
        value >>= 8
    return digest


def calculate_record_locator(cases: list[Any]) -> str:
    digest = FNV1A64_OFFSET_BASIS
    for mode_index, case_value in enumerate(cases):
        case = require_mapping(case_value, "locator case")
        digest = fnv_uint64(digest, mode_index)
        digest = fnv_uint64(
            digest,
            require_integer(case.get("evaluation_worker_count"), "worker count"),
        )
        digest = fnv_uint64(
            digest, 1 if case.get("population_recycling") is True else 0
        )
        for seed_value in require_list(case.get("seeds"), "locator seeds"):
            seed = require_mapping(seed_value, "locator seed")
            seed_hex = seed.get("seed_hex")
            require(isinstance(seed_hex, str), "locator seed identity is invalid")
            digest = fnv_uint64(digest, int(seed_hex, 16))
            result = require_mapping(seed.get("result"), "locator result")
            genome_hex = result.get("best_genome_hex")
            require(isinstance(genome_hex, str), "locator genome is invalid")
            for byte in bytes.fromhex(genome_hex):
                digest = fnv_byte(digest, byte)
            for sample_value in require_list(
                seed.get("raw_samples"), "locator samples"
            ):
                sample = require_mapping(sample_value, "locator sample")
                digest = fnv_uint64(
                    digest,
                    require_integer(sample.get("wall_time_ns"), "wall sample"),
                )
                digest = fnv_uint64(
                    digest,
                    require_integer(sample.get("cpu_clock_ticks"), "CPU sample"),
                )
    return f"{digest:016x}"


def validate(path: Path) -> dict[str, Any]:
    document = load_bounded_json(path, "document")
    schema_path = Path(__file__).with_name("evo-core-benchmark-v1.schema.json")
    schema = load_bounded_json(schema_path, "schema document")

    require(
        document.get("schema") == "catalyst.evo-core-benchmark.v1",
        "unexpected benchmark schema",
    )
    require(document.get("schema_version") == "1.0.0", "unexpected schema version")
    require(
        schema.get("properties", {}).get("schema", {}).get("const")
        == document["schema"],
        "schema identity and artifact differ",
    )
    require(
        schema.get("properties", {}).get("schema_version", {}).get("const")
        == document["schema_version"],
        "schema version and artifact differ",
    )
    require(document.get("benchmark_id") == "EVO-CORE-001", "unexpected benchmark id")
    tier = document.get("tier")
    require(tier in {"smoke", "extended"}, "invalid tier")
    require(document.get("correctness_passed") is True, "correctness gate failed")

    evo = require_mapping(document.get("evo"), "EVO identity")
    require(evo.get("version") == "0.36.0", "unexpected EVO version")
    require(
        isinstance(evo.get("commit"), str)
        and re.fullmatch(r"[0-9a-f]{40}", evo["commit"]) is not None,
        "benchmark evidence requires an immutable 40-hex commit",
    )
    require(set(evo["commit"]) != {"0"}, "zero commit sentinel is not evidence")
    environment = require_mapping(document.get("environment"), "environment")
    for field in (
        "platform",
        "architecture",
        "compiler_family",
        "compiler_version",
        "linker",
        "build_frontend",
        "build_profile",
    ):
        require(
            isinstance(environment.get(field), str) and environment[field],
            f"environment.{field} is missing",
        )
    require(environment.get("c_standard") == 201710, "unexpected C standard")
    require_integer(environment.get("size_t_bits"), "size_t bit width", 16)
    require_integer(
        environment.get("clock_ticks_per_second"), "clock ticks per second", 1
    )
    require_integer(
        environment.get("process_peak_resident_native"), "process peak RSS"
    )
    require(
        environment.get("process_peak_resident_unit") in {"bytes", "kibibytes"},
        "invalid process peak RSS unit",
    )

    locator = require_mapping(document.get("record_locator"), "record locator")
    require(locator.get("authoritative") is False, "record locator became authority")
    require(
        locator.get("algorithm") == "fnv1a64-navigation-only",
        "record locator role changed",
    )
    require(
        locator.get("scope") == "mode-seed-best-genome-and-raw-timing-fields",
        "record locator scope changed",
    )
    require(
        isinstance(locator.get("value"), str)
        and re.fullmatch(r"[0-9a-f]{16}", locator["value"]) is not None,
        "record locator is malformed",
    )

    policy = require_mapping(document.get("measurement_policy"), "measurement policy")
    expected_warmups, expected_repetitions = EXPECTED_TIER_COUNTS[tier]
    require(
        policy.get("warmup_runs_per_case_seed") == expected_warmups,
        "tier warmup count changed",
    )
    repetitions = require_integer(
        policy.get("measured_repetitions_per_case_seed"), "repetition count", 1
    )
    require(repetitions == expected_repetitions, "tier repetition count changed")
    require(policy.get("sample_order") == "case-seed-repetition", "sample order changed")
    require(
        policy.get("wall_timer") == "posix-clock-monotonic-nanoseconds",
        "wall timer changed",
    )
    require(policy.get("cpu_timer") == "iso-c-clock-ticks", "CPU timer changed")
    require(
        policy.get("timed_region") == "evo_run-only-result-destruction-excluded",
        "timed region changed",
    )
    require(policy.get("runtime_thresholds_enforced") is False, "runtime became a gate")
    require(
        policy.get("cross_machine_equivalence_claimed") is False,
        "cross-machine equivalence was claimed",
    )
    require(
        policy.get("aggregation")
        == "markdown-only-min-lower-median-max-from-raw-samples",
        "aggregation policy changed",
    )
    require(
        policy.get("platform_tolerance")
        == "runtime-and-rss-are-reporting-only; deterministic-result-fields-must-match-exactly",
        "platform tolerance changed",
    )

    memory_policy = require_mapping(document.get("memory_policy"), "memory policy")
    require(
        memory_policy
        == {
            "exact_model": "library-requested-calloc-bytes",
            "exact_model_includes": [
                "population-genomes",
                "candidate-evaluations",
                "owned-best-genome",
                "worker-scratch",
            ],
            "exact_model_excludes": [
                "allocator-overhead",
                "thread-stacks",
                "consumer-context",
                "process-runtime",
            ],
            "rss_scope": "complete-benchmark-process",
            "rss_authoritative": False,
        },
        "memory policy changed",
    )

    hra = require_mapping(
        document.get("human_readable_abstraction"), "abstraction projection"
    )
    require(
        hra
        == {
            "accelerated_authority": False,
            "canonical_authority": "ordered explicit cases, seeds, traces, and raw samples",
            "summary_projection": "companion Markdown from these records",
            "locator_role": "navigation only",
        },
        "abstraction projection changed",
    )

    workload = require_mapping(document.get("workload"), "workload")
    require(
        workload
        == {
            "name": "byte-onemax-v1",
            "genome_representation": "explicit-16-byte-array",
            "fitness_definition": "maximize-set-bits; correctness=total=bit-count",
            "population_size": 32,
            "generation_limit": 12,
            "seed_count": len(EXPECTED_SEEDS[tier]),
            "validity": "all-candidates-valid",
            "stopping": "generation-limit-only",
        },
        "workload changed",
    )
    generation_limit = 12
    seed_count = len(EXPECTED_SEEDS[tier])
    common_policy = require_mapping(document.get("common_policy"), "common policy")
    require(
        common_policy.get("selection")
        == {"kind": "stable-rank", "version": 1, "base_weight": 1, "step_weight": 1},
        "selection policy changed",
    )
    require(
        common_policy.get("crossover")
        == {"kind": "uniform-byte", "version": 1, "rate": 0.75},
        "crossover policy changed",
    )
    require(
        common_policy.get("mutation")
        == {"kind": "nonzero-byte-xor", "version": 1, "initial_rate": 0.35},
        "mutation policy changed",
    )
    require(
        common_policy.get("adaptive_mutation")
        == {
            "enabled": True,
            "version": 1,
            "min_rate": 0.1,
            "max_rate": 0.8,
            "step": 0.1,
            "diversity_threshold": 0.35,
            "reset_on_improvement": True,
        },
        "adaptive mutation policy changed",
    )
    require(
        common_policy.get("elite") == {"enabled": True, "version": 1, "count": 2},
        "elite policy changed",
    )
    require(
        common_policy.get("diversity")
        == {"kind": "byte-mismatch", "policy_version": 1, "metric_version": 1},
        "diversity policy changed",
    )
    require(
        common_policy.get("rng")
        == {
            "algorithm": "pcg-xsh-rr",
            "algorithm_version": 1,
            "operator_seed_schedule_version": 1,
        },
        "RNG policy changed",
    )
    resource_budgets = require_mapping(
        common_policy.get("resource_budgets"), "resource budgets"
    )
    evaluation_bytes = require_integer(
        resource_budgets.get("max_evaluation_bytes"), "evaluation byte budget", 1
    )
    require(
        resource_budgets
        == {
            "max_genome_bytes": 16,
            "max_population_bytes": 512,
            "max_evaluation_bytes": evaluation_bytes,
            "max_child_population_bytes": 512,
            "max_diversity_work": 7936,
            "max_checkpoint_bytes": 0,
        },
        "resource budgets changed",
    )
    require(
        common_policy.get("callback_policy")
        == {
            "initialize": "deterministic-no-op",
            "validity": "all-valid",
            "evaluation": "thread-safe-byte-popcount",
            "generation_observer": True,
            "all_other_observers": False,
        },
        "callback policy changed",
    )
    for disabled_control in (
        "secure_erasure_enabled",
        "fitness_target_enabled",
        "stagnation_enabled",
        "diversity_floor_enabled",
        "checkpointing_enabled",
    ):
        require(common_policy.get(disabled_control) is False, f"{disabled_control} changed")
    require(len(common_policy) == 14, "common policy contains unknown fields")

    cases = require_list(document.get("cases"), "cases")
    require(len(cases) == len(EXPECTED_CASES), "case count changed")
    canonical_seed_order: list[str] | None = None
    serial_results: dict[str, tuple[dict[str, Any], list[Any]]] = {}
    for case_index, expected in enumerate(EXPECTED_CASES):
        case = require_mapping(cases[case_index], f"case {case_index}")
        case_id, parallel, recycling = expected
        require(case.get("case_id") == case_id, f"case {case_index} is out of order")
        require(case.get("parallel_evaluation") is parallel, f"{case_id} parallel mismatch")
        require(case.get("population_recycling") is recycling, f"{case_id} recycling mismatch")
        expected_workers = 4 if parallel else 0
        require(
            case.get("evaluation_worker_count") == expected_workers,
            f"{case_id} worker count changed",
        )
        worker_scratch = require_integer(
            case.get("worker_scratch_bytes"), f"{case_id} worker scratch"
        )
        require((worker_scratch > 0) is parallel, f"{case_id} scratch policy changed")
        require(case.get("correctness_passed") is True, f"{case_id} correctness failed")
        owner_bytes = 512 + evaluation_bytes
        expected_peak = 2 * owner_bytes + 16 + worker_scratch
        owner_allocations = 2 if recycling else generation_limit + 1
        expected_total = (
            owner_allocations * owner_bytes
            + 16
            + (generation_limit + 1) * worker_scratch
        )
        require(
            case.get("requested_heap_peak_bytes") == expected_peak,
            f"{case_id} peak memory model changed",
        )
        require(
            case.get("requested_heap_total_bytes") == expected_total,
            f"{case_id} total memory model changed",
        )

        seeds = require_list(case.get("seeds"), f"{case_id} seeds")
        require(len(seeds) == seed_count, f"{case_id} seed count mismatch")
        seed_order: list[str] = []
        for seed_index, seed_value in enumerate(seeds):
            seed = require_mapping(seed_value, f"{case_id} seed {seed_index}")
            seed_hex = seed.get("seed_hex")
            require(
                isinstance(seed_hex, str)
                and re.fullmatch(r"[0-9a-f]{16}", seed_hex) is not None,
                "bad seed identity",
            )
            seed_order.append(seed_hex)
            require(seed.get("cross_mode_passed") is True, "cross-mode mismatch")

            oracle = require_mapping(seed.get("oracle"), "oracle")
            require(isinstance(oracle.get("applicable"), bool), "oracle applicability missing")
            require(oracle.get("passed") is True, "fixed oracle mismatch")

            result = require_mapping(seed.get("result"), "result")
            require(result.get("random_seed_hex") == seed_hex, "result seed mismatch")
            require(
                result.get("generations_completed") == generation_limit,
                "generation count mismatch",
            )
            require(result.get("termination_reason") == "generation-limit", "bad termination")
            require_mapping(result.get("best_fitness"), "best fitness")
            require_mapping(result.get("final_statistics"), "final statistics")

            trace = require_list(seed.get("generation_trace"), "generation trace")
            require(len(trace) == generation_limit + 1, "trace is incomplete")
            for generation_index, generation_value in enumerate(trace):
                generation = require_mapping(generation_value, "generation record")
                require(
                    generation.get("generation_index") == generation_index,
                    "generation trace is out of order",
                )

            if oracle["applicable"]:
                oracle_expected = require_mapping(
                    oracle.get("expected"), "explicit oracle"
                )
                require(
                    oracle_expected.get("best_genome_hex")
                    == result.get("best_genome_hex"),
                    "oracle best genome differs from result",
                )
                require(
                    oracle_expected.get("best_total")
                    == result["best_fitness"].get("total"),
                    "oracle best total differs from result",
                )
                require(
                    oracle_expected.get("generations_completed")
                    == result.get("generations_completed"),
                    "oracle generation count differs from result",
                )
                require(
                    oracle_expected.get("termination_reason")
                    == result.get("termination_reason"),
                    "oracle termination differs from result",
                )
                expected_trace = require_list(
                    oracle_expected.get("trace"), "explicit oracle trace"
                )
                require(len(expected_trace) == len(trace), "oracle trace is incomplete")
                for trace_index, expected_value in enumerate(expected_trace):
                    expected_generation = require_mapping(
                        expected_value, "explicit oracle generation"
                    )
                    actual_generation = trace[trace_index]
                    for field in (
                        "generation_index",
                        "global_best_total",
                        "generation_best_total",
                        "diversity_scaled_1e6",
                    ):
                        require(
                            expected_generation.get(field)
                            == actual_generation.get(field),
                            f"oracle generation {trace_index} {field} differs",
                        )
            else:
                require(oracle.get("expected") is None, "unexpected hidden oracle")

            if case_index == 0:
                serial_results[seed_hex] = (result, trace)
            else:
                serial_result, serial_trace = serial_results[seed_hex]
                require(result == serial_result, "case result differs from serial authority")
                require(trace == serial_trace, "case trace differs from serial authority")

            samples = require_list(seed.get("raw_samples"), "raw samples")
            require(len(samples) == repetitions, "raw sample count mismatch")
            for repetition, sample_value in enumerate(samples):
                sample = require_mapping(sample_value, "raw sample")
                require(sample.get("repetition") == repetition, "samples are out of order")
                require(sample.get("matches_seed_reference") is True, "replay mismatch")
                require(sample.get("matches_serial_reference") is True, "serial mismatch")
                require(
                    require_integer(sample.get("wall_time_ns"), "wall sample")
                    <= UINT64_MAX,
                    "wall sample exceeds uint64",
                )
                require(
                    require_integer(sample.get("cpu_clock_ticks"), "CPU sample")
                    <= UINT64_MAX,
                    "CPU sample exceeds uint64",
                )

        if canonical_seed_order is None:
            canonical_seed_order = seed_order
        else:
            require(seed_order == canonical_seed_order, "case seed order differs")
    require(canonical_seed_order == EXPECTED_SEEDS[tier], "tier seed order changed")
    require(
        locator["value"] == calculate_record_locator(cases),
        "record locator differs from its declared scope",
    )

    return document


def render_markdown(document: dict[str, Any]) -> str:
    locator = document["record_locator"]["value"]
    evo = document["evo"]
    lines = [
        "# EVO Core Benchmark Summary",
        "",
        f"- Schema: `catalyst.evo-core-benchmark.v1` (`{document['schema_version']}`)",
        f"- Benchmark: `{document['benchmark_id']}` / `{document['tier']}` tier",
        f"- EVO: `{evo['version']}` at `{evo['commit']}`",
        "- Correctness: **PASS**",
        f"- Record locator: `{locator}` (navigation only; not authority)",
        "",
        "The canonical authority is the companion JSON's explicit ordered cases, seeds, "
        "generation traces, and raw samples. This summary was generated by parsing and "
        "validating that JSON. No aggregate, locator, cache, or timing threshold can "
        "override a correctness field.",
        "",
        "## Configuration Comparison",
        "",
        "| Case | Correctness | Samples | Wall min (ns) | Wall lower median (ns) | "
        "Wall max (ns) | Requested heap peak (bytes) | Requested heap total (bytes) |",
        "|---|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for case in document["cases"]:
        values = sorted(
            sample["wall_time_ns"]
            for seed in case["seeds"]
            for sample in seed["raw_samples"]
        )
        lower_median = values[(len(values) - 1) // 2]
        lines.append(
            f"| `{case['case_id']}` | PASS | {len(values)} | {values[0]} | "
            f"{lower_median} | {values[-1]} | {case['requested_heap_peak_bytes']} | "
            f"{case['requested_heap_total_bytes']} |"
        )

    lines.extend(
        [
            "",
            "Runtime and process RSS are reporting-only and vary by platform. The exact "
            "heap model covers EVO's requested population-genome, candidate-evaluation, "
            "result-genome, and worker-scratch bytes; it excludes allocator overhead, "
            "thread stacks, caller context, and the process runtime.",
            "",
            "## Search-Quality Projection",
            "",
            "| Case | Seed | Initial global best | Final global best | Final diversity | "
            "Termination |",
            "|---|---:|---:|---:|---:|---|",
        ]
    )
    for case in document["cases"]:
        for seed in case["seeds"]:
            trace = seed["generation_trace"]
            lines.append(
                f"| `{case['case_id']}` | `0x{seed['seed_hex']}` | "
                f"{trace[0]['global_best_total']:.0f} | "
                f"{trace[-1]['global_best_total']:.0f} | "
                f"{trace[-1]['diversity']:.6f} | "
                f"`{seed['result']['termination_reason']}` |"
            )

    environment = document["environment"]
    lines.extend(
        [
            "",
            f"Process peak resident set: {environment['process_peak_resident_native']} "
            f"{environment['process_peak_resident_unit']}. Full ordered generation and "
            "sample evidence remains in the JSON artifact.",
            "",
        ]
    )
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--program", type=Path)
    parser.add_argument("--tier", choices=("smoke", "extended"))
    parser.add_argument("--commit")
    parser.add_argument("--linker")
    parser.add_argument("--json", required=True, type=Path)
    parser.add_argument("--markdown", required=True, type=Path)
    args = parser.parse_args()
    try:
        if args.program is not None:
            require(args.tier is not None, "--tier is required with --program")
            require(args.commit is not None, "--commit is required with --program")
            require(args.linker is not None, "--linker is required with --program")
            completed = subprocess.run(
                [
                    str(args.program),
                    "--tier",
                    args.tier,
                    "--commit",
                    args.commit,
                    "--linker",
                    args.linker,
                ],
                check=False,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                timeout=120 if args.tier == "smoke" else 900,
            )
            require(
                len(completed.stdout.encode()) <= MAX_ARTIFACT_BYTES,
                "benchmark output exceeds the byte limit",
            )
            args.json.write_text(completed.stdout)
            require(
                completed.returncode == 0,
                "benchmark executable failed: " + completed.stderr.strip(),
            )
        else:
            require(
                args.tier is None and args.commit is None and args.linker is None,
                "execution metadata requires --program",
            )
        document = validate(args.json)
        if args.program is not None:
            require(document["tier"] == args.tier, "program returned the wrong tier")
            require(
                document["evo"]["commit"] == args.commit,
                "program returned the wrong commit identity",
            )
            require(
                document["environment"]["linker"] == args.linker,
                "program returned the wrong linker identity",
            )
        args.markdown.write_text(render_markdown(document))
    except (
        OSError,
        UnicodeDecodeError,
        json.JSONDecodeError,
        subprocess.TimeoutExpired,
        ValueError,
    ) as error:
        print(f"benchmark artifact validation failed: {error}")
        return 1
    print("benchmark artifact validation and projection passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
