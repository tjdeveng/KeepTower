# FIDO2 YubiKey Implementation Specification

**Version:** 1.1
**Date:** 2026-02-22
**Status:** Implemented (Living Spec)
**Target Release:** v0.4.x

---

## Executive Summary

This specification defines the implementation of FIPS-140-3 compliant YubiKey support using FIDO2 `hmac-secret` extension with HMAC-SHA256. This replaces the deprecated PCSC-lite OTP implementation which only supported SHA-1.

**Key Changes:**
- **Library**: PCSC-lite → libfido2 (1.13.0+)
- **Protocol**: OTP APDU → FIDO2/WebAuthn CTAP2
- **Algorithm**: HMAC-SHA1 ❌ → HMAC-SHA256 ✅
- **Authentication**: Challenge-response → FIDO2 assertion with hmac-secret
- **User Experience**: Simple touch → PIN + touch (more secure)

---

## 1. Architecture Overview

### 1.1 Current Architecture (PCSC - Deprecated)

```
┌──────────────┐
│  VaultManager│
└──────┬───────┘
       │
       ▼
┌──────────────────┐
│ YubiKeyManager   │
│ (PCSC OTP)       │  ← Challenge-response via APDU
└──────┬───────────┘
       │
       ▼
┌──────────────────┐
│   libpcsclite    │  ← Smart card library
└──────┬───────────┘
       │
       ▼
┌──────────────────┐
│  pcscd daemon    │
└──────┬───────────┘
       │
       ▼
┌──────────────────┐
│  YubiKey OTP     │  ← HMAC-SHA1 only ❌
│  Slot 2          │
└──────────────────┘
```

**Limitations:**
- SHA-1 only (not FIPS-140-3 approved)
- No PIN protection
- Touch optional
- Deprecated protocol

### 1.2 New Architecture (FIDO2 - FIPS Compliant)

```
┌──────────────┐
│  VaultManager│
└──────┬───────┘
       │
       ▼
┌──────────────────┐
│ YubiKeyManager   │
│ (FIDO2 hmac-sec) │  ← FIDO2 assertion with extensions
└──────┬───────────┘
       │
       ▼
┌──────────────────┐
│   libfido2       │  ← Official FIDO2 library (Yubico)
└──────┬───────────┘
       │
       ▼
┌──────────────────┐
│   USB HID        │  ← Direct USB communication
└──────┬───────────┘
       │
       ▼
┌──────────────────┐
│  YubiKey FIDO2   │  ← HMAC-SHA256 ✅
│  (CTAP2)         │     PIN required
└──────────────────┘     Touch required
```

**Benefits:**
- ✅ FIPS-140-3 approved (SHA-256)
- ✅ PIN protection (security)
- ✅ Touch required (anti-malware)
- ✅ WebAuthn compatible
- ✅ Future-proof standard

---

## 2. FIDO2 Credential Model

### 2.1 Credential Type Used

KeepTower uses **resident (discoverable) credentials** with the `hmac-secret` extension.

Important implementation detail:
- Even though the credential is resident, KeepTower still stores the returned **credential ID** per user and uses it during assertions (`allow_cred`) to avoid credential enumeration and to keep vault open/verify flows deterministic.

Non-resident credential mode is **not currently implemented**.

### 2.2 FIDO2 hmac-secret Extension

The FIDO2 `hmac-secret` extension provides challenge-response functionality:

```
Input:  salt (1–32 bytes) - challenge bytes (copied into a 32-byte salt buffer)

Output: HMAC-SHA256(credential_secret, salt1) → 32 bytes
        HMAC-SHA256(credential_secret, salt2) → 32 bytes (optional)
```

**How it works:**
1. Enrollment: Create a resident credential with `hmac-secret`
2. Persist credential ID in the user’s key slot
3. Vault open / verification: Perform an assertion with `hmac-secret(challenge)`
4. YubiKey enforces **PIN + touch** and returns a 32-byte secret
5. Use the secret as the YubiKey factor for KEK composition

---

## 3. Public Interfaces (As Implemented)

### 3.1 YubiKeyManager Contract

`YubiKeyManager` is the stable façade for FIDO2 operations.

Implementation reference:
- `src/core/managers/YubiKeyManager.{h,cc}`

Key operations (simplified):

```cpp
class YubiKeyManager final {
public:
    struct YubiKeyInfo { /* version fields + supported_algorithms */ };
    struct ChallengeResponse { /* response bytes + success + error_message */ };

    bool initialize(bool enforce_fips = false) noexcept;
    std::vector<YubiKeyInfo> enumerate_devices() const noexcept;
    bool is_yubikey_present() const noexcept;
    std::optional<YubiKeyInfo> get_device_info() const noexcept;

    std::optional<std::vector<unsigned char>> create_credential(
        std::string_view user_id,
        std::string_view pin) noexcept;

    bool set_credential(std::span<const unsigned char> credential_id) noexcept;

    ChallengeResponse challenge_response(
        std::span<const unsigned char> challenge,
        YubiKeyAlgorithm algorithm = YubiKeyAlgorithm::HMAC_SHA256,
        bool require_touch = true,
        int timeout_ms = 15000,
        std::optional<std::string_view> pin = std::nullopt) noexcept;

    // Async wrappers for UI flows (callback delivered on GTK main thread)
    void create_credential_async(...);
    void challenge_response_async(...);
};
```

Notes:
- FIDO2 `hmac-secret` supports **HMAC-SHA256 only** in this implementation.
- `require_touch` cannot be disabled for FIDO2; it is accepted for API compatibility but is ignored by the authenticator.
- The `challenge` is accepted as **1–64 bytes** for compatibility with legacy callers.
       - If the challenge is **1–32 bytes**, it is copied into the 32-byte hmac-secret salt buffer (remaining bytes zero).
       - If the challenge is **33–64 bytes**, it is hashed with SHA-256 to produce the 32-byte hmac-secret salt.
- libfido2 access is globally serialized via a process-wide mutex to avoid concurrency issues.

### 3.2 Vault Integration (High-Level)

Two-factor enrollment / usage is per-user (key slot scoped):

**Enrollment (two touches):**
1. Create credential (`makeCredential` with `hmac-secret`, resident key enabled)
2. Perform `hmac-secret` assertion using the user challenge
3. Persist:
   - `KeySlot::yubikey_credential_id`
   - `KeySlot::yubikey_challenge` (32 bytes)
   - `KeySlot::yubikey_encrypted_pin` (encrypted using password-derived KEK only)

**Vault open / re-auth:**
1. Derive password-only KEK
2. Decrypt stored PIN using password-only KEK (see circular dependency note)
3. Load `KeySlot::yubikey_credential_id` via `YubiKeyManager::set_credential()`
4. Perform `hmac-secret` assertion using `KeySlot::yubikey_challenge`
5. Combine KEK with the returned 32-byte secret

### 3.3 Circular Dependency Avoidance

To avoid a circular dependency during vault opening, the YubiKey PIN is encrypted with the **password-derived KEK only**.
This allows the PIN to be decrypted *before* requesting the YubiKey factor, so YubiKey authentication does not depend on itself.

---

## 4. Data Structures (As Implemented)

### 4.1 Per-User Key Slot Fields

YubiKey enrollment data is stored per user in the V2 key slot:
- `yubikey_enrolled` (bool)
- `yubikey_challenge` (`std::array<uint8_t, 32>`)
- `yubikey_serial` (string; informational/audit)
- `yubikey_encrypted_pin` (`std::vector<uint8_t>`; encrypted under password-derived KEK)
- `yubikey_credential_id` (`std::vector<uint8_t>`; used with `allow_cred`)

### 4.2 Vault Security Policy

The vault policy selects the YubiKey algorithm ID; for FIDO2 hmac-secret this is effectively **SHA-256 only**.

Note: some policy-level YubiKey challenge fields exist for legacy/backward-compatibility and are not required by the current FIDO2 flow.

---

## 5. FIPS-140-3 Compliance

### 5.1 Approved Components

| Component | Algorithm | FIPS Status | Notes |
|-----------|-----------|-------------|-------|
| HMAC | SHA-256 | ✅ Approved | NIST SP 800-107r1 |
| Key Derivation | PBKDF2-SHA512 | ✅ Approved | SP 800-132 |
| Encryption | AES-256-GCM | ✅ Approved | FIPS 197 |
| RNG | OpenSSL DRBG | ✅ Approved | SP 800-90A |
| YubiKey Challenge | HMAC-SHA256 | ✅ Approved | Via FIDO2 |

### 5.2 FIPS Mode Enforcement

FIPS enforcement is implemented as a **software policy gate**:
- `YubiKeyManager::initialize(enforce_fips)` records whether FIPS-only behavior is required.
- The algorithm is validated during `challenge_response()`.

For FIDO2 `hmac-secret`, the implementation supports **HMAC-SHA256 only**. As a result, in practice:
- non-SHA256 algorithms are rejected regardless of FIPS mode, and
- enabling FIPS mode primarily ensures we never attempt to route to legacy/non-approved algorithms.

### 5.3 Security Properties

**FIPS-140-3 Requirements Met:**
- ✅ Approved algorithms only (SHA-256, AES-256-GCM, PBKDF2-SHA512)
- ✅ Key zeroization (OPENSSL_cleanse)
- ✅ Self-tests (unit tests verify crypto operations)
- ✅ Error states (proper error handling)
- ✅ Physical security (YubiKey tamper-resistant)

**Additional Security:**
- ✅ Multi-factor authentication (something you have + something you know)
- ✅ PIN protection (YubiKey-enforced; application validates 4–63 characters)
- ✅ Touch requirement (anti-malware, user presence)
- ✅ Replay protection (FIDO2 counter mechanism)

---

## 6. Implementation Status (As of 2026-02-22)

Implemented end-to-end:
- FIDO2 device discovery (cached) and global libfido2 initialization/serialization
- Resident credential creation (`makeCredential` with `hmac-secret`)
- Assertion (`getAssertion`) using `hmac-secret` (PIN + touch enforced by device)
- Per-user persistence of credential ID, user challenge, and encrypted PIN
- Async wrappers for UI flows (worker thread + GTK main-thread dispatch)

Key implementation files:
- `src/core/managers/YubiKeyManager.{h,cc}`
- `src/core/managers/yubikey/Fido2Global.h`
- `src/core/managers/yubikey/Fido2Discovery.h`
- `src/core/managers/yubikey/Fido2Protocol.h`
- `src/core/managers/yubikey/AsyncRunner.h`

Not implemented (by design / future work):
- Credential listing/deletion UI or administrative credential management
- Non-resident credential mode

---

## 7. Testing Strategy

### 7.1 Automated Tests

Guardrails:
- Characterization tests cover the stable behaviors expected of `YubiKeyManager`.
- Core unit/integration tests cover the vault enrollment/open flows that depend on YubiKey behavior.

Implementation references:
- `tests/test_yubikey_manager_characterization.cc`

Recommended workflow:
```bash
meson test -C build
```

### 7.2 Manual Testing Checklist

- Enumerate devices
- Enroll a user (two touches)
- Open a vault using stored credential ID + encrypted PIN
- Verify behavior when the device is absent or PIN is incorrect

**Expected Results:**
- All cryptographic operations use FIPS-approved algorithms
- SHA-1 completely unavailable in FIPS mode
- YubiKey operations enforce SHA-256 minimum

---

## 8. Migration Strategy

### 8.1 Backward Compatibility

**Old Vaults (PCSC SHA-1):**
- ❌ Cannot be opened (SHA-1 removed)
- ⚠️ Breaking change documented in CHANGELOG
- 📝 Migration guide provided

**New Vaults:**
- ✅ Use FIDO2 hmac-secret with SHA-256
- ✅ FIPS-140-3 compliant from day one

### 8.2 User Communication

**CHANGELOG Entry:**
```markdown
### Breaking Changes

- **YubiKey Support**: Migrated from PCSC OTP to FIDO2 hmac-secret
  - **Old vaults with YubiKey SHA-1 are no longer supported**
  - **Action required**: Create new vaults with FIDO2
  - **Benefit**: Full FIPS-140-3 compliance (SHA-256)
  - **Setup**: YubiKey PIN required (set with `ykman fido access change-pin`)

### New Features

- ✅ FIDO2 YubiKey support with HMAC-SHA256
- ✅ PIN protection for YubiKey operations
- ✅ Touch requirement for enhanced security
- ✅ WebAuthn/FIDO2 standard compliance
```

**README.md Update:**
```markdown
### YubiKey Setup (FIDO2)

KeepTower uses FIDO2 hmac-secret for YubiKey support.

**Requirements:**
- YubiKey 5 Series (firmware 5.0+)
- PIN configured (4–63 characters)

**Setup:**
```bash
# Check YubiKey status
ykman fido info

# Set PIN if not configured
ykman fido access change-pin

# Verify FIDO2 support
ykman fido credentials list
```

---

## 9. Security Considerations

### 9.1 Threat Model

**Threats Mitigated:**
- ✅ Stolen password (YubiKey required)
- ✅ Malware keylogging (touch required)
- ✅ Replay attacks (FIDO2 counter)
- ✅ Weak algorithms (SHA-256 enforced)

**Threats NOT Mitigated:**
- ❌ Physical theft of YubiKey + password (use disk encryption)
- ❌ Evil maid attacks (BIOS/bootloader security needed)
- ❌ Sophisticated malware with HID injection (OS-level protection)

### 9.2 PIN Security

**Implementation:**
- Length: 4–63 characters (device-enforced; application validates format)
- Retry limit: 8 attempts (hardware enforced)
- PIN storage: Persisted per-user as `KeySlot::yubikey_encrypted_pin` (encrypted under password-derived KEK only)
- PIN verification: Device-local (PIN is sent to libfido2 for UV and never used as application crypto material)

**User Guidelines:**
- Use random 6-digit PIN
- Do not reuse PINs across devices
- Store PIN securely (password manager)

### 9.3 Credential Storage

**Options Analysis:**

| Option | Pros | Cons | Recommendation |
|--------|------|------|----------------|
| Resident Key | Discoverable on device; standard passkey model | Limited to ~25 per YubiKey | ✅ **Implemented** |
| Non-Resident | Potentially more than ~25 credentials | More complex; must store credential ID and manage lifecycle | Not implemented |

Notes:
- KeepTower still stores the credential ID even for resident credentials to avoid enumeration and to select the exact credential during assertions.

---

## 10. Dependencies

### 10.1 Library Requirements

**libfido2:**
- Version: >= 1.13.0 (FIPS-compliant HMAC-SHA256 support)
- License: BSD-2-Clause (compatible with GPL-3.0)
- Maintained by: Yubico
- Repository: https://github.com/Yubico/libfido2

**Platform Support:**
- Ubuntu 22.04+: `libfido2-dev`
- Fedora 40+: `libfido2-devel`
- macOS: `brew install libfido2`
- Windows: vcpkg or prebuilt binaries

### 10.2 Build System

**meson.build:**
```meson
fido2_dep = dependency('libfido2', version: '>= 1.13.0', required: false)
```

**CI/CD:**
- GitHub Actions: Install `libfido2-dev`
- Flatpak: Include libfido2 runtime
- AppImage: Bundle libfido2.so

---

## 11. Performance Considerations

### 11.1 Expected Latency

| Operation | Time | Notes |
|-----------|------|-------|
| Device enumeration | 100ms | USB enumeration |
| Credential creation | 2-5s | User touch required |
| hmac-secret challenge | 1-2s | PIN + touch |
| PIN verification | 200ms | Local to device |

**Optimization:**
- Cache device handle (avoid re-enumeration)
- Async operations for long-running tasks
- Show progress indicators for user operations

### 11.2 Resource Usage

**Memory:**
- libfido2 context: ~100KB
- Credential data: ~256 bytes per credential
- Challenge/response buffers: 64 bytes

**Disk:**
- libfido2.so: ~200KB
- No additional data stored

---

## 12. Error Handling

### 12.1 Error Categories

```cpp
enum class YubiKeyError {
    None,
    DeviceNotFound,
    DeviceNotFIDO2,
    PinNotSet,
    PinIncorrect,
    PinLocked,
    UserCancelled,
    TouchTimeout,
    CredentialNotFound,
    CredentialFull,
    CommunicationError,
    UnsupportedAlgorithm,
    FIPSViolation
};
```

### 12.2 User-Facing Messages

**Error Messages:**
- "YubiKey not detected. Please insert your YubiKey."
- "Incorrect PIN. X attempts remaining."
- "YubiKey is locked. Please contact support."
- "Touch your YubiKey to continue..."
- "Operation cancelled by user."

**Recovery Actions:**
- Retry with correct PIN
- Reset PIN (requires existing PIN or removal counter = 0)
- Use different YubiKey
- Contact support (PIN locked after 8 failures)

---

## 13. Future Enhancements

### 13.1 Phase 2 Features (Post-v0.4.0)

**Multi-Device Support:**
- Register multiple YubiKeys per vault
- Fallback to backup YubiKey
- YubiKey A or YubiKey B

**Biometric Support:**
- YubiKey Bio series (fingerprint)
- Skip PIN if biometric verified

**Enterprise Features:**
- Centralized credential management
- Group policies for YubiKey requirements
- Audit logging

### 13.2 Standards Compliance

**Current:**
- FIDO2 / WebAuthn Level 1
- CTAP 2.0

**Future:**
- FIDO2.1 enhancements
- Passkey support
- Cross-platform credentials

---

## 14. References

### 14.1 Specifications

- [FIDO2 CTAP 2.0](https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html)
- [WebAuthn Level 1](https://www.w3.org/TR/webauthn-1/)
- [NIST FIPS 140-3](https://csrc.nist.gov/publications/detail/fips/140/3/final)
- [NIST SP 800-107r1](https://csrc.nist.gov/publications/detail/sp/800-107/rev-1/final)

### 14.2 Implementation Guides

- [libfido2 Documentation](https://developers.yubico.com/libfido2/)
- [YubiKey FIDO2 Guide](https://developers.yubico.com/FIDO2/)
- [FIDO2 hmac-secret Extension](https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#sctn-hmac-secret-extension)

### 14.3 Related Documentation

- [KeepTower CONTRIBUTING.md](../../CONTRIBUTING.md)
- [FIPS Compliance Issue](../audits/FIPS_YUBIKEY_COMPLIANCE_ISSUE.md)
- [TODO YubiKey SHA-256](../TODO_YUBIKEY_SHA256.md)

---

## 15. Approval & Sign-off

**Prepared by:** GitHub Copilot + tjdeveng
**Reviewed by:** _Pending_
**Approved by:** _Pending_
**Date:** 2026-01-02

**Next Steps:**
1. Review this specification
2. Approve for implementation
3. Begin Phase 1 development
4. Track progress in GitHub Issues

---

**END OF SPECIFICATION**
