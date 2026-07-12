# ADR-0001: Reusable C17 Library with CMake

Status: Accepted
Date: 2026-07-12
Owner: EVO

## Context

EVO must provide reusable evolutionary optimization mechanisms to AEMS, Atarix, compiler tooling, FPGA design-space exploration, and future Catalyst projects. The implementation must remain independent of any one consumer, expose a stable native interface, support deterministic testing, and build across supported development platforms.

## Decision

EVO will be implemented as a reusable C17 library named `catalyst_evo`.

The public interface lives under `include/catalyst/evo/`. Implementation modules live under `src/`. Consumer-specific examples remain outside the library core.

CMake is the canonical build description. The initial build must support GCC and Clang, warning-clean compilation, CTest, static analysis, and sanitizer-enabled CI.

Problem-specific behavior is supplied through explicit callbacks and context objects rather than repository-specific logic inside the engine.

## Alternatives Considered

- Embed genetic-algorithm logic separately in each consumer: rejected because it duplicates policy and makes validation inconsistent.
- Implement in C++ first: deferred because the intended low-level consumers benefit from a narrow C ABI and straightforward language interoperability.
- Make AEMS the authoritative implementation: rejected because EVO is reusable engineering infrastructure rather than repository-management policy.
- Use Makefiles as the only build system: rejected because cross-platform configuration and analysis targets are easier to express and consume with CMake.

## Consequences

- The C ABI becomes a compatibility surface and must be versioned deliberately.
- Ownership, allocation, buffer lengths, and callback failure behavior must be explicit.
- Consumer-specific genomes and fitness functions remain outside the core library.
- CMake and C17 support become required development dependencies.
- Major changes to the language, ABI, or build-system boundary require a superseding ADR.

## Related Standards

- AES-DEV-001
- AES-SEC-001
- AES-003

## Related Documents

- `docs/architecture.md`
- `docs/algorithms.md`
- `include/catalyst/evo/evo.h`
- `CMakeLists.txt`
