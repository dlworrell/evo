# ADR-0009: Bounded Child-Population Ownership

Status: Accepted
Date: 2026-07-31
Decision owner: EVO

## Context

EVO 0.9.0 provides independently tested selection, crossover, and mutation
operators, but crossover requires writable child views whose storage owner has
not been defined. Reusing the evaluated parent slab would destroy evidence and
violate the read-only parent contract. Allocating a second slab under
`max_population_bytes` would also silently broaden a policy that the public
contract explicitly limits to the current population.

Before operator orchestration can be specified, EVO needs a bounded second
population with clear failure, alias, and destruction behavior. That storage
must remain distinct from deciding how parents are paired, how RNG streams are
sequenced, or when child bytes become complete.

## Decision

EVO 0.10.0 appends `max_child_population_bytes` to `evo_config_t` and adds a
private child-population creation operation.

The operation:

- accepts a problem, configuration, completed parent population, and empty
  child object;
- rejects null inputs, parent/child object aliasing, and active child storage;
- validates the parent through the same completed-population invariant used by
  tournament selection;
- requires the parent genome size to match the supplied problem;
- checks `population_size * genome_size` before allocation;
- enforces `max_genome_bytes` and `max_child_population_bytes` independently;
- allocates one zero-initialized contiguous child genome slab with dimensions
  matching the parent; and
- leaves every child initialization and evaluation field empty.

The child owns its slab independently. Parent storage and evidence are never
modified. Either population may be destroyed first without invalidating the
other. An all-invalid but structurally complete parent is accepted because
storage ownership must not embed a parent-selection decision.

Completed-population validation moves from a selection-local implementation to
one shared private function. Selection behavior remains unchanged while child
allocation receives the same evidence checks.

The operation is not called by `evo_run` and does not mark child genomes as
complete.

## Consequences

- The peak allocation classes now have separate caller policies for current
  genomes, current evaluations, and child genomes.
- Existing `evo_config_t` member offsets remain stable, but its size and array
  stride increase; consumers must rebuild against 0.10.0.
- A child allocation cannot alias or overwrite parent genomes or evaluations.
- Allocation failure leaves the child empty and the parent unchanged.
- Child destruction is null-safe, repeatable, and independent of the parent.
- The child initially has writable bytes but no claim of initialization,
  validity, evaluation, best-candidate, or generation evidence.
- Parent pairing, operator stream sequencing, child completion, evaluation,
  swapping, and generation advancement remain separate milestones.

## Alternatives considered

### Reuse the parent population in place

Rejected because selection and reproducibility require completed parent genomes
and fitness evidence to remain stable while children are produced.

### Reuse `max_population_bytes` for both slabs

Rejected because it would silently double the authorized working storage and
contradict the existing allocation-class contract.

### Add a compiled-in total memory cap

Rejected because resource policy belongs to the caller and target environment.

### Allocate child evaluations at the same time

Rejected because children are not yet complete or validated. Evaluation is a
separate allocation class and lifecycle phase.

### Compose all genetic operators immediately

Rejected because parent pairing, odd population sizes, elitism, stream
ownership, failure atomicity, and child completion still need explicit
contracts.

### Reject an all-invalid completed parent

Rejected at the storage layer. Later selection or orchestration may return
`EVO_ERROR_NO_VALID_CANDIDATE`; storage ownership does not make that policy
decision.

## Verification

- `tests/child_population_test.c` covers invalid and incomplete inputs,
  completed-parent consistency, child budgets, zero initialization, distinct
  views, active-child preservation, all-invalid parents, and independent
  destruction.
- `tests/allocation_failure_test.c` injects child allocation failure and proves
  empty-child cleanup with the completed parent preserved.
- Existing selection tests continue to exercise the shared completed-
  population validator.
- The child-population test is normative in CMake, GNU Autotools, and the
  AES-BLD-001 repository profile.
- GitHub issue: `https://github.com/dlworrell/evo/issues/24`
