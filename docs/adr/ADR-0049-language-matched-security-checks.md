# ADR-0049: Language-Matched Security Checks on Every Change

Status: Proposed
Date: 2026-11-07 (America/Los_Angeles)
Owner: EVO
Builds on: [ADR-0046](ADR-0046-rea-assisted-recovery-and-source-admission.md) Amendment 1 (the `rea/` scope)
Acceptance: takes effect when the owner merges the pull request that adds it

## Context

On 2026-11-07, after REA was ingested under `rea/` (EVO #189), the owner directed:
"anytime a file changes, check the security of the appropriate programming
language."

Before this ADR:

- EVO's only security gate was AES-SEC-001 governance, which covers C and C++.
- `rea/` brought TypeScript and JavaScript (about 6,600 files), Python, Java,
  Swift, shell, and, through its vendored submodules, Rust and more.
  None of these had a security check.
- EVO's C build, test and analysis workflows ran on every change, including
  `rea/`-only changes they cannot affect. That breaks the owner's rule of
  2026-11-06 that one subgroup's gates never run on another subgroup's changes.

## Decision

1. **Each changed language gets its own check.** A `changes` job
   (`tools/ci_security/changed_languages.py`) reads the files a pull request or
   push changes and selects:
   - **CodeQL**, one analysis per changed language: C/C++, JavaScript/TypeScript,
     Python, Java/Kotlin, C#, Go, Rust, Ruby, and GitHub Actions workflows.
     It uses the `security-extended` query suite and build mode `none`
     (`autobuild` for Go). Results upload to code scanning.
   - **ShellCheck** at warning severity for changed shell scripts.
   - **Dependency review** on pull requests. It fails when a change adds a
     dependency with a known vulnerability of high severity or worse, in any
     ecosystem (npm, pip, Cargo, Maven, Go and others).
   - **C and C++** also keep AES-SEC-001 governance, which still runs on every
     change for the repository remainder and the `rea/` scope.
2. **What blocks.**
   - A change fails when CodeQL reports a result with security severity 7.0 or
     higher (high or critical) in a file the change touches
     (`tools/ci_security/sarif_gate.py`).
   - Results in other files, and lower severities, are reported in code
     scanning and the job summary but do not block. Existing code is not a
     reason to fail an unrelated change.
   - An analysis that reports failure always blocks.
   - A manual run analyses everything and reports without blocking.
3. **Languages with no check here are named, not hidden.** CodeQL's Swift
   analysis needs a macOS build. A change to Swift files produces a notice that
   no analysis ran for them.
4. **EVO's native checks skip `rea/`-only changes.**
   - Optional C workflows use `paths-ignore: rea/**`.
   - The required checks (`verify`, `formatting`, `static-analysis`) cannot use
     path filters: a required check that never reports blocks merging. A
     `changes` job therefore skips them, and a skipped job reports success.
   - Any change outside `rea/` still runs all of them.
   - A run whose change cannot be computed runs every check. That includes a
     manual run, a new branch and a missing base commit.
5. **Trust.** All of these run on hosted runners, never on
   `pull_request_target`. CodeQL jobs alone get `security-events: write` to
   upload results.

## Alternatives considered

- **GitHub's CodeQL default setup.** It analyses every language on every change.
  It cannot fail a pull request on new results in the files it changed, and the
  EVO repository cannot tune it.
- **Blocking on every result.** The existing `rea/` tree has not been
  triaged. Blocking on its existing results would stop unrelated work. The
  ratchet stays with AES-SEC-001 for C and C++.
- **A separate tool per language** (Bandit, ESLint security plugins, cargo-audit
  and so on). CodeQL covers these languages with one result format and one gate.
  Dependency review covers dependency vulnerabilities across ecosystems.

## Consequences

- A change runs only the analyses its languages need. A `rea/` TypeScript
  change runs CodeQL for JavaScript/TypeScript, REA's own checks and
  AES-SEC-001 governance. It does not run EVO's C build and tests.
- **Program scope.** AEMS's `docs/engineering/AES-CodeQL.md` says program-wide
  CodeQL adoption is a Catylist governance decision. This ADR adopts CodeQL in
  EVO only, on the owner's direction. It does not adopt the AEMS AES query packs
  or change rulesets, and the new checks are not required checks.
- Making `Security` a required check is a ruleset change for the owner. It
  should wait until the first runs show how many existing results the gate
  surfaces.
