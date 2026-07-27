# Governance

Catalyst EVO is the reusable evolutionary-optimization library in the Catalyst
ecosystem.

## Authority

The repository follows this authority chain:

```text
Catylist -> AES -> AEMS -> Project Zero -> repo_templates -> EVO
```

- Catylist defines ecosystem structure and repository relationships.
- AES defines engineering, security, and lifecycle obligations.
- AEMS evaluates repository state and preserves enforcement evidence.
- Project Zero governs repository preparation and Engineering Ready review.
- `repo_templates` supplies the canonical lifecycle scaffold.
- EVO owns its library architecture, specifications, implementation, tests,
  benchmarks, and experiment interfaces within those constraints.

EVO does not supersede its upstream authorities. Downstream consumers own their
problem-specific genomes, fitness functions, and acceptance decisions.

## Change Control

Changes shall:

1. be proposed through a focused branch and pull request;
2. identify the governing specification or ADR;
3. include reproducible validation evidence;
4. preserve the public C ABI unless a versioned migration is approved;
5. preserve correctness and safety constraints as hard gates;
6. avoid direct mutation of downstream repositories or production systems; and
7. address all unresolved review findings before merge.

## Project Zero Certification

The repository may propose a certification candidate, but it may not approve
its own transition to `ENGINEERING_READY`. Approval requires a separate,
traceable review of the candidate commit and its retained evidence.
