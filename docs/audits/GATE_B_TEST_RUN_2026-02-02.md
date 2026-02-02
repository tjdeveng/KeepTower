# Gate B Test Run (Normal Build) — 2026-02-02

Date: 2026-02-02

## Scope

This is evidence for Gate B (“Security Test Set”) from `docs/developer/COMPLIANCE_CODE_AUDIT_PLAN.md`, executed in the normal (non-sanitized) build directory `build/`.

## Commands + Results

### Non-migration Gate B tests

Command:

- `meson test -C build --print-errorlogs "FIPS Mode Tests" "Memory Locking Security Tests" vault_crypto "VaultCryptoService Unit Tests" vault_io "VaultFileService Unit Tests" "UI Security Tests"`

Result:

- PASS (7/7)

### Migration suite (Gate B)

Command:

- `meson test -C build --print-errorlogs --suite migration`

Result:

- PASS (4/4)

Notable timings (from Meson output):

- Username Hash Migration Concurrency Tests: ~115s
- Username Hash Migration Priority 3 Tests: ~135s

## Raw logs

- `build/meson-logs/testlog.txt`

## Notes

- Build emitted repeated warnings about deprecated protobuf accessor `YubiKeyConfig::serial()` while compiling tests. These are warnings only; tests passed.
