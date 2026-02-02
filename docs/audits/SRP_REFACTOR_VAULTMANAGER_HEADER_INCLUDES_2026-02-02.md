# SRP Refactor Evidence: VaultManager Header Include Decoupling (2026-02-02)

## Scope
Reduce compile-time coupling from `src/core/VaultManager.h` by replacing heavy project includes with forward declarations where safe, and moving required includes into the corresponding translation units (`.cc`) and tests.

This is an SRP/maintainability refactor intended to be behavior-preserving.

## Change Summary
- `src/core/VaultManager.h`
  - Removed several heavy transitive includes (e.g., Vault IO/format/serialization, managers, services, Reed-Solomon).
  - Added forward declarations for pointer-held types (`std::unique_ptr` / `std::shared_ptr`) and a small set of required standard headers.
- `src/core/VaultManager.cc`
  - Added explicit includes required by the implementation after header slimming (e.g., `crypto/VaultCrypto.h`, `managers/YubiKeyManager.h`).
- `src/core/VaultManagerV2.cc`
  - Added explicit includes required where `std::make_unique` / `std::make_shared` instantiate types (needs complete types).
- `tests/test_vault_manager.cc`
  - Added explicit include for `managers/YubiKeyManager.h` (no longer available transitively).

## Evidence
### Commit
- `277b74b` — `refactor(core): reduce VaultManager.h include coupling`

### Build
- `meson compile -C build` (OK)

### Focused Test Runs
- `meson test -C build --print-errorlogs "FIPS Mode Tests"` (OK)
- `meson test -C build --print-errorlogs "UI Security Tests"` (OK)
- `meson test -C build --print-errorlogs "Memory Locking Security Tests"` (OK)

## Notes / Follow-ups
- Build emitted warnings unrelated to this refactor (e.g., `[[nodiscard]]` return values ignored in some tests, deprecated protobuf accessor). No functional behavior changes were introduced here.
