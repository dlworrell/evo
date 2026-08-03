# ADR-0023: Deterministic Convergence and Stagnation Stopping

Status: Accepted
Date: 2026-08-03
Decision owner: EVO

## Context

The bounded core can stop at its generation limit, after a promoted
all-invalid child, or when an application callback requests it. It cannot
classify success against a declared fitness target or stop an unproductive
search from stable committed evidence. The diversity record introduced by
ADR-0022 supplies the final missing population signal.

Stopping must remain replayable and must not convert provisional work,
elapsed time, allocation identity, or hidden entropy into control flow. The
generation limit remains the hard upper bound on child transitions.

## Decision

EVO 0.23.0 defines stopping policy version 1 and appends seven controls to
`evo_config_t` after the complete 0.22.0 prefix:

- `fitness_target_enabled` and `fitness_target`;
- `stagnation_enabled`, `improvement_tolerance`, and
  `stagnation_patience`; and
- `diversity_floor_enabled` and `diversity_floor`.

Zero initialization disables every policy. A disabled policy requires its
numeric payload to remain zero, preventing an ignored value from being
mistaken for active policy. An enabled target must be finite. Enabled
patience requires a finite non-negative tolerance and positive generation
count. An enabled diversity floor must be finite and in `[0, 1]`. Malformed
controls return `EVO_ERROR_INVALID_ARGUMENT` before any run callback.

The fitness target is reached when the stable global-best `fitness.total` is
greater than or equal to the configured target. It may stop at generation
zero. The comparison uses the same authoritative caller total as fitness
comparison policy version 1.

Patience begins from generation zero's committed global best. After each
committed child, EVO compares the current global-best total with the total at
the last significant improvement. An improvement is significant only when:

```text
current_global_best > significant_best + improvement_tolerance
```

A significant improvement replaces the reference and resets the consecutive
stagnant-generation count to zero. Otherwise the count increases once for
that committed child. Equality at the tolerance boundary, an exact fitness
tie, and a strict improvement smaller than the tolerance do not reset the
count. Small improvements are cumulative because the reference remains the
last significant best. The run stagnates when the count is greater than or
equal to `stagnation_patience`. Generation zero establishes the reference but
does not consume patience.

The diversity floor is reached when the latest committed statistics record
has `diversity <= diversity_floor`. It is independent of patience and may stop
at generation zero. Zero or one hard-valid candidate has the existing
contractual diversity of zero, so an enabled zero floor intentionally matches
such a generation.

Every decision uses already committed result and statistics evidence. The
policy allocates no storage, invokes no new callback, consumes no RNG, and
uses generation counts rather than time.

## Termination Classification and Precedence

Version 0.23.0 appends two public reasons while preserving values 0 through 3:

- `EVO_TERMINATION_CONVERGED = 4` for a reached fitness target; and
- `EVO_TERMINATION_STAGNATED = 5` for reached patience or diversity floor.

When several conditions apply to the same committed generation, exactly this
precedence is used:

1. a promoted all-invalid child;
2. fitness-target convergence;
3. diversity-floor or patience stagnation;
4. the configured generation limit; and
5. an application stop request.

Generation-zero all-invalid remains an error before classification. Natural
reasons 1 through 4 suppress the application callback. The observer receives
the selected final reason. A target or stagnation condition reached on the
last permitted generation is therefore reported specifically, while the
generation limit still guarantees that no additional transition occurs.

## Private Evidence

Bounded-run policy advances to version 5. Its constant-space evidence records
stopping policy version 1, the current significant-best reference, the
consecutive stagnant-generation count, and distinct convergence and
stagnation flags. This adds no public history and no allocation class.

ADR-0024 later advances bounded-run policy to version 6 by appending elite and
singleton provenance; stopping policy version 1 and its classification order
remain unchanged.

## ABI Consequences

Existing termination values and all pre-0.23.0 member offsets are preserved.
The new controls extend `evo_config_t`, so its size and array stride may
change. Binary compatibility is not assumed; consumers must rebuild against
the 0.23.0 header. No public function signature or installed symbol changes.

## Alternatives Considered

### Stop after elapsed time

Rejected because scheduling and clock behavior are not replay-stable. The
generation limit is the resource safety bound and patience counts committed
generations.

### Compare only with the immediately preceding generation

Rejected because a sequence of individually small improvements could never
reset patience even after exceeding the declared tolerance cumulatively. The
last significant-best reference gives the tolerance a stable meaning.

### Require both low diversity and exhausted patience

Rejected because consumers may need either an objective plateau or a
representation-specific collapse signal. Independent enable controls keep
both policies explicit; either maps to the same public stagnation class.

### Let the application callback override natural evidence

Rejected because the existing callback contract dispatches only when the run
could otherwise continue. Preserving that order avoids calling consumer code
after the engine already has a deterministic terminal reason.

## Verification

- `tests/stopping_test.c` locks target, tolerance, and diversity equality
  boundaries; patience reset and cumulative improvement; exact ties;
  generation-zero classification; replay; malformed preflight; and disabled
  behavior.
- Coincident-condition vectors lock extinction, convergence, stagnation,
  generation-limit, and application-stop precedence.
- Existing bounded-run, application-stop, observer, lifecycle, allocation-
  failure, and downstream-consumer tests prove unchanged disabled behavior and
  final reason propagation.
- CMake, GNU Autotools, and AES-BLD-001 enumerate the same production source
  and normative test.
- GitHub issue: `https://github.com/dlworrell/evo/issues/45`
