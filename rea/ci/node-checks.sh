#!/usr/bin/env bash
# rea/ scope: REA's own install, fast static checks and source tests, as upstream's
# CI runs them (npm ci, check:ci:fast, test:fast), plus REA's Python bridge tests
# (test:python --bridge; they need no packages beyond Python). Runs third-party code: call it
# only on a hosted runner with no secrets (ADR-0048, ADR-0046 Amendment 1).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
npm ci --no-audit --no-fund
npm run check:ci:fast
npm run test:fast
python3 scripts/run-python-tests.py --bridge
