#!/usr/bin/env bash
# rea/ scope: verify rea/ against rea/UPSTREAM.json (ADR-0046 Amendment 1).
# Fails when upstream files differ and rea/LOCAL-CHANGES.md records no local change.
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
python3 "$here/../../tools/rea_recovery/rea_upstream.py" check --rea "$here/.."
