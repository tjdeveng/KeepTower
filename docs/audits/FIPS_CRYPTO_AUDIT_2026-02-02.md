# FIPS Crypto Audit — 2026-02-02

Date: 2026-02-02

## Scope

This document provides Gate D evidence (“FIPS-ready behavior”) for KeepTower. It focuses on:

- OpenSSL 3 provider initialization and FIPS enablement
- Enforcement behavior when FIPS is requested but unavailable
- Application settings → crypto mode wiring
- Algorithm-level enforcement for user-configurable crypto-relevant settings

Out of scope (tracked separately): formal FIPS 140-3 certification; external OS/OpenSSL packaging verification beyond what tests can observe; full cryptographic module validation artifacts.

## Summary (Pass/Partial)

- Provider initialization: PASS (graceful fallback)
- FIPS enable/disable switching API: PASS (with runtime-toggle caveats)
- Settings wiring (GSettings → init): PASS
- Algorithm enforcement for settings: PASS (username-hash algorithm)
- Deployment enforcement (“must be in FIPS mode” hard-fail): PARTIAL (available, but policy decision is outside core runtime)

## Evidence: Code Paths

### Startup wiring (settings → init)

- Application reads `fips-mode-enabled` from GSettings and calls `VaultManager::init_fips_mode(enable_fips)`.
- See [src/application/Application.cc](src/application/Application.cc#L1).

### Provider initialization and state

- `VaultManager::init_fips_mode(bool enable)`:
  - Loads OpenSSL config (`OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CONFIG, ...)`).
  - Tries to load the `fips` provider (`OSSL_PROVIDER_try_load`).
  - If unavailable: loads `default` provider and continues (non-fatal).
  - If available:
    - Sets `s_fips_mode_available=true`.
    - If requested, enables FIPS properties via `EVP_default_properties_enable_fips(NULL, 1)` and sets `s_fips_mode_enabled=true`.
- See [src/core/VaultManager.cc](src/core/VaultManager.cc#L1812).

### Runtime switching (enable/disable)

- `VaultManager::set_fips_mode(bool enable)` switches default properties using `EVP_default_properties_enable_fips`.
- Correctness fix (2026-02-02): disabling FIPS is now idempotent and no longer fails when the provider is unavailable (disabling is a no-op in that scenario).
- See [src/core/VaultManager.cc](src/core/VaultManager.cc#L2163).

### Settings-level algorithm enforcement

- `SettingsValidator::get_username_hash_algorithm()` blocks non-FIPS algorithms when `fips-mode-enabled` is true and falls back to SHA3-256.
- See [src/utils/SettingsValidator.h](src/utils/SettingsValidator.h#L66).

- UI adds an additional belt-and-suspenders override when loading saved preferences (prevents showing a non-FIPS selection when FIPS is enabled).
- See [src/ui/dialogs/PreferencesDialog.cc](src/ui/dialogs/PreferencesDialog.cc#L1278).

## Evidence: Test Runs

### FIPS Mode Tests (Meson)

Command:

- `meson test -C build -v "FIPS Mode Tests"`

Result (2026-02-02): PASS (12/12)

Key observed behaviors from the test output:

- Providers initialize successfully with `enable=false`.
- FIPS provider was detected as available in this environment.
- Runtime toggle path exercised (`enabled successfully` then `disabled successfully`).

Raw logs:

- Meson log: `build/meson-logs/testlog.txt`

## Findings / Notes

1. Runtime switching limitations are acknowledged in documentation: existing crypto contexts may not switch immediately and some OpenSSL deployments may require restart.
2. The application currently treats “FIPS requested but unavailable” as a graceful degradation (continue using default provider). This is correct for usability, but environments that *require* compliance may want a stricter policy (e.g., refuse to open vaults unless `VaultManager::is_fips_enabled()` is true).

## Follow-ups (if we want “strict compliance mode”)

- Decide a product policy for compliance-required deployments:
  - soft mode (current): warn and continue
  - strict mode: block vault operations when FIPS is requested but not enabled
- If strict mode is desired, implement it at the application boundary where vault operations begin (and include UI messaging).
