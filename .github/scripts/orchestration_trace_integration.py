from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f"{label}: expected text not found")
    return text.replace(old, new, 1)


# Public private-header trace view.
path = Path("src/internal/project_search_orchestration.h")
text = path.read_text()
if "evo_project_search_orchestration_trace_t" not in text:
    marker = "evo_project_search_status_t evo_project_search_run_orchestrated(\n"
    insert = r'''typedef struct evo_project_search_orchestration_batch_record {
    size_t generation;
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

'''
    text = replace_once(text, marker, insert + marker, "trace type marker")
    declaration = '''evo_project_search_status_t evo_project_search_run_orchestrated(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *orchestration_policy,
    evo_project_search_t *search);
'''
    extension = declaration + r'''

evo_project_search_status_t evo_project_search_run_orchestrated_with_trace(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *orchestration_policy,
    evo_project_search_t *search,
    evo_project_search_orchestration_trace_t *trace);

void evo_project_search_orchestration_trace_destroy(
    evo_project_search_orchestration_trace_t *trace);
'''
    text = replace_once(text, declaration, extension, "trace declaration marker")
path.write_text(text)


# Search integration.
path = Path("src/project_search.c")
text = path.read_text()
if '#include "internal/project_search_orchestration_trace.h"' not in text:
    text = replace_once(
        text,
        '#include "internal/project_search_orchestration.h"\n',
        '#include "internal/project_search_orchestration.h"\n#include "internal/project_search_orchestration_trace.h"\n',
        "trace include",
    )
old_context = '''typedef struct evo_search_run_context {
    const evo_project_search_config_t *config;
    const evo_project_search_orchestration_policy_t *orchestration_policy;
    evo_project_search_owner_t *owner;
    bool fatal_state;
} evo_search_run_context_t;
'''
new_context = '''typedef struct evo_search_run_context {
    const evo_project_search_config_t *config;
    const evo_project_search_orchestration_policy_t *orchestration_policy;
    evo_project_search_orchestration_trace_owner_t *trace_owner;
    evo_project_search_owner_t *owner;
    bool fatal_state;
} evo_search_run_context_t;
'''
if "trace_owner;" not in text:
    text = replace_once(text, old_context, new_context, "trace run context")

batch = '''    if (evo_project_orchestration_run_batch(
            &orchestration_config, &orchestration) !=
        EVO_PROJECT_ORCHESTRATION_SUCCESS) {
        context->fatal_state = true;
        status = EVO_ERROR_EVALUATION;
        goto finish;
    }
    if (orchestration.has_hard_failure ||
'''
batch_with_trace = '''    if (evo_project_orchestration_run_batch(
            &orchestration_config, &orchestration) !=
        EVO_PROJECT_ORCHESTRATION_SUCCESS) {
        context->fatal_state = true;
        status = EVO_ERROR_EVALUATION;
        goto finish;
    }
    if (context->trace_owner != NULL &&
        !evo_project_search_orchestration_trace_append(
            context->trace_owner, &orchestration)) {
        context->fatal_state = true;
        status = EVO_ERROR_EVALUATION;
        goto finish;
    }
    if (orchestration.has_hard_failure ||
'''
if "trace_append(" not in text:
    text = replace_once(text, batch, batch_with_trace, "batch trace append")

old_signature = '''static evo_project_search_status_t evo_project_search_run_common(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *orchestration_policy,
    evo_project_search_t *search)
'''
new_signature = '''static evo_project_search_status_t evo_project_search_run_common(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *orchestration_policy,
    evo_project_search_orchestration_trace_owner_t *trace_owner,
    evo_project_search_t *search)
'''
if old_signature in text:
    text = text.replace(old_signature, new_signature, 1)
old_assign = '''    run_context.config = config;
    run_context.orchestration_policy = orchestration_policy;
    run_context.owner = owner;
'''
new_assign = '''    run_context.config = config;
    run_context.orchestration_policy = orchestration_policy;
    run_context.trace_owner = trace_owner;
    run_context.owner = owner;
'''
if old_assign in text:
    text = text.replace(old_assign, new_assign, 1)

old_wrappers = '''evo_project_search_status_t evo_project_search_run(
    const evo_project_search_config_t *config,
    evo_project_search_t *search)
{
    return evo_project_search_run_common(config, NULL, search);
}

evo_project_search_status_t evo_project_search_run_orchestrated(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *orchestration_policy,
    evo_project_search_t *search)
{
    if (orchestration_policy == NULL) {
        return EVO_PROJECT_SEARCH_ERROR_INVALID_ARGUMENT;
    }
    return evo_project_search_run_common(
        config, orchestration_policy, search);
}
'''
new_wrappers = '''evo_project_search_status_t evo_project_search_run(
    const evo_project_search_config_t *config,
    evo_project_search_t *search)
{
    return evo_project_search_run_common(config, NULL, NULL, search);
}

evo_project_search_status_t evo_project_search_run_orchestrated(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *orchestration_policy,
    evo_project_search_t *search)
{
    if (orchestration_policy == NULL) {
        return EVO_PROJECT_SEARCH_ERROR_INVALID_ARGUMENT;
    }
    return evo_project_search_run_common(
        config, orchestration_policy, NULL, search);
}

evo_project_search_status_t evo_project_search_run_orchestrated_with_trace(
    const evo_project_search_config_t *config,
    const evo_project_search_orchestration_policy_t *orchestration_policy,
    evo_project_search_t *search,
    evo_project_search_orchestration_trace_t *trace)
{
    evo_project_search_orchestration_trace_owner_t *trace_owner = NULL;
    evo_project_search_status_t status;

    if (config == NULL || orchestration_policy == NULL || search == NULL ||
        trace == NULL || trace->private_owner != NULL ||
        trace->schema_version != 0U || !evo_search_config_valid(config) ||
        !evo_search_orchestration_policy_valid(config, orchestration_policy)) {
        return EVO_PROJECT_SEARCH_ERROR_INVALID_ARGUMENT;
    }
    if (!evo_project_search_orchestration_trace_owner_create(
            config, orchestration_policy, &trace_owner)) {
        return EVO_PROJECT_SEARCH_ERROR_OUT_OF_MEMORY;
    }
    status = evo_project_search_run_common(
        config, orchestration_policy, trace_owner, search);
    evo_project_search_orchestration_trace_publish(
        trace_owner, status == EVO_PROJECT_SEARCH_SUCCESS, trace);
    return status;
}
'''
if old_wrappers in text:
    text = text.replace(old_wrappers, new_wrappers, 1)
elif "evo_project_search_run_orchestrated_with_trace(" not in text:
    raise SystemExit("search wrapper block missing")
path.write_text(text)


# Normative trace assertions on existing orchestrated fixtures.
path = Path("tests/project_search_test.c")
text = path.read_text()
old = '''    evo_project_search_t serial = {0};
    evo_project_search_t parallel = {0};

    test_check(
        evo_project_search_run_orchestrated(
            &serial_config, &serial_policy, &serial) ==
'''
new = '''    evo_project_search_t serial = {0};
    evo_project_search_t parallel = {0};
    evo_project_search_orchestration_trace_t serial_trace = {0};
    evo_project_search_orchestration_trace_t parallel_trace = {0};

    test_check(
        evo_project_search_run_orchestrated_with_trace(
            &serial_config, &serial_policy, &serial, &serial_trace) ==
'''
if old in text:
    text = text.replace(old, new, 1)
old = '''    test_check(
        evo_project_search_run_orchestrated(
            &parallel_config, &parallel_policy, &parallel) ==
'''
new = '''    test_check(
        evo_project_search_run_orchestrated_with_trace(
            &parallel_config, &parallel_policy, &parallel, &parallel_trace) ==
'''
if old in text:
    text = text.replace(old, new, 1)
needle = '''    test_check(
        serial_provider.saw_live_recipe && parallel_provider.saw_live_recipe &&
'''
if "trace retains every committed evaluation batch" not in text:
    prefix = '''    test_check(
        serial_trace.run_complete && parallel_trace.run_complete &&
            serial_trace.projection_complete && parallel_trace.projection_complete &&
            !serial_trace.probabilistic_authority &&
            !parallel_trace.probabilistic_authority &&
            serial_trace.batch_count == parallel_trace.batch_count &&
            serial_trace.batch_count > 0U && serial_trace.job_count > 0U &&
            parallel_trace.job_count > 0U,
        "orchestration trace retains every committed evaluation batch");
    test_check(
        serial_trace.batches[0].generation == 0U &&
            parallel_trace.batches[0].generation == 0U &&
            serial_trace.batches[0].generation_committed &&
            parallel_trace.batches[0].generation_committed &&
            serial_trace.batches[0].jobs[0].population_index ==
                parallel_trace.batches[0].jobs[0].population_index,
        "orchestration trace preserves stable generation and candidate authority");

'''
    text = replace_once(text, needle, prefix + needle, "trace assertions")
old = '''    evo_project_search_destroy(&parallel);
    evo_project_search_destroy(&serial);
}

static void test_orchestrated_failure_is_atomic'''
new = '''    evo_project_search_orchestration_trace_destroy(&parallel_trace);
    evo_project_search_orchestration_trace_destroy(&serial_trace);
    evo_project_search_destroy(&parallel);
    evo_project_search_destroy(&serial);
}

static void test_orchestrated_failure_is_atomic'''
if old in text:
    text = text.replace(old, new, 1)
old = '''    evo_project_search_t search = {0};

    test_check(
        evo_project_search_run_orchestrated(&config, &policy, &search) !=
                EVO_PROJECT_SEARCH_SUCCESS &&
'''
new = '''    evo_project_search_t search = {0};
    evo_project_search_orchestration_trace_t trace = {0};

    test_check(
        evo_project_search_run_orchestrated_with_trace(
            &config, &policy, &search, &trace) != EVO_PROJECT_SEARCH_SUCCESS &&
'''
if old in text:
    text = text.replace(old, new, 1)
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
    index = next(i for i, line in enumerate(lines) if line.strip() == "src/project_search.c")
    lines[index] = "\tsrc/project_search.c " + slash
    lines.insert(index + 1, "\tsrc/project_search_orchestration_trace.c " + slash)
path.write_text("\n".join(lines) + "\n")
