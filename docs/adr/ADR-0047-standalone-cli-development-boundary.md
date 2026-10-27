# ADR-0047: Installed standalone CLI development boundary

Status: Proposed; draft delivery for issue #93

Date: 2026-10-27

## Context

ADR-0045 provides declarative command plans and ADR-0044 provides real built-in
providers. Neither delivers a directly runnable installed application. Issue
#93 requires a focused draft PR that preserves the stable reusable C core and
eventually implements all four product operations.

## Decision

Install one canonical `evo` binary through both native frontends. Statically
link its private product foundation and the reusable core into the program;
keep the existing separately installed core/header/pkg-config surface at
compatibility version 0.37.0. The development package remains 0.43.0 until the
entire 0.44.0 milestone qualifies. No private header, provider library, Python
launcher or source-relative asset becomes a runtime requirement.

The first executable slice implements `analyze` and a derived `report`.
`analyze` parses a bounded strict manifest, accepts a command plan only with
the versioned production analysis/execution providers, captures an immutable
baseline through Bubblewrap, checks baseline build/correctness eligibility,
and invokes the real Clang analysis adapter. Compiler identity is observed
with sandboxed `clang --version` before capture; this profile accepts Clang 18
and one declared target. LLVM identity records that same compiler distribution
line, not an independently probed LLVM executable.

`report` performs no target execution and requires no provider. It reads native
baseline/analysis JSON, checks required unique root fields, schema versions,
provider IDs, eligible state, diagnostic identity correspondence and inventory
counts, then derives a summary. This is explicitly an envelope/count reader;
complete nested semantic validation, exact byte authentication and replay
authority are not implemented by this slice.

`evolve` and `replay` retain the complete versioned command help/argument
contract but return configuration exit 3 as unavailable before file access,
provider probing or target execution. A help entry never implies an execution
path is complete. No candidate, champion, checkpoint or resume operation is
exposed. Issue #93 must remain open.

## Trust and recovery boundaries

The caller authorizes explicit input paths; relative paths resolve from the
invocation directory. Component walks reject symlinks, parent traversal,
control characters, overlong paths and special files. Output must not overlap
the source/evidence tree. Its existing parent must belong to the effective
user and have no group/other write permission. Existing output, including a
dangling symlink, is rejected. The supported host, system tool installation,
kernel and effective-user processes remain trusted; this is not a security
boundary against a compromised host or a hostile process with the same UID.

Linux execution clears inherited environment state, uses the fixed trusted
system tool path `/usr/local/bin:/usr/bin:/bin` and `LANG=C`, `LC_ALL=C`, and
passes only explicitly declared manifest environment entries into target
commands. Manifest `PATH` override is rejected. Runtime providers are built in;
the program never downloads a toolchain, discovers a user plugin, grants
network access, or mutates the authorized project. Existing Bubblewrap
filesystem/network/resource policy remains in force.

The live storage monitor tolerates only `ENOENT` for children removed during
a scan, as happens in CMake compiler probes. A missing root and all other I/O
errors still fail closed; actual retained bytes remain bounded by the same
declared storage limit. Installed tests require both a real CMake configure
and rejection of a retained 40 MiB file under the 32 MiB fixture budget.

Output is assembled in a private 0700 sibling stage. Exclusive 0600 writes
retain the exact parsed manifest and derived summary; native evidence retains
its stricter file modes. Linux `renameat2(RENAME_NOREPLACE)` commits the complete
directory atomically, preventing a concurrent creator from being overwritten.
Unsupported filesystems/platforms fail closed. This is atomic visibility,
not a claim of recovery from every power-loss/filesystem failure. A parent
fsync failure after commit reports internal failure and retains complete
output. Successful machine stdout is emitted only after commit.

SIGINT and SIGTERM handlers only store a `sig_atomic_t` request. The existing
synchronous sandbox supervisor observes that request, terminates/reaps its
owned process group and namespace descendants, and returns an interrupted
status. Adapters release retained results on failure. Cleanup failure maps to
internal failure rather than a successful interrupt. No interrupt checkpoint
is written. Before the atomic commit, interruption suppresses publication and
removes only the inode-identified owned stage; after the commit, complete
output remains. A cleanup error reports the retained stage path and exit 1.
SIGKILL and host crashes cannot run cooperative cleanup.

## Human-readable abstraction

No new accelerated index/cache or probabilistic authority is introduced.
Dispatch and provider lookup reuse the exact ordered linear registries from
ADR-0045/ADR-0044. Help displays command/provider identities and implementation
versions without probing availability. Exit definitions remain in
`project_command.h`. JSON parsing and field lookup are bounded exact scans.
The standalone summary and Markdown report are projections of native evidence;
FNV labels are diagnostics and never authenticate, admit or authorize source.
The bounded positive and adversarial installed tests compare these projections
against native output. ADR-0026's accelerated-representation equivalence gate
is not applicable because no accelerated representation is added.

## Qualification and consequences

The normative C fixture covers output ownership, atomic no-clobber publication,
special-file/path rejection and pre-cancel behavior. The installed black-box
contract runs outside the checkout with an installation-only PATH. The proposed
read-only hosted workflow requires real Clang/Bubblewrap execution for both
CMake/Clang and Autotools/GCC installations, including failures, resource
exhaustion and active SIGINT/SIGTERM cleanup. Missing runtimes fail that gate.
AES-BLD inventories, staged install parity and archive/distcheck include the
executable without relaxing core obligations. Workflow activation and addition
to exact-commit release readiness await an owner-authorized publisher able to
change workflow files. The proposal lives at `docs/engineering/ci/standalone.yml`;
it is not an active gate. Read-only bootstrap validation against an explicit
verified source commit is separate evidence. No performance improvement is claimed.

Remaining #93 blockers are full evolve/evaluation and replay execution,
checkpoint/resume loading and exact retained-identity reconciliation, complete
evidence validation, qualified supported-platform lifecycle behavior, permanent CI activation, and
independent security/release review. The executable does not alter REA's
external-reference classification or satisfy REA admission requirements.
