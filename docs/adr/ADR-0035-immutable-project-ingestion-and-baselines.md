# ADR-0035: Immutable Project Ingestion and Baselines

Status: Accepted

Date: 2026-08-09

## Context

EVO Core can search caller-defined genomes, but a source optimizer first needs
an exact, reviewable authority for the C project it is allowed to inspect. A
repository path alone is insufficient: build commands, compilation units,
dependencies, toolchains, environment, workloads, constraints, and resource
budgets all affect the meaning of a baseline. Project-controlled build code is
also untrusted and must never run against the authorized input tree.

Issue #58 requires CMake and Autotools fixtures, reproducible identities, an
applicable compilation database, immutable source and baseline bytes, isolated
derived work, and distinguishable ingestion/build/correctness/benchmark
outcomes. It does not authorize Clang analysis, candidate materialization,
source transformation, evolutionary search, or a standalone command-line
interface.

## Decision

EVO 0.34.0 introduces a private source-optimizer foundation target, separate
from the installed `catalyst_evo` ABI. It implements the following bounded
capture transaction.

1. Parse `catalyst.evo-project-manifest.v1` as strict UTF-8 JSON. Required
   fields, unknown or duplicate fields, path roots, commands, identities,
   policies, and every inner budget are validated against caller-supplied outer
   limits before project command execution.
2. Resolve one caller-authorized project root, reject path escape, overlapping
   permitted roots, symlinks, and non-regular/non-directory filesystem objects,
   then enumerate a complete file registry in UTF-8 bytewise relative-path
   order.
3. Atomically reserve a caller-selected output directory with an incomplete
   marker. Copy every captured byte into both a read-only snapshot and a
   writable derived workspace. The input project is never a command workspace.
4. Parse the retained compilation database before command execution. Each
   entry requires `directory`, `file`, and exactly one of an explicit
   `arguments` vector or opaque `command` string. Project-root absolute paths
   are normalized to relative paths, referenced source files must exist in the
   captured registry, ambiguous duplicate source entries reject, and records
   sort by stable domain fields. EVO does not interpret an opaque shell command.
5. Invoke a caller-supplied execution provider for the exact configure,
   compile, correctness, and conditionally required benchmark argument vectors.
   Every view carries timeout, memory, process, storage, output, and
   `network_access=false` policy. The provider, rather than this foundation,
   owns process isolation and command execution.
6. Re-enumerate the authorized input and compare every source byte and mode to
   the snapshot. Any addition, removal, replacement, mutation, symlink, or
   registry drift aborts the transaction.
7. Remove the derived workspace and generate bounded canonical JSON and
   Markdown from the same retained owner. After every consumer callback and
   source re-verification is complete, restore owner-write permission only on
   the snapshot root while the incomplete marker remains, move that directory
   across the staging boundary, and immediately reapply and synchronize its
   read-only mode. Then move the evidence to its final names, remove the
   incomplete marker, and make the completed output read-only. Snapshot files
   and descendant directories remain read-only throughout publication.

Capture errors return a specific ingestion status and publish no baseline.
Valid captures retain one of four baseline states: `eligible`, `build-failed`,
`correctness-failed`, or `benchmark-ineligible`. A failed gate suppresses every
later gate. Malformed execution-provider outcomes abort rather than becoming a
project result.

## Identity and Evidence

The exact authority is the read-only snapshot bytes plus the complete ordered
file, compilation-unit, manifest-policy, and command-trace registries. The
labels `fnv1a64-v1:<hex>` are deterministic, length-delimited diagnostics for
replay comparison; they are not authentication, provenance, collision-resistant
content addressing, or sole identity authority.

`baseline.json` conforms to `catalyst.evo-project-baseline.v1` and retains:

- the complete canonical manifest policy;
- an explicit empty generated-source registry under v1's `reject` policy;
- every file path, byte count, source mode, and diagnostic fingerprint;
- every normalized compilation unit and its exact argv or command form;
- every configured gate and its disposition, exit status, bounded output size,
  and diagnostic output fingerprint; and
- the Human-Readable Abstraction declaration and authority boundary.

`baseline.md` is generated from that same in-memory owner immediately before
commit. It projects the complete file registry, normalized compilation-unit
registry, policy lists, and gate trace. It cannot add, repair, suppress, or
authorize a machine-readable fact.

## Resource, Ownership, and Failure Rules

All input byte counts, token counts, nesting, strings, paths, roots, files,
file and total bytes, compilation-database bytes and units, command arguments
and bytes, command output, evidence, time, memory, processes, and storage are
bounded twice: by the manifest and by caller outer limits. Size arithmetic is
checked before allocation or filesystem mutation.

The private `project_runtime` unit is the single reviewed boundary for zeroed
allocation, release, and bounded formatting. It independently rejects zero or
overflowing allocation dimensions. Domain modules retain the exact owner and
byte-count invariants, check every returned allocation, and validate every
format result for error and truncation. This boundary reduces the security
review surface without hiding ownership or making the helper authoritative.

The returned baseline borrows views from one private owner. A successful caller
destroys it exactly once with a null-safe reset function. On failure, EVO
removes only the exact output directory it reserved, following no symlink, and
releases every partial allocation. It never removes or writes the authorized
input root.

## Human-Readable Abstraction Assessment

No compressed, cached, indexed, probabilistic, or otherwise accelerated
structure is introduced. Bounded arrays and deterministic scans are the exact
reference implementation. Sorting establishes stable presentation order; it
does not create a hidden authority. The complete JSON and Markdown registries
are direct projections of the same owner, and FNV labels remain explicitly
non-authoritative. EVO-HRA-007 retains the change-specific audit.

## Consequences

- Later Clang/LLVM work receives an explicit ordered translation-unit registry
  rather than an opaque compilation-database blob.
- Later candidate work must derive fresh workspaces from the committed snapshot
  and may not reuse the baseline capture workspace.
- The 0.34.0 package is not yet a standalone optimizer. Issue #67 defines
  orchestration/CLI behavior and issue #93 requires the installed executable,
  staged-package tests, command surface, signals, exit statuses, and path rules.
- Supporting generated compilation databases that do not exist at capture time
  requires a later versioned provider contract; v1 ingests a declared database
  already inside the authorized roots.
- Authentication of manifests, snapshots, and evidence crossing a trust
  boundary remains a deployment/product concern; FNV diagnostics cannot supply
  it.

## Rejected Alternatives

- Running builds in the input repository was rejected because project commands
  could silently modify the user's authority.
- Treating a hash, raw compilation-database byte stream, cache, or index as the
  only baseline authority was rejected because it would violate ADR-0026 and
  prevent complete human audit.
- Tokenizing a compilation database's opaque `command` string was rejected
  because shell semantics are provider- and platform-dependent. Explicit
  `arguments` remain an argv vector; opaque commands remain visibly opaque.
- Installing a provisional public C API or claiming a standalone executable was
  rejected until the later product-interface milestones fix those contracts.

## Verification

The normative C test and independent Python reference cover CMake and
Autotools fixtures, manifest key/registry reordering, stable golden manifest,
build, baseline, and file identities, exact replay evidence, absolute-path
normalization, argv and opaque-command forms, gate suppression, read-only
snapshots, workspace exclusion, malformed and duplicate JSON, missing and
overlapping roots, out-of-root paths, file and database budgets, symlinks,
ambiguous compilation entries, invalid provider output, and concurrent source
mutation. CMake/Clang and Autotools/GNU run the same target in the dedicated
project-ingestion workflow and the AES-BLD-001 parity matrix. The build
inventories also require the reviewed `project_runtime` boundary explicitly;
it is not an unlisted helper.

## Related Records

- ADR-0016
- ADR-0026
- EVO-002
- EVO-HRA-007
- Issues #38, #55, #57, #58, #67, and #93
