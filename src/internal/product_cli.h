#ifndef CATALYST_EVO_INTERNAL_PRODUCT_CLI_H
#define CATALYST_EVO_INTERNAL_PRODUCT_CLI_H

#include "internal/project_command.h"

#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define EVO_PRODUCT_PATH_BYTES 4096U
#define EVO_PRODUCT_MANIFEST_BYTES 1048576U
#define EVO_PRODUCT_EVIDENCE_BYTES 16777216U
#define EVO_PRODUCT_JSON_TOKENS 262144U

typedef struct evo_product_stage {
    int parent_fd;
    int directory_fd;
    char parent_path[EVO_PRODUCT_PATH_BYTES];
    char output_path[EVO_PRODUCT_PATH_BYTES];
    char stage_path[EVO_PRODUCT_PATH_BYTES];
    char output_name[256];
    char stage_name[256];
    dev_t device;
    ino_t inode;
    bool published;
    bool created;
} evo_product_stage_t;

typedef struct evo_product_summary {
    char baseline[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    char analysis[EVO_PROJECT_FINGERPRINT_TEXT_SIZE];
    uint64_t files;
    uint64_t bytes;
    uint64_t units;
    uint64_t declarations;
} evo_product_summary_t;

int evo_product_open_path(const char *argument, bool directory,
                          char absolute[EVO_PRODUCT_PATH_BYTES]);
int evo_product_open_relative(int directory_fd, const char *path, bool directory);
bool evo_product_read(int descriptor, size_t maximum, char **bytes, size_t *size);
bool evo_product_stage_prepare(const char *output, evo_product_stage_t *stage);
bool evo_product_stage_begin(evo_product_stage_t *stage);
bool evo_product_stage_publish(evo_product_stage_t *stage);
bool evo_product_stage_destroy(evo_product_stage_t *stage);
bool evo_product_write(int directory_fd, const char *name, const char *bytes,
                       size_t size);
bool evo_product_path_contains(const char *root, const char *path);
int evo_product_execute_analyze(const char *manifest, const char *input,
                                evo_product_stage_t *stage,
                                const volatile sig_atomic_t *interrupted,
                                evo_product_summary_t *summary);
int evo_product_execute_report(const char *evidence, evo_product_stage_t *stage,
                               evo_product_summary_t *summary);
bool evo_product_summary_write(int directory_fd,
                               const evo_product_summary_t *summary);
bool evo_product_summary_print(const evo_product_summary_t *summary);
int evo_product_cli_main(int argc, char **argv);

#endif
