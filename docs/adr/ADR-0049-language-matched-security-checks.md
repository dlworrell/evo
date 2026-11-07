# ADR-0049: Language-Matched Security Checks on Every Change

Status: Proposed
Date: 2026-11-07 (America/Los_Angeles)
Owner: EVO
Builds on: [ADR-0046](ADR-0046-rea-assisted-recovery-and-source-admission.md) Amendment 1 (the `rea/` scope)
Governed by: [Catylist ADR-0009](https://github.com/dlworrell/Catylist/blob/main/docs/adr/ADR-0009-codeql-assisted-security-review.md) (CodeQL-assisted security review; Accepted)
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
   - The CodeQL action is pinned to the v4.39.3 commit, and the CLI to bundle
     2.27.2. That is the release AEMS's AES query packs are qualified on
     (ADR-0009 decision 4).
   - **ShellCheck** at warning severity for changed shell scripts.
   - **Dependency review** on pull requests. It fails when a change adds a
     dependency with a known vulnerability of high severity or worse, in any
     ecosystem (npm, pip, Cargo, Maven, Go and others).
   - **C and C++** also keep AES-SEC-001 governance, which still runs on every
     change for the repository remainder and the `rea/` scope.
2. **CodeQL starts report-only, as Catylist ADR-0009 requires** (decisions 3
   and 7).
   - `tools/ci_security/sarif_gate.py` flags a result with security severity
     7.0 or higher (high or critical) in a file the change touches. It
     annotates the line as a warning and lists it in the job summary.
   - Results in other files, and lower severities, are listed in code scanning
     but not flagged.
   - A missing, partial or failed analysis fails the job. ADR-0009 calls this
     indeterminate: it is never a clean pass.
   - Blocking on flagged results (`--mode block`) is a promotion under ADR-0009.
     It needs the CQL-G01 to CQL-G10 evidence and the owner's recorded approval
     for EVO's profile. This ADR does not authorize it.
   - ShellCheck and dependency review are not CodeQL. They fail on their
     findings: ShellCheck at warning severity in changed scripts EVO owns
     (outside `rea/`, and `rea/ci/`), and dependency review when a pull request
     adds a high-severity vulnerable dependency. ShellCheck findings in upstream
     REA and vendored scripts are reported as warnings: those files change
     through ingestion and syncs, and a fix to them is an `REA-Local` change.
     *(Added 2026-11-07, after the first promotion carrying all of `rea/`.)*
   - Each CodeQL run keeps its SARIF as a workflow artifact for 30 days
     (ADR-0009 decision 5).
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
- **Blocking from the start.** The owner's direction reads naturally as a
  gate, but Catylist ADR-0009 requires report-only until promotion evidence
  exists. The existing `rea/` tree is also untriaged. The ratchet for C and C++
  stays with AES-SEC-001.
- **A separate tool per language** (Bandit, ESLint security plugins, cargo-audit
  and so on). CodeQL covers these languages with one result format and one gate.
  Dependency review covers dependency vulnerabilities across ecosystems.

## Consequences

- A change runs only the analyses its languages need. A `rea/` TypeScript
  change runs CodeQL for JavaScript/TypeScript, REA's own checks and
  AES-SEC-001 governance. It does not run EVO's C build and tests.
- **Program scope.** Catylist ADR-0009 adopted CodeQL as a security-review
  input program-wide. This ADR is EVO's first per-language profile under it,
  in report-only mode.
  - It does not yet run the AEMS AES-SEC-001/002 query packs. Wiring those for
    C/C++ through the AEMS evaluator is a follow-up.
  - It does not change rulesets, and none of the new checks is required.
  - Swift and the other surfaces ADR-0009 lists as needing their own
    extraction are named as not assessed.
- **Promotion is a later owner decision.** Promoting a language profile to
  blocking, or making `Security` a required check, needs the ADR-0009 evidence
  package. The first report-only runs are the start of that evidence.
