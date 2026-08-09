# Repository Scoring Reference Adapter

`repository_scoring.c` searches four one-byte weights over an immutable,
explicit four-record review fixture. Each byte maps to `1 + byte modulo 8` for
tests, review, documentation, and security. A sum above 24 is hard invalid;
security weight below three adds a visible soft penalty already included in
`fitness.total`.

The adapter captures the complete format-3 checkpoint at generation two,
inspects all 12 candidates in stable population order, resumes from the copied
bytes, and requires the resumed final result and notification suffix to equal
the uninterrupted run. A second uninterrupted run must also match result,
trace, and checkpoint bytes exactly.

It scores only the declared fixture. It does not inspect, modify, commit,
publish, or make an authoritative readiness decision about a real repository.
Real reviewed training/validation corpora and downstream policy remain outside
this bounded core proof.
