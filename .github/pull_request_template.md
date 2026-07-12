# Pull Request

## Summary

Describe the change and why it is needed.

## AES-DEV-001 Compliance

- [ ] Architecture or specification impact is documented.
- [ ] Documentation is updated in this change series, or no update is required and the reason is stated.
- [ ] Tests are included, or the absence of tests is justified.
- [ ] Interface and versioning impact is addressed.
- [ ] Failure modes and recovery behavior are addressed where applicable.
- [ ] Observability and diagnostic impact are addressed where applicable.
- [ ] Major architectural decisions have an ADR.
- [ ] The change is one logical, reviewable unit suitable for revert and `git bisect`.

## AES-SEC-001 Compliance

- [ ] Trust boundaries and authority crossings are identified where applicable.
- [ ] External lengths, indices, and serialized values are validated before use.
- [ ] Allocation and copy-size arithmetic is checked for overflow.
- [ ] Signed-to-unsigned conversions occur only after bounds are proven.
- [ ] Banned unsafe interfaces are not introduced.
- [ ] Unsafe operations are isolated and their safety invariant is documented.
- [ ] Static analysis and warning-clean build evidence is provided.
- [ ] Sanitizer and fuzz evidence is provided where applicable.
- [ ] No custom cryptography is introduced.
- [ ] Any exception is recorded in `docs/engineering/AES-SEC-001-waivers.md`.

## Evidence

List commands, test results, workflow runs, benchmark results, and relevant documentation or ADR paths.

## Security and Recovery Notes

Describe security, trust-boundary, failure-mode, and recovery implications. Write `Not applicable` with a reason when appropriate.