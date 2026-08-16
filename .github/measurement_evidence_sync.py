from pathlib import Path
import json


def replace(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text()
    if old not in text:
        raise SystemExit(f"missing evidence anchor in {path}: {old[:160]!r}")
    p.write_text(text.replace(old, new, 1))


# Public-to-private name helpers keep policy projections stable and readable.
replace(
    "src/internal/project_measurement.h",
    "const char *evo_project_measurement_phase_name(evo_project_measurement_phase_t phase);\n"
    "const char *evo_project_measurement_comparison_name(evo_project_measurement_comparison_t comparison);",
    "const char *evo_project_measurement_phase_name(evo_project_measurement_phase_t phase);\n"
    "const char *evo_project_measurement_order_name(evo_project_measurement_order_t order);\n"
    "const char *evo_project_measurement_outlier_policy_name(\n"
    "    evo_project_measurement_outlier_policy_t policy);\n"
    "const char *evo_project_measurement_comparison_name(evo_project_measurement_comparison_t comparison);",
)

replace(
    "src/project_measurement_model.c",
    "const char *evo_project_measurement_comparison_name(\n"
    "    evo_project_measurement_comparison_t comparison)\n"
    "{",
    "const char *evo_project_measurement_order_name(\n"
    "    evo_project_measurement_order_t order)\n"
    "{\n"
    "    switch (order) {\n"
    "    case EVO_PROJECT_MEASUREMENT_ALTERNATE_BASELINE_FIRST:\n"
    "        return \"alternate-baseline-first\";\n"
    "    case EVO_PROJECT_MEASUREMENT_ALTERNATE_CANDIDATE_FIRST:\n"
    "        return \"alternate-candidate-first\";\n"
    "    default:\n"
    "        return \"unknown\";\n"
    "    }\n"
    "}\n\n"
    "const char *evo_project_measurement_outlier_policy_name(\n"
    "    evo_project_measurement_outlier_policy_t policy)\n"
    "{\n"
    "    switch (policy) {\n"
    "    case EVO_PROJECT_MEASUREMENT_OUTLIER_NONE:\n"
    "        return \"none\";\n"
    "    case EVO_PROJECT_MEASUREMENT_OUTLIER_ABSOLUTE_MEDIAN:\n"
    "        return \"absolute-median\";\n"
    "    default:\n"
    "        return \"unknown\";\n"
    "    }\n"
    "}\n\n"
    "const char *evo_project_measurement_comparison_name(\n"
    "    evo_project_measurement_comparison_t comparison)\n"
    "{",
)

# Make the canonical workload record sufficient to reconstruct every aggregate
# and the final scalar fitness.
replace(
    "src/project_measurement_runtime.c",
    "            !evo_candidate_buffer_append_size(\n"
    "                json, policy->minimum_included_repetitions) ||\n"
    "            !evo_candidate_buffer_append_text(json, \",\\\"max_runtime_range_ppm\\\":\") ||",
    "            !evo_candidate_buffer_append_size(\n"
    "                json, policy->minimum_included_repetitions) ||\n"
    "            !evo_candidate_buffer_append_text(json, \",\\\"order\\\":\") ||\n"
    "            !evo_candidate_buffer_append_json_string(\n"
    "                json, evo_project_measurement_order_name(policy->order)) ||\n"
    "            !evo_candidate_buffer_append_text(json, \",\\\"outlier_policy\\\":\") ||\n"
    "            !evo_candidate_buffer_append_json_string(\n"
    "                json,\n"
    "                evo_project_measurement_outlier_policy_name(\n"
    "                    policy->outlier_policy)) ||\n"
    "            !evo_candidate_buffer_append_text(\n"
    "                json, \",\\\"outlier_deviation_ns\\\":\") ||\n"
    "            !evo_candidate_buffer_append_u64(\n"
    "                json, policy->outlier_deviation_ns) ||\n"
    "            !evo_candidate_buffer_append_text(json, \",\\\"max_runtime_range_ppm\\\":\") ||",
)

replace(
    "src/project_measurement_runtime.c",
    "            !evo_measurement_append_u32(json, policy->minimum_improvement_ppm) ||\n"
    "            !evo_candidate_buffer_append_text(json, \",\\\"baseline\\\":\") ||",
    "            !evo_measurement_append_u32(json, policy->minimum_improvement_ppm) ||\n"
    "            !evo_candidate_buffer_append_text(json, \",\\\"timeout_ms\\\":\") ||\n"
    "            !evo_candidate_buffer_append_u64(json, policy->timeout_ms) ||\n"
    "            !evo_candidate_buffer_append_text(json, \",\\\"workload_weight\\\":\") ||\n"
    "            !evo_measurement_append_double(json, policy->workload_weight) ||\n"
    "            !evo_candidate_buffer_append_text(\n"
    "                json, \",\\\"peak_memory_mix_weight\\\":\") ||\n"
    "            !evo_measurement_append_double(\n"
    "                json, policy->peak_memory_mix_weight) ||\n"
    "            !evo_candidate_buffer_append_text(\n"
    "                json, \",\\\"binary_size_mix_weight\\\":\") ||\n"
    "            !evo_measurement_append_double(\n"
    "                json, policy->binary_size_mix_weight) ||\n"
    "            !evo_candidate_buffer_append_text(json, \",\\\"baseline\\\":\") ||",
)

runtime_path = Path("src/project_measurement_runtime.c")
runtime = runtime_path.read_text()
start = runtime.find("static bool evo_measurement_build_markdown(\n")
end = runtime.find("\nbool evo_measurement_build_evidence(\n", start)
if start < 0 or end < 0:
    raise SystemExit("measurement Markdown function boundary missing")

markdown_impl = r'''static bool evo_measurement_append_fitness_row(
    evo_candidate_buffer_t *markdown,
    const char *name,
    double value,
    double weight,
    bool subtract)
{
    char row[256];
    const double contribution = subtract ? -(value * weight) : value * weight;
    const int written = evo_project_format(
        row,
        sizeof(row),
        "| %s | %.17g | %.17g | %.17g |\n",
        name,
        value,
        weight,
        contribution);

    return written > 0 && (size_t)written < sizeof(row) &&
           evo_candidate_buffer_append_text(markdown, row);
}

static bool evo_measurement_build_markdown(
    const evo_project_measurement_config_t *config,
    const evo_project_measurement_owner_t *owner,
    evo_candidate_buffer_t *markdown)
{
    size_t index;
    char total_row[128];
    int total_written;

    if (!evo_candidate_buffer_append_text(
            markdown, "# EVO Candidate Measurement and Fitness\n\n") ||
        !evo_candidate_buffer_append_text(markdown, "- Candidate: `") ||
        !evo_candidate_buffer_append_text(markdown, owner->view.candidate_fingerprint) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Assurance: `") ||
        !evo_candidate_buffer_append_text(markdown, owner->view.assurance_fingerprint) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Baseline: `") ||
        !evo_candidate_buffer_append_text(markdown, owner->view.baseline_identity) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Policy: `") ||
        !evo_candidate_buffer_append_text(markdown, owner->view.policy_id) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Policy fingerprint: `") ||
        !evo_candidate_buffer_append_text(markdown, owner->view.policy_fingerprint) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Measurement provider: `") ||
        !evo_candidate_buffer_append_text(
            markdown, owner->view.measurement_provider_identity) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Condition fingerprint: `") ||
        !evo_candidate_buffer_append_text(markdown, owner->view.condition_fingerprint) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Hardware: `") ||
        !evo_candidate_buffer_append_text(markdown, config->condition.hardware_identity) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Operating system: `") ||
        !evo_candidate_buffer_append_text(
            markdown, config->condition.operating_system_identity) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Compiler: `") ||
        !evo_candidate_buffer_append_text(markdown, config->condition.compiler_identity) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Linker: `") ||
        !evo_candidate_buffer_append_text(markdown, config->condition.linker_identity) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Environment: `") ||
        !evo_candidate_buffer_append_text(
            markdown, config->condition.environment_identity) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Dataset: `") ||
        !evo_candidate_buffer_append_text(markdown, config->condition.dataset_identity) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Baseline binary: `") ||
        !evo_candidate_buffer_append_text(
            markdown, config->condition.baseline_binary_identity) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Candidate binary: `") ||
        !evo_candidate_buffer_append_text(
            markdown, config->condition.candidate_binary_identity) ||
        !evo_candidate_buffer_append_text(markdown, "`\n- Overall comparison: **") ||
        !evo_candidate_buffer_append_text(
            markdown,
            evo_project_measurement_comparison_name(owner->view.overall_comparison)) ||
        !evo_candidate_buffer_append_text(markdown, "**\n- Fitness available: **") ||
        !evo_candidate_buffer_append_text(
            markdown, owner->view.fitness_available ? "yes" : "no") ||
        !evo_candidate_buffer_append_text(
            markdown,
            "**\n\nCorrectness authority is unchanged from candidate assurance; performance evidence cannot alter it.\n\n") ||
        !evo_candidate_buffer_append_text(
            markdown,
            "## Workload policy\n\n| Workload | Order | Warmups | Repetitions | Min included | Outlier policy | Outlier deviation ns | Max range ppm | Tolerance ppm | Min improvement ppm | Timeout ms | Weight | Peak-memory mix | Binary-size mix |\n|---|---|---:|---:|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|\n")) {
        return false;
    }

    for (index = 0U; index < config->workload_count; index += 1U) {
        const evo_project_measurement_workload_policy_t *policy =
            &config->workloads[index];
        char row[768];
        const int written = evo_project_format(
            row,
            sizeof(row),
            "| %s | %s | %zu | %zu | %zu | %s | %llu | %u | %u | %u | %llu | %.17g | %.17g | %.17g |\n",
            policy->workload_id,
            evo_project_measurement_order_name(policy->order),
            policy->warmup_count,
            policy->repetition_count,
            policy->minimum_included_repetitions,
            evo_project_measurement_outlier_policy_name(policy->outlier_policy),
            (unsigned long long)policy->outlier_deviation_ns,
            policy->max_runtime_range_ppm,
            policy->comparison_tolerance_ppm,
            policy->minimum_improvement_ppm,
            (unsigned long long)policy->timeout_ms,
            policy->workload_weight,
            policy->peak_memory_mix_weight,
            policy->binary_size_mix_weight);

        if (written <= 0 || (size_t)written >= sizeof(row) ||
            !evo_candidate_buffer_append_text(markdown, row)) {
            return false;
        }
    }

    if (!evo_candidate_buffer_append_text(
            markdown,
            "\n## Workload results\n\n| Workload | Comparison | Baseline ns | Candidate ns | Baseline range ppm | Candidate range ppm | Included B/C | Runtime improvement | Memory improvement | Reliability improvement | Maintainability improvement |\n|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n")) {
        return false;
    }

    for (index = 0U; index < owner->view.workload_count; index += 1U) {
        const evo_project_measurement_workload_result_t *result =
            &owner->workloads[index];
        char row[768];
        const int written = evo_project_format(
            row,
            sizeof(row),
            "| %s | %s | %llu | %llu | %u | %u | %zu/%zu | %.17g | %.17g | %.17g | %.17g |\n",
            result->workload_id,
            evo_project_measurement_comparison_name(result->comparison),
            (unsigned long long)result->baseline.runtime_ns,
            (unsigned long long)result->candidate.runtime_ns,
            result->baseline.runtime_range_ppm,
            result->candidate.runtime_range_ppm,
            result->baseline.included_count,
            result->candidate.included_count,
            result->runtime_improvement,
            result->memory_improvement,
            result->reliability_improvement,
            result->maintainability_improvement);

        if (written <= 0 || (size_t)written >= sizeof(row) ||
            !evo_candidate_buffer_append_text(markdown, row)) {
            return false;
        }
    }

    if (!evo_candidate_buffer_append_text(
            markdown,
            "\n## Raw sample trace\n\n| Seq | Pair | Workload | Phase | Subject | Completed | Timed out | Failed | Condition | Runtime ns | Memory bytes | Binary bytes | Reliability ppm | Maintainability ppm | Exclusion |\n|---:|---:|---|---|---|---|---|---|---|---:|---:|---:|---:|---:|---|\n")) {
        return false;
    }
    for (index = 0U; index < owner->view.sample_count; index += 1U) {
        const evo_project_measurement_sample_t *sample = &owner->samples[index];
        char row[1024];
        char condition[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
        const char *exclusion = sample->excluded
                                    ? (sample->exclusion_reason == NULL
                                           ? "excluded"
                                           : sample->exclusion_reason)
                                    : "none";
        int written;

        evo_project_fingerprint_format(sample->condition_fingerprint, condition);
        written = evo_project_format(
            row,
            sizeof(row),
            "| %zu | %zu | %s | %s | %s | %s | %s | %s | %s | %llu | %llu | %llu | %u | %u | %s |\n",
            sample->sequence_index,
            sample->pair_index,
            sample->workload_id,
            evo_project_measurement_phase_name(sample->phase),
            evo_project_measurement_subject_name(sample->subject),
            sample->completed ? "yes" : "no",
            sample->timed_out ? "yes" : "no",
            sample->failed ? "yes" : "no",
            condition,
            (unsigned long long)sample->runtime_ns,
            (unsigned long long)sample->peak_memory_bytes,
            (unsigned long long)sample->binary_size_bytes,
            sample->reliability_ppm,
            sample->maintainability_ppm,
            exclusion);

        if (written <= 0 || (size_t)written >= sizeof(row) ||
            !evo_candidate_buffer_append_text(markdown, row)) {
            return false;
        }
    }

    if (!evo_candidate_buffer_append_text(
            markdown,
            "\n## Fitness\n\n| Component | Value | Weight | Contribution |\n|---|---:|---:|---:|\n") ||
        !evo_measurement_append_fitness_row(
            markdown,
            "correctness",
            owner->view.fitness.correctness,
            config->fitness_weights.correctness,
            false) ||
        !evo_measurement_append_fitness_row(
            markdown,
            "performance",
            owner->view.fitness.performance,
            config->fitness_weights.performance,
            false) ||
        !evo_measurement_append_fitness_row(
            markdown,
            "memory_use",
            owner->view.fitness.memory_use,
            config->fitness_weights.memory_use,
            false) ||
        !evo_measurement_append_fitness_row(
            markdown,
            "reliability",
            owner->view.fitness.reliability,
            config->fitness_weights.reliability,
            false) ||
        !evo_measurement_append_fitness_row(
            markdown,
            "maintainability",
            owner->view.fitness.maintainability,
            config->fitness_weights.maintainability,
            false) ||
        !evo_measurement_append_fitness_row(
            markdown,
            "constraint_penalty",
            owner->view.fitness.constraint_penalty,
            config->fitness_weights.constraint_penalty,
            true)) {
        return false;
    }

    total_written = evo_project_format(
        total_row,
        sizeof(total_row),
        "\nRecorded scalar total: `%.17g`\n\n",
        owner->view.fitness.total);
    if (total_written <= 0 || (size_t)total_written >= sizeof(total_row) ||
        !evo_candidate_buffer_append_text(markdown, total_row) ||
        !evo_candidate_buffer_append_text(
            markdown,
            "The scalar total is the sum of the recorded component contributions above. No default consumer objective is supplied.\n\n") ||
        !evo_candidate_buffer_append_text(
            markdown,
            "A later selected result may be described only as the **best verified candidate found within the recorded bounded search contract**; this evidence does not claim a globally optimal program.\n")) {
        return false;
    }
    return true;
}
'''

runtime_path.write_text(runtime[:start] + markdown_impl + runtime[end:])

# Schema: workload records must contain every field needed to replay policy and
# reconstruct the scalar ranking authority.
schema_path = Path("docs/schemas/evo-project-measurement-v1.schema.json")
schema = json.loads(schema_path.read_text())
workload = schema["$defs"]["workload"]
required = workload["required"]
for field in (
    "order",
    "outlier_policy",
    "outlier_deviation_ns",
    "timeout_ms",
    "workload_weight",
    "peak_memory_mix_weight",
    "binary_size_mix_weight",
):
    if field not in required:
        insert_at = required.index("max_runtime_range_ppm")
        if field in ("timeout_ms", "workload_weight", "peak_memory_mix_weight", "binary_size_mix_weight"):
            insert_at = required.index("baseline")
        required.insert(insert_at, field)
props = workload["properties"]
props["order"] = {
    "enum": ["alternate-baseline-first", "alternate-candidate-first"]
}
props["outlier_policy"] = {"enum": ["none", "absolute-median"]}
props["outlier_deviation_ns"] = {"type": "integer", "minimum": 0}
props["timeout_ms"] = {"type": "integer", "minimum": 1}
props["workload_weight"] = {"type": "number", "exclusiveMinimum": 0}
props["peak_memory_mix_weight"] = {"type": "number", "minimum": 0}
props["binary_size_mix_weight"] = {"type": "number", "minimum": 0}
schema_path.write_text(json.dumps(schema, indent=2) + "\n")

# Normative fixture checks both canonical and human-readable reconstructibility.
test_path = Path("tests/project_measurement_test.c")
test = test_path.read_text()
needle = '''    CHECK(strstr(
              measurement.canonical_json,
              "\\\"correctness_preserved\\\":true") != NULL);
    CHECK(strstr(
              measurement.audit_markdown,
              "best verified candidate found within the recorded bounded search contract") !=
          NULL);'''
replacement = '''    CHECK(strstr(
              measurement.canonical_json,
              "\\\"correctness_preserved\\\":true") != NULL);
    CHECK(strstr(
              measurement.canonical_json,
              "\\\"order\\\":\\\"alternate-baseline-first\\\"") != NULL);
    CHECK(strstr(
              measurement.canonical_json,
              "\\\"outlier_policy\\\":\\\"none\\\"") != NULL ||
          mode == FAKE_OUTLIER);
    CHECK(strstr(measurement.canonical_json, "\\\"workload_weight\\\":1") != NULL);
    CHECK(strstr(
              measurement.canonical_json,
              "\\\"peak_memory_mix_weight\\\":1") != NULL);
    CHECK(strstr(
              measurement.canonical_json,
              "\\\"binary_size_mix_weight\\\":1") != NULL);
    CHECK(strstr(measurement.audit_markdown, "Measurement provider") != NULL);
    CHECK(strstr(measurement.audit_markdown, "hardware:test-host") != NULL);
    CHECK(strstr(measurement.audit_markdown, "| performance |") != NULL);
    CHECK(strstr(
              measurement.audit_markdown,
              "best verified candidate found within the recorded bounded search contract") !=
          NULL);'''
if needle not in test:
    raise SystemExit("measurement fixture evidence assertion anchor missing")
test_path.write_text(test.replace(needle, replacement, 1))

# Structural validator locks the evidence-completeness acceptance criterion.
validator_path = Path("tests/validate_project_measurement.py")
validator = validator_path.read_text()
needle = '''    require("best verified candidate found within the recorded bounded search contract" in runtime, "bounded result wording missing")
    require("config->limits.max_evidence_bytes" in runtime, "evidence buffer is not resource bounded")'''
replacement = '''    require("best verified candidate found within the recorded bounded search contract" in runtime, "bounded result wording missing")
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
    require("config->limits.max_evidence_bytes" in runtime, "evidence buffer is not resource bounded")'''
if needle not in validator:
    raise SystemExit("measurement validator runtime anchor missing")
validator = validator.replace(needle, replacement, 1)
needle = '''    require("total" not in schema["$defs"]["weights"]["required"], "fitness weights must not invent a total weight")'''
replacement = '''    workload_required = set(schema["$defs"]["workload"]["required"])
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
    require("total" not in schema["$defs"]["weights"]["required"], "fitness weights must not invent a total weight")'''
if needle not in validator:
    raise SystemExit("measurement validator schema anchor missing")
validator_path.write_text(validator.replace(needle, replacement, 1))
