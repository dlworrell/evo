from pathlib import Path


def replace_once(text, old, new, label):
    if old not in text:
        raise SystemExit(f"missing {label}")
    return text.replace(old, new, 1)


# Extend the private search/orchestration contract with an owned, persistent
# trace that survives each temporary batch transaction.
path = Path("src/internal/project_search_orchestration.h")
text = path.read_text()
if "evo_project_search_orchestration_batch_record" not in text:
    marker = '''typedef struct evo_project_search_orchestration_policy {
    uint32_t schema_version;
    const char *identity;
    evo_project_orchestration_resource_policy_t resources;
    evo_project_orchestration_provider_t provider;
    evo_project_orchestration_limits_t limits;
} evo_project_search_orchestration_policy_t;
'''
    block = marker + '''
typedef struct evo_project_search_orchestration_batch_record {
    uint64_t generation;
    size_t external_worker_count;
    size_t completion_count;
    size_t committed_count;
    size_t first_hard_failure_index;
    bool has_hard_failure;
    bool generation_committed;
    bool cleanup_complete;
    size_t job_count;
    const evo_project_orchestration_job_record_t *jobs;
} evo_project_search_orchestration_batch_record_t;

typedef struct evo_project_search_orchestration_trace {
    uint32_t schema_version;
    const char *policy_identity;
    const char *provider_identity;
    size_t batch_count;
    const evo_project_search_orchestration_batch_record_t *batches;
    size_t job_count;
    bool run_complete;
    bool projection_complete;
    bool probabilistic_authority;
    void *private_owner;
} evo_project_search_orchestration_trace_t;

void evo_project_search_orchestration_trace_destroy(
    evo_project_search_orchestration_trace_t *trace);
'''
    text = replace_once(text, marker, block, "search orchestration policy marker")
if "evo_project_search_run_orchestrated_with_trace" not in text:
    marker = '''evo_project_search_status_t evo_project_search_run_orchestrated(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *orchestration_policy,
    evo_project_search_t *search);
'''
    block = marker + '''

evo_project_search_status_t evo_project_search_run_orchestrated_with_trace(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *orchestration_policy,
    evo_project_search_t *search,
    evo_project_search_orchestration_trace_t *trace);
'''
    text = replace_once(text, marker, block, "orchestrated search declaration")
path.write_text(text)


# Bind one optional trace owner into search runtime context. Serial search leaves
# it NULL; orchestrated search creates and owns it for the complete run.
path = Path("src/internal/project_search_owner.h")
text = path.read_text()
if "project_search_orchestration_trace" not in text:
    text = text.replace(
        '#include "internal/project_search.h"\n',
        '#include "internal/project_search.h"\n#include "internal/project_search_orchestration_trace.h"\n',
        1,
    )
if "orchestration_trace_owner" not in text:
    marker = "    evo_project_search_birth_event_t *birth_events;\n"
    text = replace_once(
        text,
        marker,
        marker + "    evo_project_search_orchestration_trace_owner_t *orchestration_trace_owner;\n",
        "search owner birth events",
    )
path.write_text(text)

path = Path("src/internal/project_search_internal.h")
text = path.read_text()
if "orchestration_trace_owner" not in text:
    marker = "    bool fatal_state;\n"
    text = replace_once(
        text,
        marker,
        marker + "    evo_project_search_orchestration_trace_owner_t *orchestration_trace_owner;\n",
        "search runtime fatal state",
    )
path.write_text(text)


# Capture each completed orchestration transaction before its temporary owner is
# destroyed. This preserves failure evidence while still failing the search
# atomically.
path = Path("src/project_search.c")
text = path.read_text()
if 'internal/project_search_orchestration_trace.h' not in text:
    text = text.replace(
        '#include "internal/project_search_internal.h"\n',
        '#include "internal/project_search_internal.h"\n#include "internal/project_search_orchestration_trace.h"\n',
        1,
    )
needle = '''    if (evo_project_orchestration_run_batch(
            &orchestration_config, &orchestration) !=
        EVO_PROJECT_ORCHESTRATION_SUCCESS) {
        context->fatal_state = true;
        status = EVO_ERROR_EVALUATION;
        goto finish;
    }
'''
if "append(context->orchestration_trace_owner" not in text:
    replacement = needle + '''    if (context->orchestration_trace_owner != NULL &&
        !evo_project_search_orchestration_trace_append(
            context->orchestration_trace_owner, &orchestration)) {
        context->fatal_state = true;
        status = EVO_ERROR_EVALUATION;
        goto finish;
    }
'''
    text = replace_once(text, needle, replacement, "orchestration batch append point")
path.write_text(text)


# Public private entry point: ordinary orchestrated search remains compatible;
# the traced variant transfers trace ownership even when the search itself fails.
path = Path("src/project_search_runtime.c")
text = path.read_text()
if 'internal/project_search_orchestration_trace.h' not in text:
    text = text.replace(
        '#include "internal/project_search_internal.h"\n',
        '#include "internal/project_search_internal.h"\n#include "internal/project_search_orchestration_trace.h"\n',
        1,
    )
needle = '''evo_project_search_status_t evo_project_search_run_orchestrated(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *orchestration_policy,
    evo_project_search_t *search)
'''
if "evo_project_search_run_orchestrated_with_trace" not in text:
    start = text.index(needle)
    # Keep the existing function intact and add a traced sibling before it.
    block = '''evo_project_search_status_t evo_project_search_run_orchestrated_with_trace(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *orchestration_policy,
    evo_project_search_t *search,
    evo_project_search_orchestration_trace_t *trace)
{
    evo_project_search_orchestration_trace_owner_t *trace_owner = NULL;
    evo_project_search_status_t status;

    if (trace == NULL || trace->private_owner != NULL) {
        return EVO_PROJECT_SEARCH_ERROR_INVALID_ARGUMENT;
    }
    *trace = (evo_project_search_orchestration_trace_t){0};
    if (!evo_project_search_orchestration_trace_owner_create(
            config, orchestration_policy, &trace_owner)) {
        return EVO_PROJECT_SEARCH_ERROR_RESOURCE_LIMIT;
    }
    status = evo_project_search_run_internal(
        config, orchestration_policy, trace_owner, search);
    evo_project_search_orchestration_trace_publish(
        trace_owner, status == EVO_PROJECT_SEARCH_SUCCESS, trace);
    return status;
}

'''
    text = text[:start] + block + text[start:]
# The internal helper signature receives an optional trace owner.
text = text.replace(
    '''static evo_project_search_status_t evo_project_search_run_internal(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *orchestration_policy,
    evo_project_search_t *search)''',
    '''static evo_project_search_status_t evo_project_search_run_internal(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *orchestration_policy,
    evo_project_search_orchestration_trace_owner_t *orchestration_trace_owner,
    evo_project_search_t *search)''',
    1,
)
text = text.replace(
    "    context.orchestration_policy = orchestration_policy;\n",
    "    context.orchestration_policy = orchestration_policy;\n    context.orchestration_trace_owner = orchestration_trace_owner;\n",
    1,
)
text = text.replace(
    "    return evo_project_search_run_internal(config, NULL, search);\n",
    "    return evo_project_search_run_internal(config, NULL, NULL, search);\n",
    1,
)
text = text.replace(
    "    return evo_project_search_run_internal(\n        config, orchestration_policy, search);\n",
    "    return evo_project_search_run_internal(\n        config, orchestration_policy, NULL, search);\n",
    1,
)
path.write_text(text)


# Tests: reuse the async fixtures, compare serial/parallel traces, and prove a
# failed generation leaves trustworthy cleanup/failure evidence without a
# partial search result.
path = Path("tests/project_search_test.c")
text = path.read_text()
if 'internal/project_search_orchestration_trace.h' not in text:
    text = text.replace(
        '#include "internal/project_search_internal.h"\n',
        '#include "internal/project_search_internal.h"\n#include "internal/project_search_orchestration_trace.h"\n',
        1,
    )
text = text.replace(
    "    evo_project_search_t serial = {0};\n    evo_project_search_t parallel = {0};\n",
    "    evo_project_search_t serial = {0};\n    evo_project_search_t parallel = {0};\n    evo_project_search_orchestration_trace_t serial_trace = {0};\n    evo_project_search_orchestration_trace_t parallel_trace = {0};\n",
    1,
)
text = text.replace(
    '''        evo_project_search_run_orchestrated(
            &serial_config, &serial_policy, &serial) ==
''',
    '''        evo_project_search_run_orchestrated_with_trace(
            &serial_config, &serial_policy, &serial, &serial_trace) ==
''',
    1,
)
text = text.replace(
    '''        evo_project_search_run_orchestrated(
            &parallel_config, &parallel_policy, &parallel) ==
''',
    '''        evo_project_search_run_orchestrated_with_trace(
            &parallel_config, &parallel_policy, &parallel, &parallel_trace) ==
''',
    1,
)
needle = '''    test_check(
        strcmp(serial.search_fingerprint, parallel.search_fingerprint) == 0 &&
            strcmp(serial.best_recipe_fingerprint,
                   parallel.best_recipe_fingerprint) == 0 &&
            serial.termination_reason == parallel.termination_reason &&
            serial.best_fitness.total == parallel.best_fitness.total &&
            serial.lineage_count == parallel.lineage_count &&
            serial.operator_event_count == parallel.operator_event_count &&
            strcmp(serial.canonical_json, parallel.canonical_json) == 0,
        "external worker count preserves complete logical search authority");
'''
if "persistent worker traces retain every generation" not in text:
    replacement = needle + '''    test_check(
        serial_trace.run_complete && parallel_trace.run_complete &&
            serial_trace.projection_complete && parallel_trace.projection_complete &&
            !serial_trace.probabilistic_authority &&
            !parallel_trace.probabilistic_authority &&
            serial_trace.batch_count == parallel_trace.batch_count &&
            serial_trace.batch_count == serial.generations_completed + 1U &&
            serial_trace.job_count == parallel_trace.job_count &&
            serial_trace.job_count ==
                serial_trace.batch_count * serial_config.population_size,
        "persistent worker traces retain every generation");
    test_check(
        serial_trace.batches[0].external_worker_count == 1U &&
            parallel_trace.batches[0].external_worker_count == 4U &&
            serial_trace.batches[0].generation ==
                parallel_trace.batches[0].generation &&
            serial_trace.batches[0].generation_committed &&
            parallel_trace.batches[0].generation_committed &&
            serial_trace.batches[0].cleanup_complete &&
            parallel_trace.batches[0].cleanup_complete,
        "worker schedule remains diagnostic while generation authority matches");
    evo_project_search_orchestration_trace_destroy(&parallel_trace);
    evo_project_search_orchestration_trace_destroy(&serial_trace);
'''
    text = replace_once(text, needle, replacement, "equivalence trace assertion")
# Failure test gets a trace result.
text = text.replace(
    "    evo_project_search_t search = {0};\n\n    test_check(\n        evo_project_search_run_orchestrated(&config, &policy, &search) !=\n",
    "    evo_project_search_t search = {0};\n    evo_project_search_orchestration_trace_t trace = {0};\n\n    test_check(\n        evo_project_search_run_orchestrated_with_trace(\n            &config, &policy, &search, &trace) !=\n",
    1,
)
needle = '''    test_check(
        provider.start_count > 0U &&
            provider.start_count == provider.join_count &&
            provider.cancel_count > 0U,
        "hard external worker failure cancels and joins started siblings");
}
'''
if "failed search retains complete trustworthy worker schedule evidence" not in text:
    replacement = '''    test_check(
        provider.start_count > 0U &&
            provider.start_count == provider.join_count &&
            provider.cancel_count > 0U,
        "hard external worker failure cancels and joins started siblings");
    test_check(
        !trace.run_complete && trace.projection_complete &&
            trace.batch_count == 1U && trace.job_count > 0U &&
            trace.batches[0].has_hard_failure &&
            !trace.batches[0].generation_committed &&
            trace.batches[0].cleanup_complete,
        "failed search retains complete trustworthy worker schedule evidence");
    evo_project_search_orchestration_trace_destroy(&trace);
}
'''
    text = replace_once(text, needle, replacement, "failure trace assertion")
path.write_text(text)


# Build-system registration.
path = Path("CMakeLists.txt")
text = path.read_text()
if "src/project_search_orchestration_trace.c" not in text:
    text = replace_once(
        text,
        "    src/project_search.c\n",
        "    src/project_search.c\n    src/project_search_orchestration_trace.c\n",
        "CMake search source marker",
    )
path.write_text(text)

path = Path("Makefile.am")
lines = path.read_text().splitlines()
slash = chr(92)
if not any("src/internal/project_search_orchestration_trace.h" in line for line in lines):
    index = next(i for i, line in enumerate(lines) if "src/internal/project_search_orchestration.h" in line)
    if not lines[index].rstrip().endswith(slash):
        lines[index] += " " + slash
    lines.insert(index + 1, "\tsrc/internal/project_search_orchestration_trace.h " + slash)
if not any("src/project_search_orchestration_trace.c" in line for line in lines):
    index = next(i for i, line in enumerate(lines) if "src/project_search.c" in line)
    if not lines[index].rstrip().endswith(slash):
        lines[index] += " " + slash
    lines.insert(index + 1, "\tsrc/project_search_orchestration_trace.c " + slash)
path.write_text("\n".join(lines) + "\n")
