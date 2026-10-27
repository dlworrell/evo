#define _XOPEN_SOURCE 700

#include "internal/product_cli.h"
#include "internal/project_provider_sandbox.h"
#include "internal/project_runtime.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures;

static void check(bool result, const char *message)
{
    if (!result) {
        (void)fprintf(stderr, "product CLI test: %s\n", message);
        failures += 1;
    }
}

int main(void)
{
    char temporary[] = "/tmp/evo-product-cli-XXXXXX";
    char absolute[EVO_PRODUCT_PATH_BYTES];
    char path[EVO_PRODUCT_PATH_BYTES];
    char resolved[EVO_PRODUCT_PATH_BYTES];
    char *created = mkdtemp(temporary);
    char *root = created == NULL ? NULL : realpath(created, resolved);
    if (root == NULL) {
        return 1;
    }
    const int directory = evo_product_open_path(root, true, absolute);
    check(directory >= 0 && strcmp(root, absolute) == 0, "open explicit directory");
    if (directory < 0) {
        return 1;
    }
    check(evo_product_write(directory, "source", "original", 8U), "create regular source fixture");
    check(symlinkat("source", directory, "alias") == 0, "create symlink fixture");
    check(evo_product_open_relative(directory, "alias", false) < 0, "reject symlink file");
    check(evo_product_open_relative(directory, "../source", false) < 0, "reject parent traversal");
    check(evo_product_path_contains(root, root), "same path intersects");
    check(!evo_product_path_contains("/tmp/root", "/tmp/root-sibling"), "prefix is not directory containment");
    int source = evo_product_open_relative(directory, "source", false);
    char *bytes = NULL;
    size_t size = 0U;
    check(source >= 0 && !evo_product_read(source, 4U, &bytes, &size) && errno == EFBIG, "oversized file rejected before allocation");
    if (source >= 0) {
        (void)close(source);
    }
    check(mkfifoat(directory, "fifo", 0600) == 0, "create FIFO fixture");
    check(evo_product_open_relative(directory, "fifo", false) < 0, "special input rejected without blocking");

    evo_product_stage_t stage = {.parent_fd = -1, .directory_fd = -1};
    check(evo_project_format(path, sizeof(path), "%s/output", root) > 0, "format output fixture");
    check(evo_product_stage_prepare(path, &stage), "prepare explicit new output");
    check(access(path, F_OK) != 0 && stage.directory_fd < 0, "preflight creates no output or stage");
    check(evo_product_stage_begin(&stage), "begin owned private stage");
    check(evo_product_write(stage.directory_fd, "payload", "ours", 4U), "write staged payload");
    check(mkdirat(directory, "output", 0700) == 0, "simulate concurrent output creation");
    const int winner = openat(directory, "output", O_RDONLY | O_DIRECTORY);
    check(winner >= 0 && evo_product_write(winner, "marker", "winner", 6U), "retain concurrent creator marker");
    if (winner >= 0) {
        (void)close(winner);
    }
    check(!evo_product_stage_publish(&stage), "atomic publication cannot replace concurrent output");
    check(evo_product_stage_destroy(&stage), "discard only owned stage");
    source = evo_product_open_relative(directory, "output/marker", false);
    check(source >= 0, "concurrent output remains untouched");
    if (source >= 0) {
        (void)close(source);
    }
    check(unlinkat(directory, "output/marker", 0) == 0 && unlinkat(directory, "output", AT_REMOVEDIR) == 0, "remove own race fixture");

    check(evo_product_stage_prepare(path, &stage) && evo_product_stage_begin(&stage), "new successful publication");
    check(evo_product_write(stage.directory_fd, "payload", "complete", 8U), "write complete payload");
#if defined(__linux__)
    check(evo_product_stage_publish(&stage), "atomic no-replace directory commit");
    check(evo_product_stage_destroy(&stage), "release published ownership without deletion");
    source = evo_product_open_relative(directory, "output/payload", false);
    check(source >= 0 && evo_product_read(source, 8U, &bytes, &size), "read committed bytes");
    check(bytes != NULL && size == 8U && memcmp(bytes, "complete", 8U) == 0, "committed bytes exact");
    evo_project_release(bytes);
    if (source >= 0) {
        (void)close(source);
    }
    check(unlinkat(directory, "output/payload", 0) == 0 && unlinkat(directory, "output", AT_REMOVEDIR) == 0, "remove own committed fixture");
#else
    check(!evo_product_stage_publish(&stage), "unsupported atomic publication fails closed");
    check(evo_product_stage_destroy(&stage), "unsupported platform discards owned stage");
#endif
    check(chmod(root, 0777) == 0, "create unsafe parent mode");
    check(!evo_product_stage_prepare(path, &stage), "reject shared writable output parent");
    check(evo_product_stage_destroy(&stage), "failed preflight releases descriptors");
    check(chmod(root, 0700) == 0, "restore fixture parent mode");

    volatile sig_atomic_t canceled = 1;
    static const char *const arguments[] = {"/bin/true"};
    const evo_project_sandbox_command_t command = {
        .schema_version = EVO_PROJECT_SANDBOX_SCHEMA_VERSION,
        .workspace_path = root,
        .working_directory = ".",
        .argument_count = 1U,
        .arguments = arguments,
        .limits = {1000U, 67108864U, 8U, 1048576U, 4096U, 1000U, false},
        .cancel_requested = &canceled,
    };
    evo_project_sandbox_result_t result = {0};
    check(evo_project_sandbox_run(&command, &result) == EVO_PROJECT_SANDBOX_ERROR_INTERRUPTED,
          "pre-cancellation refuses admission before probing or child execution");
    check(result.private_owner == NULL && !result.completed, "canceled work has no result ownership");

    check(unlinkat(directory, "fifo", 0) == 0 && unlinkat(directory, "alias", 0) == 0 && unlinkat(directory, "source", 0) == 0, "remove own inputs");
    check(close(directory) == 0 && rmdir(root) == 0, "all stage ownership settled");
    return failures == 0 ? 0 : 1;
}
