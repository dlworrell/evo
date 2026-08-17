#!/usr/bin/env python3
from __future__ import annotations

import json
import re
import runpy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
AEMS_PATH = ROOT / ".aems/aes-bld-001.json"
PROFILE_PATH = ROOT / "docs/engineering/AES-BLD-001-toolchain-profile.md"
CMAKE_PATH = ROOT / "CMakeLists.txt"
MAKEFILE_PATH = ROOT / "Makefile.am"
TEST_CMAKE_PATH = ROOT / "tests/CMakeLists.txt"
DOC_WORKFLOW = ROOT / ".github/workflows/documentation.yml"
VERIFY_WORKFLOW = ROOT / ".github/workflows/verify.yml"
RELEASE_READINESS = ROOT / "scripts/release_readiness.py"

PROVIDER_SOURCES = [
    "src/project_provider.c",
    "src/project_provider_probe.c",
    "src/project_provider_sandbox.c",
    "src/project_provider_adapters.c",
    "src/project_provider_async.c",
    "src/project_provider_clang.c",
    "src/project_provider_clang_ast.c",
    "src/project_provider_clang_ast_authority.c",
]

PROVIDER_TESTS = [
    {
        "id": "project-provider",
        "source": "tests/project_provider_test.c",
        "cmake": "evo_project_provider_test",
        "autotools": "tests/evo_project_provider_test",
    },
    {
        "id": "project-provider-async",
        "source": "tests/project_provider_async_test.c",
        "cmake": "evo_project_provider_async_test",
        "autotools": "tests/evo_project_provider_async_test",
    },
    {
        "id": "project-provider-sandbox",
        "source": "tests/project_provider_sandbox_test.c",
        "cmake": "evo_project_provider_sandbox_test",
        "autotools": "tests/evo_project_provider_sandbox_test",
    },
    {
        "id": "project-provider-clang",
        "source": "tests/project_provider_clang_test.c",
        "cmake": "evo_project_provider_clang_test",
        "autotools": "tests/evo_project_provider_clang_test",
    },
    {
        "id": "project-provider-clang-ast",
        "source": "tests/project_provider_clang_ast_test.c",
        "cmake": "evo_project_provider_clang_ast_test",
        "autotools": "tests/evo_project_provider_clang_ast_test",
    },
]


def target_sources(text: str, target: str) -> list[str]:
    match = re.search(
        rf"add_library\(\s*{re.escape(target)}\s+STATIC(?P<body>.*?)\n\)",
        text,
        flags=re.DOTALL,
    )
    if match is None:
        raise RuntimeError(f"unable to find CMake target {target}")
    return re.findall(r"\bsrc/[A-Za-z0-9_./-]+\.c\b", match.group("body"))


def source_optimizer_gates() -> list[tuple[str, str]]:
    namespace = runpy.run_path(str(RELEASE_READINESS))
    gates = namespace["GATES"]
    return [
        (gate.display_name, gate.workflow_path)
        for gate in gates
        if gate.category == "source-optimizer"
    ]


def reconcile_aems(core: list[str], private: list[str]) -> tuple[int, int]:
    data = json.loads(AEMS_PATH.read_text(encoding="utf-8"))
    build = data["build"]
    expected_sources = core + private
    build["production_sources"] = expected_sources

    tests = build["normative_tests"]
    by_id = {entry["id"]: entry for entry in tests}
    for entry in PROVIDER_TESTS:
        by_id[entry["id"]] = entry

    existing_ids = [entry["id"] for entry in tests]
    command_index = existing_ids.index("project-command") + 1 if "project-command" in existing_ids else len(tests)
    ordered = [entry for entry in tests if entry["id"] not in {item["id"] for item in PROVIDER_TESTS}]
    for offset, entry in enumerate(PROVIDER_TESTS):
        ordered.insert(command_index + offset, entry)
    build["normative_tests"] = ordered

    AEMS_PATH.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    return len(expected_sources), len(ordered)


def write_profile(core_count: int, private_count: int, production_count: int, test_count: int) -> None:
    gates = source_optimizer_gates()
    gate_rows = "\n".join(
        f"| {name} | `{path}` | Candidate + production release evidence |"
        for name, path in gates
    )
    profile = f"""# AES-BLD-001 Toolchain Profile

## Status

- Repository: `dlworrell/evo`
- Applicability: `active-native`
- Tracking issue: `dlworrell/evo#14`
- Audit reconciliation: `dlworrell/evo#110`
- Standard authority: `dlworrell/AES`
- Enforcement authority: `dlworrell/AEMS`
- Waivers: none
- Product version: 0.43.0
- Installed reusable-core compatibility version: 0.37.0

<!-- AES-BLD-INVENTORY installed_core={core_count} private_product={private_count} production={production_count} normative_tests={test_count} -->

## Build-output boundary

EVO is deliberately a two-layer C17 repository rather than only a static-library
project. The installed output at the current 0.43.0 boundary remains the stable
`catalyst_evo` reusable library, public header, and `catalyst-evo.pc`. The
source-optimizer product foundation, command planner, and built-in production
providers are compiled as a private, uninstalled product layer. Issue #93 owns
installation of the standalone executable; until that boundary lands, the
AES-BLD machine profile's `build.kind: c-library` describes the installed
distribution surface, not the complete repository mission.

CMake and GNU Autotools are independent supported build frontends over the same
declared source and normative-test inventory.

## Authoritative toolchains

| Path | Compiler | Archive and inspection tools | Linker |
|---|---|---|---|
| CMake/Clang | Clang 18 | LLVM 18 `ar`, `ranlib`, `nm`, and `objdump` | LLD 18 |
| CMake/GCC | GCC 13 | GNU `ar`, `ranlib`, `nm`, and `objdump` | GNU BFD |
| Autotools/Clang | Clang 18 | LLVM 18 tools | LLD 18 |
| Autotools/GCC | GCC 13 | GNU tools | GNU BFD |

The Clang analysis path additionally records Clang-Tidy 18, `llvm-cov`, and
`llvm-profdata`. Checked-in CMake presets select the exact binary-tool family;
the reusable AEMS workflow supplies versioned Ubuntu 24.04 packages and records
their versions as evidence.

## Machine-authority projection

`.aems/aes-bld-001.json` is the authoritative AES-BLD inventory. At this
revision it projects to:

- **{core_count} installed-core production sources** in `catalyst_evo`;
- **{private_count} private source-optimizer/product production sources** in
  `catalyst_evo_project_foundation`;
- **{production_count} total production sources**; and
- **{test_count} normative build/test targets**.

`scripts/verify_aes_bld_profile.py` derives the CMake target membership, compares
it byte-for-byte by ordered path against the machine inventory, validates every
normative test mapping against both build frontends, and compares these counts
with the marker embedded above. The Documentation Report and Verify Repository
gates execute that verifier, so a source/test inventory change cannot leave this
active human-readable profile stale.

## Frontend parity

Both frontends:

- compile the reusable core and private product foundation as C17 with the same
  warnings-as-errors policy;
- keep the public installed core separate from private source-optimizer headers
  and implementation;
- expose sanitizer instrumentation explicitly and retain independent Clang/GCC
  build authority;
- link the same POSIX thread requirements and preserve deterministic worker
  semantics;
- independently detect `explicit_bzero` for the installed core;
- build the private seed-schedule research support only for tests;
- build ingestion, analysis, recipe, transformation, candidate, assurance,
  measurement, structured search, bounded orchestration/checkpoint-resume,
  the 0.43 command planner, and built-in production-provider plumbing without
  widening the installed public C API;
- build the private core-benchmark executable and equivalent bounded smoke
  evidence targets;
- install `libcatalyst_evo.a`, the public header, and `catalyst-evo.pc`; and
- support out-of-tree operation without network access after bootstrap.

The AEMS consumer inventory remains public-only: installed smoke and reference
adapter translation units compile only through staged package metadata, never a
private include path.

## Repository-owned source-optimizer validation

The release-readiness catalog is the machine authority for required hosted
workflow coverage. Its current `source-optimizer` gates are:

| Gate | Workflow | Release role |
|---|---|---|
{gate_rows}

Together these gates cover immutable ingestion, normalized analysis, canonical
recipes, AST-aware transformations, isolated candidate materialization,
candidate correctness/assurance, reproducible measurement, structured recipe
search, bounded orchestration/checkpoint-resume, and concrete production
provider lifecycle behavior. They supplement rather than replace the complete
AES-BLD CMake/Autotools compiler matrix.

The 0.43 `Project Command` contract is represented in the normative inventory by
`tests/project_command_test.c`. It is exercised by the complete build/test
matrices; the release-readiness catalog does not currently define a separate
Project Command workflow, so this profile does not invent one.

## Observable evidence

AES-BLD-001 provides:

1. structure, declared tool bindings, and exact tool-version evidence;
2. independent CMake and Autotools build/test matrices with Clang and GCC;
3. Clang-Tidy plus sanitizer/static-analysis evidence;
4. staged install, package metadata, public-symbol, consumer, uninstall, and
   source-distribution parity; and
5. repository-owned source-optimizer gates listed above, including production
   provider and asynchronous lifecycle proof.

The staged CMake library is inspected with LLVM tooling while the GNU Autotools
staged library is independently inspected with GNU tooling; matching evidence
cannot be produced by silently routing both frontends through one tool family.

## GitHub Actions runtime and supply-chain policy

Issue #116 updates official GitHub actions away from deprecated Node 20 runtime
releases. EVO requires `actions/checkout` major version 5 or newer and
`actions/upload-artifact` major version 6 or newer; those baselines execute on
Node 24. `scripts/verify_github_actions_runtime.py` enforces the repository
floor in `Verify Repository`.

This ticket does **not** change the repository's supply-chain reference policy:
official `actions/*` dependencies remain major-version tags where the existing
workflow fleet already uses that model. Moving to reviewed commit-SHA pins is a
separate governance decision and must not be inferred from this runtime upgrade.
Least-privilege workflow permissions remain unchanged.
"""
    PROFILE_PATH.write_text(profile, encoding="utf-8")


def write_profile_verifier() -> None:
    content = '''#!/usr/bin/env python3
"""Verify the AES-BLD machine inventory and its human-readable profile projection."""
from __future__ import annotations

import json
import re
import runpy
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
AEMS = ROOT / ".aems/aes-bld-001.json"
PROFILE = ROOT / "docs/engineering/AES-BLD-001-toolchain-profile.md"
CMAKE = ROOT / "CMakeLists.txt"
MAKEFILE = ROOT / "Makefile.am"
TEST_CMAKE = ROOT / "tests/CMakeLists.txt"
READINESS = ROOT / "scripts/release_readiness.py"


def target_sources(text: str, target: str) -> list[str]:
    match = re.search(rf"add_library\\(\\s*{re.escape(target)}\\s+STATIC(?P<body>.*?)\\n\\)", text, re.DOTALL)
    if match is None:
        raise RuntimeError(f"missing CMake target: {target}")
    return re.findall(r"\\bsrc/[A-Za-z0-9_./-]+\\.c\\b", match.group("body"))


def fail(message: str) -> None:
    print(f"AES-BLD profile verification failed: {message}", file=sys.stderr)
    raise SystemExit(1)


data = json.loads(AEMS.read_text(encoding="utf-8"))
profile = PROFILE.read_text(encoding="utf-8")
cmake = CMAKE.read_text(encoding="utf-8")
makefile = MAKEFILE.read_text(encoding="utf-8")
test_cmake = TEST_CMAKE.read_text(encoding="utf-8")
core = target_sources(cmake, "catalyst_evo")
private = target_sources(cmake, "catalyst_evo_project_foundation")
production = data["build"]["production_sources"]
normative = data["build"]["normative_tests"]

if production != core + private:
    fail(".aems production_sources does not exactly match ordered CMake core + private target membership")
if len(production) != len(set(production)):
    fail("duplicate production source in .aems inventory")

combined_build = cmake + "\\n" + test_cmake + "\\n" + makefile
for test in normative:
    source = test.get("source", "")
    cmake_target = test.get("cmake", "")
    autotools_target = test.get("autotools", "")
    if not source or not (ROOT / source).is_file():
        fail(f"normative test source missing: {source!r}")
    if cmake_target not in combined_build:
        fail(f"normative CMake target not declared: {cmake_target!r}")
    if autotools_target not in makefile:
        fail(f"normative Autotools target not declared: {autotools_target!r}")

marker = re.search(
    r"<!-- AES-BLD-INVENTORY installed_core=(\\d+) private_product=(\\d+) production=(\\d+) normative_tests=(\\d+) -->",
    profile,
)
if marker is None:
    fail("profile inventory marker is missing")
actual = (len(core), len(private), len(production), len(normative))
recorded = tuple(int(value) for value in marker.groups())
if recorded != actual:
    fail(f"profile inventory marker {recorded} does not match machine/build authority {actual}")

namespace = runpy.run_path(str(READINESS))
for gate in namespace["GATES"]:
    if gate.category != "source-optimizer":
        continue
    if gate.display_name not in profile or f"`{gate.workflow_path}`" not in profile:
        fail(f"source-optimizer release gate missing from profile: {gate.display_name}")

print(
    "AES-BLD profile verified: "
    f"core={len(core)} private={len(private)} production={len(production)} normative_tests={len(normative)}"
)
'''
    path = ROOT / "scripts/verify_aes_bld_profile.py"
    path.write_text(content, encoding="utf-8")


def write_actions_verifier() -> None:
    content = '''#!/usr/bin/env python3
"""Reject official GitHub action majors that still declare the deprecated Node 20 runtime."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
POLICY = {
    "actions/checkout": 5,
    "actions/upload-artifact": 6,
}
failures: list[str] = []
seen: dict[str, int] = {name: 0 for name in POLICY}

for path in sorted((ROOT / ".github/workflows").glob("*.yml")):
    text = path.read_text(encoding="utf-8")
    for action, minimum in POLICY.items():
        pattern = re.compile(rf"uses:\\s*{re.escape(action)}@v(\\d+)(?:\\.\\d+)*")
        for match in pattern.finditer(text):
            seen[action] += 1
            major = int(match.group(1))
            if major < minimum:
                failures.append(f"{path.relative_to(ROOT)}: {action}@v{major} is below required v{minimum}")

if failures:
    for failure in failures:
        print(f"GitHub Actions runtime verification failed: {failure}", file=sys.stderr)
    raise SystemExit(1)
for action, minimum in POLICY.items():
    if seen[action] == 0:
        print(f"GitHub Actions runtime verification failed: no {action} references found", file=sys.stderr)
        raise SystemExit(1)
    print(f"{action}: {seen[action]} references satisfy >= v{minimum}")
'''
    path = ROOT / "scripts/verify_github_actions_runtime.py"
    path.write_text(content, encoding="utf-8")


def patch_workflows() -> None:
    for path in sorted((ROOT / ".github/workflows").glob("*.yml")):
        if path.name == "issues-110-116-maintenance.yml":
            continue
        text = path.read_text(encoding="utf-8")
        text = text.replace("actions/checkout@v4", "actions/checkout@v5")
        text = text.replace("actions/upload-artifact@v4", "actions/upload-artifact@v6")
        path.write_text(text, encoding="utf-8")

    doc = DOC_WORKFLOW.read_text(encoding="utf-8")
    trigger_anchor = "      - '.github/workflows/documentation.yml'\n"
    additions = (
        "      - '.aems/aes-bld-001.json'\n"
        "      - 'CMakeLists.txt'\n"
        "      - 'Makefile.am'\n"
        "      - 'tests/CMakeLists.txt'\n"
        "      - 'scripts/release_readiness.py'\n"
        "      - 'scripts/verify_aes_bld_profile.py'\n"
        "      - 'docs/engineering/AES-BLD-001-toolchain-profile.md'\n"
    )
    if "scripts/verify_aes_bld_profile.py" not in doc:
        doc = doc.replace(trigger_anchor, trigger_anchor + additions)
    checkout_anchor = "      - uses: actions/checkout@v5\n"
    verify_step = (
        "      - name: Verify AES-BLD profile projection\n"
        "        run: python3 scripts/verify_aes_bld_profile.py\n"
    )
    if "Verify AES-BLD profile projection" not in doc:
        doc = doc.replace(checkout_anchor, checkout_anchor + verify_step, 1)
    DOC_WORKFLOW.write_text(doc, encoding="utf-8")

    verify = VERIFY_WORKFLOW.read_text(encoding="utf-8")
    verify_steps = (
        "      - name: Verify AES-BLD profile projection\n"
        "        run: python3 scripts/verify_aes_bld_profile.py\n"
        "      - name: Verify GitHub Actions runtime floor\n"
        "        run: python3 scripts/verify_github_actions_runtime.py\n"
    )
    if "Verify GitHub Actions runtime floor" not in verify:
        verify = verify.replace(checkout_anchor, checkout_anchor + verify_steps, 1)
    VERIFY_WORKFLOW.write_text(verify, encoding="utf-8")


def main() -> None:
    cmake = CMAKE_PATH.read_text(encoding="utf-8")
    core = target_sources(cmake, "catalyst_evo")
    private = target_sources(cmake, "catalyst_evo_project_foundation")
    missing = [source for source in PROVIDER_SOURCES if source not in private]
    if missing:
        raise RuntimeError(f"provider sources missing from CMake private target: {missing}")

    production_count, test_count = reconcile_aems(core, private)
    write_profile(len(core), len(private), production_count, test_count)
    write_profile_verifier()
    write_actions_verifier()
    patch_workflows()

    # Run the persistent verifiers against the reconciled tree.
    namespace = {"__name__": "__main__"}
    exec((ROOT / "scripts/verify_aes_bld_profile.py").read_text(encoding="utf-8"), namespace)
    exec((ROOT / "scripts/verify_github_actions_runtime.py").read_text(encoding="utf-8"), {"__name__": "__main__"})


if __name__ == "__main__":
    main()
