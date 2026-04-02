# Phase C & D: Preferences Refactor + Library Extraction Plan

**Status:** Planning (Post-Phase B)
**Target:** v0.4.0
**Duration:** ~4-5 days estimated
**Quality Gate:** 58/58 tests passing, zero compiler warnings, Doxygen clean

---

## Overview

Phase C and D form a coordinated refactoring cycle to extract internal libraries and consolidate runtime preferences. This work:

1. **Reduces VaultManager surface area** by moving 14 preference accessors to a dedicated struct
2. **Establishes library extraction pattern** by creating clean boundaries
3. **Prepares for Phase 9** (user password history) and beyond
4. **Enables A+ code quality** through isolated, independently-testable libraries

---

## Phase C: Extract VaultRuntimePreferences (~0.5 day)

### Scope
Move runtime preference getters/setters into a dedicated, independently-managed struct held by `VaultManager`.

### Work Items

**C.1: Create VaultRuntimePreferences struct** (trivial)
- New file: `src/core/VaultRuntimePreferences.h`
- Struct members:
  ```cpp
  int clipboard_timeout_seconds = 30;
  bool auto_lock_enabled = true;
  int auto_lock_timeout_seconds = 300;
  bool account_password_history_enabled = false;
  int account_password_history_limit = 5;
  bool undo_redo_enabled = true;
  int undo_history_limit = 50;
  // (+ 7 others from current VaultManager)
  ```
- Move getters/setters from `VaultManager` → struct accessor methods

**C.2: Update VaultManager to use VaultRuntimePreferences** (trivial)
- Add `std::unique_ptr<VaultRuntimePreferences> m_preferences` member
- Change call sites from `vault_manager->get_clipboard_timeout()` to `vault_manager->preferences().get_clipboard_timeout()`
- Update all test fixtures and production code call sites

**C.3: Migrate existing preference tests** (trivial)
- Move tests from `test_vault_manager.cc` preference sections to new `test_vault_runtime_preferences.cc`
- Verify 14 preference accessors remain correct

### Files Modified
- **New:** `src/core/VaultRuntimePreferences.h`
- **New:** `tests/test_vault_runtime_preferences.cc`
- **Modified:** `src/core/VaultManager.h/cc` (remove 14 accessor methods)
- **Modified:** All call sites (UI, controllers, services)

### Validation Gate
```bash
meson test -C build "Preferences" "VaultManager Tests"
meson compile -C build doxygen
```

---

## Phase D: Library Extraction Foundation (~4 days)

### Architecture

Libraries live in `src/lib/`, each with:
```
src/lib/
  backup/
    include/
      keeptower/backup/VaultBackupPolicy.h
      keeptower/backup/BackupSettings.h
    src/
      VaultBackupPolicy.cc
    tests/
      test_backup_policy.cc
    meson.build

  fec/
    include/keeptower/fec/...
    src/...
    tests/...
    meson.build

  crypto/
    include/keeptower/crypto/...
    src/...
    tests/...
    meson.build

  fips/
    include/keeptower/fips/...
    src/...
    tests/...
    meson.build
```

Each library has:
- Public header-only API in `include/keeptower/<lib>/`
- Implementation in `src/`
- Independent unit tests in `tests/`
- Meson build target with clear dependencies

### D.1: libkeeptower-backup (2 days)

**Ready because Phase B just cleaned this up.**

**Files extracted:**
- `VaultBackupPolicy` (header + implementation)
- `SettingsValidator::BackupPreferences` helper
- Backup constants: `MIN_BACKUP_COUNT`, `MAX_BACKUP_COUNT`, `DEFAULT_BACKUP_COUNT`

**Public API:**
```cpp
// keeptower/backup/BackupSettings.h
struct BackupSettings {
  bool enabled;
  int count;          // 1-50
  std::string path;
};

// keeptower/backup/BackupPolicy.h
class VaultBackupPolicy {
public:
  static bool is_valid_backup_count(int count);
  static int clamp_backup_count(int count);
  // ... policy methods
};
```

**Dependencies:**
- Glib (minimal)
- No VaultManager dependency (clean boundary)

**Tests:**
- Move existing `test_settings_validator.cc` backup sections
- Add `test_backup_policy.cc` with new library-specific tests

**Integration:**
- VaultManager links against libkeeptower-backup
- UI/controllers depend on library public API
- SettingsValidator becomes thin wrapper around library

---

### D.2: libkeeptower-fec (1.5 days)

**Self-contained, low coupling.**

**Files extracted:**
- `ReedSolomon` class + helpers
- FEC constants: `MIN_RS_REDUNDANCY`, `MAX_RS_REDUNDANCY`, `DEFAULT_RS_REDUNDANCY`
- Validation helpers

**Public API:**
```cpp
// keeptower/fec/ReedSolomon.h
class ReedSolomon {
public:
  static std::expected<std::vector<uint8_t>, VaultError>
    encode(std::span<const uint8_t> data, uint8_t redundancy_percent);

  static std::expected<std::vector<uint8_t>, VaultError>
    decode(std::span<const uint8_t> encoded);
};

// keeptower/fec/FecPolicy.h
class FecPolicy {
  static bool is_valid_redundancy(int percent);
  static int clamp_redundancy(int percent);
};
```

**Dependencies:**
- libcorrect (external)
- No vault/crypto dependencies

**Tests:**
- Move `test_reed_solomon.cc`, `test_vault_reed_solomon.cc`
- Library is already well-tested from Phase B work

---

### D.3: libkeeptower-crypto (1.5 days)

**Moderate scope, critical for FIPS.**

**Files extracted:**
- `VaultCrypto` service
- `KeyWrapping` utilities
- EVP context handling (consolidate with SecureMemory)
- Cryptographic constants

**Public API:**
```cpp
// keeptower/crypto/VaultCrypto.h
class VaultCryptoService {
public:
  static std::expected<std::vector<uint8_t>, VaultError>
    derive_kek(std::string_view password, std::span<const uint8_t, 32> salt);

  static std::expected<std::vector<uint8_t>, VaultError>
    encrypt(std::span<const uint8_t> plaintext, const std::vector<uint8_t>& kek);
};

// keeptower/crypto/SecureMemory.h (consolidated)
class EVPCipherContextPtr { /* RAII wrapper */ };
void secure_clear(std::vector<uint8_t>& buf);
```

**Dependencies:**
- OpenSSL 3.5+ (external)
- Self-contained (no VaultManager)

**Tests:**
- Move `test_vault_crypto` tests
- Consolidate memory-clearing tests into library

**Quality improvements:**
- Standardize all `OPENSSL_cleanse()` usage
- Add `GCM_IV_SIZE` constant (was magic "12")
- Explicit KEK secure_clear() enforcement

---

### D.4: libkeeptower-fips (0.5 day)

**Strategic, isolates compliance logic.**

**Files extracted:**
- `FipsProviderManager`
- FIPS mode checking, enforcement
- FIPS-approved algorithm validation

**Public API:**
```cpp
// keeptower/fips/FipsProvider.h
class FipsProviderManager {
public:
  static bool is_fips_available();
  static bool is_fips_mode_enabled();
  static std::expected<void, VaultError> enable_fips_mode();

  static bool is_algorithm_fips_approved(Algorithm algo);
};
```

**Dependencies:**
- OpenSSL 3.5+ FIPS provider
- No VaultManager

**Tests:**
- Move FIPS mode tests
- Library can be tested in isolation

---

## Sequencing & Integration

### Build Order

```
Phase C (0.5 day)
  ↓ (blocking: trivial, unblocks Phase D)

Phase D.1: libkeeptower-backup (2 days)
Phase D.2: libkeeptower-fec (1.5 days) — can run parallel after D.1
Phase D.3: libkeeptower-crypto (1.5 days) — can run parallel after D.1
Phase D.4: libkeeptower-fips (0.5 day) — can run parallel after D.3

Main app depends on:
  ✓ VaultRuntimePreferences (Phase C)
  ✓ libkeeptower-backup, libkeeptower-fec, libkeeptower-crypto, libkeeptower-fips (Phase D)
```

### Call Site Changes

**Before:**
```cpp
vault_manager->get_clipboard_timeout();
vault_manager->apply_backup_settings({...});
vault_manager->is_reed_solomon_enabled();
VaultCrypto::derive_kek(...);
FipsProviderManager::is_fips_available();
```

**After:**
```cpp
vault_manager->preferences().get_clipboard_timeout();  // Phase C
vault_manager->backup().apply_settings({...});        // Phase D.1
vault_manager->fec().is_enabled();                     // Phase D.2
Crypto::derive_kek(...);                               // D.3 (library namespace)
Fips::is_available();                                  // D.4 (library namespace)
```

### Meson Integration

Main `meson.build` adds:
```meson
subdir('src/lib/backup')
subdir('src/lib/fec')
subdir('src/lib/crypto')
subdir('src/lib/fips')

# Link main app
executable('keeptower',
  ...
  dependencies: [
    ...,
    keeptower_backup_lib,
    keeptower_fec_lib,
    keeptower_crypto_lib,
    keeptower_fips_lib,
  ]
)
```

---

## Testing & Validation Gates

### Per-Library Gates (run after each D.x)

```bash
# After D.1
meson test -C build "Settings Validator" "Preferences Presenter"
meson test -C build "Backup"

# After D.2
meson test -C build "Reed-Solomon" "FEC Preferences"

# After D.3
meson test -C build "VaultCrypto" "KeyWrapping" "Secure Memory"

# After D.4
meson test -C build "FIPS Mode"
```

### Integrated Gate (run after Phase D complete)

```bash
meson test -C build --print-errorlogs    # All 58+N tests
meson compile -C build doxygen           # API docs
meson compile -C build 2>&1 | grep -i warning  # Zero warnings
```

---

## Benefits & Outcomes

### A+ Code Quality
- ✅ **Isolation:** Each library independently testable, auditable
- ✅ **Clear boundaries:** Public/private APIs enforced by header structure
- ✅ **Maintainability:** 14 separate concerns, not mixed into VaultManager
- ✅ **Security:** Crypto/FIPS logic isolated, easier to audit

### Operational
- ✅ **Reusability:** Libraries can be linked by other projects
- ✅ **Distribution:** Future CLI tools, server components can use libraries
- ✅ **Documentation:** Each library gets own Doxygen section

### Foundation for Phase 9 & Beyond
- Phase 9 (user password history) can depend on `libkeeptower-backup` for serialization
- Cloud sync (Phase C long-term) can package libraries separately
- Enterprise features (Phase D long-term) benefit from tested, isolated libraries

---

## Risk Analysis

| Phase | Risk | Mitigation |
|-------|------|-----------|
| C (VaultRuntimePreferences) | Call site churn | Search/replace, incremental updates, per-subsystem validation |
| D.1 (backup lib) | Dependency confusion | Phase B already isolated backup logic; minimal risk |
| D.2 (FEC lib) | ReedSolomon stability | Already heavily tested; library just reorganizes code |
| D.3 (crypto lib) | OpenSSL/memory handling | Consolidate existing safe patterns; no new crypto logic |
| D.4 (FIPS lib) | FIPS provider availability | Already gated; library just isolates checks |

---

## Checkpoints & Commits

Each phase produces a local checkpoint:
```
Phase C (1 commit)
  c_runtime_preferences: VaultRuntimePreferences extraction + call site migration

Phase D.1 (1-2 commits)
  d1_backup_lib: Create libkeeptower-backup with tests
  d1_backup_integration: Link main app against library, migrate call sites

Phase D.2 (1-2 commits)
  d2_fec_lib: Create libkeeptower-fec with tests
  d2_fec_integration: Link main app, migrate call sites

Phase D.3 (1-2 commits)
  d3_crypto_lib: Create libkeeptower-crypto with tests + secure memory consolidation
  d3_crypto_integration: Link main app, migrate call sites

Phase D.4 (1 commit)
  d4_fips_lib: Create libkeeptower-fips with tests
  d4_fips_integration: Link main app, migrate call sites

Final integration (1 commit)
  refactor: comprehensive library extraction complete; full gate passing
```

---

## Next Steps

1. **Start Phase C** — Extract VaultRuntimePreferences (trivial, unblocks Phase D)
2. **Run targeted gate** — Preferences + VaultManager tests
3. **Begin Phase D.1** — libkeeptower-backup (follows Phase B cleanup directly)
4. **Parallel work** — Phase D.2 can start while D.1 is in testing
5. **Full integration test** — After all libraries linked and migrated

---

## Appendix: File Structure After Phase D

```
src/
  lib/
    backup/
      include/keeptower/backup/
        BackupPolicy.h
        BackupSettings.h
      src/
        BackupPolicy.cc
      tests/
        test_backup_policy.cc
      meson.build

    fec/
      include/keeptower/fec/
        ReedSolomon.h
        FecPolicy.h
      src/
        ReedSolomon.cc
      tests/
        test_reed_solomon.cc
      meson.build

    crypto/
      include/keeptower/crypto/
        VaultCrypto.h
        KeyWrapping.h
        SecureMemory.h
      src/
        VaultCrypto.cc
        KeyWrapping.cc
        SecureMemory.cc
      tests/
        test_vault_crypto.cc
        test_secure_memory.cc
      meson.build

    fips/
      include/keeptower/fips/
        FipsProvider.h
      src/
        FipsProvider.cc
      tests/
        test_fips_provider.cc
      meson.build

  core/
    VaultRuntimePreferences.h (Phase C)
    VaultManager.h/cc (Phase C + D: reduced surface)
    ... (other core files)

  meson.build (updated to subdir('lib/*'))
```
