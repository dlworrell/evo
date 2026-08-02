# ADR-0017: Explicit Public Termination Reason

Status: Accepted
Date: 2026-08-02
Decision owner: EVO

## Context

EVO 0.16.0 returns `EVO_SUCCESS` both when every requested child transition
completes and when a promoted later child contains no valid candidate. A
consumer can distinguish those outcomes only by comparing
`result.generations_completed` with `config.generation_limit`.

That inference is exact for the two current success paths, but it is not a
stable public evidence model. Planned application stopping, convergence, and
stagnation would add successful early-stop conditions that cannot be encoded
unambiguously in the completed-transition count. Failure and destruction also
need to remain visibly distinct from every successful stop.

## Decision

EVO 0.17.0 defines `evo_termination_reason_t` with three initial values:

- `EVO_TERMINATION_NONE` is zero and represents an unstarted, failed, or
  destroyed result;
- `EVO_TERMINATION_GENERATION_LIMIT` represents successful completion of the
  configured transition bound, including a zero-limit generation-zero run;
- `EVO_TERMINATION_ALL_INVALID` represents successful termination after a
  later all-invalid child was evaluated and promoted.

`evo_result_t.termination_reason` is appended after every existing result
member. Existing member offsets are preserved, while the result size and array
stride change. Consumers must rebuild against the 0.17.0 header.

The field is published only after all fallible run work succeeds. Every public
failure path therefore retains the complete zero result, including
`EVO_TERMINATION_NONE`. Active-result rejection preserves the caller's entire
result, including its existing reason. `evo_result_destroy` clears the whole
structure and restores `EVO_TERMINATION_NONE`.

The private bounded-run operation continues to record its versioned internal
evidence without publishing public completion. Public `evo_run` maps that
evidence to one of the two successful reasons after private cleanup succeeds.
No RNG word, callback, allocation, selection, ownership transfer, winner
comparison, generation count, or stopping decision changes.

## Consequences

- Every successful `evo_run` has a nonzero explicit termination reason.
- `generations_completed` remains independent quantitative evidence rather
  than an encoded reason.
- Future reason values can represent application stop, convergence, and
  stagnation without changing current numeric values.
- Generation-zero all-invalid remains
  `EVO_ERROR_NO_VALID_CANDIDATE` with `EVO_TERMINATION_NONE`, because no public
  winner exists.
- A later all-invalid child remains successful, promoted, and counted exactly
  as in 0.16.0.
- Appending the field preserves prior member offsets but requires consumers to
  rebuild because `sizeof(evo_result_t)` changes.

## Alternatives considered

### Continue inferring the reason from the completion count

Rejected because the representation cannot distinguish multiple future early-
stop policies and would couple quantitative progress to qualitative outcome.

### Add one status code for every successful stop

Rejected because `evo_status_t` distinguishes success from failure. Returning
different success codes would complicate the established `status ==
EVO_SUCCESS` contract and still leave no durable result evidence.

### Store a private reason only

Rejected because consumers, replay artifacts, and later observers need the
outcome after internal run state has been destroyed.

### Publish the reason before internal cleanup

Rejected because a later internal failure could expose a successful reason on
an otherwise failed, empty result.

## Verification

- `tests/bounded_run_test.c` proves generation-limit completion for zero,
  single, multiple, odd, one-member, and tie runs and proves all-invalid
  termination after promotion.
- `tests/evo_lifecycle_test.c` locks the zero enumerator, appended result
  layout, generation-zero reason, active-result preservation, and destruction
  reset.
- `tests/allocation_failure_test.c` proves every injected failure leaves
  `EVO_TERMINATION_NONE` without changing allocation or release counts.
- The installed consumer verifies the new successful and destroyed states.
- GitHub issue: `https://github.com/dlworrell/evo/issues/39`
