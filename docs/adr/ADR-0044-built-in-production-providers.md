# ADR-0044: Built-in Production Providers for Standalone EVO

Status: Accepted

Date: 2026-08-16

## Context

EVO 0.42.0 has deterministic source-optimizer contracts for project analysis,
AST-aware transformation, baseline and candidate execution, performance
measurement, structured search, and bounded external-process orchestration.
Those contracts intentionally accept caller-supplied callbacks. That was the
correct boundary while the source optimizer remained an uninstalled foundation,
but it is not a sufficient product architecture for the standalone executable
owned by issue #93. An installed EVO binary cannot require a consumer to write a
custom embedding application merely to provide Clang analysis, execute declared
build/test commands, benchmark a candidate, or satisfy the asynchronous
start/poll/cancel/join contract.

The existing callbacks are still useful internal seams and test oracles. They
must not, however, become the only implementation of product-critical work.
EVO also must not freeze a public third-party provider ABI before the product
command and configuration contracts in issue #67 stabilize.

Issue #114 therefore needs a concrete provider delivery model that preserves the
0.42 contracts, supplies a supported production path, binds provider identity
into replay/checkpoint authority, and fails closed on platforms where required
isolation cannot be provided.

## Decision

EVO adopts **built-in, versioned production providers** for the first standalone
product release. The existing callback contracts remain private implementation
seams; they are not promoted to a public plug-in ABI by this decision.

1. The source-optimizer foundation owns a deterministic provider registry. Each
   provider record has a stable identity, implementation version, provider kind,
   supported platform set, exact capability flags, external runtime/tool
   requirements, and an explicit availability probe. Registry order is stable
   and is the human-readable reference representation for provider discovery.
2. The initial analysis provider is
   `catalyst.evo.provider.clang-analysis.v1`. It consumes the captured
   compilation-unit registry, invokes a declared Clang executable without shell
   interpretation, reads only the immutable snapshot, emits normalized stable
   source/declaration/compiler evidence, and never treats Clang pointer values,
   process IDs, temporary paths, timestamps, or output order as identity.
3. The initial AST inspection provider is
   `catalyst.evo.provider.clang-ast.v1`. It is the production implementation of
   the transformation AST callback required by the 0.37 transformation
   catalogue. It resolves one recorded target against the immutable snapshot and
   returns only the bounded structural facts required by the selected catalogue
   operation. This closes the otherwise hidden callback dependency between
   analysis and candidate materialization.
4. The initial command/execution provider is
   `catalyst.evo.provider.linux-bwrap.v1`. It is supported only on Linux hosts
   where its capability probe succeeds. It executes argv vectors directly,
   never through a shell, and uses a Bubblewrap namespace plus EVO-owned process
   supervision. The candidate or baseline workspace is the only writable
   project tree. Network access is disabled when policy requires it. CPU,
   address-space, descendant-process, storage, captured-output, and wall-time
   budgets are supervised under explicit limits. Every started process group is
   terminated and joined before cleanup can be reported complete.
5. Capability evidence is factual rather than aspirational. A provider sets an
   enforcement flag only when that exact mechanism was active for the run. If a
   requested or orchestration-required capability cannot be enforced, the work
   is rejected before a candidate can receive performance fitness. Platform
   availability therefore changes admission, never the meaning of a successful
   result.
6. The baseline command runner and candidate-assurance runner are adapters over
   the same execution provider. This prevents baseline commands from receiving a
   weaker sandbox than candidate commands. The assurance adapter additionally
   preserves the existing filesystem, network, process-group, source/snapshot,
   diagnostic, and toolchain evidence fields.
7. The measurement provider is an adapter over the same execution provider. A
   versioned workload registry maps each declared workload identity to exact
   baseline and candidate argv vectors and working directories. Warmup and
   recorded calls use the ordering supplied by the 0.40 measurement contract;
   the provider measures monotonic elapsed time and records condition identity
   without altering correctness authority.
8. The production orchestration provider is
   `catalyst.evo.provider.local-evaluation.v1`. It owns asynchronous job state
   and supplies the 0.42 start/poll/cancel/join interface. A job runs the
   EVO-owned synchronous evaluation pipeline for one live recipe: AST
   inspection, candidate materialization, required assurance, measurement, and
   exact search-evaluation outcome construction. Cancellation stops admission
   and terminates any active sandbox process before join returns.
9. Provider handles, threads, process identifiers, and temporary runtime paths
   are not replay authority. The stable provider registry identity, provider
   implementation version, sandbox policy identity, analysis/toolchain identity,
   workload identity, and existing product identities are replay authority and
   are bound into product checkpoint validation before execution resumes.
10. Issue #67 owns the versioned analyze/evolve/replay/report command contracts
    and selects only provider identities from this production registry. Issue
    #114 owns the provider implementations and registry. Issue #93 owns
    installation and proves that the installed executable can resolve and use
    the supported providers from an unrelated working directory. The dependency
    order is therefore `#67 -> #114 -> #93`; #114 does not install the final CLI.
11. A public third-party provider ABI or remote-provider protocol is deferred.
    If introduced later it must have its own compatibility, trust, capability,
    cancellation, evidence, and replay contract. The private callbacks in the
    0.34-0.42 implementation are not that ABI.
12. Linux is the first supported production execution platform. Other hosts may
    compile the provider registry and report the Linux sandbox as unavailable;
    they must not silently fall back to unsandboxed execution. A later platform
    provider may be added under a new stable identity without changing evidence
    produced by this provider.
13. Real-provider integration fixtures are separate from fake callback unit
    tests. Hosted Linux validation installs/probes the declared Clang and
    Bubblewrap dependencies, analyzes a small captured C project, executes
    bounded build/test/benchmark commands under the production sandbox, verifies
    cancellation/timeout/output/resource failure, and checks exact provider
    identity/capability evidence.
14. The #69 end-to-end proof and #93 staged-install proof may not substitute the
    historical fake providers or private test-only callbacks for these product
    providers. Fake providers remain useful only for deterministic fault
    injection and unit-level contract verification.

## Linux v1 Isolation Contract

The Linux v1 provider is fail-closed. A supported execution requires all of the
following mechanisms to be available for the requested policy:

- Bubblewrap user/mount/PID namespace isolation;
- read-only declared system/toolchain roots and a single explicitly writable
  project workspace;
- a private `/tmp`, `/proc`, and minimal `/dev` view;
- network namespace isolation when network is denied;
- direct `exec` of the declared argv vector with no shell expansion;
- `RLIMIT_CPU` and `RLIMIT_AS` for CPU/address-space limits;
- EVO supervisor checks for descendant-process and writable-workspace storage
  budgets;
- bounded stdout/stderr capture with termination on overflow;
- monotonic wall-time supervision;
- one process group per sandbox job, mandatory cancellation, `waitpid` join, and
  no successful cleanup evidence while a descendant remains live.

The process/storage supervisor is enforcement, not merely telemetry: crossing a
bound terminates the sandbox and returns the corresponding stable terminal or
gate disposition. The provider does not claim Linux cgroups, seccomp policy,
container-image isolation, or confidentiality from read-only system roots unless
those mechanisms are separately introduced and recorded.

## Provider Identity and Replay

Provider identity is a compatibility surface, not a display string. A behavior
change that can alter normalized analysis, transformation facts, sandbox policy,
measurement semantics, capability claims, or terminal classification requires a
new implementation version and, when replay compatibility is not exact, a new
provider identity.

Checkpoint and replay validation compare the complete provider identity set
before candidate execution. A missing, unavailable, or version-mismatched
provider is an identity/capability failure; EVO never substitutes the closest
installed provider.

## Human-Readable Abstraction Assessment

The provider registry is a small, fixed, ordered array and direct scan. It is
its own exact reference representation; no hash table, cache, probabilistic
filter, compressed registry, or generated dispatch index is authority. The audit
projection enumerates every provider record in registry order with identity,
version, kind, platform, capabilities, requirements, and availability status.

Runtime process tables and asynchronous handles are execution mechanisms only.
They do not suppress registry members or authorize replay. This change therefore
introduces no accelerated authority. ADR-0026 differential requirements for a
new accelerator are not applicable; the issue-specific audit must still prove
that the registry projection is complete and ordered.

## Consequences

- Standalone EVO gains a supported provider path without freezing a premature
  public plug-in ABI.
- Linux production execution fails closed instead of silently running target
  code on the host when Bubblewrap or a required control is unavailable.
- Analysis, baseline, assurance, measurement, and orchestration use shared
  resource/isolation semantics instead of independent ad-hoc process runners.
- macOS and other platforms remain explicit availability gaps until a provider
  with equivalent declared guarantees is implemented.
- #93 becomes dependent on #114 in addition to #67; #69 consumes the same
  providers rather than test callbacks.

## Rejected Alternatives

- **Keep callbacks as the product boundary** was rejected because the installed
  executable would still require an embedding application and #93 could not be
  operational on its own.
- **Freeze a public C plug-in ABI now** was rejected because #67 has not yet
  fixed the complete product command/configuration surface or long-term provider
  compatibility policy.
- **Run commands directly on the host and document the risk** was rejected
  because candidate code and build systems are an untrusted execution boundary.
- **Treat Docker/OCI as the only v1 provider** was rejected because image
  identity, daemon/rootless configuration, storage drivers, and distribution
  would become additional product authorities. OCI may be added later under a
  distinct provider identity.
- **Claim unsupported limits as enforced** was rejected because capability
  attestation is part of deterministic evidence and checkpoint safety.

## Verification

Normative provider tests must cover stable registry order and uniqueness,
unsupported-platform reporting, direct argv execution, shell-metacharacter
literal handling, read-only host/project boundaries, denied network, CPU/memory/
process/storage/output/time limits, timeout and cancellation, process-group
cleanup, deterministic terminal classification, and capability truthfulness.

Real Clang integration must use a captured compilation database and prove stable
normalized identities across repeated runs while ignoring runtime-specific
Clang AST addresses and process output ordering. It must reject shell-form
compile database records, response-file/plugin injection, out-of-snapshot source
paths, and over-limit analysis output.

Hosted validation must retain the existing fake-provider unit tests and add a
separate Linux real-provider job using supported Clang/Bubblewrap packages. Both
CMake/Clang and Autotools/GNU builds must compile the production provider code.

## Related Records

- ADR-0016
- ADR-0026
- ADR-0035
- ADR-0036
- ADR-0038
- ADR-0040
- ADR-0041
- ADR-0042
- ADR-0043
- EVO-002
- Issues #38, #67, #69, #83, #93, and #114
