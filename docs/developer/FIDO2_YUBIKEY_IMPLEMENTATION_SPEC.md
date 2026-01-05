# FIDO2 YubiKey Implementation Specification

**Version:** 1.0
**Date:** 2026-01-02
**Status:** Specification Phase
**Target Release:** v0.4.0

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

### 2.1 Credential Types

**Option A: Resident Credential (Discoverable)**
- Stored on YubiKey (max 25 credentials)
- No credential ID needed
- Better UX (enumerate credentials)
- Requires PIN
- **Recommended for KeepTower**

**Option B: Non-Resident Credential**
- Credential ID stored in vault header
- Unlimited credentials per YubiKey
- Requires credential ID for each vault
- More complex vault format

### 2.2 FIDO2 hmac-secret Extension

The FIDO2 `hmac-secret` extension provides challenge-response functionality:

```
Input:  salt1 (32 bytes) - random challenge
        salt2 (32 bytes) - optional second salt

Output: HMAC-SHA256(credential_secret, salt1) → 32 bytes
        HMAC-SHA256(credential_secret, salt2) → 32 bytes (optional)
```

**How it works:**
1. Vault creation: Generate credential with hmac-secret extension
2. Store credential ID (if non-resident) or relying party ID
3. Vault opening: Request assertion with hmac-secret(challenge)
4. YubiKey: Prompt for PIN + touch, compute HMAC-SHA256
5. Return 32-byte response
6. Use response to derive KEK (same as current implementation)

---

## 3. API Design

### 3.1 YubiKeyManager Public Interface

```cpp
class YubiKeyManager {
public:
    // Initialization
    bool initialize(bool enforce_fips) noexcept;
    bool is_available() const noexcept;

    // Device enumeration
    struct YubiKeyInfo {
        std::string device_path;
        std::string serial_number;
        uint8_t version_major;
        uint8_t version_minor;
        uint8_t version_build;
        bool supports_hmac_secret;
        bool has_pin_set;
        int remaining_credentials;  // For resident keys
        std::vector<YubiKeyAlgorithm> supported_algorithms;
    };
    std::vector<YubiKeyInfo> enumerate_devices() const noexcept;
    YubiKeyInfo get_device_info() const noexcept;

    // Credential management (for vault setup)
    struct CredentialOptions {
        std::string relying_party_id;      // e.g., "com.example.keeptower"
        std::string relying_party_name;    // e.g., "KeepTower"
        std::string user_id;               // Vault identifier
        std::string user_name;             // User-friendly name
        bool resident_key;                 // true = discoverable
        bool require_touch;                // Always true for security
    };

    struct Credential {
        std::vector<uint8_t> credential_id;
        std::string relying_party_id;
        std::string user_name;
        bool success;
        std::string error_message;
    };

    Credential create_credential(const CredentialOptions& options) noexcept;
    bool delete_credential(const std::vector<uint8_t>& credential_id) noexcept;
    std::vector<Credential> list_credentials(const std::string& rp_id) noexcept;

    // Challenge-response (for vault operations)
    struct ChallengeResponse {
        std::array<uint8_t, 32> response{};  // HMAC-SHA256 output
        YubiKeyAlgorithm algorithm{YubiKeyAlgorithm::HMAC_SHA256};
        bool success{false};
        std::string error_message;
        bool user_cancelled{false};  // User cancelled PIN entry
    };

    ChallengeResponse hmac_secret_challenge(
        const std::vector<uint8_t>& credential_id,
        std::span<const unsigned char> challenge,
        const std::string& pin,
        int timeout_ms = 30000
    ) noexcept;

    // PIN management
    bool has_pin() const noexcept;
    bool verify_pin(const std::string& pin) noexcept;
    int get_pin_retries() const noexcept;

    // Error handling
    std::string get_last_error() const noexcept;
    void clear_error() noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_initialized{false};
    bool m_fips_mode{false};
    std::string m_last_error;
};
```

### 3.2 Integration with VaultManager

**Vault Creation Flow:**

```cpp
// In VaultManager::create_vault_v2()
if (policy.require_yubikey) {
    YubiKeyManager yk_manager;
    yk_manager.initialize(m_fips_mode);

    // Prompt user for PIN if not set
    std::string pin = prompt_yubikey_pin();

    // Create credential for this vault
    YubiKeyManager::CredentialOptions options{
        .relying_party_id = "com.example.keeptower",
        .relying_party_name = "KeepTower",
        .user_id = vault_uuid,  // Unique per vault
        .user_name = username + "@" + vault_name,
        .resident_key = true,   // Discoverable
        .require_touch = true
    };

    auto credential = yk_manager.create_credential(options);
    if (!credential.success) {
        return std::unexpected(VaultError::YubiKeyEnrollmentFailed);
    }

    // Store credential ID in vault header (if non-resident)
    header.yubikey_credential_id = credential.credential_id;

    // Get HMAC-SHA256 response for challenge
    auto response = yk_manager.hmac_secret_challenge(
        credential.credential_id,
        policy.yubikey_challenge,
        pin
    );

    if (!response.success) {
        return std::unexpected(VaultError::YubiKeyChallengeResponseFailed);
    }

    // Combine with KEK (existing logic)
    auto final_kek = KeyWrapping::combine_with_yubikey_v2(
        kek, response.response, YubiKeyAlgorithm::HMAC_SHA256
    );
}
```

**Vault Opening Flow:**

```cpp
// In VaultManager::open_vault_v2()
if (header.security_policy.require_yubikey) {
    YubiKeyManager yk_manager;
    yk_manager.initialize(m_fips_mode);

    // Prompt user for PIN
    std::string pin = prompt_yubikey_pin();

    // Get HMAC-SHA256 response
    auto response = yk_manager.hmac_secret_challenge(
        header.yubikey_credential_id,
        header.security_policy.yubikey_challenge,
        pin
    );

    if (!response.success) {
        if (response.user_cancelled) {
            return std::unexpected(VaultError::UserCancelled);
        }
        return std::unexpected(VaultError::YubiKeyChallengeResponseFailed);
    }

    // Combine with KEK
    auto final_kek = KeyWrapping::combine_with_yubikey_v2(
        kek, response.response, YubiKeyAlgorithm::HMAC_SHA256
    );
}
```

---

## 4. Data Structures

### 4.1 VaultHeaderV2 Changes

Add credential storage to VaultHeaderV2:

```cpp
struct VaultHeaderV2 {
    // ... existing fields ...

    // FIDO2 credential data (new)
    std::vector<uint8_t> yubikey_credential_id;  // Empty if resident key
    std::string yubikey_relying_party_id;        // "com.example.keeptower"

    // Serialization
    std::vector<uint8_t> serialize() const;
    static std::optional<VaultHeaderV2> deserialize(const std::vector<uint8_t>& data);
};
```

### 4.2 VaultSecurityPolicy Changes (Already Has Algorithm Field)

Current structure already supports FIPS algorithms:

```cpp
struct VaultSecurityPolicy {
    bool require_yubikey = false;
    uint8_t yubikey_algorithm = 0x02;  // 0x02 = HMAC-SHA256
    std::array<uint8_t, 64> yubikey_challenge = {};
    // ... existing fields ...
};
```

**No changes needed** - SHA-256 is already the default.

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

```cpp
bool YubiKeyManager::initialize(bool enforce_fips) noexcept {
    m_fips_mode = enforce_fips;

    if (m_fips_mode) {
        // Verify FIDO2 hmac-secret supports SHA-256
        // Reject devices that only support SHA-1
        Log::info("YubiKey: FIPS-140-3 mode enabled (SHA-256 minimum)");
    }

    return true;
}
```

### 5.3 Security Properties

**FIPS-140-3 Requirements Met:**
- ✅ Approved algorithms only (SHA-256, AES-256-GCM, PBKDF2-SHA512)
- ✅ Key zeroization (OPENSSL_cleanse)
- ✅ Self-tests (unit tests verify crypto operations)
- ✅ Error states (proper error handling)
- ✅ Physical security (YubiKey tamper-resistant)

**Additional Security:**
- ✅ Multi-factor authentication (something you have + something you know)
- ✅ PIN protection (6-digit minimum, device-locked after retries)
- ✅ Touch requirement (anti-malware, user presence)
- ✅ Replay protection (FIDO2 counter mechanism)

---

## 6. Implementation Roadmap

### 6.1 Phase 1: Core FIDO2 Integration (Priority: HIGH)

**Tasks:**
1. ✅ Update build system (meson.build, CI/CD) - **DONE**
2. Implement YubiKeyManager::Impl using libfido2
3. Implement device enumeration
4. Implement credential creation (resident keys)
5. Implement hmac-secret challenge-response
6. Update YubiKeyAlgorithm.h (already done)

**Estimated Time:** 1 day
**Files Modified:**
- `src/core/managers/YubiKeyManager.cc` (complete rewrite, ~600 lines)
- `src/core/managers/YubiKeyManager.h` (API update)

### 6.2 Phase 2: VaultManager Integration (Priority: HIGH)

**Tasks:**
1. Update VaultHeaderV2 serialization
2. Add credential ID storage
3. Integrate FIDO2 into create_vault_v2()
4. Integrate FIDO2 into open_vault_v2()
5. Handle credential lifecycle

**Estimated Time:** 4 hours
**Files Modified:**
- `src/core/VaultManagerV2.cc` (~200 lines changed)
- `src/core/MultiUserTypes.h` (header structure)
- `src/core/MultiUserTypes.cc` (serialization)

### 6.3 Phase 3: User Interface (Priority: MEDIUM)

**Tasks:**
1. Create PIN entry dialog
2. Add YubiKey status indicators
3. Implement PIN caching (optional, with timeout)
4. Add credential management UI
5. User prompts for touch

**Estimated Time:** 6 hours
**Files Modified:**
- `src/ui/dialogs/YubiKeyPinDialog.cc` (new)
- `src/ui/dialogs/YubiKeyPinDialog.h` (new)
- `src/ui/windows/MainWindow.cc` (status indicators)

### 6.4 Phase 4: Testing (Priority: HIGH)

**Tasks:**
1. Update existing YubiKey tests
2. Add FIDO2-specific tests
3. Add credential lifecycle tests
4. Test error conditions
5. Test FIPS mode enforcement
6. Integration testing with real YubiKey

**Estimated Time:** 4 hours
**Files Modified:**
- `tests/test_yubikey_algorithms.cc` (updated)
- `tests/test_yubikey_fido2.cc` (new)
- `tests/test_vault_manager.cc` (updated)

### 6.5 Phase 5: Documentation (Priority: MEDIUM)

**Tasks:**
1. Update README.md (YubiKey setup instructions)
2. Update INSTALL.md (dependencies)
3. Update FIPS compliance documentation
4. Create user guide for FIDO2 setup
5. API documentation

**Estimated Time:** 2 hours
**Files Modified:**
- `README.md`
- `INSTALL.md`
- `docs/user/YUBIKEY_FIPS_SETUP.md`
- `docs/audits/FIPS_YUBIKEY_COMPLIANCE_ISSUE.md` (close)

### 6.6 Total Estimated Time

**Development:** 1.5 days
**Testing:** 0.5 days
**Documentation:** 0.25 days
**Total:** ~2 days (focused work)

---

## 7. Testing Strategy

### 7.1 Unit Tests

**Test Coverage Requirements (per CONTRIBUTING.md):**
- Minimum 80% line coverage
- All public APIs tested
- Edge cases and error conditions
- FIPS mode enforcement

**New Test Files:**
```cpp
// tests/test_yubikey_fido2.cc
TEST(YubiKeyFIDO2Test, DeviceEnumeration)
TEST(YubiKeyFIDO2Test, CredentialCreation_Resident)
TEST(YubiKeyFIDO2Test, CredentialCreation_NonResident)
TEST(YubiKeyFIDO2Test, HmacSecretChallenge_SHA256)
TEST(YubiKeyFIDO2Test, PinVerification)
TEST(YubiKeyFIDO2Test, PinRetries)
TEST(YubiKeyFIDO2Test, CredentialDeletion)
TEST(YubiKeyFIDO2Test, ListCredentials)
TEST(YubiKeyFIDO2Test, FIPSModeEnforcement)
TEST(YubiKeyFIDO2Test, ErrorHandling_NoDevice)
TEST(YubiKeyFIDO2Test, ErrorHandling_WrongPin)
TEST(YubiKeyFIDO2Test, ErrorHandling_UserCancelled)
```

**Updated Test Files:**
```cpp
// tests/test_vault_manager.cc
TEST(VaultManagerTest, CreateVault_WithYubiKeyFIDO2)
TEST(VaultManagerTest, OpenVault_WithYubiKeyFIDO2)
TEST(VaultManagerTest, YubiKeyFIDO2_CredentialStored)
TEST(VaultManagerTest, YubiKeyFIDO2_WrongPin_Fails)
```

### 7.2 Integration Tests

**Manual Testing Checklist:**
- [ ] Enumerate YubiKeys
- [ ] Set/change YubiKey PIN
- [ ] Create vault with YubiKey (resident credential)
- [ ] Create vault with YubiKey (non-resident credential)
- [ ] Open vault with correct PIN
- [ ] Open vault with wrong PIN (should fail after 8 attempts)
- [ ] Touch requirement (verify prompt appears)
- [ ] Multiple vaults on same YubiKey
- [ ] Remove and re-insert YubiKey during operation
- [ ] FIPS mode enforcement (SHA-256 only)
- [ ] Credential management (list, delete)
- [ ] Performance (should be < 2 seconds for challenge-response)

### 7.3 FIPS Compliance Testing

**Required Tests:**
```bash
# Verify FIPS mode
./build/src/keeptower --fips-check

# Run FIPS test suite
meson test -C build test_security_features

# Verify only approved algorithms
meson test -C build test_yubikey_fido2
```

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
- PIN configured (6-8 digits)

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
- Minimum length: 6 digits (FIDO2 spec)
- Maximum length: 8 digits (YubiKey limitation)
- Retry limit: 8 attempts (hardware enforced)
- PIN storage: On YubiKey only (never transmitted or stored)
- PIN verification: Device-local

**User Guidelines:**
- Use random 6-digit PIN
- Do not reuse PINs across devices
- Store PIN securely (password manager)

### 9.3 Credential Storage

**Options Analysis:**

| Option | Pros | Cons | Recommendation |
|--------|------|------|----------------|
| Resident Key | No credential ID needed, simpler UX | Limited to 25 per YubiKey | ✅ **Recommended** |
| Non-Resident | Unlimited vaults per YubiKey | Must store credential ID | Use if >25 vaults |

**Recommendation:** Use **resident credentials** by default. Users with >25 vaults can use non-resident mode.

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
