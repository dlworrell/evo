# Standalone CLI development guide

Status: Draft implementation for [#93](https://github.com/dlworrell/evo/issues/93).
The canonical installed executable is `evo`. This slice implements production
`analyze` and a derived `report`; `evolve` and `replay` return exit 3 as
unavailable. Their help remains available. Package version is 0.43.0 development;
the separately installed reusable C core retains compatibility version 0.37.0.
This is not completion of the 0.44.0 milestone.

## Installation and dependencies

Use either frontend from a checkout:

```sh
cmake -S . -B build/cli -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PWD/build/evo-prefix"
cmake --build build/cli --parallel 2
cmake --install build/cli

# Alternative, independently supported frontend:
autoreconf -fvi
mkdir -p build/cli-autotools
cd build/cli-autotools
../../configure --prefix="$PWD/evo-prefix"
make -j2
make install
```

The inventory is the configured `bindir/evo`, `libdir/libcatalyst_evo.a`,
`includedir/catalyst/evo/evo.h` and `libdir/pkgconfig/catalyst-evo.pc`.
Autotools may additionally install its declared libtool metadata exclusion.
The program statically links private product code; no private `.so`, header,
schema file, Python launcher, checkout, build tree or test fixture is loaded at
runtime. The compiled strict parsers and fixed registries are its runtime
assets. JSON schemas under `docs/schemas` are reference/distribution material,
not a hidden runtime search path. The binary's system C/math/thread runtime
dependencies remain external and are inspected in hosted installation tests.
The Apache-2.0 repository license covers both EVO components. External Clang,
Bubblewrap, build tools and target dependencies retain their own licenses;
this change does not bundle them or add a package-manager dependency.

The current execution and output-publication profile requires Linux with
working unprivileged user/PID/network namespaces, Bubblewrap, Clang 18 exposed
as `clang`, and the explicitly declared target build/test commands. Supported
system-tool directories are `/usr/local/bin`, `/usr/bin` and `/bin`; install
runtimes there before invoking EVO. The example below additionally requires
CMake 3.20+ and a C17 `cc`. EVO validates required production provider identity,
implementation version and capabilities, probes isolation, and records the
observed Clang version before source capture. The Clang distribution line also
labels LLVM; it does not attest to a separate LLVM installation. Arbitrary
target dependency/toolchain identity declarations are not independently
authenticated by this draft. Unsupported tools, options, resource policy or
isolation fail closed. EVO never downloads missing runtimes.

Help/version do not probe providers. macOS can build and install the program
for these provider-free help surfaces; actual analyze/report publication is
Linux-only in this draft. Windows execution is not supported.

## Complete small-project invocation

Put an installed `bin` directory on PATH, then run this from an unrelated
owner-controlled directory. All generated paths are explicit user inputs;
none refer to the EVO checkout. The sample's independently expected result is
42. It demonstrates baseline execution and analysis, not an optimization.

```sh
mkdir -m 700 project results
cat > project/main.c <<'C'
static int answer(void) { return 42; }
int main(void) { return answer() == 42 ? 0 : 1; }
C
cat > project/CMakeLists.txt <<'CMAKE'
cmake_minimum_required(VERSION 3.20)
project(sample C)
set(CMAKE_C_STANDARD 17)
add_executable(sample main.c)
CMAKE
cat > project/compile_commands.json <<'JSON'
[{"directory":".","file":"main.c","arguments":["cc","-std=c17","-c","main.c"]}]
JSON
cat > manifest.json <<'JSON'
{
  "schema":"catalyst.evo-project-manifest.v1",
  "manifest_id":"standalone-example-v1",
  "source":{
    "declared_identity":"answer-42-source-v1",
    "permitted_roots":["CMakeLists.txt","compile_commands.json","main.c"],
    "compilation_database":"compile_commands.json",
    "generated_source_policy":"reject"
  },
  "build":{
    "frontend":"cmake",
    "configure":["cmake","-S",".","-B","build"],
    "compile":["cmake","--build","build","--parallel","1"],
    "correctness":["build/sample"],
    "benchmark":["/bin/true"],
    "benchmark_required":false
  },
  "target":{"language":"c17","platforms":["host-c17"]},
  "dependencies":[{"name":"c-runtime","identity":"iso-c17"}],
  "toolchains":[{"name":"c-compiler","identity":"host-cc-and-clang-18"}],
  "environment":[{"name":"LANG","value":"C"},{"name":"LC_ALL","value":"C"}],
  "workloads":["answer-42"],
  "constraints":["immutable-input","no-network"],
  "search":{"seed":17,"population":2,"generations":1,"workers":1},
  "budgets":{
    "max_files":32,"max_file_bytes":65536,"max_total_bytes":262144,
    "max_path_bytes":512,"max_compilation_database_bytes":65536,
    "max_command_output_bytes":1048576,"max_evidence_bytes":4194304,
    "command_timeout_ms":30000,"max_memory_bytes":1073741824,
    "max_processes":32,"max_storage_bytes":33554432,"network_access":false
  },
  "artifacts":{"retention":"baseline-only","cleanup":"remove-derived-workspace"}
}
JSON
evo analyze --manifest manifest.json --input project --output results/analysis
evo report --evidence results/analysis --output results/report
```

`analyze` retains the exact parsed `manifest.json`, a native
`baseline/baseline.json` and immutable `baseline/snapshot`, native
`analysis/analysis.json`, `summary.json` and `report.md`. The selected successful
machine output is always one JSON summary on stdout after publication.
Diagnostics use stderr. The source directory is never a build workspace.
No Git commit, push, merge, deployment or downstream overwrite operation exists.

`report` needs no compiler or sandbox. It checks unique required root fields,
native schemas/versions, eligible state, fixed provider IDs, corresponding
baseline labels and file/unit/declaration counts, then writes a derived
summary/report to another new directory. It does not re-run source, validate
every nested record or authenticate bytes. Native FNV identities are diagnostic
labels; a matching label is not proof of correctness, integrity or admission.
The summary explicitly sets `derived_projection:true` and
`optimization_claim:false`.

## Paths, environment and failure contract

Relative arguments resolve from the invocation directory. No hidden source,
cache, credential or network location is inferred. Symlink components, `..`,
control characters, overlong paths and special files are rejected. Output must
be a new directory disjoint from source/evidence. Its existing parent must
belong to the effective user and be unwritable by group/others; direct output
under shared `/tmp` is rejected, so first create a private directory there.
Existing output, including dangling symlinks and concurrent creators, is never
replaced. The supported host, system installation and same-UID host processes
are trusted; this is not isolation from a compromised host.

Before Linux analysis probes, inherited environment state is cleared. Host
tools receive only fixed PATH and C locale. Bubblewrap clears target
environment, then supplies fixed PATH and explicitly declared manifest entries.
The manifest cannot override PATH. All target execution retains the provider's
network isolation and resource limits. No weaker fallback is selected.

The CLI caps manifests at 1 MiB, paths at 4095 bytes, individual source files
at 16 MiB, total source at 128 MiB and source inventory at 65,536 files.
Manifest JSON is bounded to 262,144 tokens/depth 64. Native analysis records
are bounded by kind (65,536; 4,096 translation units), with 16 MiB evidence.
The report reader caps each native JSON file at 16 MiB and 1,048,576 tokens /
depth 64; exceeding its token cap fails closed and may require a later reader
profile for larger native outputs. Execution budget ceilings are 600 seconds
per command, 2 GiB address space, 128 descendants, 1 GiB workspace storage and
4 MiB captured command output. A manifest can request lower limits. Owned
stage cleanup is bounded to depth 128 and one million entries.

| Exit | Meaning in this draft |
|---:|---|
| 0 | Complete output committed, or requested help/version emitted |
| 1 | Internal, publication, stdout or cleanup failure |
| 2 | Usage/argument error |
| 3 | Invalid configuration/evidence, unavailable operation/provider/tool/platform |
| 10 | Source ingestion/capture failure |
| 11 | Baseline configure/build failure |
| 12 | Baseline correctness failure |
| 13 | Required baseline benchmark ineligible |
| 20 | Analysis failure |
| 21–24 | Reserved command-contract materialization/candidate/no-champion/replay classes |
| 70 | Allocation, parser/input or execution resource exhaustion |
| 130 | SIGINT/SIGTERM handled before commit, after owned work stops and cleanup succeeds |

SIGINT/SIGTERM request cooperative cancellation. The sandbox supervisor stops
and joins owned work; the CLI removes only its own unpublished stage and
publishes no partial successful directory. No automatic checkpoint is written.
The complete directory becomes visible with Linux atomic no-replace rename.
If commit wins the race with a signal, complete output remains. A later parent
fsync/stdout failure reports exit 1 while retaining committed output. This is
an atomic-visibility guarantee, not a power-loss recovery guarantee. Cleanup
failure overrides interruption with exit 1 and identifies the owned staging
path. SIGKILL/host failure cannot run cooperative cleanup; inspect any retained
`.evo-run-*` directory before manually removing it.

## Qualification and remaining work

`tests/product_cli_test.c` is normative for both frontends. Run
`python3 tests/test_standalone_cli.py --prefix /absolute/installed-prefix`
for provider-free paths and observed availability behavior. For qualification,
use `--require-providers`: no unavailable-runtime skip or fake provider is
accepted. The proposed Standalone CLI workflow does this for relocated
CMake/Clang and Autotools/GCC installations, checks dynamic dependencies and
RPATH, and exercises real analysis/report, build/correctness failures,
attempted input mutation, resource exhaustion and active SIGINT/SIGTERM.
AES-BLD independently checks install inventories/core symbols and distcheck.
The archive includes this guide, ADR, workflow proposal and installed contract test.

The proposal is `docs/engineering/ci/standalone.yml`, not an active CI gate.
The existing owner-gated signed publisher was refused permission when its
payload included `.github/workflows/standalone.yml`. An owner-authorized
workflow-capable publisher must activate that exact reviewed proposal and add
it to the required source-optimizer release-readiness catalog before #93 can
qualify. A separate read-only bootstrap run against an explicit verified
source commit can provide validation while activation remains pending; it does
not substitute for a normative PR/release gate.

Issue #93 remains blocked on evolve/evaluation, replay and complete retained
identity validation, checkpoint/resume, a full evidence loader, supported
platform lifecycle qualification, permanent CI activation and independent security/release review.
There is no third-party provider ABI or runtime plugin search. REA remains an
external reference; this work changes no REA intake or admission decision.
See [ADR-0047](../adr/ADR-0047-standalone-cli-development-boundary.md).
