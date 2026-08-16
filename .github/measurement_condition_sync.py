from pathlib import Path


def replace(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text()
    if old not in text:
        raise SystemExit(f"missing condition anchor in {path}: {old[:160]!r}")
    p.write_text(text.replace(old, new, 1))


# Reject a policy whose memory/binary mix overflows even though each operand is
# individually finite.
replace(
    "src/project_measurement_model.c",
    "{\n    if (!evo_measurement_text_valid(\n            policy->workload_id, config->limits.max_string_bytes) ||",
    "{\n    const double mix_weight_total = policy->peak_memory_mix_weight +\n"
    "                                    policy->binary_size_mix_weight;\n\n"
    "    if (!evo_measurement_text_valid(\n            policy->workload_id, config->limits.max_string_bytes) ||",
)
replace(
    "src/project_measurement_model.c",
    "        !evo_measurement_weight_valid(policy->binary_size_mix_weight) ||\n"
    "        policy->peak_memory_mix_weight + policy->binary_size_mix_weight <= 0.0) {",
    "        !evo_measurement_weight_valid(policy->binary_size_mix_weight) ||\n"
    "        !isfinite(mix_weight_total) || mix_weight_total <= 0.0) {",
)

# A well-formed provider outcome from a different recorded condition is raw
# evidence of an incomparable sample, not a transaction-level provider crash.
replace(
    "src/project_measurement.c",
    "    if (outcome->schema_version != EVO_PROJECT_MEASUREMENT_SCHEMA_VERSION ||\n"
    "        outcome->condition_fingerprint != owner->condition_fingerprint_value ||\n"
    "        outcome->reliability_ppm > EVO_PROJECT_MEASUREMENT_PPM_SCALE ||",
    "    if (outcome->schema_version != EVO_PROJECT_MEASUREMENT_SCHEMA_VERSION ||\n"
    "        outcome->reliability_ppm > EVO_PROJECT_MEASUREMENT_PPM_SCALE ||",
)

# Preserve the raw condition fingerprint, explicitly exclude a mismatch, and
# ensure it cannot participate in outlier medians or aggregate completeness.
replace(
    "src/project_measurement_runtime.c",
    "    sample->maintainability_ppm = outcome->maintainability_ppm;\n"
    "    if (!outcome->completed) {\n"
    "        if (!evo_measurement_set_exclusion(owner, index, \"incomplete-provider-sample\")) {",
    "    sample->maintainability_ppm = outcome->maintainability_ppm;\n"
    "    if (outcome->condition_fingerprint != owner->condition_fingerprint_value) {\n"
    "        if (!evo_measurement_set_exclusion(\n"
    "                owner, index, \"condition-mismatch\")) {\n"
    "            return false;\n"
    "        }\n"
    "    } else if (!outcome->completed) {\n"
    "        if (!evo_measurement_set_exclusion(owner, index, \"incomplete-provider-sample\")) {",
)
replace(
    "src/project_measurement_runtime.c",
    "        if (evo_measurement_sample_matches(sample, workload_id, subject) &&\n"
    "            sample->completed && !sample->timed_out && !sample->failed &&\n"
    "            (include_excluded || !sample->excluded)) {",
    "        if (evo_measurement_sample_matches(sample, workload_id, subject) &&\n"
    "            sample->condition_fingerprint == owner->condition_fingerprint_value &&\n"
    "            sample->completed && !sample->timed_out && !sample->failed &&\n"
    "            (include_excluded || !sample->excluded)) {",
)
replace(
    "src/project_measurement_runtime.c",
    "        if (strcmp(sample->workload_id, workload_id) == 0 &&\n"
    "            (!sample->completed || sample->timed_out || sample->failed)) {",
    "        if (strcmp(sample->workload_id, workload_id) == 0 &&\n"
    "            (sample->condition_fingerprint != owner->condition_fingerprint_value ||\n"
    "             !sample->completed || sample->timed_out || sample->failed)) {",
)

# Normative oracle: one completed candidate sample under the wrong exact
# condition must make the workload incomplete, retain the mismatch evidence,
# and withhold fitness.
test_path = Path("tests/project_measurement_test.c")
test = test_path.read_text()
test = test.replace(
    "    FAKE_INCOMPLETE = 5,\n    FAKE_OUTLIER = 6\n",
    "    FAKE_INCOMPLETE = 5,\n    FAKE_OUTLIER = 6,\n    FAKE_CONDITION_MISMATCH = 7\n",
    1,
)
test = test.replace(
    "        case FAKE_OUTLIER:\n"
    "            runtime = request->pair_index == 1U ? UINT64_C(5000000)\n"
    "                                                : UINT64_C(800000);\n"
    "            break;\n",
    "        case FAKE_OUTLIER:\n"
    "            runtime = request->pair_index == 1U ? UINT64_C(5000000)\n"
    "                                                : UINT64_C(800000);\n"
    "            break;\n"
    "        case FAKE_CONDITION_MISMATCH:\n"
    "            runtime = UINT64_C(800000);\n"
    "            if (request->phase == EVO_PROJECT_MEASUREMENT_RECORDED &&\n"
    "                request->pair_index == 1U) {\n"
    "                outcome->condition_fingerprint ^= UINT64_C(1);\n"
    "            }\n"
    "            break;\n",
    1,
)
needle = '''    if (mode == FAKE_OUTLIER) {
        bool saw_outlier = false;
        for (index = 0U; index < measurement.sample_count; index += 1U) {
            if (measurement.samples[index].excluded &&
                measurement.samples[index].exclusion_reason != NULL &&
                strcmp(
                    measurement.samples[index].exclusion_reason,
                    "runtime-median-deviation") == 0) {
                saw_outlier = true;
            }
        }
        CHECK(saw_outlier);
        CHECK(measurement.workloads[0].candidate.included_count == 4U);
    }'''
replacement = needle + '''
    if (mode == FAKE_CONDITION_MISMATCH) {
        bool saw_condition_mismatch = false;
        for (index = 0U; index < measurement.sample_count; index += 1U) {
            if (measurement.samples[index].excluded &&
                measurement.samples[index].exclusion_reason != NULL &&
                strcmp(
                    measurement.samples[index].exclusion_reason,
                    "condition-mismatch") == 0) {
                saw_condition_mismatch = true;
            }
        }
        CHECK(saw_condition_mismatch);
        CHECK(!measurement.fitness_available);
    }'''
if needle not in test:
    raise SystemExit("measurement mode assertion anchor missing")
test = test.replace(needle, replacement, 1)
needle = '''    CHECK(run_case(
              root,
              "outlier",
              FAKE_OUTLIER,
              EVO_PROJECT_MEASUREMENT_FASTER,
              true,
              NULL) == 0);'''
replacement = needle + '''
    CHECK(run_case(
              root,
              "condition-mismatch",
              FAKE_CONDITION_MISMATCH,
              EVO_PROJECT_MEASUREMENT_INCOMPLETE,
              false,
              NULL) == 0);'''
if needle not in test:
    raise SystemExit("measurement main fixture anchor missing")
test_path.write_text(test.replace(needle, replacement, 1))

validator_path = Path("tests/validate_project_measurement.py")
validator = validator_path.read_text()
validator = validator.replace(
    '    require("runtime-median-deviation" in runtime, "outlier evidence missing")\n',
    '    require("runtime-median-deviation" in runtime, "outlier evidence missing")\n'
    '    require("condition-mismatch" in runtime, "condition mismatch evidence missing")\n'
    '    require("isfinite(mix_weight_total)" in model, "memory/binary mix overflow guard missing")\n',
    1,
)
validator = validator.replace(
    '        "FAKE_OUTLIER",\n        "replay-a",\n',
    '        "FAKE_OUTLIER",\n        "FAKE_CONDITION_MISMATCH",\n        "replay-a",\n',
    1,
)
validator_path.write_text(validator)
