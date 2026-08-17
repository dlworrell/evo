# EVO-002A: Product Command and Provider Addendum

Status: Normative for EVO 0.43.0

This addendum reconciles EVO-002's historical private-provider language with the product command contract in EVO-003 and the production-provider architecture in ADR-0044.

## Product authority

EVO-002 versions 0.34.0 through 0.42.0 describe private foundation APIs that accept caller-supplied providers. Those seams remain valid for unit testing and internal embedding, but they are not the standalone product path.

For EVO 0.43.0 and later:

- EVO-003 defines `analyze`, `evolve`, `replay`, and `report` command semantics.
- Issue #114 supplies the governed production-provider registry.
- Issue #93 installs the executable and consumes the command plan with those production providers.
- Issue #69 proves that installed path end to end.

The governed sequence is `#67 -> #114 -> #93 -> #68 -> #69 -> #56`.

## Provider authority

The v1 provider policy is `catalyst.evo.provider-policy.v1`.

The initial production provider identities are:

- `catalyst.evo.provider.clang-analysis.v1`;
- `catalyst.evo.provider.clang-ast.v1`;
- `catalyst.evo.provider.linux-bwrap.v1`; and
- `catalyst.evo.provider.local-evaluation.v1`.

Commands select required providers by exact identity, implementation version, availability, and capability policy. A mismatch rejects the command before external project work is authorized. The standalone product does not substitute a private test provider or a weaker execution path.

## Checkpoint and replay

Provider identity, implementation version, and capability-policy identity are checkpoint and replay authority together with the baseline, analysis, catalogue, toolchain, target, workload, manifest, search, orchestration, and artifact identities required by EVO-002.

Resume or replay rejects incompatible provider authority before external candidate execution. Runtime process identifiers, temporary paths, provider handles, and scheduling order are diagnostic only.

EVO-003 additionally requires replay requests to declare their recorded identity set complete and all external inputs explicit. Issue #93 must independently verify retained identities before execution.

## Interpretation of historical callback language

Where EVO-002 says analysis, AST inspection, assurance, measurement, or orchestration consumes a caller-supplied provider, that statement describes the private 0.34.0-0.42.0 implementation boundary.

At the 0.43.0 product command boundary and above, this addendum and EVO-003 govern provider selection. Private callbacks remain internal seams and are not an alternative standalone product mode.

## Preserved invariants

The command boundary preserves the existing EVO-002 rules: the input repository is read-only; output publication is explicit; network access is not implicit; canonical machine evidence remains authority; and human-readable reports are derived projections.

The first production execution provider is Linux-specific. Hosts that cannot satisfy the declared provider contract report it unavailable rather than changing the meaning of successful evidence.

EVO-002 remains authoritative for source-optimizer semantics below the product command layer. EVO-002A changes only the provider interpretation at that layer; EVO-003 is the normative command specification.
