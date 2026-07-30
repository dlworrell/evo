# EVO-RNG-001 Seed-Schedule Research

Date: 2026-07-30  
Issue: `https://github.com/dlworrell/evo/issues/10`  
Baseline: EVO `main` commit `e24071d553aee73f972a6d19e9240511b80b510b`

## Outcome

Do not add prime-indexed or elliptic seed derivation to the production library.
Both candidates separate the tested tuple domains, but neither improves the
measured result over a simpler tuple-mixed control. The elliptic candidate is
approximately 589 to 636 times slower than the non-elliptic candidates in this
capture and adds substantially more arithmetic and review surface.

EVO RNG algorithm version 1 and all version-1 vectors remain unchanged. If EVO
later needs independently addressable streams for selection, crossover, or
mutation, the plain tuple-mixed control is the preferred starting point. That
would require a separately specified seed-schedule version, new fixed vectors,
and a new implementation issue.

This is a deterministic engineering experiment, not a cryptographic design or
a certification of the underlying PCG stream.

## Question

The experiment tested whether prime-generation and prime-seeding ideas from
`dlworrell/code-noodling` could improve deterministic stream separation, and
whether placing those inputs into a finite-field elliptic mapping produced an
additional measurable benefit.

The Code Noodling provenance is commit
`43c4b386acfcc634f1d62e96a5b7809e96d8a1ec`:

- `OSE.c`: segmented 6-wheel prime generation;
- `OSE_CUDA.cc`: odd-only CUDA segmented prime generation; and
- `dice_cpu.cc`: deterministic prime-to-SplitMix64 seed derivation.

Primes are public deterministic constants. They do not add entropy,
unpredictability, or cryptographic strength.

## Candidates

### Version-1 baseline

The baseline calls the existing private version-1 seed procedure with only the
master seed. It intentionally ignores generation, population index, and
operation domain because version 1 is one operation-local stream, not a
random-access substream schedule.

### Plain tuple-mixed control

The control mixes the complete tuple:

```text
(master_seed, generation, population_index, operation_domain)
```

It then derives a 64-bit PCG state and an odd 64-bit stream increment. This
control is necessary: without it, any improvement over the version-1 baseline
could incorrectly be attributed to primality or elliptic arithmetic when the
actual cause was simply adding domain separation.

### Prime-indexed schedule

The prime candidate generates the first 4,099 primes in ascending order.
Candidates after 3 are visited as `6k - 1` and `6k + 1`, and divisibility is
tested only through the square-root boundary using already generated primes.
The vector begins at 2 and ends at 38,917.

Its canonical encoding is each prime as a four-byte unsigned integer in
least-significant-byte-first order:

```text
sha256:a6ad2811fbf74c2879900a93fecd6ae85b4915e0d9fb2192f4241ac0a2b91869
fnv1a64:0df978217b36289c
```

Population index selects three adjacent primes. Master seed, generation, and
operation domain are mixed with the first; the next two contribute to state
and stream derivation. The table is generated once by the experiment. Prime
generation time is not included in per-derivation throughput.

### Elliptic schedule

The elliptic candidate uses:

```text
P = 2147483647
y^2 = x^3 + x + 1 (mod P)
G = (0, 1)
```

The nonzero curve discriminant is tested. The first 4,096 positive scalar
multiples of `G` are tested as finite, deterministic curve members, and fixed
points are locked for scalars 1, 2, 3, 17, and 65,537.

Tuple and prime material is reduced to a scalar in `[1, P - 1]`. Scalar
multiplication uses double-and-add. Field multiplication is safe in standard
`uint64_t` because both operands are less than the 31-bit field prime; no
nonstandard 128-bit integer is required. A finite point is encoded as
`x << 32 | y`. The point-at-infinity case has a fixed sentinel mapping.

The curve and base point were selected only for a portable nonlinear research
mapping. They were not selected or reviewed for cryptography.

## Corpus and measurements

The deterministic corpus contains 4,096 tuples:

- master seeds `0`, `1`, `42`, and `UINT64_MAX`;
- generations 0 through 7;
- population indices 0 through 31; and
- initialization, selection, crossover, and mutation domains.

For each candidate, the experiment measures:

- exact `(state, increment)` collisions;
- collisions among the first four PCG output words;
- one-bit fractions for state and odd increment;
- a 17-bucket distribution of 128-bit schedule Hamming distance after each
  master-seed bit is flipped;
- Hamming distance and first-word Pearson correlation for all six operation
  domain pairs; and
- seed-derivation throughput.

The Hamming histogram buckets are `0-7`, `8-15`, through `120-127`, followed by
an exact `128` bucket.

## Results

| Candidate | Schedule collisions | Four-word collisions | State one-bit fraction | Increment one-bit fraction | Seed-flip mean / 128 | Domain Hamming range / 128 | Maximum absolute domain correlation | Derivations / CPU second |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| version-1 baseline | 4,092 | 4,092 | 0.484375 | 0.546875 | 17.308594 | 0.000-0.000 | 1.000000 | 89,764,295 |
| plain mixed control | 0 | 0 | 0.498802 | 0.507488 | 63.501465 | 63.321-63.544 | 0.067606 | 89,425,069 |
| prime-indexed | 0 | 0 | 0.500221 | 0.508530 | 63.630981 | 63.536-63.771 | 0.056579 | 96,472,344 |
| elliptic | 0 | 0 | 0.500240 | 0.509090 | 63.603394 | 63.304-63.765 | 0.067277 | 151,753 |

The odd stream increment must always have its low bit set, so its expected
one-bit fraction is slightly above 0.5.

The baseline's 4,092 collisions are expected: four master seeds produce four
schedules, each repeated over the other 1,024 tuple combinations. This does
not show a collision defect in PCG. It shows that version 1 does not claim
random-access domain separation.

The mixed, prime-indexed, and elliptic candidates all produced:

- zero schedule collisions;
- zero four-word prefix collisions;
- approximately half one-bits;
- approximately 64 changed bits out of 128 after a master-seed bit flip; and
- low observed cross-domain prefix correlation in this corpus.

The small differences among those three deterministic diagnostics do not
establish a prime or elliptic advantage. The prime-indexed throughput was about
1.08 times the mixed-control throughput in this capture, but both completed in
the same high-throughput tier and the ordering is not an adoption criterion.
The prime candidate also requires a bounded vector, its provenance, and an
index-capacity rule.

The elliptic candidate achieved no separation improvement while running about
589 times slower than the mixed control and 636 times slower than the
prime-indexed candidate.

## Size and complexity

With GCC 13.3.0, C17, `-O2`, and function sections, the complete experimental
schedule object contained 2,945 bytes of text and no data or BSS:

- prime-vector generator symbol: 169 bytes;
- tuple-material helper symbol: 214 bytes;
- curve-add helper symbol: 800 bytes;
- curve-multiply symbol: 217 bytes; and
- combined candidate-dispatch and derivation symbol: 1,102 bytes.

The curve helpers alone account for at least 1,017 bytes before attributing
their share of the combined derivation function. The research implementation
is 393 C lines plus a 77-line private header. Test and measurement code adds
1,149 lines. None of this code is linked into `catalyst_evo`; it is built only
as test/research targets.

Symbol sizes are compiler- and optimization-specific. They are comparative
evidence, not an ABI guarantee.

## Reproducibility

Fixed schedule and four-word prefix vectors are tested for the tuple:

```text
master_seed = 42
generation = 7
population_index = 11
domain = mutation
```

The GitHub build matrix runs the same vectors with GCC and Clang on Linux and
with Clang on macOS. A compiler or platform disagreement fails the research
test.

The committed raw capture is:

```text
docs/engineering/reports/data/EVO-RNG-001-results-gcc13-linux.json
sha256:4522dbb12c45a121c262083a4d400ed5e5b79f72ad01af957e43a7f32afab82b
```

It was captured on Linux with GCC 13.3.0, C17, and `-O2`. Timing uses process
CPU time and is machine-specific. Re-running the experiment should reproduce
the integer vectors and deterministic diagnostic counts; timing fields are
expected to differ.

## Verification

Local verification completed:

- all eight production and research executables passed under GCC C17 with
  `-Wall -Wextra -Wpedantic -Werror`;
- the fixed-vector research test also passed at `-O2`;
- GCC `-fanalyzer` passed for all new C sources;
- AddressSanitizer and UndefinedBehaviorSanitizer passed all eight
  executables;
- clang-format 15 dry-run passed;
- the strict AES-SEC-001 review ratchet reported zero banned APIs, four
  existing reviewed primitives, and no new, unresolved, drifted, or stale
  findings; and
- the raw artifact is valid JSON and byte-identical to experiment output.

The local supervisor prevents LeakSanitizer from inspecting `/proc`, so local
ASan used `detect_leaks=0`. The GitHub sanitizer job retains
`detect_leaks=1`. CMake was unavailable in the local image; GitHub's CMake
matrix is the authoritative integration check.

## Limitations

- This 4,096-tuple corpus is a diagnostic sample, not proof of collision
  resistance.
- Pearson correlation on first words is not a complete statistical battery.
- The experiment evaluates seed schedules; it does not replace TestU01,
  PractRand, or another suitable analysis of the underlying PCG stream.
- Prime-vector generation is excluded from per-derivation timing.
- The research prime vector supports population indices through 4,096. That
  is an experimental bound, not a proposed production limit.
- Curve group order and cryptographic properties were deliberately not relied
  upon. The curve candidate is rejected without making a security claim.

## Recommendation

1. Preserve EVO RNG algorithm version 1 and its fixed vectors.
2. Reject the elliptic schedule for production because its cost and review
   surface are not justified by measured separation.
3. Do not adopt the prime-indexed schedule because it did not outperform the
   simpler control on the adoption criteria and adds artifact/capacity
   governance.
4. Keep the research implementation private and non-production.
5. When independently addressable operator streams become necessary, open a
   separate versioned design issue starting from the plain tuple-mixed control
   and test it against the actual operator-consumption model.
