from pathlib import Path


def replace_once(text, old, new, label):
    if old not in text:
        raise SystemExit(f"missing {label}")
    return text.replace(old, new, 1)


# Extend the private search/orchestration contract with a persistent,
# human-readable batch/job trace owned independently of the search result.
path = Path("src/internal/project_search_orchestration.h")
text = path.read_text()
policy_marker = '''typedef struct evo_project_search_orchestration_policy {
    uint32_t schema_version;
    const char *identity;
    evo_project_orchestration_resource_policy_t resources;
    evo_project_orchestration_provider_t provider;
    evo_project_orchestration_limits_t limits;
} evo_project_search_orchestration_policy_t;
'''
if "evo_project_search_orchestration_batch_record" not in text:
    text = replace_once(
        text,
        policy_marker,
        policy_marker + '''
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
''',
        "search orchestration policy marker",
    )
run_marker = '''evo_project_search_status_t evo_project_search_run_orchestrated(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *orchestration_policy,
    evo_project_search_t *search);
'''
if "evo_project_search_run_orchestrated_with_trace" not in text:
    text = replace_once(
        text,
        run_marker,
        run_marker + '''

evo_project_search_status_t evo_project_search_run_orchestrated_with_trace(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *orchestration_policy,
    evo_project_search_t *search,
    evo_project_search_orchestration_trace_t *trace);
''',
        "orchestrated search declaration",
    )
path.write_text(text)


# Retain every completed batch before its temporary orchestration owner is
# destroyed. Runtime completion timing remains diagnostic, while stable batch
# and population order remain authority.
path = Path("src/project_search.c")
text = path.read_text()
if 'internal/project_search_orchestration_trace.h' not in text:
    text = text.replace(
        '#include "internal/project_search_orchestration.h"\n',
        '#include "internal/project_search_orchestration.h"\n#include "internal/project_search_orchestration_trace.h"\n',
        1,
    )
context_marker = '''typedef struct evo_search_run_context {
    const evo_project_search_config_t *config;
    const evo_project_search_orchestration_policy_t *orchestration_policy;
    evo_project_search_owner_t *owner;
    bool fatal_state;
} evo_search_run_context_t;
'''
if "orchestration_trace_owner" not in text:
    text = replace_once(
        text,
        context_marker,
        '''typedef struct evo_search_run_context {
    const evo_project_search_config_t *config;
    const evo_project_search_orchestration_policy_t *orchestration_policy;
    evo_project_search_orchestration_trace_owner_t *orchestration_trace_owner;
    evo_project_search_owner_t *owner;
    bool fatal_state;
} evo_search_run_context_t;
''',
        "search run context",
    )
batch_marker = '''    if (evo_project_orchestration_run_batch(
            &orchestration_config, &orchestration) !=
        EVO_PROJECT_ORCHESTRATION_SUCCESS) {
        context->fatal_state = true;
        status = EVO_ERROR_EVALUATION;
        goto finish;
    }
'''
if "evo_project_search_orchestration_trace_append(" not in text:
    text = replace_once(
        text,
        batch_marker,
        batch_marker + '''    if (context->orchestration_trace_owner != NULL &&
        !evo_project_search_orchestration_trace_append(
            context->orchestration_trace_owner, &orchestration)) {
        context->fatal_state = true;
        status = EVO_ERROR_EVALUATION;
        goto finish;
    }
''',
        "orchestration batch append point",
    )
common_marker = '''static evo_project_search_status_t evo_project_search_run_common(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *orchestration_policy,
    evo_project_search_t *search)
'''
if "evo_project_search_orchestration_trace_owner_t *orchestration_trace_owner" not in text[text.find(common_marker) if common_marker in text else 0:]:
    text = replace_once(
        text,
        common_marker,
        '''static evo_project_search_status_t evo_project_search_run_common(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *orchestration_policy,
    evo_project_search_orchestration_trace_owner_t *orchestration_trace_owner,
    evo_project_search_t *search)
''',
        "search run common signature",
    )
owner_bind = '''    run_context.config = config;
    run_context.orchestration_policy = orchestration_policy;
    run_context.owner = owner;
'''
if "run_context.orchestration_trace_owner" not in text:
    text = replace_once(
        text,
        owner_bind,
        '''    run_context.config = config;
    run_context.orchestration_policy = orchestration_policy;
    run_context.orchestration_trace_owner = orchestration_trace_owner;
    run_context.owner = owner;
''',
        "search run context binding",
    )
text = text.replace(
    "    return evo_project_search_run_common(config, NULL, search);\n",
    "    return evo_project_search_run_common(config, NULL, NULL, search);\n",
    1,
)
text = text.replace(
    '''    return evo_project_search_run_common(
        config, orchestration_policy, search);
''',
    '''    return evo_project_search_run_common(
        config, orchestration_policy, NULL, search);
''',
    1,
)
if "evo_project_search_run_orchestrated_with_trace(" not in text:
    insertion = '''evo_project_search_status_t evo_project_search_run_orchestrated_with_trace(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *orchestration_policy,
    evo_project_search_t *search,
    evo_project_search_orchestration_trace_t *trace)
{
    evo_project_search_orchestration_trace_owner_t *trace_owner = NULL;
    evo_project_search_status_t status;

    if (config == NULL || orchestration_policy == NULL || search == NULL ||
        trace == NULL || trace->private_owner != NULL ||
        !evo_search_config_valid(config) ||
        !evo_search_orchestration_policy_valid(config, orchestration_policy)) {
        return EVO_PROJECT_SEARCH_ERROR_INVALID_ARGUMENT;
    }
    if (search->private_owner != NULL || search->schema_version != 0U) {
        return EVO_PROJECT_SEARCH_ERROR_RESULT_ACTIVE;
    }
    *trace = (evo_project_search_orchestration_trace_t){0};
    if (!evo_project_search_orchestration_trace_owner_create(
            config, orchestration_policy, &trace_owner)) {
        return EVO_PROJECT_SEARCH_ERROR_RESOURCE_LIMIT;
    }
    status = evo_project_search_run_common(
        config, orchestration_policy, trace_owner, search);
    evo_project_search_orchestration_trace_publish(
        trace_owner, status == EVO_PROJECT_SEARCH_SUCCESS, trace);
    return status;
}

'''
    destroy_marker = "void evo_project_search_destroy(evo_project_search_t *search)\n"
    text = replace_once(
        text,
        destroy_marker,
        insertion + destroy_marker,
        "search destroy marker",
    )
path.write_text(text)


# Normative integration tests compare serial and parallel search authority while
# retaining complete batch traces. A hard worker failure must keep cleanup and
# rejection evidence while publishing no partial search result.
path = Path("tests/project_search_test.c")
text = path.read_text()
serial_decl = '''    evo_project_search_t serial = {0};
    evo_project_search_t parallel = {0};
'''
if "serial_trace" not in text:
    text = replace_once(
        text,
        serial_decl,
        serial_decl + '''    evo_project_search_orchestration_trace_t serial_trace = {0};
    evo_project_search_orchestration_trace_t parallel_trace = {0};
''',
        "orchestrated search trace declarations",
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
early_marker = '''    if (serial.private_owner == NULL || parallel.private_owner == NULL) {
        evo_project_search_destroy(&parallel);
        evo_project_search_destroy(&serial);
        return;
    }
'''
if "trace_destroy(&parallel_trace)" not in text:
    text = replace_once(
        text,
        early_marker,
        '''    if (serial.private_owner == NULL || parallel.private_owner == NULL) {
        evo_project_search_orchestration_trace_destroy(&parallel_trace);
        evo_project_search_orchestration_trace_destroy(&serial_trace);
        evo_project_search_destroy(&parallel);
        evo_project_search_destroy(&serial);
        return;
    }
''',
        "orchestrated search early cleanup",
    )
equivalence_marker = '''    test_check(
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
    text = replace_once(
        text,
        equivalence_marker,
        equivalence_marker + '''    test_check(
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
''',
        "orchestrated search trace assertions",
    )
failure_decl = '''    evo_project_search_t search = {0};

    test_check(
        evo_project_search_run_orchestrated(&config, &policy, &search) !=
                EVO_PROJECT_SEARCH_SUCCESS &&
'''
if "evo_project_search_orchestration_trace_t trace" not in text:
    text = replace_once(
        text,
        failure_decl,
        '''    evo_project_search_t search = {0};
    evo_project_search_orchestration_trace_t trace = {0};

    test_check(
        evo_project_search_run_orchestrated_with_trace(
            &config, &policy, &search, &trace) !=
                EVO_PROJECT_SEARCH_SUCCESS &&
''',
        "orchestrated failure trace call",
    )
failure_marker = '''    test_check(
        provider.start_count > 0U &&
            provider.start_count == provider.join_count &&
            provider.cancel_count > 0U,
        "hard external worker failure cancels and joins started siblings");
}
'''
if "failed search retains complete trustworthy worker schedule evidence" not in text:
    text = replace_once(
        text,
        failure_marker,
        '''    test_check(
        provider.start_count > 0U &&
            provider.start_count == provider.join_count &&
            provider.cancel_count > 0U,
        "hard external worker failure cancels and joins started siblings");
    test_check(
        !trace.run_complete && trace.projection_complete &&
            !trace.probabilistic_authority && trace.batch_count == 1U &&
            trace.job_count > 0U && trace.batches[0].has_hard_failure &&
            !trace.batches[0].generation_committed &&
            trace.batches[0].cleanup_complete,
        "failed search retains complete trustworthy worker schedule evidence");
    evo_project_search_orchestration_trace_destroy(&trace);
}
''',
        "orchestrated failure trace assertion",
    )
path.write_text(text)


# Register the trace owner in both build frontends.
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
    index = next(
        i for i, line in enumerate(lines)
        if "src/internal/project_search_orchestration.h" in line
    )
    if not lines[index].rstrip().endswith(slash):
        lines[index] += " " + slash
    lines.insert(
        index + 1,
        "\tsrc/internal/project_search_orchestration_trace.h " + slash,
    )
if not any("src/project_search_orchestration_trace.c" in line for line in lines):
    index = next(
        i for i, line in enumerate(lines)
        if "src/project_search.c" in line
    )
    if not lines[index].rstrip().endswith(slash):
        lines[index] += " " + slash
    lines.insert(index + 1, "\tsrc/project_search_orchestration_trace.c " + slash)
path.write_text("\n".join(lines) + "\n")
