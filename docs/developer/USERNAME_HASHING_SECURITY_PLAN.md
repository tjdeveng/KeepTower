# Username Hashing Security Enhancement Plan

**Issue ID:** SECURITY-2026-01-18-USERNAME-PLAINTEXT
**Severity:** HIGH
**Date:** 18 January 2026
**Status:** PLANNING PHASE

---

## Executive Summary

**Current Vulnerability:** User identifiers (usernames) are currently stored in plaintext within the vault's KeySlot structure. This presents a security risk where vault metadata can reveal user identities even without successful authentication.

**Proposed Solution:** Implement cryptographic hashing of usernames using FIPS-140-3 approved algorithms, with user-configurable hash algorithm preference (similar to clipboard timeout). Algorithm is selectable via Preferences → Security and applies to newly created vaults.

**Impact:** Enhancement to existing V2 vault format. Not a breaking change - V2 vaults can be upgraded to use hashed usernames by admin preference change. Maintains consistency with existing preference behavior (changes don't affect currently open vault).

---

## Current State Analysis

### Current Implementation

**Location:** `src/core/MultiUserTypes.h` - `KeySlot` structure

```cpp
struct KeySlot {
    bool active = false;
    std::string username;  // ⚠️ PLAINTEXT - SECURITY ISSUE
    std::array<uint8_t, 32> salt = {};
    std::array<uint8_t, 40> wrapped_dek = {};
    UserRole role = UserRole::STANDARD_USER;
    // ... additional fields
};
```

**Serialization:** `src/core/MultiUserTypes.cc`

```cpp
std::vector<uint8_t> KeySlot::serialize() const {
    // ...
    result.push_back(static_cast<uint8_t>(username.size()));
    result.insert(result.end(), username.begin(), username.end());  // ⚠️ Plaintext
    // ...
}
```

**Exposure Points:**
1. **Vault File:** Username stored in plaintext in FEC-protected header
2. **Memory:** Username string in clear during runtime
3. **Logs:** Potential username leakage in debug logs
4. **Backup Files:** Plaintext usernames in `.backup` files

### Security Implications

**Threat Model:**
- **Information Disclosure:** Attacker with vault file access can enumerate users
- **Targeted Attacks:** Knowing usernames enables focused password attacks
- **Privacy Violation:** User identities revealed without authentication
- **Compliance Risk:** May violate data protection regulations (GDPR, HIPAA)

**Attack Scenarios:**
1. **Stolen Vault File:** Attacker obtains encrypted vault, reads plaintext usernames
2. **Backup Exposure:** Vault backup leaked, user identities compromised
3. **Forensic Analysis:** Deleted vaults reveal user information
4. **Social Engineering:** Attacker uses known usernames for phishing

**Current Mitigations (Insufficient):**
- ✅ Vault data encrypted with AES-256-GCM
- ✅ Password protected with PBKDF2-HMAC-SHA256
- ❌ **No protection for username metadata**

---

## Proposed Solution: Cryptographic Username Hashing

### Design Goals

1. **FIPS-140-3 Compliance:** Use only NIST-approved algorithms
2. **User Preference:** Algorithm configurable via GSchema preferences (Preferences → Security)
3. **Admin Control:** Only administrators can upgrade vault's hash algorithm
4. **Non-Reversibility:** One-way hashing prevents username recovery
5. **Uniqueness:** Collision-resistant hashing ensures unique identifiers
6. **Performance:** Balance security with authentication speed
7. **Future-Proof:** Support post-quantum algorithms (SHA-3)
8. **Preference Consistency:** Changes don't affect currently open vault (follows existing pattern)

### Algorithm Options

#### Option 1: SHA-3 (Keccak) Family ✅ FIPS-140-3 APPROVED

**Algorithm:** SHA3-256, SHA3-384, SHA3-512
**FIPS Status:** ✅ Approved (FIPS 202, NIST SP 800-185)
**OpenSSL Support:** ✅ Available in OpenSSL 3.0+

**Advantages:**
- FIPS-140-3 approved cryptographic hash
- Quantum-resistant (Grover's algorithm provides only quadratic speedup)
- Different construction than SHA-2 (Keccak vs Merkle-Damgård)
- No known cryptanalytic weaknesses
- Fast computation (single-pass hashing)
- Deterministic output (same username → same hash)

**Disadvantages:**
- Not key-stretching (vulnerable to rainbow tables without salting)
- Requires additional salt management
- Simpler than password-based KDFs

**Parameters:**
```cpp
SHA3-256: 256-bit output (32 bytes)
SHA3-384: 384-bit output (48 bytes)
SHA3-512: 512-bit output (64 bytes)
```

**Recommended Variant:** SHA3-256 (balance of security and size)

#### Option 2: PBKDF2-HMAC-SHA256 ✅ FIPS-140-3 APPROVED

**Algorithm:** PBKDF2 (Password-Based Key Derivation Function 2)
**FIPS Status:** ✅ Approved (NIST SP 800-132)
**OpenSSL Support:** ✅ Available in OpenSSL 1.1+

**Advantages:**
- FIPS-140-3 approved key derivation function
- Computational cost adjustable (iteration count)
- Resistant to brute-force attacks (work factor)
- Already used in KeepTower for password hashing
- Salt-based (prevents rainbow tables)
- Wide industry adoption (proven track record)

**Disadvantages:**
- Slower than raw SHA-3 (intentional design)
- Vulnerable to GPU/ASIC attacks (compared to memory-hard functions)
- May be overkill for username hashing (not a password)

**Parameters:**
```cpp
Hash: SHA-256
Iterations: 10,000 (username hashing, less than password 100,000)
Salt: 32 bytes (same as password salt)
Output: 32 bytes (256 bits)
```

**Use Case:** Organizations requiring maximum FIPS compliance

#### Option 3: Argon2id ❌ NOT FIPS-140-3 APPROVED (User Option)

**Algorithm:** Argon2id (hybrid mode)
**FIPS Status:** ❌ **NOT APPROVED** (Winner of Password Hashing Competition)
**OpenSSL Support:** ❌ Not in OpenSSL (requires libargon2)

**Advantages:**
- Resistant to GPU/ASIC attacks (memory-hard)
- Winner of Password Hashing Competition (2015)
- Best-in-class resistance to brute force
- Configurable memory cost
- Industry best practice (OWASP recommendation)

**Disadvantages:**
- **NOT FIPS-140-3 approved** (non-compliance risk)
- Requires additional dependency (libargon2)
- Slower than SHA-3 or PBKDF2
- May block FIPS-only deployments

**Parameters:**
```cpp
Variant: Argon2id (hybrid of Argon2i and Argon2d)
Memory: 64 MB (m = 65536)
Iterations: 3 (t = 3)
Parallelism: 4 (p = 4)
Salt: 16 bytes minimum
Output: 32 bytes
```

**Use Case:** Maximum security for non-FIPS deployments

### Recommendation Matrix

| Requirement | SHA-3 | PBKDF2 | Argon2id |
|------------|-------|--------|----------|
| **FIPS-140-3 Compliance** | ✅ Yes | ✅ Yes | ❌ No |
| **Performance (Fast)** | ✅ Excellent | ⚠️ Moderate | ❌ Slow |
| **Brute-Force Resistance** | ⚠️ Good* | ✅ Very Good | ✅ Excellent |
| **Quantum Resistance** | ✅ Yes | ✅ Yes | ✅ Yes |
| **Dependency** | OpenSSL 3.0+ | OpenSSL 1.1+ | libargon2 |
| **NIST Approval** | ✅ FIPS 202 | ✅ SP 800-132 | ❌ None |
| **Default Choice** | ✅ **Recommended** | ⚠️ Alternative | ❌ Advanced |

*With proper salting

---

## Implementation Strategy
2 Vault Format Enhancement

**Approach:** Extend existing V2 format without version bump (maintains compatibility)

**Key Changes:**

1. **VaultSecurityPolicy Enhancement:** Add username hash algorithm field
2. **KeySlot Enhancement:** Add hashed username storage alongside plaintext (migration)
3. **GSchema Preferences:** Add username_hash_algorithm preference
4. **Preferences UI:** Admin-only algorithm selection in Security tab

#### Enhanced KeySlot Structure (V2.1)
#### KeySlot V3 Structure:

```cpp
struct KeySlot {  // Existing V2 structure, enhanced
    bool active = false;

    // Username storage (MODIFIED)
    std::string username;  // Plaintext (deprecated, used during migration only)

    // Username hashing (NEW - added to existing V2 format)
    std::array<uint8_t, 64> username_hash = {};  // Max size for SHA3-512
    uint8_t username_hash_size = 0;              // 0 = plaintext mode, >0 = hashed
    std::array<uint8_t, 16> username_salt = {};  // Optional salt for rainbow table protection

    // Existing fields (unchanged)
    std::array<uint8_t, 32> password_salt = {};  // For password hashing
    std::array<uint8_t, 40> wrapped_dek = {};
    UserRole role = UserRole::STANDARD_USER;
    bool must_change_password = false;
    int64_t password_changed_at = 0;
    int64_t last_login_at = 0;

    // YubiKey fields (unchanged)
    bool yubikey_enrolled = false;
    std::array<uint8_t, 32> yubikey_challenge = {};
    std::string yubikey_serial;
    int64_t yubikey_enrolled_at = 0;
    std::vector<uint8_t> yubikey_encrypted_pin;
    std::vector<uint8_t> yubikey_credential_id;

    // Password history (unchanged)
    std::vector<PasswordHistoryEntry> password_history;
};Enhancement:

```cpp
struct VaultSecurityPolicy {  // Enhanced existing V2 structure
    // Existing V2 fields (unchanged)
    bool require_yubikey = false;
    uint8_t yubikey_algorithm;
    uint32_t min_password_length = 12;
    uint32_t pbkdf2_iterations = 100000;
    uint32_t password_history_depth = 5;
    std::array<uint8_t, 64> yubikey_challenge = {};
    legacy mode, NOT RECOMMENDED, default for compatibility)
     * - 0x01: SHA3-256 (FIPS-approved, recommended)
     * - 0x02: SHA3-384 (FIPS-approved)
     * - 0x03: SHA3-512 (FIPS-approved)
     * - 0x04: PBKDF2-HMAC-SHA256 (FIPS-approved, high security)
     * - 0x05: Argon2id (NOT FIPS-approved, maximum security)
     *
     * @note Set from GSchema preference at vault creation
     * @note Admin can upgrade existing vault algorithm via Preferences → Security
     * Defines the cryptographic hash algorithm used for username storage.
     * This setting applies to ALL key slots in the vault.
     *
     * Values:
     * - 0x00: Plaintext (V2 legacy, NOT RECOMMENDED)
     * - 0x01: SHA3-256 (DEFAULT, FIPS-approved)
     * - 0x02: SHA3-384 (FIPS-approved)
     * - 0x03: SHA3-512 (FIPS-approved)
     * - 0x04: PBKDF2-HMAC-SHA256 (FIPS-approved, high security)
     * - 0x05: Argon2id (NOT FIPS-approved, maximum security)
     *
     * @note Cannot be changed after vault creation
     * @note FIPS mode restricts to 0x01-0x04 only
     */
    uint8_t username_hash_algorithm = 0x01;  // Default: SHA3-256

    /**
     * @brief PBKDF2 iterations for username hashing (if using PBKDF2)
     *
     * Only used when username_hash_algorithm = 0x04.
     * Default: 10,000 (less than password hashing)
     *
     * @note Username hashing doesn't need password-level security
     */
    uint32_t username_pbkdf2_iterations = 10000;

    /**
     * @brief Argon2 memory cost for username hashing (if using Argon2)
     *
     * Only used when username_hash_algorithm = 0x05.
     * Default: 65536 KB (64 MB)
     */
    uint32_t username_argon2_memory_kb = 65536;

    /**
     * @brief Argon2 time cost (iterations) for username hashing
     *
     * Only used when username_hash_algorithm = 0x05.
     * Default: 3
     */
    uint32_t username_argon2_iterations = 3;
};
```

### Hashing Implementation

#### Interface Design:

**File:** `src/core/services/UsernameHashService.h` (NEW)

```cpp
namespace KeepTower {

/**
 * @brief Username hashing service for secure user identifier storage
 *
 * Provides cryptographic hashing of usernames to prevent information
 * disclosure in vault files. Supports multiple FIPS-approved algorithms
 * plus Argon2id for maximum security in non-FIPS deployments.
 */
class UsernameHashService {
public:
    /**
     * @brief Username hashing algorithm identifiers
     */
    enum class Algorithm : uint8_t {
        PLAINTEXT_LEGACY = 0x00,  // V2 compatibility only
        SHA3_256 = 0x01,          // Default, FIPS-approved
        SHA3_384 = 0x02,          // FIPS-approved
        SHA3_512 = 0x03,          // FIPS-approved
        PBKDF2_SHA256 = 0x04,     // FIPS-approved
        ARGON2ID = 0x05           // NOT FIPS-approved
    };

    /**
     * @brief Hash a username using the specified algorithm
     *
     * @param username Plaintext username (UTF-8)
     * @param algorithm Hashing algorithm to use
     * @param salt Optional salt (required for PBKDF2/Argon2, recommended for SHA-3)
     * @param iterations PBKDF2 iteration count (default: 10,000)
     * @return Hashed username bytes, or error
     */
    static VaultResult<std::vector<uint8_t>> hash_username(
        const std::string& username,
        Algorithm algorithm,
        const std::vector<uint8_t>& salt = {},
        uint32_t iterations = 10000);

    /**
     * @brief Verify username matches a hash
     *
     * @param username Plaintext username to verify
     * @param hash Expected hash value
     * @param algorithm Hashing algorithm used
     * @param salt Salt used during hashing
     * @param iterations PBKDF2 iteration count (if applicable)
     * @return true if username matches hash
     */
    static bool verify_username(
        const std::string& username,
        const std::vector<uint8_t>& hash,
        Algorithm algorithm,
        const std::vector<uint8_t>& salt = {},
        uint32_t iterations = 10000);

    /**
     * @brief Get expected hash output size for algorithm
     *
     * @param algorithm Hashing algorithm
     * @return Hash size in bytes
     */
    static size_t get_hash_size(Algorithm algorithm);

    /**
     * @brief Check if algorithm is FIPS-approved
     *
     * @param algorithm Algorithm to check
     * @return true if FIPS-140-3 approved
     */
    static bool is_fips_approved(Algorithm algorithm);

    /**
     * @brief Generate random salt for username hashing
     *
     * @param size Salt size in bytes (default: 16)
     * @return Random salt bytes
     */
    static VaultResult<std::vector<uint8_t>> generate_salt(size_t size = 16);

private:
    static VaultResult<std::vector<uint8_t>> hash_sha3(
        const std::string& username,
        const EVP_MD* md,
        const std::vector<uint8_t>& salt);

    static VaultResult<std::vector<uint8_t>> hash_pbkdf2(
        const std::string& username,
        const std::vector<uint8_t>& salt,
        uint32_t iterations);

    static VaultResult<std::vector<uint8_t>> hash_argon2(
        const std::string& username,
        constFlow (Plaintext Mode):
```
1. User enters username (plaintext) and password
2. System searches KeySlots for matching plaintext username
3. If found, derive KEK from password using slot's password_salt
4. Attempt to unwrap DEK with KEK
5. Success → vault unlocked, Failure → wrong password
```

#### Enhanced Flow (Hashed Mode):
```
1. User enters username (plaintext) and password
2. System checks KeySlot.username_hash_size:
   - If 0: Use plaintext matching (legacy mode)
   - If >0: Hash username using vault's algorithm
3. System searches KeySlots for matching username or hash
4. If found, derive KEK from password using slot's password_salt
5. Attempt to unwrap DEK with KEK
6. Success → vault unlocked, Failure → wrong username or password
```

**Backward Compatibility:** Existing vaults with username_hash_size = 0 continue using plaintext matching until admin upgrades algorithm.

###GSchema Integration

### New Preference Settings

**File:** `data/com.example.keeptower.gschema.xml`

```xml
<!-- Username Hashing Configuration -->
<key name="username-hash-algorithm" type="s">
  <default>'plaintext'</default>
  <summary>Username hashing algorithm</summary>
  <description>
    Algorithm used for hashing usernames in newly created vaults.
    Options: 'plaintext' (legacy), 'sha3-256' (recommended), 'sha3-384',
    'sha3-512', 'pbkdf2-sha256', 'argon2id' (non-FIPS).
    Changing this setting only affects NEW vaults created after the change.
    To upgrade an existing vault, use Preferences → Security → Upgrade Vault Algorithm.
  </description>
</key>

<key name="username-pbkdf2-iterations" type="u">
  <default>10000</default>
  <range min="1000" max="100000"/>
  <summary>PBKDF2 iterations for username hashing</summary>
  <description>
    Iteration count when using PBKDF2 for username hashing.
    Only applies when username-hash-algorithm is 'pbkdf2-sha256'.
  Admin-controlled (follows least-privilege principle)
- No disruption (users don't notice)
- Automatic backup
- Reversible (restore from backup if needed)

**Disadvantages:**
- None (recommended path)

#### Path 2: Preference Default for New Vaults
    Memory cost for Argon2id username hashing (in kilobytes).
    Only applies when username-hash-algorithm is 'argon2id'.
    Default: 64 MB.
  <Admin opens Preferences → Security
2. Changes "Default Username Hashing Algorithm" from "Plaintext" to "SHA3-256"
3. Clicks "Apply" (does NOT affect existing vaults)
4. Creates new vault → uses SHA3-256 automatically
5. Old vaults remain in plaintext mode until explicitly upgraded

**Advantages:**
- Forward-looking (all new vaults secure)
- No impact on existing vaults
- Follows existing preference pattern

**Disadvantages:**
- Existing vaults still vulnerable until upgradeame field (security)
  4. Updates VaultSecurityPolicy.username_hash_algorithm
  5. Saves vault
- Operation is **one-way** (cannot downgrade to plaintext)

## Migration Strategy

### Migration Paths

#### Path 1: Admin-Initiated Upgrade (Recommended)

**Process:**
1. Admin opens existing vault (plaintext mode)
2. Opens Preferences → Security
3. Sees warning: "Username Hashing: Plaintext (INSECURE)"
4. Clicks "Upgrade Username Hashing Algorithm"
5. Selects algorithm from dropdown (SHA3-256 recommended)
6. Confirms upgrade
7. System:
   - Re-hashes all usernames with selected algorithm
   - Updates KeySlot.username_hash and username_hash_size
   - Clears KeySlot.username (plaintext field)
   - Updates VaultSecurityPolicy.username_hash_algorithm
   - Saves vault
8.Hashed → Plaintext:** ❌ **NOT POSSIBLE** (one-way hashing, cannot recover plaintext usernames)

**Plaintext → Hashed:** ✅ **SUPPORTED** (admin upgrade via Preferences)

**Mixed Mode:** ✅ **SUPPORTED** (vault can have both plaintext and hashed KeySlots during transition)

**File Format:** ✅ **FULLY COMPATIBLE** (remains V2, no version bump)

### Upgrade Verification

After upgrade, admin can verify in Preferences → Vault Information:
```
Vault Information
─────────────────
Format Version: 2 (Multi-user)
Created: 2026-01-15 14:23:00
Modified: 2026-01-18 10:45:00
Users: 3 (2 admin, 1 standard)
Username Hashing: SHA3-256 (FIPS-approved) ✅
YubiKey Required: No
PBKDF2 Iterations: 100,000
Password History: 5 previous passwords
```
5. V2 backup created automatically (`.v2backup` extension)

**Advantages:**
- Single operation
- No external tools required
- Automatic backup

**Disadvantages:**
- No rollback without backup
- Requires all users to re-authenticate if multi-user vault

#### Path 2: Export/Import Migration (Safest)

**Process:**
1. User exporGSchema and Preferences Infrastructure (Week 2-3)

**Deliverables:**
- [ ] GSchema keys for username hashing preferences
- [ ] Settings validator for algorithm validation
- [ ] Preference persistence and loading
- [ ] Default value handling
- [ ] FIPS mode preference restrictions

**Files:**
- `data/com.example.keeptower.gschema.xml` (MODIFY)
- `src/utils/SettingsValidator.h/.cc` (MODIFY)
- `tests/test_settings_validator.cc` (MODIFY)

### Phase 3: V2 Vault Format Enhancement (Week 3-4)

**Deliverables:**
- [ ] `KeySlot` structure enhancement (add username_hash fields)
- [ ] `VaultSecurityPolicy` enhancement (add algorithm field)
- [ ] Serialization updates (backward compatible)
- [ ] Deserialization with fallback to plaintext
- [ ] Mixed-mode support (plaintext + hashed)

**Files:**
- `src/core/MultiUserTypes.h` (MODIFY)
- `src/core/MultiUserTypes.cc` (MODIFY)
- `tests/test_multiuser.cc` (MODIFY
#### Path 3: Side-by-Side Conversion (Enterprise)

**Process:**
1. Admin runs conversion tool on V2 vault
2. Tool creates V3 vault alongside V2
3. All users re-authenticate to populate V3 key slots
4. After all users migrated, V2 vault archived
5. V3 becomes primary vault

**Advantages:**
- Gradual migration
- No downtime
- Both vaults coexist

**Disadvantages:**
- Complex
- Requires all users to participate
- Temporary storage overhead

### Backward Compatibility

**V3 → V2:** ❌ **NOT POSSIBLE** (one-way hashing, cannot recover plaintext usernames)

**V2 → V3:** ✅ **SUPPORTED** (migration paths above)

**V1 → V3:** ✅ **SUPPORTED** (via V1→V2→V3 chain, or direct V1→V3 with automatic hashing)

### AppImage Fallback

For users needing to access old V2 vaults without migration:

1. **Archive AppImage:** Provide AppImage of last V2-compatible version (0.3.3.2)
2. **Export Tool:** Include export-to-JSON utility in V2 AppImage
3. **Documentation:** Clear guide on V2 export → V3 import process
4. **Warning:** V2 AppImage flagged as "legacy support only, security risk"

---

## Implementation Phases

### Phase 1: Core Hashing Infrastructure (Week 1-2)

**Deliverables:**
- [ ] `UsernameHashService` implementation (SHA3-256, SHA3-384, SHA3-512)
- [ ] PBKDF2 username hashing support
- [ ] Argon2id integration (optional, feature flag)
- [ ] Unit tests (100% coverage)
- [ ] FIPS mode validation tests

**Files:**
- `src/core/services/UsernameHashService.h` (NEW)
- `src/core/services/UsernameHashService.cc` (NEW)
- `tests/test_username_hashing.cc` (NEW)

### Phase 2: Vault Format V3 (Week 3-4)

**Deliverables:**
- [ ] `VaultFormatV3.h/.cc` implementation
- [ ] `KeySlotV3` structure and serialization
- [ ] `VaultSecurityPolicyV3` with algorithm selection
- [ ] V3 header read/write functions
- [ ] Format detection and version handling

**Files:**
- `src/core/VaultFormatV3.h` (NEW)
- `src/core/VaultFormatV3.cc` (NEW)
- `src/core/MultiUserTypesV3.h` (NEW)
- `src/core/MultiUserTypesV3.cc` (NEW)
- `tests/test_vault_format_v3.cc` (NEW)
4: Authentication Updates (Week 4-5)

**Deliverables:**
- [ ] Update `VaultManager` for hashed username authentication
- [ ] Dual-mode lookup (plaintext fallback for migration)
- [ ] Username hash verification in `authenticate_user()`
- [ ] Login dialog changes (generic "authentication failed" message)
- [ ] User creation with automatic hashing (reads GSchema preference)
- [ ] Username change handling (re-hash with current algorithm)

**Files:**
- `src/core/VaultManager.cc` (MODIFY)
- `src/ui/dialogs/V2UserLoginDialog.cc` (MODIFY)
- `src/ui/dialogs/UserManagementDialog.cc` (MODIFY)

### Phase 5: Preferences UI Implementation (Week 5-6)

**Deliverables:**
- [ ] Preferences → Security: Username Hashing section
- [ ] Algorithm dropdown (default preference)
- [ ] Current vault algorithm display (read-only when vault open)
- [ ] "Upgrade Vault Algorithm" button (admin only)
- [ ] Upgrade confirmation dialog with warnings
- [ ] Algorithm parameter controls (PBKDF2 iterations, Argon2 memory)
- [ ] FIPS mode UI restrictions (hide Argon2id option)

**Files:**
- `src/ui/dialogs/PreferencesDialog.h/.cc` (MODIFY)
- `src/ui/dialogs/AlgorithmUpgradeDialog.h/.cc` (NEW)

### Phase 6: Vault Upgrade Service (Week 6-7)

**Deliverables:**
- [ ] `VaultUpgradeService` for algorithm migration
- [ ] Re-hash all KeySlots with new algorithm
- [ ] Automatic backup before upgrade
- [ ] Rollback capability
- [ ] Upgrade validation and verification
- [ ] Admin permission enforcement

**Files:**
- `src/core/services/VaultUpgradeService.h` (NEW)
- `src/core/services/VaultUpgradeService.cc` (NEW)
- `tests/t8: Testing & Documentation (Week 8)

**Deliverables:**
- [ ] Integration tests (plaintext → hashed upgrade)
- [ ] Mixed-mode authentication tests
- [ ] Performance benchmarks (hash algorithm comparison)
- [ ] FIPS mode validation
- [ ] GSchema preference tests
- [ ] Admin permission enforcement tests
- [ ] Security audit
- [ ] User documentation
- [ ] Developer documentation

**Files:**
- `tests/integration/test_username_hashing_migration.cc` (NEW)
- `tests/test_preferences_username_hashing.cc` (NEW)
- `docs/developer/USERNAME_HASHING_IMPLEMENTATION.md` (NEW)
- `docs/user/USERNAME_HASHING

### Phase 6: Testing & Documentation (Week 8)

**Deliverables:**
- [ ] Integration tests (V2→V3 migration)
- [ ] Performance benchmarks (hash algorithm comparison)
- [ ] FIPS mode validation
- [ ] Security audit
- [ ] User documentation
- [ ] Developer documentation
- [ ] Migration guide

**Files:**
- `tests/integration/test_v2_to_v3_migration.cc` (NEW)
- `docs/developer/USERNAME_HASHING_IMPLEMENTATION.md` (NEW)
- `docs/user/VAULT_MIGRATION_GUIDE.md` (NEW)
- `docs/security/USERNAME_HASHING_SECURITY_ANALYSIS.md` (NEW)

---

## Testing Strategy

### Unit Tests

1. **Username Hashing:**
   - SHA3-256/384/512 correctness
   - PBKDF2 with various iterations
   - Argon2id parameter validation
   - Salt generation uniqueness
   - FIPS mode enforcement

2. **Serialization:**
   - KeySlotV3 serialize/deserialize
   - VaultSecurityPolicyV3 serialize/deserialize
   - Hash size validation
   - Salt handling

3. **Authentication:**
   - Username hash matching
   - Failed authentication (wrong username)
   - Failed authentication (wrong password)
   - Hash algorithm mismatch

### Integration Tests

1. **Migration:**
   - V2→V3 in-place migration
   - Export/import migration
   - Backup creation and restoration
   - Multi-user vault migration

2. **Vault Operations:**
   - Create V3 vault with each algorithm
   - Add users to V3 vault
   - Remove users from V3 vault
   - Change username (re-hash)

3. **FIPS Compliance:**
   - FIPS mode blocks Argon2id
   - FIPS mode accepts SHA3/PBKDF2
   - Algorithm selection validation

### Performance Tests

**Benchmark:** Hash 1000 usernames

| Algorithm | Time (ms) | Notes |
|-----------|-----------|-------|
| SHA3-256 | ~5 ms | Fastest |
| SHA3-512 | ~8 ms | Slightly slower |
| PBKDF2 (10k) | ~50 ms | 10x slower |
| Argon2id | ~300 ms | 60x slower (memory-hard) |

**Recommendation:** SHA3-256 for optimal performance

---

## Security Considerations

### Threat Model

**Threats Mitigated:**
- ✅ Username enumeration from stolen vault file
- ✅ Information disclosure in backups
- ✅ Forensic analysis of deleted vaults
- ✅ Social engineering based on known usernames

**Threats NOT Mitigated:**
- ❌ Rainbow table attacks (mitigated by salting)
- ❌ Timing attacks on username verification (mitigated by constant-time comparison)
- ❌ Username disclosure through network traffic (out of scope, local app)

### Hash Algorithm Security

**SHA3-256 Security Margin:**
- Collision resistance: 2^128 operations (infeasible)
- Preimage resistance: 2^256 operations (quantum-resistant)
- Second preimage resistance: 2^256 operations
No Breaking Changes

**Version:** 0.3.4 or 0.4.0 (Minor or feature release, NOT major bump)

**Reason:** V2 format maintained, fully backward compatible

**Enhanced APIs (Signature Unchanged):**
- `VaultManager::create_vault_v2()` - Now reads username_hash_algorithm preference
- `VaultManager::authenticate_user()` - Now supports both plaintext and hashed username lookup
- `VaultManager::add_user()` - Now hashes username based on vault's algorithm

**New APIs:**
```cpp
namespace KeepTower {

/**
 * @brief Upgrade vault's username hashing algorithm (admin only)
 *
 * Re-hashes all user KeySlots with new algorithm. Creates automatic backup.
 * Requires administra3.4 / v0.4.0)

```markdown
## Security Enhancement: Username Hashing

**Optional Security Upgrade:** Administrators can now enable username hashing for enhanced privacy.

### What Changed?

KeepTower now offers optional cryptographic hashing of usernames to protect
user identities in vault files. This prevents username disclosure even if
a vault file is stolen.

### New Features:

**1. Configurable Default (Preferences → Security):**
   - Choose default algorithm for NEW vaults
   - Options: SHA3-256 (recommended), SHA3-512, PBKDF2-SHA256, Argon2id
   - Existing vaults unaffected by preference change

**2. Admin-Controlled Upgrade (Existing Vaults):**
   - Administrators can upgrade vault's hashing algorithm
   - Preferences → Security → "Upgrade Username Hashing Algorithm"
   - One-click upgrade with automatic backup

**3. Algorithm Choices:**
   - **SHA3-256** (Recommended): FIPS-approved, fast, secure
   - **SHA3-512**: FIPS-approved, maximum hash size
   - **PBKDF2-SHA256**: FIPS-approved, slower, higher security
   - **Argon2id**: Not FIPS-approved, maximum brute-force resistance

### How to Enable:

**For New Vaults:**
1. Open Preferences → Security
2. Set "Default Username Hashing Algorithm" to "SHA3-256"
3. Click "Apply"
4. All future vaults will use SHA3-256

**For Existing Vaults (Admin Required):**
1. Open vault as administrator
2. Open Preferences → Security
3. Click "Upgrade Username Hashing Algorithm"
4. Select algorithm (SHA3-256 recommended)
5. Resolved Design Decisions

1. **Vault Format:** ✅ Stay with V2 format (no version bump)
   - Maintains compatibility with existing deployments
   - Uses reserved space in VaultSecurityPolicy for new fields

2. **Configuration Method:** ✅ GSchema preferences (like clipboard timeout)
   - Consistent with existing preference patterns
   - Changes don't affect currently open vault
   - Admin-only upgrade for existing vaults

3. **Salt Storage:** ✅ Per-user salt (16 bytes)
   - Better security, no correlation between users
   - Prevents rainbow table attacks

4. **Hash Output Display:** ✅ Show in Vault Information dialog
   - Visible in Preferences → Vault Information
   - Security indicator (✅ hashed, ⚠️ plaintext)

5. **Emergency Access:** ✅ No username recovery (by design)
   - Admin can list all users' hashes
   - Admin can verify hash of suspected username
   - No plaintext recovery possible (security by design)

6. **Performance:** ✅ No caching needed
   - SHA3-256 hashing is fast (~5ms)
   - Negligible impact on authentication

7. **Argon2id Dependency:** ✅ Optional build flag
   - Requires libargon2 (not in OpenSSL)
   - Feature flag: `ENABLE_ARGON2` (default: OFF for FIPS compliance)
   - FIPS mode: Argon2id option hidden in UI

## Implementation Clarifications

### GSchema Preference Behavior

**Scenario 1: New Vault Creation**
```
User Action: Create new vault
System Reads: username-hash-algorithm preference (e.g., "sha3-256")
Result: New vault has algorithm = SHA3-256
Vault File: Sets VaultSecurityPolicy.username_hash_algorithm = 0x01
```

**Scenario 2: Open Existing Vault**
```
User Action: Open vault with plaintext usernames
Preferences UI: Shows "Current Vault: Plaintext (INSECURE)"
User Changes Preference: Set default to "sha3-256"
Result: Preference saved, but DOES NOT affect open vault
Current Vault: Still using plaintext mode
```DESIGN FINALIZED - READY FOR IMPLEMENTATION
**Next Steps:** Begin Phase 1 (Core Hashing Infrastructure)
**Estimated Effort:** 8 weeks (1 developer)
**Risk Level:** Low (no breaking changes, fully backward compatible, admin-controlled upgrade
User Action: Admin clicks "Upgrade Vault Algorithm"
System: Shows confirmation dialog
Admin Confirms: Select "SHA3-256"
Result:
  1. Create backup: vault.keeptower.pre-hash-upgrade.backup
  2. Re-hash all KeySlots with SHA3-256
  3. Clear plaintext username fields
  4. Set VaultSecurityPolicy.username_hash_algorithm = 0x01
  5. Save vault
Vault File: Now uses SHA3-256 for all users
```

### Preference-to-Algorithm Mapping

```cpp
// GSchema string → UsernameHashService::Algorithm
std::string pref = settings->get_string("username-hash-algorithm");

if (pref == "plaintext") → Algorithm::PLAINTEXT_LEGACY
if (pref == "sha3-256") → Algorithm::SHA3_256
if (pref == "sha3-384") → Algorithm::SHA3_384
if (pref == "sha3-512") → Algorithm::SHA3_512
if (pref == "pbkdf2-sha256") → Algorithm::PBKDF2_SHA256
if (pref == "argon2id") → Algorithm::ARGON2ID
```
We recommend enabling SHA3-256 for all new vaults and upgrading existing
vaults when convenient. Plaintext username storage will be deprecated in
a future release
} // namespace KeepTower
```

---

## User Communication

### Release Notes (v0.4.0)

```markdown
## Security Enhancement: Username Hashing

**Action Required:** Existing vaults will prompt for migration on first open.

### What Changed?

KeepTower now protects user identities by cryptographically hashing usernames
before storing them in vault files. This prevents username disclosure even if
a vault file is stolen.

### Algorithm Choices:

- **SHA3-256** (Default): FIPS-approved, fast, secure
- **SHA3-512**: FIPS-approved, maximum hash size
- **PBKDF2-SHA256**: FIPS-approved, slower, higher security
- **Argon2id**: Not FIPS-approved, maximum brute-force resistance

### Migration:

1. Open existing vault (last time with old format)
2. Choose username hashing algorithm (SHA3-256 recommended)
3. Vault automatically migrates (backup created)
4. Re-enter password on first login after migration

### Backward Compatibility:

⚠️ **V3 vaults cannot be opened by older KeepTower versions**

If you need to access the old format:
- Download KeepTower v0.3.3.2 AppImage (legacy support)
- Export vault to JSON
- Import into latest KeepTower

### FIPS Mode:

FIPS mode users: Only SHA3-256, SHA3-512, and PBKDF2 are available.
Argon2id is blocked in FIPS mode (not NIST-approved).
```

---

## Open Questions

1. **Salt Storage:** Should username salt be per-user or vault-wide?
   - **Recommendation:** Per-user (better security, no correlation between users)

2. **Hash Output Display:** Should UI show hash algorithm in vault info?
   - **Recommendation:** Yes, in Preferences → Vault Information

3. **Emergency Access:** How to handle "forgot username" scenario?
   - **Recommendation:** No recovery possible (by design). Admin can list hashed usernames in debug mode (compare with hash of suspected username)

4. **Performance:** Should we cache username hashes in memory?
   - **Recommendation:** No. Hashing is fast enough (~5ms for SHA3-256)

5. **Argon2id Dependency:** Include libargon2 or make it optional?
   - **Recommendation:** Optional dependency (feature flag). FIPS users don't need it.

---

## Success Criteria

- [ ] All usernames stored as cryptographic hashes (no plaintext)
- [ ] FIPS-140-3 compliance maintained (SHA-3, PBKDF2)
- [ ] User choice of hash algorithm during vault creation
- [ ] Smooth V2→V3 migration with automatic backup
- [ ] No performance regression (authentication < 100ms)
- [ ] All unit tests passing (including new username hashing tests)
- [ ] Security audit approval
- [ ] Documentation complete

---

## References

- FIPS 202: SHA-3 Standard (Keccak)
- NIST SP 800-132: PBKDF2 Recommendation
- NIST SP 800-185: SHA-3 Derived Functions
- Argon2 RFC 9106: Password Hashing Competition Winner
- OWASP Password Storage Cheat Sheet
- KeepTower FIPS Compliance Documentation

---

**Status:** DESIGN FINALIZED - READY FOR IMPLEMENTATION
**Next Steps:** Begin Phase 1 (Core Hashing Infrastructure)
**Estimated Effort:** 8 weeks (1 developer)
**Risk Level:** Low (no breaking changes, fully backward compatible, admin-controlled upgrade)

---

## Code Quality Requirements

### Single Responsibility Principle (SRP) Compliance

All new code must adhere to SOLID principles, particularly the Single Responsibility Principle. Each class should have **one reason to change**.

#### Class Design (SRP Applied)

**UsernameHashService** - Single Responsibility: Username hashing operations
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @brief Service for cryptographic hashing of usernames
 *
 * Single Responsibility: Compute username hashes using various algorithms.
 * Does NOT handle vault operations, authentication, or preferences.
 */
class UsernameHashService {
public:
    enum class Algorithm {
        PLAINTEXT_LEGACY = 0x00,  // Not recommended
        SHA3_256 = 0x01,          // Default, FIPS-approved
        SHA3_384 = 0x02,          // FIPS-approved
        SHA3_512 = 0x03,          // FIPS-approved
        PBKDF2_SHA256 = 0x04,     // FIPS-approved
        ARGON2ID = 0x05           // NOT FIPS-approved
    };

    /**
     * @brief Compute username hash
     * @param username Plaintext username to hash
     * @param algorithm Hash algorithm to use
     * @param salt Optional salt (16 bytes)
     * @param iterations Iterations for PBKDF2/Argon2 (ignored for SHA-3)
     * @return Hash bytes (size depends on algorithm)
     */
    [[nodiscard]] static std::expected<std::vector<uint8_t>, VaultError>
    hash_username(std::string_view username,
                  Algorithm algorithm,
                  std::span<const uint8_t, 16> salt,
                  uint32_t iterations = 10000);

    /**
     * @brief Verify username against stored hash
     * @param username Plaintext username to verify
     * @param stored_hash Hash to compare against
     * @param algorithm Hash algorithm used
     * @param salt Salt used during hashing
     * @param iterations Iterations (PBKDF2/Argon2 only)
     * @return true if match, false otherwise
     */
    [[nodiscard]] static bool verify_username(
        std::string_view username,
        std::span<const uint8_t> stored_hash,
        Algorithm algorithm,
        std::span<const uint8_t, 16> salt,
        uint32_t iterations = 10000);

    /**
     * @brief Get expected hash size for algorithm
     * @param algorithm Hash algorithm
     * @return Hash size in bytes (32/48/64)
     */
    [[nodiscard]] static constexpr size_t get_hash_size(Algorithm algorithm) noexcept;

private:
    // Private implementation methods (one per algorithm)
    static std::expected<std::vector<uint8_t>, VaultError> hash_sha3_256(
        std::string_view username, std::span<const uint8_t, 16> salt);

    static std::expected<std::vector<uint8_t>, VaultError> hash_sha3_384(
        std::string_view username, std::span<const uint8_t, 16> salt);

    static std::expected<std::vector<uint8_t>, VaultError> hash_sha3_512(
        std::string_view username, std::span<const uint8_t, 16> salt);

    static std::expected<std::vector<uint8_t>, VaultError> hash_pbkdf2_sha256(
        std::string_view username, std::span<const uint8_t, 16> salt, uint32_t iterations);

    static std::expected<std::vector<uint8_t>, VaultError> hash_argon2id(
        std::string_view username, std::span<const uint8_t, 16> salt, uint32_t iterations);
};
```

**VaultUpgradeService** - Single Responsibility: Vault algorithm upgrades
```cpp
/**
 * @brief Service for upgrading vault security algorithms
 *
 * Single Responsibility: Perform vault algorithm upgrades with backup/rollback.
 * Does NOT handle hashing (delegates to UsernameHashService).
 * Does NOT handle UI (delegates to PreferencesDialog).
 */
class VaultUpgradeService {
public:
    explicit VaultUpgradeService(VaultManager& vault_manager);

    /**
     * @brief Upgrade vault's username hashing algorithm
     * @param new_algorithm Target hash algorithm
     * @param current_user_session User session (for admin check)
     * @return Success or error (permission denied, backup failed, etc.)
     */
    [[nodiscard]] std::expected<void, VaultError> upgrade_username_hashing(
        UsernameHashService::Algorithm new_algorithm,
        const UserSession& current_user_session);

    /**
     * @brief Create backup before upgrade
     * @return Backup file path or error
     */
    [[nodiscard]] std::expected<std::filesystem::path, VaultError> create_backup();

    /**
     * @brief Rollback to backup after failed upgrade
     * @param backup_path Path to backup file
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, VaultError> rollback_from_backup(
        const std::filesystem::path& backup_path);

private:
    VaultManager& m_vault_manager;

    // Private helper: verify admin permissions
    [[nodiscard]] bool is_admin(const UserSession& session) const;

    // Private helper: re-hash all KeySlots
    [[nodiscard]] std::expected<void, VaultError> rehash_all_keyslots(
        UsernameHashService::Algorithm algorithm);
};
```

**PreferencesDialog** - Single Responsibility: UI for user preferences
```cpp
/**
 * @brief Preferences dialog UI
 *
 * Single Responsibility: Display and manage user preferences UI.
 * Delegates hashing to UsernameHashService.
 * Delegates upgrade operations to VaultUpgradeService.
 * Delegates GSchema reads/writes to SettingsManager.
 */
class PreferencesDialog : public Gtk::Dialog {
public:
    explicit PreferencesDialog(Gtk::Window& parent,
                               VaultManager& vault_manager,
                               SettingsManager& settings_manager);

private:
    // UI components
    Gtk::ComboBoxText m_algorithm_combo;
    Gtk::Button m_upgrade_button;
    Gtk::Label m_current_algorithm_label;

    // Service dependencies (injected)
    VaultManager& m_vault_manager;
    SettingsManager& m_settings_manager;

    // Event handlers
    void on_algorithm_changed();
    void on_upgrade_clicked();
    void update_current_algorithm_display();

    // UI helpers
    void show_upgrade_confirmation_dialog();
    void show_upgrade_success_dialog();
    void show_upgrade_error_dialog(const VaultError& error);
};
```

**SettingsManager** - Single Responsibility: GSchema preference management
```cpp
/**
 * @brief Settings manager for GSchema preferences
 *
 * Single Responsibility: Read/write GSchema preferences.
 * Does NOT perform hashing (delegates to UsernameHashService).
 * Does NOT handle UI (delegates to PreferencesDialog).
 */
class SettingsManager {
public:
    SettingsManager();

    // Username hashing preferences
    [[nodiscard]] UsernameHashService::Algorithm get_default_username_hash_algorithm() const;
    void set_default_username_hash_algorithm(UsernameHashService::Algorithm algorithm);

    [[nodiscard]] uint32_t get_username_pbkdf2_iterations() const;
    void set_username_pbkdf2_iterations(uint32_t iterations);

    [[nodiscard]] uint32_t get_username_argon2_memory_kb() const;
    void set_username_argon2_memory_kb(uint32_t memory_kb);

    [[nodiscard]] uint32_t get_username_argon2_iterations() const;
    void set_username_argon2_iterations(uint32_t iterations);

private:
    Glib::RefPtr<Gio::Settings> m_settings;

    // Helper: convert string preference to Algorithm enum
    [[nodiscard]] UsernameHashService::Algorithm parse_algorithm(const std::string& str) const;

    // Helper: convert Algorithm enum to string preference
    [[nodiscard]] std::string algorithm_to_string(UsernameHashService::Algorithm alg) const;
};
```

#### SRP Anti-Patterns to Avoid

❌ **BAD: God Object (Multiple Responsibilities)**
```cpp
// VIOLATES SRP: This class does EVERYTHING
class UsernameHashingManagerAndUIAndPreferencesAndVaultOperations {
    // Responsibility 1: Hashing
    std::vector<uint8_t> hash_username(const std::string& username);

    // Responsibility 2: UI
    void show_upgrade_dialog();
    void update_preferences_ui();

    // Responsibility 3: GSchema
    void save_preferences();
    std::string load_algorithm_preference();

    // Responsibility 4: Vault Operations
    void upgrade_vault();
    void create_backup();

    // Responsibility 5: Authentication
    bool authenticate_user(const std::string& username, const std::string& password);

    // TOO MANY REASONS TO CHANGE!
};
```

✅ **GOOD: Separated Responsibilities**
```cpp
// Each class has ONE reason to change

// Reason to change: Hash algorithm implementation changes
class UsernameHashService { /* hashing only */ };

// Reason to change: UI design changes
class PreferencesDialog { /* UI only */ };

// Reason to change: GSchema schema changes
class SettingsManager { /* preferences only */ };

// Reason to change: Vault upgrade logic changes
class VaultUpgradeService { /* upgrade only */ };

// Reason to change: Authentication logic changes
class VaultManager { /* vault operations only */ };
```

### CONTRIBUTING.md Compliance

All code in this feature must comply with [CONTRIBUTING.md](../../CONTRIBUTING.md):

#### Code Style

- ✅ **C++23 features**: Use `std::expected`, `std::span`, range-based loops
- ✅ **Naming conventions**:
  - Classes: `PascalCase` (e.g., `UsernameHashService`)
  - Functions/Methods: `snake_case` (e.g., `hash_username()`)
  - Variables: `snake_case` (e.g., `hash_algorithm`)
  - Constants: `UPPER_SNAKE_CASE` (e.g., `SHA3_256_HASH_SIZE`)
  - Member variables: `m_` prefix (e.g., `m_vault_manager`)
- ✅ **Formatting**: 4 spaces, 100 char lines, braces on same line
- ✅ **Modern C++**: RAII, smart pointers, `auto`, `constexpr`, range-for

#### FIPS-140-3 Compliance

All cryptographic code MUST follow FIPS requirements from CONTRIBUTING.md:

**SHA-3 Hashing (FIPS-Approved)**
```cpp
// ✅ GOOD: FIPS-approved SHA3-256 using OpenSSL EVP API
std::expected<std::vector<uint8_t>, VaultError>
UsernameHashService::hash_sha3_256(std::string_view username,
                                     std::span<const uint8_t, 16> salt) {
    // Combine username + salt
    std::vector<uint8_t> input;
    input.insert(input.end(), username.begin(), username.end());
    input.insert(input.end(), salt.begin(), salt.end());

    // Use FIPS-approved EVP API (not deprecated low-level API)
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return std::unexpected(VaultError::CRYPTO_ERROR);
    }

    auto cleanup = [ctx]() { EVP_MD_CTX_free(ctx); };
    std::unique_ptr<EVP_MD_CTX, decltype(cleanup)> ctx_guard(ctx, cleanup);

    // SHA3-256 is FIPS-approved (FIPS 202)
    if (EVP_DigestInit_ex(ctx, EVP_sha3_256(), nullptr) != 1) {
        return std::unexpected(VaultError::CRYPTO_ERROR);
    }

    if (EVP_DigestUpdate(ctx, input.data(), input.size()) != 1) {
        return std::unexpected(VaultError::CRYPTO_ERROR);
    }

    std::vector<uint8_t> hash(32);  // SHA3-256 = 32 bytes
    unsigned int hash_len = 0;
    if (EVP_DigestFinal_ex(ctx, hash.data(), &hash_len) != 1) {
        return std::unexpected(VaultError::CRYPTO_ERROR);
    }

    return hash;
}

// ❌ BAD: Low-level API (deprecated, not FIPS-compliant)
// DO NOT USE: SHA3_256(input.data(), input.size(), hash.data());
```

**PBKDF2 (FIPS-Approved)**
```cpp
// ✅ GOOD: FIPS-approved PBKDF2-HMAC-SHA256
std::expected<std::vector<uint8_t>, VaultError>
UsernameHashService::hash_pbkdf2_sha256(std::string_view username,
                                         std::span<const uint8_t, 16> salt,
                                         uint32_t iterations) {
    std::vector<uint8_t> hash(32);  // 256 bits

    // PBKDF2-HMAC-SHA256 is FIPS-approved (SP 800-132)
    if (PKCS5_PBKDF2_HMAC(username.data(), username.size(),
                          salt.data(), salt.size(),
                          iterations,
                          EVP_sha256(),  // FIPS-approved
                          hash.size(),
                          hash.data()) != 1) {
        return std::unexpected(VaultError::CRYPTO_ERROR);
    }

    return hash;
}

// ❌ BAD: MD5 not FIPS-approved
// DO NOT USE: EVP_md5()
```

**Argon2id (NOT FIPS-Approved, Optional)**
```cpp
#ifdef ENABLE_ARGON2
// ⚠️ WARNING: Argon2id is NOT FIPS-approved (blocked in FIPS mode)
std::expected<std::vector<uint8_t>, VaultError>
UsernameHashService::hash_argon2id(std::string_view username,
                                    std::span<const uint8_t, 16> salt,
                                    uint32_t iterations) {
    // Check FIPS mode: block Argon2id if FIPS is enabled
    if (FIPS_mode()) {
        return std::unexpected(VaultError::FIPS_ALGORITHM_NOT_APPROVED);
    }

    std::vector<uint8_t> hash(32);  // 256 bits
    uint32_t memory_kb = 65536;  // 64 MB (from GSchema preference)

    // Use libargon2 (external dependency, NOT OpenSSL)
    int result = argon2id_hash_raw(
        iterations,              // t_cost (time iterations)
        memory_kb,               // m_cost (memory in KB)
        1,                       // parallelism
        username.data(), username.size(),
        salt.data(), salt.size(),
        hash.data(), hash.size()
    );

    if (result != ARGON2_OK) {
        return std::unexpected(VaultError::CRYPTO_ERROR);
    }

    return hash;
}
#endif  // ENABLE_ARGON2
```

**Key Cleanup (FIPS-Approved)**
```cpp
// ✅ GOOD: Secure memory cleanup (FIPS-approved)
void clear_sensitive_data(std::vector<uint8_t>& data) {
    if (!data.empty()) {
        OPENSSL_cleanse(data.data(), data.size());
        data.clear();
    }
}

// ❌ BAD: May be optimized away by compiler
// DO NOT USE: memset(data.data(), 0, data.size());
```

#### File Organization

All files MUST be placed in appropriate directories per CONTRIBUTING.md:

**New Files:**
- `src/core/services/UsernameHashService.h` (NEW)
- `src/core/services/UsernameHashService.cc` (NEW)
- `src/core/services/VaultUpgradeService.h` (NEW)
- `src/core/services/VaultUpgradeService.cc` (NEW)
- `src/utils/services/SettingsManager.h` (MODIFY - add username hash methods)
- `src/utils/services/SettingsManager.cc` (MODIFY)

**Modified Files:**
- `src/core/MultiUserTypes.h` (ADD: username_hash fields to KeySlot)
- `src/core/MultiUserTypes.cc` (MODIFY: serialization)
- `src/core/VaultManager.cc` (MODIFY: authenticate_user, add_user)
- `src/ui/dialogs/PreferencesDialog.h` (ADD: username hashing UI section)
- `src/ui/dialogs/PreferencesDialog.cc` (MODIFY)
- `data/com.example.keeptower.gschema.xml` (ADD: username-hash-* keys)

**Test Files:**
- `tests/test_username_hashing.cc` (NEW)
- `tests/test_vault_upgrade.cc` (NEW)
- `tests/test_settings_manager.cc` (MODIFY)
- `tests/integration/test_username_hashing_migration.cc` (NEW)

**Documentation (Per CONTRIBUTING.md Structure):**
- `docs/developer/USERNAME_HASHING_SECURITY_PLAN.md` (THIS FILE - already created)
- `docs/developer/USERNAME_HASHING_IMPLEMENTATION.md` (NEW - Phase 1)
- `docs/user/USERNAME_HASHING_GUIDE.md` (NEW - Phase 8)
- `docs/security/USERNAME_HASHING_SECURITY_ANALYSIS.md` (NEW - Phase 8)

**❌ DO NOT place in root directory:**
- Implementation details, phase summaries, status reports
- Test reports, coverage reports, audit documents
- (See CONTRIBUTING.md File Organization section)

#### Testing Requirements

All new code MUST include comprehensive tests per CONTRIBUTING.md:

**Unit Test Example:**
```cpp
// tests/test_username_hashing.cc
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include <gtest/gtest.h>
#include "core/services/UsernameHashService.h"

TEST(UsernameHashService, HashSHA3_256_ProducesCorrectSize) {
    std::array<uint8_t, 16> salt = {/* random salt */};
    auto result = UsernameHashService::hash_username(
        "testuser", UsernameHashService::Algorithm::SHA3_256, salt);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 32);  // SHA3-256 = 32 bytes
}

TEST(UsernameHashService, VerifyUsername_CorrectUsernameReturnsTrue) {
    std::array<uint8_t, 16> salt = {/* random salt */};
    auto hash = UsernameHashService::hash_username("testuser",
        UsernameHashService::Algorithm::SHA3_256, salt);

    ASSERT_TRUE(hash.has_value());

    bool verified = UsernameHashService::verify_username("testuser",
        *hash, UsernameHashService::Algorithm::SHA3_256, salt);

    EXPECT_TRUE(verified);
}
```

#### Documentation Requirements

All public APIs MUST include Doxygen documentation:

```cpp
/**
 * @brief Compute cryptographic hash of username
 *
 * Hashes the given username using the specified algorithm and salt.
 * Supported algorithms: SHA3-256, SHA3-384, SHA3-512, PBKDF2-SHA256, Argon2id.
 *
 * FIPS Compliance:
 * - SHA3-256, SHA3-384, SHA3-512: FIPS-approved (FIPS 202)
 * - PBKDF2-SHA256: FIPS-approved (SP 800-132)
 * - Argon2id: NOT FIPS-approved (returns error in FIPS mode)
 *
 * @param username Plaintext username to hash (UTF-8 encoded)
 * @param algorithm Hash algorithm to use
 * @param salt 16-byte random salt (unique per user)
 * @param iterations Iteration count (PBKDF2/Argon2 only)
 * @return Hash bytes on success, VaultError on failure
 *
 * @note Thread-safe, no side effects
 * @note Performance: SHA3-256 ~5ms, PBKDF2 ~50ms (10k iterations)
 */
[[nodiscard]] static std::expected<std::vector<uint8_t>, VaultError>
hash_username(std::string_view username,
              Algorithm algorithm,
              std::span<const uint8_t, 16> salt,
              uint32_t iterations = 10000);
```

#### Commit Message Format

Follow conventional commits per CONTRIBUTING.md:

```
feat(security): implement username hashing with FIPS-approved algorithms

Add UsernameHashService for cryptographic username hashing using
SHA3-256, SHA3-512, and PBKDF2-SHA256 (all FIPS-approved).

Features:
- SHA3-256 default hash algorithm (5ms performance)
- PBKDF2-SHA256 for high-security use cases
- Optional Argon2id support (non-FIPS, feature flag)
- Per-user salt (16 bytes random)
- FIPS mode validation

Adheres to SRP: separate classes for hashing, upgrade, preferences, UI.

Closes #XXX
```

#### License Headers

All new files MUST include SPDX headers per CONTRIBUTING.md:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#pragma once

#include <expected>
#include <vector>
// ...
```

---

## Implementation Checklist

Before starting each phase, verify adherence to all requirements:

### Code Quality
- [ ] **SRP Compliance**: Each class has single, clear responsibility
- [ ] **OCP Compliance**: Open for extension, closed for modification
- [ ] **DIP Compliance**: Depend on abstractions, use dependency injection
- [ ] **Composition**: Favor object composition over inheritance
- [ ] **Encapsulation**: Minimal public interface, implementation private

### FIPS Compliance
- [ ] **Approved Algorithms Only**: SHA-3, PBKDF2 (no MD5, SHA1, RC4)
- [ ] **OpenSSL EVP API**: Use `EVP_*` functions, not low-level APIs
- [ ] **FIPS Mode Checks**: Block non-approved algorithms in FIPS mode
- [ ] **Key Cleanup**: Use `OPENSSL_cleanse()` for sensitive data
- [ ] **Self-Tests**: Unit tests verify FIPS mode compatibility

### Code Style
- [ ] **C++23 Features**: `std::expected`, `std::span`, ranges
- [ ] **Naming**: PascalCase classes, snake_case functions/vars, m_ prefix
- [ ] **Formatting**: 4 spaces, 100 char lines, braces on same line
- [ ] **Modern C++**: RAII, smart pointers, `auto`, `constexpr`
- [ ] **No Warnings**: Compiles clean with `-Wall -Wextra -Werror`

### File Organization
- [ ] **Source Files**: Correct `src/` subdirectories
- [ ] **Test Files**: Matching test files in `tests/`
- [ ] **Documentation**: Correct `docs/` subdirectories (NOT root)
- [ ] **Headers**: One class per header file
- [ ] **Includes**: Proper order (header, C, C++, external, project)

### Testing
- [ ] **Unit Tests**: 100% coverage target for new code
- [ ] **Integration Tests**: End-to-end migration scenarios
- [ ] **FIPS Tests**: Verify FIPS mode enforcement
- [ ] **Performance Tests**: Benchmark hash algorithms
- [ ] **Edge Cases**: Boundary conditions, error paths

### Documentation
- [ ] **Doxygen Comments**: All public APIs documented
- [ ] **Code Comments**: Explain "why" not "what"
- [ ] **User Guides**: Create user-facing documentation
- [ ] **Developer Docs**: Implementation details in `docs/developer/`
- [ ] **License Headers**: SPDX identifiers on all new files

### Commits & PRs
- [ ] **Conventional Commits**: `feat/fix/docs/test/refactor` format
- [ ] **Atomic Commits**: One logical change per commit
- [ ] **Clear Messages**: Describe what and why
- [ ] **CHANGELOG.md**: Update for user-facing changes
- [ ] **PR Template**: Fill out all sections

---
