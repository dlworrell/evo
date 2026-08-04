# AES-DEV-001 Local Profile

Status: Active
Owner: EVO
Repository: `dlworrell/evo`
Documentation Authority: local

## Inheritance

This repository inherits `AES-DEV-001: Development Principles and Check-In Discipline` from:

```text
dlworrell/AES/standards/AES-DEV-001-development-principles-and-check-in-discipline.md
```

## Authoritative Paths

- `docs/architecture.md`
- `docs/specs/`
- `docs/theory.md`
- `docs/algorithms.md`
- `docs/benchmarks.md`
- `docs/engineering/`
- `include/`
- `src/`
- `tests/`
- `.github/workflows/`

## Local Development Rules

1. Architecture and interfaces are documented before stable implementation.
2. Public API or behavior changes update documentation in the same change series.
3. Public interfaces are versioned when compatibility matters.
4. Commits contain one logical change and use an area-prefixed imperative subject.
5. Code changes include tests or an explicit test rationale.
6. New subsystems define observability, failure modes, and recovery behavior where applicable.
7. Security-sensitive changes identify trust boundaries and authority crossings.
8. Major architectural decisions require an ADR.
9. Changes must be understandable, reviewable, testable, reversible, and suitable for `git bisect`.
10. Every compressed, cached, indexed, probabilistic, or accelerated structure
    retains exact reference semantics and a deterministic human-readable audit
    projection; no such structure may become opaque authority.

## Review Evidence

Every code-change issue and pull request must explicitly address:

- specification or design impact;
- documentation impact;
- test evidence or rationale;
- interface/versioning impact;
- failure and recovery impact;
- observability impact;
- accelerated-structure, exact-authority, and human-readable audit-projection
  impact, or an explicit not-applicable rationale;
- security and trust-boundary impact;
- ADR requirement.

## Deviations

Any weakening of inherited requirements requires an explicit waiver or ADR.
