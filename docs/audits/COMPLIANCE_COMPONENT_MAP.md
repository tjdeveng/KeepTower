# Compliance Audit Component Map

**Project:** KeepTower

**Date:** 2026-02-01

**Purpose:** Identify security-critical components and define what “done” looks like for each.

---

## Component Map (Initial)

| Component | Location(s) | Risk | What to Verify | Tests/Commands | Notes |
|---|---|---:|---|---|---|
| Vault manager (orchestration + lifecycle) | `src/core/VaultManager.cc`, `src/core/VaultManagerV2.cc` | High | Clear separation of responsibilities; error handling; no secret logging; correct close/save semantics | `meson test -C build --print-errorlogs "VaultManager Tests"` | This is a likely “god object” hotspot; audit boundaries first. |
| Vault encryption/decryption (AES-256-GCM) | `src/core/crypto/VaultCrypto.cc`, `src/core/services/VaultCryptoService.cc` | High | EVP-only APIs; tag verification; IV sizing/uniqueness; error handling; cleanse of intermediate buffers | `meson test -C build --print-errorlogs vault_crypto "VaultCryptoService Unit Tests"` | Confirm behavior when auth fails (bad tag). |
| Key derivation / KEK derivation | `src/core/services/KekDerivationService.cc`, `src/core/KeyWrapping.cc` | High | Parameter validation (salt/iters); FIPS-mode algorithm restrictions; consistent error reporting | `meson test -C build --print-errorlogs "KEK Derivation Tests" "KeyWrapping Unit Tests"` | Validate iteration counts are policy-driven and tested. |
| Vault file I/O + atomic writes | `src/core/io/VaultIO.cc`, `src/core/services/VaultFileService.cc` | High | Atomic write strategy; fsync/flush semantics; permissions (0600); safe temp paths; backup creation/rotation | `meson test -C build --print-errorlogs vault_io "VaultFileService Unit Tests"` | Confirm no partial writes on crash simulation tests (if present). |
| Vault formats + parsing | `src/core/format/VaultFormat.cc`, `src/core/VaultFormatV2.cc`, `src/core/serialization/VaultSerialization.cc` | High | Strict parsing; length limits; canonicalization; reject malformed inputs; no UB on bad data | `meson test -C build --print-errorlogs "VaultFormatV2 Unit Tests" vault_serialization` | Consider adding fuzz harness later (out-of-scope for this first pass). |
| FIPS mode plumbing | `src/core/VaultManager.cc`, `src/core/VaultManagerV2.cc`, `src/ui/dialogs/PreferencesDialog.cc` | High | Explicit enable/disable behavior; provider availability check; only approved algorithms reachable when enabled | `meson test -C build --print-errorlogs "FIPS Mode Tests"` | UI must not enable a mode the core can’t enforce. |
| Memory locking + secure clearing | `src/core/VaultManager.cc`, `src/core/VaultManagerV2.cc`, `src/core/services/VaultCryptoService.cc`, `src/ui/managers/VaultIOHandler.cc`, `src/ui/managers/V2AuthenticationHandler.cc` | High | `mlock`/`munlock` used where intended; failures handled safely; `OPENSSL_cleanse` called on secrets and intermediates | `meson test -C build --print-errorlogs "Memory Locking Security Tests" "Secure Memory Tests"` | Verify `OPENSSL_cleanse` is applied to *all* secret-bearing buffers, including UI input copies. |
| YubiKey integration | `src/core/managers/YubiKeyManager.cc`, `src/core/services/VaultYubiKeyService.cc`, `src/ui/managers/YubiKeyHandler.cc` | High | FIPS-approved algorithms when required; correct challenge/response handling; no secret logging; error handling | `meson test -C build --print-errorlogs "VaultYubiKeyService Unit Tests" "YubiKey Algorithm Tests"` | Hardware presence variance: document expectations for CI vs local. |
| Username hashing + migrations | `src/core/services/UsernameHashService.cc`, `src/core/MultiUserTypes.cc`, tests in `tests/test_username_hash_migration*.cc` | Medium | Determinism; backup/restore recovery; no data loss; stable ordering; concurrency behavior | `meson test -C build --print-errorlogs --suite migration` | Priority 2 is merge-gated in CI (per `scripts/ci_test_runner.py`). |
| UI security features (clipboard/autolock) | `src/ui/managers/Clipboard*`, `src/ui/managers/AutoLockHandler.cc` (and friends) | Medium | Clipboard auto-clear; no persistence; autolock triggers and edge cases; no secret exposure in UI logs | `meson test -C build --print-errorlogs "UI Security Tests" clipboard_manager_test auto_lock_manager_test` | Confirm time-based tests are stable across CI. |
| Input validation | `src/core/...`, `src/ui/...` (various entry points) | Medium | Bounds/length checks; reject invalid data early; no unchecked conversions | `meson test -C build --print-errorlogs "Input Validation Tests"` | Expand with targeted tests if gaps appear. |
| Settings persistence + validation | `src/application/Application.cc`, `src/ui/dialogs/PreferencesDialog.cc`, `src/utils/SettingsValidator.h` | Medium | GSettings schema/key handling; safe defaults; FIPS setting can’t be silently enabled; validation blocks insecure values | `meson test -C build --print-errorlogs "Settings Validator Tests" "FIPS Mode Tests"` | Schema-missing path should be explicit and safe-by-default. |
| Logging (secret redaction) | `src/utils/Log.h` and all `Log::*` call sites | High | No secrets in logs (passwords/DEK/KEK/PIN/challenges); error messages don’t leak sensitive material; debug-only logs are safe | `meson test -C build --print-errorlogs test_security_features ui_security_test` | Treat log statements in `src/core/*` as high-risk. |
| Import/Export (plaintext export risks) | `src/utils/ImportExport.cc` | High | Export files permissions (0600); no secret leaks on error paths; safe parsing; explicit user warnings/UX expectations | `meson test -C build --print-errorlogs` | Consider adding/expanding tests here if none exist (track as finding). |
| Account/group persistence (repositories) | `src/core/repositories/AccountRepository.cc`, `src/core/repositories/GroupRepository.cc` | Medium | Data integrity; input validation; no sensitive logging; error handling correctness | `meson test -C build --print-errorlogs account_repository_test group_repository_test` | Verify repository layer doesn’t bypass crypto invariants. |
| Account/group business logic (services) | `src/core/services/AccountService.cc`, `src/core/services/GroupService.cc` | Medium | Authorization boundaries; validation; correct propagation of errors; no secrets in logs | `meson test -C build --print-errorlogs account_service_test group_service_test` | Often becomes a second “orchestrator” hotspot; watch SRP. |
| Password validation + history (constant-time) | `src/core/PasswordHistory.cc`, `src/ui/dialogs/ChangePasswordDialog.cc` | High | Constant-time comparisons; history enforcement correctness; sensitive buffer cleansing; no side-channel regressions | `meson test -C build --print-errorlogs "Password Validation Tests" password_history` | Confirm comparisons use OpenSSL constant-time primitives where possible. |
| UI authentication flows (password/PIN handling) | `src/ui/managers/V2AuthenticationHandler.cc`, `src/ui/dialogs/V2UserLoginDialog.cc`, `src/ui/dialogs/PasswordDialog.cc`, `src/ui/dialogs/ChangePasswordDialog.cc` | High | Secret lifetime (copies); `OPENSSL_cleanse` usage; failure paths; clipboard preservation disabled on auth | `meson test -C build --print-errorlogs v2_auth_test ui_security_test` | High risk for accidental string copies and logs. |
| Auto-lock manager/controller | `src/ui/controllers/AutoLockManager.cc`, `src/ui/managers/AutoLockHandler.cc` | Medium | Timer correctness; lock triggers; no bypass when switching windows; safe defaults | `meson test -C build --print-errorlogs auto_lock_manager_test` | Time-based tests can be flaky; record timing assumptions. |
| Clipboard manager/controller | `src/ui/controllers/ClipboardManager.cc` | Medium | Auto-clear behavior; preserve/disable-preservation semantics; clearing on teardown | `meson test -C build --print-errorlogs clipboard_manager_test` | Ensure "preservation" doesn’t accidentally persist secrets. |

---

## Immediate “Audit First” Focus

1. `src/core/VaultManager*.cc` (responsibility boundaries + secrets lifecycle)
2. `src/core/crypto/` + `src/core/services/VaultCryptoService.cc` (crypto correctness + FIPS-ready routing)
3. `src/core/io/` + `src/core/services/VaultFileService.cc` (atomicity, permissions, backups)
4. FIPS enable/disable path end-to-end (UI preference → core enforcement)
