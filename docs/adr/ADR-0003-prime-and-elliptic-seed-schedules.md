# ADR-0003: Prime and Elliptic Seed Schedules

Status: Accepted  
Date: 2026-07-30  
Decision owner: EVO

## Context

EVO RNG algorithm version 1 supplies a reproducible operation-local
PCG-XSH-RR stream from a recorded `uint64_t` seed. Future selection,
crossover, and mutation stages may need independently addressable streams
derived from a larger tuple such as:

```text
(master_seed, generation, population_index, operation_domain)
```

The `dlworrell/code-noodling` repository contains deterministic prime
generation and prime-to-SplitMix64 seeding experiments. EVO issue #10 asked
whether those prime inputs, including a finite-field elliptic mapping, should
become part of EVO's seed schedule.

Prime inputs and elliptic arithmetic are deterministic transformations. They
do not add entropy or make the non-cryptographic EVO stream suitable for
secrets.

## Decision

EVO does not adopt the prime-indexed or elliptic candidate.

RNG algorithm version 1, its fixed stream increment, seed procedure, and
output vectors remain unchanged.

The issue #10 implementation remains private research/test code. It is not
linked into `catalyst_evo`, has no public API, and does not define a production
seed-schedule version.

The experiment includes a plain tuple-mixed control so that domain separation
is not confused with a benefit from primality or elliptic arithmetic. Across
the defined 4,096-tuple corpus, the mixed control, prime-indexed schedule, and
elliptic schedule all produced zero schedule and four-word-prefix collisions,
approximately 64 changed schedule bits per master-seed bit flip, and similarly
low observed cross-domain correlation.

The prime-indexed candidate did not establish a separation advantage over the
plain control. It additionally requires a canonical prime artifact, a
population-index capacity rule, and continued provenance governance.

The elliptic candidate did not establish a separation advantage and was
approximately 589 to 636 times slower than the non-elliptic candidates in the
recorded GCC 13.3.0 capture. It also adds field inversion, point addition,
scalar multiplication, exceptional-point handling, parameter review, and
substantially more object code.

If independently addressable operator streams become necessary, EVO will open
a separate design issue using the plain tuple-mixed control as the initial
candidate. Adoption would require:

- an explicit new seed-schedule or RNG version;
- fixed vectors for every supported compiler and platform;
- evidence tied to the actual operator-consumption model;
- preserved version-1 replay evidence; and
- no cryptographic claim.

## Consequences

- Existing version-1 runs remain reproducible without migration.
- EVO gains evidence about domain-separated schedules without expanding the
  production binary or public API.
- Code Noodling prime-generation provenance and a reproducible 4,099-prime
  vector remain available for future experiments.
- No production population limit is created from the research vector's
  capacity.
- The elliptic expression is documented and testable, but its mathematical
  complexity is not treated as evidence of better randomness or security.
- A future substream design must be justified when operator semantics and
  stream-consumption boundaries exist.

## Alternatives considered

### Adopt the prime-indexed schedule

Rejected because the measured separation matched the simpler control and did
not justify prime-vector generation, indexing, digest, and capacity
governance.

### Adopt the elliptic schedule

Rejected because it matched the simpler candidates on the measured
diagnostics while adding an approximately 600-fold derivation cost and a much
larger implementation and review surface.

### Add domain tags directly to RNG algorithm version 1

Rejected because silently changing the seed procedure would invalidate the
version-1 reproducibility contract and its fixed vectors.

### Choose a production substream schedule now

Deferred because selection, crossover, and mutation do not yet have
implemented consumption semantics. Choosing before those boundaries exist
would optimize an assumed call model rather than an evidenced one.

## Evidence

- Research issue: `https://github.com/dlworrell/evo/issues/10`
- Baseline decision:
  `docs/adr/ADR-0002-deterministic-rng-and-population-initialization.md`
- Report:
  `docs/engineering/reports/EVO-RNG-001-seed-schedule-research.md`
- Raw results:
  `docs/engineering/reports/data/EVO-RNG-001-results-gcc13-linux.json`
- Fixed vectors and portability tests:
  `tests/seed_schedule_research_test.c`
