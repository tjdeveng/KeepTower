# CRITICAL: YubiKey HMAC-SHA1 FIPS-140-3 Compliance Violation

**Date:** 2026-01-01
**Severity:** CRITICAL
**Status:** IDENTIFIED - REQUIRES IMMEDIATE ACTION
**Impact:** Application cannot be certified as FIPS-140-3 compliant with current YubiKey implementation

---

## Executive Summary

KeepTower's YubiKey challenge-response implementation uses **HMAC-SHA1**, which is **NOT** approved for FIPS-140-3 compliance. SHA-1 has been deprecated since 2011 due to collision vulnerabilities and is explicitly forbidden in FIPS 140-3 standards.

**Risk Level:** HIGH - Blocks FIPS certification, violates security compliance requirements

---

## Technical Details

### Current Implementation

**File:** `src/core/managers/YubiKeyManager.h`
```cpp
/**
 * @brief Result of a challenge-response operation
 */
struct ChallengeResponse {
    std::array<unsigned char, 20> response{};  ///< HMAC-SHA1 response (20 bytes) ❌
    bool success{false};
    std::string error_message{};
};

static inline constexpr size_t RESPONSE_SIZE{20};  ///< HMAC-SHA1 response size ❌
```

**Current Flow:**
1. Application generates random 64-byte challenge
2. YubiKey computes `HMAC-SHA1(secret_key, challenge)` → 20 bytes ❌
3. Response XORed with password-derived KEK
4. Combined key used for vault encryption

### FIPS-140-3 Requirement

**NIST SP 800-140B** (FIPS 140-3 Implementation Guidance):
- **Section 3.1:** Hash functions
  - **Approved:** SHA-256, SHA-384, SHA-512, SHA3-256, SHA3-384, SHA3-512
  - **Deprecated:** MD5, SHA-1
  - **Status:** SHA-1 use is **prohibited** for new implementations

**NIST SP 800-107 Rev. 1:**
> "SHA-1 shall not be used for digital signatures, HMAC-based key derivation, or any application where collision resistance is required."

### Impact Assessment

#### Affected Components
1. ✅ **V1 Vaults** - YubiKey challenge-response (HMAC-SHA1)
2. ✅ **V2 Vaults** - Per-user YubiKey enrollment (HMAC-SHA1)
3. ✅ **Export Authentication** - YubiKey verification (HMAC-SHA1)
4. ✅ **All Documentation** - References HMAC-SHA1 as "FIPS-compliant"

#### Compliance Status
| Component | Current Algorithm | FIPS Status | Required Action |
|-----------|------------------|-------------|-----------------|
| Password Key Derivation | PBKDF2-HMAC-SHA256 | ✅ Approved | None |
| Vault Encryption | AES-256-GCM | ✅ Approved | None |
| Key Wrapping | AES Key Wrap (RFC 3394) | ✅ Approved | None |
| YubiKey Challenge-Response | **HMAC-SHA1** | ❌ **PROHIBITED** | **MIGRATE TO SHA-256** |
| Random Number Generation | OpenSSL DRBG | ✅ Approved | None |

---

## YubiKey Hardware Capabilities

### HMAC-SHA256 Support

**YubiKey 5 Series (Firmware 5.0+):**
- ✅ Supports HMAC-SHA256 challenge-response
- ✅ 32-byte response (vs 20-byte for SHA-1)
- ✅ FIPS 140-2 Level 1 certified models available (YubiKey 5 FIPS)
- ✅ FIPS 140-3 Level 1 certification in progress

**YubiKey 4 Series (Firmware 4.0-4.3):**
- ❌ HMAC-SHA1 only
- ⚠️ End of life, no longer manufactured

### ykpers Library Support

**libykpers-1 (v1.20.0+):**
```c
// SHA-1 mode (current implementation)
int yk_challenge_response(YK_KEY *yk, uint8_t slot, int may_block,
                         unsigned int challenge_len, unsigned char *challenge,
                         unsigned int response_len, unsigned char *response);

// SHA-256 mode (available since 2019)
#define HMAC_SHA256_SIZE 32
// Use ykpers API with YK_FLAG_SHA256 configuration
```

**Status:** ✅ Library supports HMAC-SHA256, requires code migration

---

## Recommended Solution

### Phase 1: Immediate - Add FIPS Mode Check (1-2 days)

**Objective:** Prevent YubiKey usage when FIPS mode is enabled until migration complete

```cpp
// In YubiKeyManager::initialize()
#ifdef FIPS_MODE_ENABLED
    if (FIPS_mode()) {
        KeepTower::Log::error("YubiKey HMAC-SHA1 not FIPS-approved. "
                             "Disable FIPS mode or use password-only authentication.");
        return false;
    }
#endif
```

**Impact:**
- YubiKey features disabled in FIPS mode
- Users warned at vault creation/opening
- Prevents non-compliant operation

### Phase 2: Migration - Implement HMAC-SHA256 (1-2 weeks)

#### 2.1 Update YubiKeyManager

```cpp
enum class YubiKeyHashAlgorithm {
    SHA1,   // Legacy, non-FIPS
    SHA256  // FIPS-approved
};

struct ChallengeResponse {
    std::array<unsigned char, 32> response{};  // SHA-256 = 32 bytes
    YubiKeyHashAlgorithm algorithm{YubiKeyHashAlgorithm::SHA256};
    bool success{false};
    std::string error_message{};
};

// New method with algorithm selection
[[nodiscard]] ChallengeResponse challenge_response_sha256(
    std::span<const uint8_t> challenge,
    bool may_block,
    int timeout_ms = DEFAULT_TIMEOUT_MS
) noexcept;
```

#### 2.2 Update Vault Format

**V2 Vaults - Add Hash Algorithm Field:**
```cpp
struct KeySlot {
    // ... existing fields ...

    // New field for hash algorithm
    enum class HashAlgorithm : uint8_t {
        HMAC_SHA1 = 0,   // Legacy
        HMAC_SHA256 = 1  // FIPS-approved
    };
    HashAlgorithm yubikey_hash_algorithm = HashAlgorithm::HMAC_SHA256;

    // Challenge/response size depends on algorithm
    std::vector<uint8_t> yubikey_challenge;  // 64 bytes
    // Response: 20 bytes (SHA1) or 32 bytes (SHA256)
};
```

#### 2.3 Backward Compatibility

**Detection:**
```cpp
// Detect algorithm based on enrolled response size
if (slot.yubikey_response.size() == 20) {
    algorithm = HMAC_SHA1;  // Legacy vault
} else if (slot.yubikey_response.size() == 32) {
    algorithm = HMAC_SHA256;  // FIPS-compliant vault
}
```

**FIPS Mode Enforcement:**
```cpp
if (FIPS_mode() && algorithm == HMAC_SHA1) {
    return std::unexpected(VaultError::FIPSViolation);
}
```

#### 2.4 Migration Tool

```bash
# Command-line tool for migrating existing vaults
./keeptower --migrate-yubikey-sha256 /path/to/vault.vault

Steps:
1. Open vault with old YubiKey (HMAC-SHA1)
2. Verify user can authenticate
3. Prompt user to touch YubiKey for new HMAC-SHA256 enrollment
4. Re-wrap DEK with new HMAC-SHA256 response
5. Update key slot metadata
6. Save vault with new format
```

### Phase 3: Documentation Update (1 day)

**Files to Update:**
- README.md
- CONTRIBUTING.md (FIPS section)
- docs/user/*.md
- resources/help/*.html
- All references to "HMAC-SHA1"

**New User Guidance:**
```markdown
### YubiKey FIPS Compliance

**FIPS-140-3 Mode:**
- Requires YubiKey 5 Series with firmware 5.0+
- Uses HMAC-SHA256 challenge-response (FIPS-approved)
- Legacy SHA-1 vaults cannot be opened in FIPS mode

**YubiKey Setup:**
```bash
# Program YubiKey with HMAC-SHA256 (slot 2)
ykpersonalize -2 -ochal-resp -ochal-hmac -ohmac-sha256 -oserial-api-visible

# Verify configuration
ykinfo -a
```
```

---

## Migration Timeline

| Phase | Duration | Effort | Priority |
|-------|----------|--------|----------|
| 1. FIPS Mode Check | 1-2 days | Low | **CRITICAL** |
| 2. HMAC-SHA256 Implementation | 1-2 weeks | High | High |
| 3. Documentation Update | 1 day | Low | High |
| 4. Testing & Validation | 3-5 days | Medium | High |
| **Total** | **2-3 weeks** | **Medium-High** | **CRITICAL** |

---

## Testing Requirements

### Unit Tests
```cpp
TEST(YubiKeyManager, ChallengeResponseSHA256_FIPSCompliant) {
    YubiKeyManager yk;
    ASSERT_TRUE(yk.initialize());

    std::array<uint8_t, 64> challenge = generate_random_challenge();
    auto result = yk.challenge_response_sha256(challenge, true);

    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.response.size(), 32);  // SHA-256 = 32 bytes
    EXPECT_EQ(result.algorithm, YubiKeyHashAlgorithm::SHA256);
}

TEST(YubiKeyManager, LegacySHA1_RejectedInFIPSMode) {
    #ifdef FIPS_MODE_ENABLED
    FIPS_mode_set(1);

    YubiKeyManager yk;
    EXPECT_FALSE(yk.initialize());  // Should fail in FIPS mode

    FIPS_mode_set(0);
    #endif
}
```

### Integration Tests
1. Create V2 vault with HMAC-SHA256 YubiKey
2. Open vault in FIPS mode
3. Export vault in FIPS mode
4. Add user with HMAC-SHA256 YubiKey
5. Migrate legacy HMAC-SHA1 vault to SHA-256

### Hardware Tests
- YubiKey 5 Series (firmware 5.x)
- YubiKey 5 FIPS Edition
- Multiple YubiKeys (backup keys)

---

## Risk Assessment

### If Not Fixed

**Compliance Risks:**
- ❌ Cannot obtain FIPS-140-3 certification
- ❌ Violates federal/government security requirements
- ❌ May violate contractual compliance obligations
- ❌ Security audits will flag as critical vulnerability

**Security Risks:**
- ⚠️ SHA-1 collision attacks (though HMAC-SHA1 still considered secure for HMAC)
- ⚠️ Non-compliance with modern cryptographic standards
- ⚠️ Future deprecation in OpenSSL FIPS provider

**Business Risks:**
- Cannot market as "FIPS-140-3 compliant"
- Government/enterprise customers cannot use application
- Reputational damage if discovered post-deployment

### Mitigation Priority

**Priority Level:** 🔴 **CRITICAL - P0**

**Justification:**
1. Blocks FIPS certification
2. Violates documented security policy
3. Relatively straightforward to fix (2-3 weeks)
4. Hardware supports required functionality
5. Affects all YubiKey users

---

## Recommendation

**Immediate Action Required:**

1. **Today (2026-01-01):**
   - Add FIPS mode check to block YubiKey usage
   - Update documentation to remove "FIPS-approved HMAC-SHA1" claims
   - File internal security issue

2. **This Week:**
   - Implement HMAC-SHA256 support in YubiKeyManager
   - Update vault format to support algorithm field
   - Create backward compatibility layer

3. **This Month:**
   - Complete migration tool
   - Update all documentation
   - Test with YubiKey 5 Series hardware
   - Release patch version with fix

4. **Future:**
   - Consider SHA3-256 for next major version
   - Evaluate passwordless FIDO2/WebAuthn support

---

## References

1. **NIST SP 800-140B** - FIPS 140-3 Implementation Guidance
   https://csrc.nist.gov/publications/detail/sp/800-140b/final

2. **NIST SP 800-107 Rev. 1** - Recommendation for Applications Using Approved Hash Algorithms
   https://csrc.nist.gov/publications/detail/sp/800-107/rev-1/final

3. **FIPS 140-3** - Security Requirements for Cryptographic Modules
   https://csrc.nist.gov/publications/detail/fips/140/3/final

4. **YubiKey 5 Series Technical Manual**
   https://docs.yubico.com/hardware/yubikey/yk-tech-manual/

5. **OpenSSL FIPS Provider Documentation**
   https://www.openssl.org/docs/fips.html

---

**Document Owner:** Security Team
**Last Updated:** 2026-01-01
**Next Review:** Upon completion of Phase 1 (FIPS check)

**CRITICAL: This issue must be resolved before claiming FIPS-140-3 compliance.**
