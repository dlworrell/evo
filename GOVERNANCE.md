# Governance

Catalyst EVO is the source-to-source C optimization product in the Catalyst
ecosystem. It contains a reusable deterministic evolutionary-search core and
will add separately bounded source-analysis, transformation, candidate-
evaluation, orchestration, and artifact components on the path to 1.0.

## Authority

The repository follows this authority chain:

```text
Catylist -> AES -> AEMS -> repo_templates -> EVO
```

- Catylist defines ecosystem structure and repository relationships.
- AES defines engineering, security, and lifecycle obligations.
- AEMS evaluates repository state and preserves enforcement evidence.
- Project Zero performed the one-time preparation and Engineering Ready
  onboarding recorded by EVO-P0-002; it is not a standing development gate.
- `repo_templates` supplies the canonical lifecycle scaffold.
- EVO owns its core-library architecture and its source-analysis,
  transformation, candidate-evaluation, optimization, evidence, and artifact
  contracts within those constraints.

EVO does not supersede its upstream authorities. Target-project maintainers
remain authoritative for their source, tests, workloads, acceptance criteria,
and decision to apply an EVO patch. EVO owns only the isolated optimization
experiment and its evidence; it may not silently mutate or publish to the
target repository.

## Component Boundaries

- `catalyst_evo` owns deterministic bounded evolutionary-search mechanics and
  the stable public C ABI defined by EVO-001.
- The source optimizer owns the product contract defined by EVO-002, including
  project ingestion, analysis, structured transformation recipes, candidate
  isolation, validation, measurement, replay, and artifact emission.
- Raw textual mutation or crossover of C source is prohibited. Source genomes
  encode versioned structured transformation recipes.
- An optimized patch is a proposal. Applying, committing, pushing, merging,
  releasing, or deploying it requires an explicit downstream action outside
  EVO.

## Change Control

Changes shall:

1. be proposed through a focused branch and pull request;
2. identify the governing specification or ADR;
3. include reproducible validation evidence;
4. preserve the public C ABI and versioned product schemas unless an approved
   migration is provided;
5. preserve correctness and safety constraints as hard gates;
6. derive candidate workspaces from a recorded immutable baseline;
7. isolate and resource-bound every compiler, test, benchmark, and candidate
   process;
8. avoid direct mutation of downstream repositories or production systems;
9. describe optimization results as bounded measured findings rather than
   global-optimality or universal-correctness proofs;
10. preserve explicit reference semantics and deterministic human-readable
    audit projections for compressed, cached, indexed, probabilistic, or
    accelerated structures, with probabilistic structures limited to
    non-authoritative prechecks; and
11. address all unresolved review findings before merge.

## Project Zero Certification

EVO-P0-002 records the separately reviewed transition to
`ENGINEERING_READY`. Project Zero's role ends at that initial oversight
handoff. Ordinary development and release work do not rerun Project Zero;
they remain governed by Catylist, AES, AEMS, repository contracts, build
parity, tests, security review, analyzers, and sanitizers.

The manual Project Zero entry point remains available only if Catylist or AEMS
explicitly invalidates the retained certification or requests a new onboarding
assessment. Product-scope growth, including the source optimizer, is handled
through ordinary ADR, contract, threat-boundary, and AES/AEMS review rather
than implicitly resetting the repository to Project Zero.
