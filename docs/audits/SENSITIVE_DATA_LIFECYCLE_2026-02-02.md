# Sensitive Data Lifecycle & Memory Protection Audit

**Project:** KeepTower
**Date:** 2026-02-02
**Scope:** Plan section 7 (“Sensitive data lifecycle review deliverable”)

## 1. Goal

Document where secrets exist in KeepTower, how they are protected in-memory, where they cross trust boundaries (disk/clipboard/logs), and how they are cleared.

This is an engineering-focused lifecycle map (what exists today), plus any gaps/risks observed during review.

## 2. Primary Secret Types (Inventory)

| Secret / Sensitive Data | Where it originates | Where it is used | Intended persistence | Notes |
|---|---|---|---|---|
| Vault master password (V1) | UI password entry | Key derivation (PBKDF2) and vault open | Never persisted | Should be cleared as soon as derivation/verification completes |
| User password (V2) | V2 login dialog | KEK derivation, user auth, DEK unwrap | Never persisted | V2 also supports YubiKey enrollment/login flows |
| Derived KEK (key-encryption key) | PBKDF2 / KDF output | AES key-wrap unwrap/wrap for DEK | Never persisted | High sensitivity; must be zeroized |
| DEK (data-encryption key) | Random at vault create; unwrapped at login | Encrypt/decrypt vault payload | Never persisted | Exists in-memory while vault is open |
| YubiKey PIN | UI PIN entry | YubiKey enroll / auth | Never persisted in plaintext | Should be cleared immediately after use |
| Clipboard contents (account passwords) | UI “copy password” | System clipboard | System-level persistence until cleared | Auto-clear reduces exposure but cannot prevent other apps reading clipboard |
| Plaintext vault record fields (account passwords, notes, etc.) | After decrypt | UI display, edit, export | Persisted encrypted on disk | In-memory plaintext exists while vault is unlocked |
| User identifiers (username) | UI | UI display, keyslot headers (in-memory only) | Persisted? V2 aims to avoid writing plaintext usernames | Treat as “sensitive identifier” (privacy), even if not cryptographic secret |

## 3. In-Memory Protections

### 3.1 Zeroization / secure clearing

Central primitives are in [src/utils/SecureMemory.h](src/utils/SecureMemory.h):

- `OPENSSL_cleanse()` wrappers:
  - `secure_clear(std::array<uint8_t, N>&)`
  - `secure_clear_ustring(Glib::ustring&)`
  - `secure_clear_std_string(std::string&)`
- RAII helpers:
  - `SecureAllocator<T>` / `SecureVector<T>` (zeroes memory on deallocation)
  - `SecureBuffer<T>` (cleanses in destructor; move-only)
  - `SecureString` (Glib::ustring wrapper that auto-clears on destruction)

Observed usage:
- UI credential handling clears passwords/PINs after use (examples: [src/ui/dialogs/V2UserLoginDialog.cc](src/ui/dialogs/V2UserLoginDialog.cc), [src/ui/dialogs/ChangePasswordDialog.cc](src/ui/dialogs/ChangePasswordDialog.cc), [src/ui/managers/VaultIOHandler.cc](src/ui/managers/VaultIOHandler.cc)).
- Core vault close clears key material, including unlocking then cleansing V2 DEK and YubiKey challenge buffers (see [src/core/VaultManager.cc](src/core/VaultManager.cc)).

### 3.2 Memory locking (swap resistance)

Memory locking is implemented in [src/core/VaultManager.cc](src/core/VaultManager.cc) via `mlock/munlock` on Linux.

Observed usage:
- V1 vault open attempts to lock `m_encryption_key` and always locks `m_salt`.
- YubiKey challenge buffers are locked when stored.
- V2 vault close explicitly unlocks then zeroizes the DEK and YubiKey challenge buffers.

Notes:
- Lock failures are non-fatal and logged as warnings (good availability tradeoff).
- Locking is best-effort; sensitive data may still be paged by the OS in some circumstances.

## 4. Lifecycle Maps (Key Flows)

### 4.1 V1: Open vault

1. User enters password in UI.
2. `VaultManager::open_vault()` derives `m_encryption_key` using PBKDF2 parameters and vault salt.
3. Key material is locked best-effort (`mlock`) and used to decrypt vault payload.
4. Decrypted protobuf is kept in-memory as `m_vault_data` while vault is open.
5. On close (`VaultManager::close_vault()`), key material is securely cleared.

### 4.2 V2: Open vault / multi-user

High-level intent:
- Avoid writing plaintext usernames to disk.
- Store per-user keyslots containing username hashes + wrapped DEK.

Key points observed:
- Serialization of keyslot data writes hashes/salts/wrapped keys, not plaintext usernames (see [src/core/MultiUserTypes.cc](src/core/MultiUserTypes.cc)).
- `VaultManager::save_vault()` explicitly clears plaintext usernames from the V2 header prior to serialization (see [src/core/VaultManager.cc](src/core/VaultManager.cc)).

In-memory while unlocked:
- DEK remains in memory and is used to encrypt/decrypt vault data.

On close:
- V2 DEK and YubiKey challenge buffers are explicitly `munlock`’d then `OPENSSL_cleanse()`’d.

### 4.3 Export flow

The export flow re-prompts for password and performs verification.

- Password is cleansed both on failure and on success immediately after authentication (see [src/ui/managers/VaultIOHandler.cc](src/ui/managers/VaultIOHandler.cc)).

### 4.4 Change password flow

- `PasswordChangeRequest::clear()` securely clears current/new passwords and optional YubiKey PIN (see [src/ui/dialogs/ChangePasswordDialog.cc](src/ui/dialogs/ChangePasswordDialog.cc)).

## 5. Clipboard Lifecycle

Clipboard interactions are centralized in [src/ui/controllers/ClipboardManager.h](src/ui/controllers/ClipboardManager.h) and [src/ui/controllers/ClipboardManager.cc](src/ui/controllers/ClipboardManager.cc).

- Copy writes plaintext to the system clipboard and schedules an auto-clear timeout.
- Auto-clear sets clipboard text to an empty string.
- “Preservation” mode exists (intended for admin onboarding flows) and includes a safety timeout.

Important limitation:
- The application cannot prevent other processes from reading clipboard contents before the timeout.

## 6. Logging & Telemetry

Rule of thumb:
- Logs must not contain secrets (passwords, derived keys, DEK/KEK), and should avoid sensitive identifiers when possible.

Remediations applied as part of this lifecycle review:
- Removed password “hex preview” logging during PBKDF2 input in [src/core/KeyWrapping.cc](src/core/KeyWrapping.cc).
- Removed plaintext username logging during V2 serialization and V2 vault save in:
  - [src/core/MultiUserTypes.cc](src/core/MultiUserTypes.cc)
  - [src/core/VaultManager.cc](src/core/VaultManager.cc)
- Adjusted clipboard copied signal to avoid propagating clipboard contents to listeners:
  - [src/ui/controllers/ClipboardManager.h](src/ui/controllers/ClipboardManager.h)
  - [src/ui/controllers/ClipboardManager.cc](src/ui/controllers/ClipboardManager.cc)
  - [src/ui/windows/MainWindow.cc](src/ui/windows/MainWindow.cc)

## 7. Risks / Gaps (Follow-ups)

Non-blocking follow-ups worth tracking:

1. **Credential duplication via captures/copies (UI lambdas)**
   - Some UI flows capture `password` values by copy into lambdas for async operations.
   - Even with explicit cleansing at some call sites, copies can extend lifetime.
   - Recommendation: prefer `SecureString`/move-only credential objects, minimize copies, and centralize “clear-on-exit” patterns.

2. **Plaintext vault content lifetime**
   - Once decrypted, `m_vault_data` contains plaintext sensitive fields while unlocked.
   - Recommendation: consider “field-level secure buffers” or encrypt-at-rest-in-memory patterns only if threat model requires it (often heavy UX/complexity tradeoff).

3. **Log framework callsite accuracy**
   - `KeepTower::Log` currently uses `std::source_location::current()` inside the logging function, which reports `Log.h` rather than the caller location.
   - Recommendation: pass source location from call sites so logs remain actionable.

## 8. Summary

KeepTower already has strong baseline primitives for secure clearing and best-effort memory locking. The primary lifecycle risks observed were **secret propagation into logs** and **unnecessary distribution of secrets via signals**, both of which were corrected during this review. Remaining work is largely about reducing accidental copies and clearly documenting clipboard limitations.
