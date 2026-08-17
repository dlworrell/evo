#!/usr/bin/env python3
from pathlib import Path
import subprocess

REPO_BRANCH = "agent/issue-114-production-providers"
EXPECTED_CONFLICTS = {"CMakeLists.txt", "Makefile.am", "docs/roadmap.md"}


def run(*args, check=True):
    return subprocess.run(args, check=check, text=True, capture_output=False)


def output(*args):
    return subprocess.check_output(args, text=True).strip()


def replace_once(path, old, new):
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one match, found {count}: {old!r}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


run("git", "config", "user.name", "github-actions[bot]")
run("git", "config", "user.email", "41898282+github-actions[bot]@users.noreply.github.com")
run("git", "fetch", "origin", "main")
merge = run(
    "git",
    "merge",
    "--no-ff",
    "origin/main",
    "-m",
    "Merge v0.43 command contract into production providers",
    check=False,
)

if merge.returncode != 0:
    conflicts = set(filter(None, output("git", "diff", "--name-only", "--diff-filter=U").splitlines()))
    if conflicts != EXPECTED_CONFLICTS:
        raise SystemExit(f"unexpected merge conflicts: {sorted(conflicts)!r}")

    run("git", "checkout", "--theirs", *sorted(EXPECTED_CONFLICTS))

    cmake = Path("CMakeLists.txt")
    text = cmake.read_text(encoding="utf-8")
    old_comment = (
        "# The source-optimizer foundation is deliberately separate from the installed\n"
        "# stable core until its product-facing API is fixed by the later CLI milestone.\n"
    )
    new_comment = (
        "# The source-optimizer foundation and production providers remain separate from\n"
        "# the installed stable core; issue #93 owns standalone executable installation.\n"
    )
    if text.count(old_comment) != 1:
        raise SystemExit("CMakeLists.txt: source-optimizer comment anchor mismatch")
    text = text.replace(old_comment, new_comment, 1)

    old_sources = "    src/project_runtime.c\n    src/project_json.c\n"
    provider_sources = """    src/project_runtime.c
    src/project_provider.c
    src/project_provider_probe.c
    src/project_provider_sandbox.c
    src/project_provider_adapters.c
    src/project_provider_async.c
    src/project_provider_clang.c
    src/project_provider_clang_ast.c
    src/project_provider_clang_ast_authority.c
    src/project_json.c
"""
    if text.count(old_sources) != 1:
        raise SystemExit("CMakeLists.txt: provider source anchor mismatch")
    text = text.replace(old_sources, provider_sources, 1)

    old_tests = "    add_test(NAME evo_project_command_test COMMAND evo_project_command_test)\nendif()\n"
    provider_tests = """    add_test(NAME evo_project_command_test COMMAND evo_project_command_test)

    add_executable(evo_project_provider_test tests/project_provider_test.c)
    target_include_directories(
        evo_project_provider_test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(
        evo_project_provider_test
        PRIVATE
            catalyst_evo_project_foundation
    )
    add_test(NAME evo_project_provider_test COMMAND evo_project_provider_test)

    add_executable(
        evo_project_provider_async_test
        tests/project_provider_async_test.c
    )
    target_include_directories(
        evo_project_provider_async_test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(
        evo_project_provider_async_test
        PRIVATE
            catalyst_evo_project_foundation
    )
    add_test(
        NAME evo_project_provider_async_test
        COMMAND evo_project_provider_async_test
    )

    add_executable(
        evo_project_provider_sandbox_test
        tests/project_provider_sandbox_test.c
    )
    target_include_directories(
        evo_project_provider_sandbox_test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(
        evo_project_provider_sandbox_test
        PRIVATE
            catalyst_evo_project_foundation
    )
    add_test(
        NAME evo_project_provider_sandbox_test
        COMMAND evo_project_provider_sandbox_test
    )
    set_tests_properties(
        evo_project_provider_sandbox_test
        PROPERTIES SKIP_RETURN_CODE 77
    )

    add_executable(
        evo_project_provider_clang_test
        tests/project_provider_clang_test.c
    )
    target_include_directories(
        evo_project_provider_clang_test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(
        evo_project_provider_clang_test
        PRIVATE
            catalyst_evo_project_foundation
    )
    add_test(
        NAME evo_project_provider_clang_test
        COMMAND evo_project_provider_clang_test
    )
    set_tests_properties(
        evo_project_provider_clang_test
        PROPERTIES SKIP_RETURN_CODE 77
    )

    add_executable(
        evo_project_provider_clang_ast_test
        tests/project_provider_clang_ast_test.c
    )
    target_include_directories(
        evo_project_provider_clang_ast_test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(
        evo_project_provider_clang_ast_test
        PRIVATE
            catalyst_evo_project_foundation
    )
    add_test(
        NAME evo_project_provider_clang_ast_test
        COMMAND evo_project_provider_clang_ast_test
    )
    set_tests_properties(
        evo_project_provider_clang_ast_test
        PROPERTIES SKIP_RETURN_CODE 77
    )
endif()
"""
    if text.count(old_tests) != 1:
        raise SystemExit("CMakeLists.txt: provider test anchor mismatch")
    cmake.write_text(text.replace(old_tests, provider_tests, 1), encoding="utf-8")

    replace_once(
        "Makefile.am",
        "\tsrc/internal/project_runtime.h \\\n\tsrc/internal/project_json.h \\\n",
        "\tsrc/internal/project_runtime.h \\\n\tsrc/internal/project_provider.h \\\n\tsrc/internal/project_provider_probe.h \\\n\tsrc/internal/project_provider_sandbox.h \\\n\tsrc/internal/project_provider_adapters.h \\\n\tsrc/internal/project_provider_async.h \\\n\tsrc/internal/project_provider_clang.h \\\n\tsrc/internal/project_provider_clang_ast.h \\\n\tsrc/internal/project_provider_clang_ast_authority.h \\\n\tsrc/internal/project_json.h \\\n",
    )
    replace_once(
        "Makefile.am",
        "\tsrc/project_runtime.c \\\n\tsrc/project_json.c \\\n",
        "\tsrc/project_runtime.c \\\n\tsrc/project_provider.c \\\n\tsrc/project_provider_probe.c \\\n\tsrc/project_provider_sandbox.c \\\n\tsrc/project_provider_adapters.c \\\n\tsrc/project_provider_async.c \\\n\tsrc/project_provider_clang.c \\\n\tsrc/project_provider_clang_ast.c \\\n\tsrc/project_provider_clang_ast_authority.c \\\n\tsrc/project_json.c \\\n",
    )
    replace_once(
        "Makefile.am",
        "\ttests/evo_project_command_test \\\n\tbenchmarks/evo_core_benchmark \\\n",
        "\ttests/evo_project_command_test \\\n\ttests/evo_project_provider_test \\\n\ttests/evo_project_provider_async_test \\\n\ttests/evo_project_provider_sandbox_test \\\n\ttests/evo_project_provider_clang_test \\\n\ttests/evo_project_provider_clang_ast_test \\\n\tbenchmarks/evo_core_benchmark \\\n",
    )
    replace_once(
        "Makefile.am",
        "tests_evo_project_command_test_SOURCES = tests/project_command_test.c\n"
        "tests_evo_project_command_test_LDADD = libevo_project_foundation.la\n\n"
        "benchmarks_evo_core_benchmark_SOURCES",
        """tests_evo_project_command_test_SOURCES = tests/project_command_test.c
tests_evo_project_command_test_LDADD = libevo_project_foundation.la

tests_evo_project_provider_test_SOURCES = \\
\ttests/project_provider_test.c
tests_evo_project_provider_test_LDADD = \\
\tlibevo_project_foundation.la

tests_evo_project_provider_async_test_SOURCES = \\
\ttests/project_provider_async_test.c
tests_evo_project_provider_async_test_LDADD = \\
\tlibevo_project_foundation.la

tests_evo_project_provider_sandbox_test_SOURCES = \\
\ttests/project_provider_sandbox_test.c
tests_evo_project_provider_sandbox_test_LDADD = \\
\tlibevo_project_foundation.la

tests_evo_project_provider_clang_test_SOURCES = \\
\ttests/project_provider_clang_test.c
tests_evo_project_provider_clang_test_LDADD = \\
\tlibevo_project_foundation.la

tests_evo_project_provider_clang_ast_test_SOURCES = \\
\ttests/project_provider_clang_ast_test.c
tests_evo_project_provider_clang_ast_test_LDADD = \\
\tlibevo_project_foundation.la

benchmarks_evo_core_benchmark_SOURCES""",
    )
    replace_once(
        "Makefile.am",
        "\t.github/workflows/project-measurement.yml \\\n\tCMakeLists.txt \\\n",
        "\t.github/workflows/project-measurement.yml \\\n\t.github/workflows/production-providers.yml \\\n\t.github/workflows/production-provider-async.yml \\\n\tCMakeLists.txt \\\n",
    )
    replace_once(
        "Makefile.am",
        "\tdocs/adr/ADR-0041-reproducible-candidate-performance-fitness.md \\\n"
        "\tdocs/schemas/evo-project-manifest-v1.schema.json \\\n",
        "\tdocs/adr/ADR-0041-reproducible-candidate-performance-fitness.md \\\n"
        "\tdocs/adr/ADR-0044-built-in-production-providers.md \\\n"
        "\tdocs/schemas/evo-project-manifest-v1.schema.json \\\n",
    )

    roadmap = Path("docs/roadmap.md")
    text = roadmap.read_text(encoding="utf-8")
    old_handoff = """**After #67 lands, #114 is the next dependency-ready implementation boundary.**
Its concrete provider implementation may already be prepared on a separate
branch, but final #114 closure must reconcile against the landed EVO-003
command/provider contract before #93 installs the executable.
"""
    new_handoff = """**#67 is complete; #114 is the current dependency-ready implementation boundary.**
The concrete provider implementation now reconciles against the landed EVO-003
command/provider contract. Final #114 closure precedes #93 installation of the
standalone executable.
"""
    if text.count(old_handoff) != 1:
        raise SystemExit("docs/roadmap.md: #114 handoff anchor mismatch")
    text = text.replace(old_handoff, new_handoff, 1)
    hra_anchor = """For issue #67, the command registry and provider-requirement registry are fixed
ordered arrays with direct scans. They are their own exact reference
representations; no accelerated authority is introduced. EVO-HRA-016 records
that issue-specific assessment.
"""
    hra_addition = hra_anchor + """
For issue #114, the production-provider registry is likewise a fixed ordered
registry with direct selection and exact capability checks. No compressed,
probabilistic, cached, or indexed provider authority is introduced; provider
availability and capability evidence remain explicit and fail closed.
"""
    if text.count(hra_anchor) != 1:
        raise SystemExit("docs/roadmap.md: HRA anchor mismatch")
    roadmap.write_text(text.replace(hra_anchor, hra_addition, 1), encoding="utf-8")

    run("git", "add", "CMakeLists.txt", "Makefile.am", "docs/roadmap.md")
    run("git", "diff", "--cached", "--check")
    remaining = output("git", "diff", "--name-only", "--diff-filter=U")
    if remaining:
        raise SystemExit(f"unresolved conflicts remain: {remaining}")
    run("git", "commit", "--no-edit")

for helper in (
    Path(".github/workflows/issue-114-refresh-main.yml"),
    Path(".github/scripts/issue-114-refresh.py"),
):
    helper.unlink()
run("git", "add", "-A")
run("git", "diff", "--cached", "--check")
run("git", "commit", "-m", "Remove issue 114 refresh helpers")
run("git", "push", "origin", f"HEAD:{REPO_BRANCH}")
