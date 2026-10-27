#define _GNU_SOURCE

#include "internal/product_cli.h"
#include "internal/project_provider.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__cppcheck__)
/* Standalone source scans do not import frontend -D string definitions. */
#undef EVO_PRODUCT_VERSION
#define EVO_PRODUCT_VERSION "source-analysis-only"
#endif
#ifndef EVO_PRODUCT_VERSION
#error "The build frontend must define EVO_PRODUCT_VERSION"
#endif

static volatile sig_atomic_t evo_product_interrupted;

static void evo_product_signal(int number)
{
    evo_product_interrupted = number;
}

static int evo_product_help(const evo_project_command_descriptor_t *command)
{
    if (command != NULL) {
        (void)printf("Usage: evo %s\n", command->help);
        if (command->operation == EVO_PROJECT_COMMAND_EVOLVE || command->operation == EVO_PROJECT_COMMAND_REPLAY) {
            (void)printf("This operation is not available in this development build.\n");
        }
        return EVO_PROJECT_COMMAND_EXIT_SUCCESS;
    }
    (void)printf("EVO %s standalone development build\nUsage: evo <command> [options]\n\n", EVO_PRODUCT_VERSION);
    for (size_t index = 0U; index < evo_project_command_registry_count(); index += 1U) {
        const evo_project_command_descriptor_t *descriptor = evo_project_command_registry_at(index);
        (void)printf("  %s%s\n", descriptor->help,
                     descriptor->operation == EVO_PROJECT_COMMAND_EVOLVE || descriptor->operation == EVO_PROJECT_COMMAND_REPLAY ? " [unavailable]" : "");
    }
    (void)printf("\n--help and --version need no providers. Execution/report publication currently require Linux.\n"
                 "Paths resolve from the invocation directory; symlink components and '..' are rejected.\n"
                 "Outputs must be new directories under an owner-controlled parent.\n"
                 "Runtime tools use /usr/local/bin:/usr/bin:/bin, independently of the caller's PATH.\n\nProduction providers:\n");
    for (size_t index = 0U; index < evo_project_provider_registry_count(); index += 1U) {
        const evo_project_provider_record_t *provider = evo_project_provider_registry_at(index);
        (void)printf("  %s (implementation %u; %s)\n", provider->identity, provider->implementation_version, provider->runtime_requirement);
    }
    return ferror(stdout) == 0 ? EVO_PROJECT_COMMAND_EXIT_SUCCESS : EVO_PROJECT_COMMAND_EXIT_INTERNAL;
}

static bool evo_product_runtime_path(void)
{
    static const char *const directories[] = {"/usr/local/bin", "/usr/bin", "/bin"};
    for (size_t index = 0U; index < sizeof(directories) / sizeof(directories[0]); index += 1U) {
        struct stat metadata;
        if (stat(directories[index], &metadata) != 0) {
            if (errno == ENOENT) {
                continue;
            }
            return false;
        }
        if (!S_ISDIR(metadata.st_mode) || (metadata.st_uid != 0U && metadata.st_uid != geteuid()) ||
            (metadata.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
            (void)fprintf(stderr, "evo: unsafe runtime directory %s (owner %lu, mode %04o)\n",
                          directories[index], (unsigned long)metadata.st_uid, (unsigned int)(metadata.st_mode & 0777U));
            return false;
        }
    }
#if defined(__linux__)
    if (clearenv() != 0) {
        return false;
    }
#endif
    return setenv("PATH", "/usr/local/bin:/usr/bin:/bin", 1) == 0 &&
           setenv("LANG", "C", 1) == 0 && setenv("LC_ALL", "C", 1) == 0;
}

int evo_product_cli_main(int argc, char **argv)
{
    const char *manifest = NULL;
    const char *input = NULL;
    const char *output = NULL;
    const char *evidence = NULL;
    const char *checkpoint = NULL;
    bool resume = false;
    evo_product_stage_t stage = {.parent_fd = -1, .directory_fd = -1};
    evo_product_summary_t summary = {0};

    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        return printf("evo %s (standalone development)\n", EVO_PRODUCT_VERSION) >= 0 ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        return evo_product_help(NULL);
    }
    const evo_project_command_descriptor_t *command = argc > 1 ? evo_project_command_find(argv[1]) : NULL;
    if (command == NULL) {
        (void)fprintf(stderr, "evo: usage: select analyze, evolve, replay or report; use --help\n");
        return EVO_PROJECT_COMMAND_EXIT_USAGE;
    }
    if (argc == 3 && strcmp(argv[2], "--help") == 0) {
        return evo_product_help(command);
    }
    for (int index = 2; index < argc; index += 1) {
        const char **slot = NULL;
        if (strcmp(argv[index], "--resume") == 0 && !resume) {
            resume = true;
            continue;
        }
        if (strcmp(argv[index], "--manifest") == 0) {
            slot = &manifest;
        } else if (strcmp(argv[index], "--input") == 0) {
            slot = &input;
        } else if (strcmp(argv[index], "--output") == 0) {
            slot = &output;
        } else if (strcmp(argv[index], "--evidence") == 0) {
            slot = &evidence;
        } else if (strcmp(argv[index], "--checkpoint") == 0) {
            slot = &checkpoint;
        }
        if (slot == NULL || *slot != NULL || index + 1 >= argc ||
            argv[index + 1][0] == '\0' || strncmp(argv[index + 1], "--", 2U) == 0) {
            (void)fprintf(stderr, "evo: usage: unknown, duplicate or incomplete option\n");
            return EVO_PROJECT_COMMAND_EXIT_USAGE;
        }
        *slot = argv[++index];
    }
    if (output == NULL || (command->requires_manifest && manifest == NULL) ||
        (command->requires_input && input == NULL) || (command->requires_evidence && evidence == NULL) ||
        (!command->requires_manifest && manifest != NULL) || (!command->requires_input && input != NULL) ||
        (!command->requires_evidence && evidence != NULL) ||
        (!command->allows_checkpoint && (checkpoint != NULL || resume)) || (resume && checkpoint == NULL)) {
        (void)fprintf(stderr, "evo: usage: missing required input or option incompatible with operation\n");
        return EVO_PROJECT_COMMAND_EXIT_USAGE;
    }
    if (command->operation == EVO_PROJECT_COMMAND_EVOLVE || command->operation == EVO_PROJECT_COMMAND_REPLAY) {
        (void)fprintf(stderr, "evo: configuration: operation-unavailable in this development build\n");
        return EVO_PROJECT_COMMAND_EXIT_CONFIGURATION;
    }
#if !defined(__linux__)
    (void)fprintf(stderr, "evo: configuration: the current standalone execution/publication profile requires Linux\n");
    return EVO_PROJECT_COMMAND_EXIT_CONFIGURATION;
#endif
    if (command->operation == EVO_PROJECT_COMMAND_ANALYZE && !evo_product_runtime_path()) {
        (void)fprintf(stderr, "evo: configuration: trusted runtime directories are unavailable or unsafe\n");
        return EVO_PROJECT_COMMAND_EXIT_CONFIGURATION;
    }
    struct sigaction action = {0};
    struct sigaction previous_int;
    struct sigaction previous_term;
    action.sa_handler = evo_product_signal;
    evo_product_interrupted = 0;
    if (sigemptyset(&action.sa_mask) != 0 || sigaction(SIGINT, &action, &previous_int) != 0) {
        return EVO_PROJECT_COMMAND_EXIT_INTERNAL;
    }
    if (sigaction(SIGTERM, &action, &previous_term) != 0) {
        (void)sigaction(SIGINT, &previous_int, NULL);
        return EVO_PROJECT_COMMAND_EXIT_INTERNAL;
    }
    int status = EVO_PROJECT_COMMAND_EXIT_CONFIGURATION;
    if (!evo_product_stage_prepare(output, &stage)) {
        (void)fprintf(stderr, "evo: configuration: output must be new with a safe, existing parent\n");
        goto done;
    }
    status = command->operation == EVO_PROJECT_COMMAND_ANALYZE
                 ? evo_product_execute_analyze(manifest, input, &stage, &evo_product_interrupted, &summary)
                 : evo_product_execute_report(evidence, &stage, &summary);
    if (evo_product_interrupted != 0 && status != EVO_PROJECT_COMMAND_EXIT_INTERNAL) {
        status = EVO_PROJECT_COMMAND_EXIT_INTERRUPTED;
    }
    if (status == EVO_PROJECT_COMMAND_EXIT_SUCCESS && !evo_product_summary_write(stage.directory_fd, &summary)) {
        status = EVO_PROJECT_COMMAND_EXIT_INTERNAL;
    }
    if (evo_product_interrupted != 0 && status != EVO_PROJECT_COMMAND_EXIT_INTERNAL) {
        status = EVO_PROJECT_COMMAND_EXIT_INTERRUPTED;
    }
    if (status == EVO_PROJECT_COMMAND_EXIT_SUCCESS && !evo_product_stage_publish(&stage)) {
        (void)fprintf(stderr, "evo: publication failed%s\n", stage.published ? " after atomic commit; complete output retained" : "");
        status = EVO_PROJECT_COMMAND_EXIT_INTERNAL;
    }
    if (status == EVO_PROJECT_COMMAND_EXIT_SUCCESS && !evo_product_summary_print(&summary)) {
        status = EVO_PROJECT_COMMAND_EXIT_INTERNAL;
    }

done:
    if (!evo_product_stage_destroy(&stage)) {
        (void)fprintf(stderr, "evo: cleanup incomplete; owned staging path retained: %s\n", stage.stage_path);
        status = EVO_PROJECT_COMMAND_EXIT_INTERNAL;
    }
    (void)sigaction(SIGINT, &previous_int, NULL);
    (void)sigaction(SIGTERM, &previous_term, NULL);
    if (status != EVO_PROJECT_COMMAND_EXIT_SUCCESS) {
        (void)fprintf(stderr, "evo: %s (exit %d)\n", evo_project_command_exit_status_name((evo_project_command_exit_status_t)status), status);
    }
    return status;
}
