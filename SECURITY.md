# Security Policy

## Supported Versions

Until the first stable release, security fixes apply to the `main` branch.
Release branches, tags, or downstream packages may receive a fix only when the
maintainer explicitly declares them supported.

## Product Security Scope

EVO has two security-relevant layers:

1. `catalyst_evo`, the reusable native C17 evolutionary-search core governed by
   EVO-001; and
2. the source-optimizer product layer governed by
   `docs/specs/EVO-002-source-optimizer-contract.md`.

The source-optimizer layer implemented through the 0.42 boundary handles
project ingestion, immutable baselines, normalized analysis evidence,
structured transformation recipes, isolated candidate workspaces, build and
correctness assurance, measurement, bounded search/orchestration, and product
checkpoint/resume identity. Some product interfaces remain private and
uninstalled while the installed application boundary is developed.

A private or not-yet-installed interface is still inside the repository threat
model. Moving an existing interface into an installed executable does not make
its inputs trusted.

This policy is read together with:

- `GOVERNANCE.md`;
- EVO-001 and EVO-002 under `docs/specs/`;
- the local AES-SEC-001 profile in `docs/engineering/SECURE-C-CXX.md`; and
- approved exceptions in `docs/engineering/AES-SEC-001-waivers.md`.

Where those documents constrain the same operation, the stricter security or
governance requirement applies.

## Reporting a Vulnerability

Report suspected vulnerabilities privately to the repository owner, preferably
through a private GitHub security advisory when that option is available. Do
not disclose a suspected vulnerability in a public issue before the owner has
had a reasonable opportunity to assess and remediate it.

Include, when applicable:

- a clear description of the issue and its impact;
- affected versions, files, symbols, provider identities, and schemas;
- reproduction steps or a minimal test case;
- the relevant input, memory, ABI, process, filesystem, provider, evidence, or
  checkpoint trust boundary;
- the requested and actually enforced resource/network/filesystem policy;
- whether a target-controlled process or descendant remained alive;
- whether the baseline, candidate workspace, evidence, or output tree changed;
  and
- a suggested mitigation, if known.

Security reports are specifically requested for suspected:

- sandbox or namespace escape;
- path, symlink, workspace, or output-destination escape;
- execution without required isolation;
- CPU, address-space, process-count, storage, output, or wall-time limit bypass;
- undeclared network access;
- provider protocol, capability, identity, or result-validation bypass;
- compiler/analyzer/test/benchmark output that can corrupt or confuse retained
  authority;
- baseline, checkpoint, recipe, evidence, or artifact tampering that is
  accepted as valid;
- candidate-process cancellation, descendant cleanup, or join failures that are
  reported as success; and
- unintended mutation, commit, push, publication, deployment, or other action
  against a downstream project.

## Trust Model

EVO must treat target-project-controlled data and provider-returned data as
untrusted unless a more restrictive contract explicitly proves otherwise.
Syntactic validity, a fingerprint match, a provider capability claim, or a
successful child-process exit status does not by itself make data trusted.

### Untrusted and externally controlled surfaces

The threat model includes at least:

- target source trees, generated files, build products, and repository metadata;
- optimization manifests, compilation databases, command vectors, environment
  declarations, schemas, and workload descriptions;
- repository roots, permitted source roots, candidate roots, output paths,
  relative paths, symlinks, hard links, and filesystem metadata;
- compiler, linker, analyzer, test, fuzz, sanitizer, and benchmark inputs and
  output;
- provider-returned records, diagnostics, capability attestations, process
  results, resource accounting, and cleanup evidence;
- analysis evidence, transformation proposals, recipe genomes, candidate
  identities, measurement records, and fitness inputs;
- serialized checkpoints, resume identities, persistent traces, and artifact
  evidence;
- candidate binaries, scripts, tests, benchmarks, helper programs, and every
  descendant they may create; and
- any future installed command-line, service, plugin, or provider interface
  that exposes the source optimizer to external input.

Target-project maintainers remain authoritative for their source, tests,
workloads, acceptance criteria, and decision to apply a patch. EVO is not
trusted to silently alter that authority.

## Native C17 Security Requirements

All EVO native code follows AES-SEC-001 and the local secure-C/C++ profile.
Changes must:

- validate externally controlled lengths, indices, counts, enums, serialized
  values, and cross-references before use;
- check allocation, multiplication, addition, offset, and copy-size arithmetic
  for overflow and truncation;
- avoid banned unsafe interfaces and custom cryptography;
- carry explicit bounds across external buffer and parser boundaries;
- fail closed on malformed, unknown, stale, duplicate, conflicting, or
  over-budget input when the governing schema requires rejection;
- preserve correctness and safety constraints as hard optimization boundaries;
- keep GitHub Actions permissions at least privilege;
- run warning-clean, static-analysis, sanitizer, parser/fuzz, and other
  applicable security checks; and
- record approved exceptions in `docs/engineering/AES-SEC-001-waivers.md`.

Diagnostic FNV fingerprints used by EVO are deterministic replay/integrity
labels, not cryptographic authentication. They must not become the sole
security authority for an untrusted object.

## Source-Optimizer Security Invariants

### Project ingestion and immutable baselines

Before project-controlled execution or candidate work, EVO must validate the
authorized project boundary and capture or validate the immutable baseline
required by EVO-002.

Path handling must be fail closed. An operation must reject any path, link, or
filesystem object that escapes the declared root or violates the governing
input policy. Path normalization must not turn an unauthorized absolute path,
parent traversal, symlink target, mount transition, or output destination into
an authorized object.

Analysis, transformation, candidate, measurement, checkpoint, and artifact
operations must not mutate the input project or the committed immutable
baseline. Baseline integrity must be revalidated at the boundaries required by
the governing contract. Unexpected drift is a security-relevant failure, not a
new implicit baseline.

### Provider boundary

Analysis, assurance, measurement, and candidate execution consume provider
results. Provider records and capability attestations are inputs to validate,
not authority to accept blindly.

EVO is responsible for validating the provider-facing protocol and the product
invariants it owns. Depending on the interface, that includes bounded lengths
and counts, known enum/schema values, exact identities, cross-references,
source/candidate identity, policy identity, result ordering, deep-copy/ownership
rules, evidence limits, checkpoint compatibility, and baseline integrity.
Malformed, contradictory, stale, incomplete, over-limit, or identity-mismatched
provider output must fail closed.

A provider may not widen the manifest or caller policy by claiming a broader
capability. A capability that is absent, unverifiable, or unavailable must be
reported as unavailable/failure according to the governing contract; EVO must
not silently fall back to a less isolated execution path.

Through the current 0.42 product boundary, portable operating-system sandboxing
and execution of target-controlled workloads remain provider responsibilities.
That responsibility includes actually enforcing the process policy requested by
EVO. EVO remains responsible for rejecting provider outcomes that do not meet
the declared protocol, identity, evidence, and policy requirements. A future
EVO-supplied production provider inherits the same obligations; moving the
implementation into this repository does not weaken them.

### Candidate-process controls

Every compiler, linker, analyzer, test, benchmark, candidate program, and other
target-controlled process must run under the isolation and resource policy
required by EVO-002 and `GOVERNANCE.md`.

The effective policy must account for, as applicable:

- an isolated candidate workspace derived from the recorded baseline;
- filesystem visibility and write destinations;
- current working directory and path traversal;
- environment variables, inherited descriptors, credentials, and secrets;
- CPU time and scheduling budget;
- address-space and memory budget;
- process/thread/descendant count;
- storage and temporary-space budget;
- captured stdout/stderr and total output budget;
- wall-clock timeout;
- network policy; and
- cancellation, termination, descendant cleanup, and final join/reap behavior.

The current v1 optimization manifest disables network access. EVO must not
silently enable network access because a compiler, test, benchmark, provider,
or candidate requests it. Any later schema that permits network access must
make that permission explicit, bounded, reviewable, and part of the recorded
run identity, and requires a corresponding security-policy review.

If required isolation or a required limit cannot be established or verified,
the process must not run as though the requirement were optional. Unsupported
sandboxing is a capability-unavailable or execution failure, not permission for
an unsandboxed retry.

A timeout, provider error, orchestration failure, checkpoint, or cancellation
must not leave started target-controlled descendants running. Cleanup/join
failure must remain visible in authoritative evidence and must prevent the run
from being represented as successfully isolated and complete.

### No silent downstream mutation or publication

EVO owns the isolated optimization experiment and its evidence. It does not own
the downstream repository or production environment.

EVO must not silently apply, commit, push, merge, publish, release, deploy, or
otherwise mutate a target repository or production system. Candidate
workspaces and generated artifacts must remain within declared output
boundaries. An optimized patch is a proposal; downstream application requires
an explicit action outside the optimization run unless a future separately
specified interface establishes a new authority boundary.

Any future interface that adds write-back or publication authority requires a
security-policy and governance update before implementation is considered
complete.

### Evidence, checkpoints, and replay

Evidence and checkpoints can influence replay, ranking, resumption, and final
publication decisions and therefore are security-relevant input.

EVO must validate their versioned schema, bounds, canonical identities,
provider/policy identity, baseline identity, and required cross-references
before use. Corrupt, stale, truncated, incompatible, identity-mismatched, or
otherwise unreconcilable state must fail closed or be rebuilt from the exact
authority allowed by the governing specification.

Caches, summaries, fingerprints, indexes, compressed forms, probabilistic
prechecks, Markdown projections, or provider assertions may not independently
become acceptance authority when EVO-002 requires canonical exact records.
Human-readable evidence must derive from and reconcile with those records.

Resume must not silently substitute a different baseline, provider, capability
policy, transformation catalogue, workload, resource policy, or other identity
that the checkpoint contract declares authoritative.

### Output and artifact handling

Evidence, checkpoint, patch, source-tree, and artifact output must be bounded to
the declared destination and written using the atomicity/hardening requirements
of the governing contract. Failure during staging, synchronization, rename,
hardening, or cleanup must not expose a partial output as a successful
completed artifact.

Output paths and filenames derived from external data require the same escape,
normalization, and link protections as input paths. Generated output must not
be used as an implicit command, path, or repository authority.

## Fail-Closed Security Events

The following conditions must not be converted into a successful candidate,
measurement, checkpoint, or publication result merely to continue an
optimization run:

- baseline or immutable-input drift;
- sandbox setup failure or detected sandbox escape;
- path/workspace/output escape;
- undeclared network access or inability to enforce required network policy;
- required resource-limit failure or bypass;
- provider schema/protocol/capability/identity mismatch;
- malformed or over-budget provider evidence;
- checkpoint or replay identity mismatch;
- target-controlled process timeout when success requires completion;
- failure to cancel, terminate, reap, or join required descendants; or
- unauthorized downstream mutation or publication.

A lower-level failure may be represented as an explicit rejected/unavailable
candidate when the governing algorithm permits that state. It may not be
misrepresented as a successful gate or omitted from authoritative evidence.

## Security Review Trigger for Product-Surface Growth

Issues #67 and #93, and any successor work that installs or exposes the source
optimizer, must not broaden the execution or trust surface without security
review.

A pull request requires an accompanying update to this policy, EVO-002, or a
linked reviewed threat-model document when it introduces or materially changes
any of the following:

- externally reachable commands, services, plugins, or provider interfaces;
- accepted source/project/schema/checkpoint/evidence formats;
- provider capabilities or trust assumptions;
- sandbox technology or isolation guarantees;
- filesystem visibility, writable paths, or output destinations;
- target-controlled command execution;
- network access;
- credential, environment, or descriptor inheritance;
- process/resource budgets or cleanup semantics;
- checkpoint/resume authority;
- downstream write-back, publication, or deployment authority; or
- cryptographic/authentication claims.

If a change does not alter a security boundary, the pull request should say so
explicitly. Installed-product delivery is not allowed to convert a previously
explicit provider responsibility, denied capability, or fail-closed condition
into an implicit trusted default.
