# Synthetic companion-manifest fixture (v0.1)

**Synthetic.** These files were written by hand for tests. No REA run, instrument, capture or calibration produced them, and they support no compatibility, measurement, timing or safety claim.

- `store/native-bundle.json` follows the seven-array shape of REA's native evidence bundle at the EVO-004 pin (`9e22da8`, `src/domain/evidenceBundle.ts`). Its records are placeholders.
- `manifest.json` is a companion manifest v0.1 that references the bundle by sha256 and byte count.
