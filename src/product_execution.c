#define _POSIX_C_SOURCE 200809L

#include "internal/product_cli.h"
#include "internal/project_analysis.h"
#include "internal/project_json.h"
#include "internal/project_manifest.h"
#include "internal/project_provider.h"
#include "internal/project_provider_adapters.h"
#include "internal/project_provider_clang.h"
#include "internal/project_runtime.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if defined(__cppcheck__)
/* Standalone source scans do not import frontend -D string definitions. */
#undef EVO_PRODUCT_VERSION
#define EVO_PRODUCT_VERSION "source-analysis-only"
#endif
#ifndef EVO_PRODUCT_VERSION
#error "The build frontend must define EVO_PRODUCT_VERSION"
#endif

static int evo_product_io_status(void)
{
    return errno == ENOMEM || errno == EFBIG || errno == EMFILE || errno == ENFILE
               ? EVO_PROJECT_COMMAND_EXIT_RESOURCE
               : EVO_PROJECT_COMMAND_EXIT_CONFIGURATION;
}

static evo_project_ingest_limits_t evo_product_limits(void)
{
    return (evo_project_ingest_limits_t){
        .max_manifest_bytes = EVO_PRODUCT_MANIFEST_BYTES,
        .max_json_tokens = EVO_PRODUCT_JSON_TOKENS,
        .max_json_depth = 64U,
        .max_string_bytes = 8192U,
        .max_path_bytes = EVO_PRODUCT_PATH_BYTES - 1U,
        .max_files = 65536U,
        .max_file_bytes = 16777216U,
        .max_total_bytes = 134217728U,
        .max_compilation_database_bytes = 16777216U,
        .max_permitted_roots = 1024U,
        .max_dependencies = 128U,
        .max_toolchains = 128U,
        .max_environment_entries = 128U,
        .max_targets = 128U,
        .max_workloads = 128U,
        .max_constraints = 128U,
        .max_command_args = 1024U,
        .max_command_bytes = 65536U,
        .max_command_output_bytes = 4194304U,
        .max_evidence_bytes = EVO_PRODUCT_EVIDENCE_BYTES,
        .max_command_timeout_ms = UINT64_C(600000),
        .max_memory_bytes = UINT64_C(2147483648),
        .max_processes = 128U,
        .max_storage_bytes = UINT64_C(1073741824),
    };
}

static void evo_product_environment_destroy(char **environment, size_t count)
{
    for (size_t index = 0U; index < count; index += 1U) {
        evo_project_release(environment[index]);
    }
    evo_project_release(environment);
}

static char **evo_product_environment(const evo_project_manifest_t *manifest)
{
    char **environment = evo_project_allocate_zeroed(manifest->environment_count + 1U, sizeof(*environment));
    if (environment == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < manifest->environment_count; index += 1U) {
        const evo_project_environment_entry_t *entry = &manifest->environment[index];
        const size_t name_size = strlen(entry->name);
        const size_t value_size = strlen(entry->value);
        if (name_size > SIZE_MAX - 2U || value_size > SIZE_MAX - name_size - 2U) {
            evo_product_environment_destroy(environment, manifest->environment_count);
            return NULL;
        }
        const size_t capacity = name_size + value_size + 2U;
        environment[index] = evo_project_allocate_zeroed(capacity, sizeof(char));
        if (environment[index] == NULL ||
            evo_project_format(environment[index], capacity, "%s=%s", entry->name, entry->value) < 0) {
            evo_product_environment_destroy(environment, manifest->environment_count);
            return NULL;
        }
    }
    return environment;
}

static int evo_product_compiler_version(const evo_project_manifest_t *manifest,
                                        const char *workspace,
                                        const volatile sig_atomic_t *interrupted,
                                        char version[8192])
{
    static const char *const arguments[] = {"clang", "--version"};
    static const char *const environment[] = {"LANG=C", "LC_ALL=C"};
    evo_project_sandbox_result_t result = {0};
    const evo_project_sandbox_command_t command = {
        .schema_version = EVO_PROJECT_SANDBOX_SCHEMA_VERSION,
        .workspace_path = workspace,
        .working_directory = ".",
        .argument_count = 2U,
        .arguments = arguments,
        .environment_count = 2U,
        .environment = environment,
        .limits = {
            .cpu_time_ms = manifest->budget.command_timeout_ms,
            .address_space_bytes = manifest->budget.max_memory_bytes,
            .descendant_process_count = manifest->budget.max_processes,
            .storage_bytes = manifest->budget.max_storage_bytes,
            .output_bytes = manifest->budget.max_command_output_bytes,
            .wall_timeout_ms = manifest->budget.command_timeout_ms,
            .network_access = false,
        },
        .cancel_requested = interrupted,
    };
    const evo_project_sandbox_status_t status = evo_project_sandbox_run(&command, &result);
    int exit_status = EVO_PROJECT_COMMAND_EXIT_CONFIGURATION;

    if (status == EVO_PROJECT_SANDBOX_ERROR_CLEANUP) {
        exit_status = EVO_PROJECT_COMMAND_EXIT_INTERNAL;
    } else if (*interrupted != 0) {
        exit_status = EVO_PROJECT_COMMAND_EXIT_INTERRUPTED;
    } else if (status == EVO_PROJECT_SANDBOX_ERROR_OUT_OF_MEMORY ||
               status == EVO_PROJECT_SANDBOX_ERROR_RESOURCE_LIMIT || result.resource_exhausted) {
        exit_status = EVO_PROJECT_COMMAND_EXIT_RESOURCE;
    } else if (status == EVO_PROJECT_SANDBOX_SUCCESS && result.completed && result.exit_code == 0 &&
               result.stdout_text != NULL) {
        size_t length = 0U;
        bool valid = true;
        while (length < result.stdout_bytes && result.stdout_text[length] != '\n' && result.stdout_text[length] != '\r') {
            if (length >= 8191U || (unsigned char)result.stdout_text[length] < 0x20U) {
                valid = false;
                break;
            }
            version[length] = result.stdout_text[length];
            length += 1U;
        }
        version[length] = '\0';
        if (valid && length > 0U && strstr(version, "clang version 18.") != NULL) {
            exit_status = EVO_PROJECT_COMMAND_EXIT_SUCCESS;
        }
    }
    evo_project_sandbox_result_destroy(&result);
    return exit_status;
}

int evo_product_execute_analyze(const char *manifest_argument, const char *input_argument,
                                evo_product_stage_t *stage,
                                const volatile sig_atomic_t *interrupted,
                                evo_product_summary_t *summary)
{
    char manifest_path[EVO_PRODUCT_PATH_BYTES];
    char input_path[EVO_PRODUCT_PATH_BYTES];
    char stable_manifest[EVO_PRODUCT_PATH_BYTES];
    char baseline_path[EVO_PRODUCT_PATH_BYTES];
    char analysis_path[EVO_PRODUCT_PATH_BYTES];
    char compiler_version[8192];
    char *text = NULL;
    char **environment = NULL;
    size_t text_size = 0U;
    evo_project_manifest_t manifest = {0};
    evo_project_baseline_t baseline = {0};
    evo_project_analysis_t analysis = {0};
    evo_project_clang_analysis_context_t clang_context = {0};
    evo_project_sandbox_command_context_t runner = {.cancel_requested = interrupted};
    const evo_project_ingest_limits_t limits = evo_product_limits();
    int exit_status = EVO_PROJECT_COMMAND_EXIT_CONFIGURATION;
    int descriptor = evo_product_open_path(manifest_argument, false, manifest_path);

    if (descriptor < 0) {
        return evo_product_io_status();
    }
    const bool read_ok = evo_product_read(descriptor, EVO_PRODUCT_MANIFEST_BYTES, &text, &text_size);
    const int read_error = errno;
    (void)close(descriptor);
    errno = read_error;
    if (!read_ok) {
        return evo_product_io_status();
    }
    evo_project_status_t capture_status = evo_project_manifest_parse(text, text_size, &limits, &manifest);
    if (capture_status != EVO_PROJECT_SUCCESS) {
        (void)fprintf(stderr, "evo: manifest: %s\n", evo_project_status_name(capture_status));
        exit_status = capture_status == EVO_PROJECT_ERROR_OUT_OF_MEMORY || capture_status == EVO_PROJECT_ERROR_RESOURCE_LIMIT
                          ? EVO_PROJECT_COMMAND_EXIT_RESOURCE
                          : EVO_PROJECT_COMMAND_EXIT_CONFIGURATION;
        goto done;
    }
    descriptor = evo_product_open_path(input_argument, true, input_path);
    if (descriptor < 0) {
        goto done;
    }
    (void)close(descriptor);
    if (evo_product_path_contains(input_path, stage->output_path) ||
        evo_product_path_contains(stage->output_path, input_path) || manifest.target_count != 1U) {
        (void)fprintf(stderr, "evo: configuration: disjoint output and one declared target are required\n");
        goto done;
    }
    for (size_t index = 0U; index < manifest.environment_count; index += 1U) {
        if (strcmp(manifest.environment[index].name, "PATH") == 0) {
            (void)fprintf(stderr, "evo: configuration: PATH is fixed by the standalone runtime profile\n");
            goto done;
        }
    }
    evo_project_command_request_t request = {
        .schema_version = EVO_PROJECT_COMMAND_SCHEMA_VERSION,
        .operation = EVO_PROJECT_COMMAND_ANALYZE,
        .manifest_path = manifest_path,
        .manifest = &manifest,
        .input_path = input_path,
        .output_path = stage->output_path,
        .provider_policy_identity = EVO_PROJECT_COMMAND_PROVIDER_POLICY_ID,
    };
    for (size_t index = 0U; index < EVO_PROJECT_COMMAND_PROVIDER_COUNT; index += 1U) {
        if (index != EVO_PROJECT_COMMAND_PROVIDER_ANALYSIS && index != EVO_PROJECT_COMMAND_PROVIDER_EXECUTION) {
            continue;
        }
        const evo_project_command_provider_requirement_t *requirement =
            evo_project_command_provider_requirement((evo_project_command_provider_slot_t)index);
        const evo_project_provider_record_t *provider = evo_project_provider_find(requirement->identity);
        if (*interrupted != 0) {
            exit_status = EVO_PROJECT_COMMAND_EXIT_INTERRUPTED;
            goto done;
        }
        if (provider == NULL) {
            goto done;
        }
        request.providers[index] = (evo_project_command_provider_selection_t){
            .identity = provider->identity,
            .implementation_version = provider->implementation_version,
            .capabilities = provider->capabilities,
            .available = evo_project_provider_available(provider),
        };
    }
    evo_project_command_plan_t plan = {0};
    const evo_project_command_status_t planned = evo_project_command_plan_build(&request, &plan);
    if (planned != EVO_PROJECT_COMMAND_SUCCESS || !plan.execution_permitted) {
        (void)fprintf(stderr, "evo: preflight: %s (requires working Clang and Bubblewrap isolation)\n", evo_project_command_status_name(planned));
        goto done;
    }
    if (*interrupted != 0) {
        exit_status = EVO_PROJECT_COMMAND_EXIT_INTERRUPTED;
        goto done;
    }
    if (!evo_product_stage_begin(stage)) {
        exit_status = evo_product_io_status();
        goto done;
    }
    environment = evo_product_environment(&manifest);
    if (environment == NULL) {
        exit_status = EVO_PROJECT_COMMAND_EXIT_RESOURCE;
        goto done;
    }
    runner.environment_count = manifest.environment_count;
    runner.environment = (const char *const *)environment;
    if (!evo_product_write(stage->directory_fd, "manifest.json", text, text_size)) {
        exit_status = EVO_PROJECT_COMMAND_EXIT_INTERNAL;
        goto done;
    }
    const int m = evo_project_format(stable_manifest, sizeof(stable_manifest), "%s/manifest.json", stage->stage_path);
    const int b = evo_project_format(baseline_path, sizeof(baseline_path), "%s/baseline", stage->stage_path);
    const int a = evo_project_format(analysis_path, sizeof(analysis_path), "%s/analysis", stage->stage_path);
    if (m < 0 || (size_t)m >= sizeof(stable_manifest) || b < 0 || (size_t)b >= sizeof(baseline_path) ||
        a < 0 || (size_t)a >= sizeof(analysis_path)) {
        exit_status = EVO_PROJECT_COMMAND_EXIT_RESOURCE;
        goto done;
    }
    exit_status = evo_product_compiler_version(&manifest, stage->stage_path, interrupted, compiler_version);
    if (exit_status != EVO_PROJECT_COMMAND_EXIT_SUCCESS) {
        (void)fprintf(stderr, "evo: compiler preflight requires sandboxed Clang 18 within the manifest budget\n");
        goto done;
    }
    const evo_project_capture_config_t capture = {
        .manifest_path = stable_manifest,
        .authorized_project_root = input_path,
        .output_path = baseline_path,
        .execution_provider_identity = EVO_PROJECT_PROVIDER_LINUX_BWRAP_ID,
        .limits = limits,
        .command_runner = evo_project_sandbox_command_runner,
        .command_runner_context = &runner,
    };
    capture_status = evo_project_capture_baseline(&capture, &baseline);
    if (runner.cleanup_failed) {
        exit_status = EVO_PROJECT_COMMAND_EXIT_INTERNAL;
        goto done;
    }
    if (*interrupted != 0 || runner.interrupted) {
        exit_status = EVO_PROJECT_COMMAND_EXIT_INTERRUPTED;
        goto done;
    }
    if (runner.resource_exhausted || capture_status == EVO_PROJECT_ERROR_RESOURCE_LIMIT || capture_status == EVO_PROJECT_ERROR_OUT_OF_MEMORY) {
        (void)fprintf(stderr, "evo: capture: %s; command stage %u; sandbox %s / %s\n",
                      evo_project_status_name(capture_status), (unsigned int)runner.last_stage,
                      evo_project_sandbox_status_name(runner.last_status),
                      evo_project_sandbox_resource_name(runner.exhausted_resource));
        exit_status = EVO_PROJECT_COMMAND_EXIT_RESOURCE;
        goto done;
    }
    if (capture_status != EVO_PROJECT_SUCCESS) {
        (void)fprintf(stderr, "evo: capture: %s\n", evo_project_status_name(capture_status));
        exit_status = EVO_PROJECT_COMMAND_EXIT_INGESTION;
        goto done;
    }
    if (baseline.state != EVO_PROJECT_BASELINE_ELIGIBLE) {
        exit_status = baseline.state == EVO_PROJECT_BASELINE_BUILD_FAILED         ? EVO_PROJECT_COMMAND_EXIT_BASELINE_BUILD
                      : baseline.state == EVO_PROJECT_BASELINE_CORRECTNESS_FAILED ? EVO_PROJECT_COMMAND_EXIT_CORRECTNESS
                                                                                  : EVO_PROJECT_COMMAND_EXIT_BENCHMARK_INELIGIBLE;
        goto done;
    }
    const evo_project_analysis_config_t config = {
        .baseline = &baseline,
        .output_path = analysis_path,
        .provider_identity = EVO_PROJECT_PROVIDER_CLANG_ANALYSIS_ID,
        .clang_identity = compiler_version,
        .llvm_identity = compiler_version,
        .target_identity = manifest.targets[0],
        .flags_identity = baseline.normalized_build_fingerprint,
        .runtime_profile_state = EVO_PROJECT_RUNTIME_NOT_CONFIGURED,
        .limits = {
            .max_string_bytes = 8192U,
            .max_path_bytes = EVO_PRODUCT_PATH_BYTES - 1U,
            .max_translation_units = 4096U,
            .max_source_locations = 65536U,
            .max_declarations = 65536U,
            .max_calls = 65536U,
            .max_control_flows = 65536U,
            .max_data_flows = 65536U,
            .max_optimization_records = 65536U,
            .max_runtime_records = 65536U,
            .max_opportunities = 65536U,
            .max_evidence_bytes = EVO_PRODUCT_EVIDENCE_BYTES,
        },
        .provider = evo_project_clang_analysis_provider,
        .provider_context = &clang_context,
        .cancel_requested = interrupted,
    };
    const evo_project_analysis_status_t analyzed = evo_project_analyze(&config, &analysis);
    if (analyzed == EVO_PROJECT_ANALYSIS_ERROR_CLEANUP) {
        exit_status = EVO_PROJECT_COMMAND_EXIT_INTERNAL;
        goto done;
    }
    if (*interrupted != 0) {
        exit_status = EVO_PROJECT_COMMAND_EXIT_INTERRUPTED;
        goto done;
    }
    if (analyzed != EVO_PROJECT_ANALYSIS_SUCCESS) {
        (void)fprintf(stderr, "evo: analysis: %s\n", evo_project_analysis_status_name(analyzed));
        exit_status = analyzed == EVO_PROJECT_ANALYSIS_ERROR_RESOURCE_LIMIT || analyzed == EVO_PROJECT_ANALYSIS_ERROR_OUT_OF_MEMORY
                          ? EVO_PROJECT_COMMAND_EXIT_RESOURCE
                          : EVO_PROJECT_COMMAND_EXIT_ANALYSIS;
        goto done;
    }
    if (evo_project_format(summary->baseline, sizeof(summary->baseline), "%s", baseline.baseline_fingerprint) < 0 ||
        evo_project_format(summary->analysis, sizeof(summary->analysis), "%s", analysis.analysis_fingerprint) < 0) {
        exit_status = EVO_PROJECT_COMMAND_EXIT_INTERNAL;
        goto done;
    }
    summary->files = (uint64_t)baseline.file_count;
    summary->bytes = baseline.total_file_bytes;
    summary->units = (uint64_t)baseline.compilation_unit_count;
    summary->declarations = (uint64_t)analysis.declaration_count;
    exit_status = EVO_PROJECT_COMMAND_EXIT_SUCCESS;

done:
    evo_project_clang_analysis_context_destroy(&clang_context);
    evo_project_analysis_destroy(&analysis);
    evo_project_baseline_destroy(&baseline);
    evo_product_environment_destroy(environment, environment == NULL ? 0U : manifest.environment_count);
    evo_project_manifest_destroy(&manifest);
    evo_project_release(text);
    return exit_status;
}

static int evo_product_summary_format(char *output, size_t capacity, const evo_product_summary_t *summary)
{
    return evo_project_format(output, capacity,
                              "{\"schema\":\"catalyst.evo.standalone-analysis-summary.v1\","
                              "\"product_version\":\"" EVO_PRODUCT_VERSION "\","
                              "\"baseline_fingerprint\":\"%s\",\"analysis_fingerprint\":\"%s\","
                              "\"source_files\":%" PRIu64 ",\"source_bytes\":%" PRIu64 ","
                              "\"translation_units\":%" PRIu64 ",\"declarations\":%" PRIu64 ","
                              "\"derived_projection\":true,\"optimization_claim\":false}\n",
                              summary->baseline, summary->analysis, summary->files, summary->bytes, summary->units, summary->declarations);
}

bool evo_product_summary_print(const evo_product_summary_t *summary)
{
    char output[1024];
    const int written = evo_product_summary_format(output, sizeof(output), summary);
    return written > 0 && (size_t)written < sizeof(output) && fputs(output, stdout) >= 0 && fflush(stdout) == 0;
}

bool evo_product_summary_write(int directory_fd, const evo_product_summary_t *summary)
{
    char output[2048];
    int written = evo_product_summary_format(output, sizeof(output), summary);
    if (written <= 0 || (size_t)written >= sizeof(output) ||
        !evo_product_write(directory_fd, "summary.json", output, (size_t)written)) {
        return false;
    }
    written = evo_project_format(output, sizeof(output),
                                 "# EVO source analysis\n\n"
                                 "| Recorded field | Value |\n|---|---|\n"
                                 "| Baseline identity | `%s` |\n| Analysis identity | `%s` |\n"
                                 "| Source files | %" PRIu64 " |\n| Source bytes | %" PRIu64 " |\n"
                                 "| Translation units | %" PRIu64 " |\n| Declarations | %" PRIu64 " |\n\n"
                                 "Derived from native baseline/analysis evidence. FNV labels are diagnostic, not authentication. "
                                 "This report makes no optimization, performance, admission or independent-correctness claim.\n",
                                 summary->baseline, summary->analysis, summary->files, summary->bytes, summary->units, summary->declarations);
    return written > 0 && (size_t)written < sizeof(output) && evo_product_write(directory_fd, "report.md", output, (size_t)written);
}
