// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file MultiUserTypes.h
 * @brief Type definitions for multi-user vault architecture
 *
 * This file defines the LUKS-style key slot architecture for multi-user
 * vault authentication. Each user has a key slot containing their wrapped
 * Data Encryption Key (DEK), enabling multiple users to unlock the same
 * vault with different passwords.
 */

#ifndef MULTIUSERTYPES_H
#define MULTIUSERTYPES_H

#include <string>
#include <array>
#include <cstdint>
#include <vector>
#include <optional>

namespace KeepTower {

/**
 * @brief User role in vault access control
 *
 * Defines permission levels for vault operations:
 * - ADMINISTRATOR: Full access including user management
 * - STANDARD_USER: View and edit accounts (no user management)
 */
enum class UserRole : uint8_t {
    STANDARD_USER = 0,   ///< Standard access (view/edit accounts)
    ADMINISTRATOR = 1    ///< Full access including user management
};

/**
 * @brief Password history entry for reuse prevention
 *
 * Stores PBKDF2-HMAC-SHA512 hash of a previous password with timestamp.
 * Used to prevent users from reusing recent passwords.
 *
 * @section security_design Security Design
 * - **PBKDF2-HMAC-SHA512 hashing**: FIPS 140-3 approved algorithm
 * - **Random salts**: Each entry has unique 32-byte salt (FIPS-approved DRBG)
 * - **Constant-time comparison**: Prevents timing side-channel attacks
 * - **Ring buffer storage**: FIFO eviction when depth limit reached
 * - **Secure destruction**: Hash is securely cleared on destruction
 *
 * @note Hash size: 48 bytes (PBKDF2-HMAC-SHA512 output)
 * @note Total entry size: 88 bytes (8 timestamp + 32 salt + 48 hash)
 */
struct PasswordHistoryEntry {
    /**
     * @brief Timestamp when password was set (Unix epoch seconds)
     *
     * Used for audit logging and age-based expiration (future feature).
     */
    int64_t timestamp = 0;

    /**
     * @brief Random salt for PBKDF2-HMAC-SHA512 hashing (32 bytes)
     *
     * Unique per-entry salt ensures rainbow table attacks are infeasible.
     * Generated with RAND_bytes() (FIPS-approved DRBG when FIPS mode enabled).
     */
    std::array<uint8_t, 32> salt = {};

    /**
     * @brief PBKDF2-HMAC-SHA512 hash of password (48 bytes)
     *
     * Hash parameters:
     * - Algorithm: PBKDF2-HMAC-SHA512 (FIPS 140-3 approved)
     * - Iterations: 600,000 (OWASP 2023 recommendation for PBKDF2-SHA512)
     * - Hash function: SHA-512 (FIPS-approved)
     * - Output length: 48 bytes
     *
     * @note Higher iteration count than KEK derivation (this is for storage, not auth)
     * @note FIPS-compliant when OpenSSL FIPS provider is enabled
     */
    std::array<uint8_t, 48> hash = {};

    /**
     * @brief Destructor securely clears the password hash
     *
     * Uses OPENSSL_cleanse to prevent hash from remaining in memory.
     */
    ~PasswordHistoryEntry();

    /**
     * @brief Serialize to binary format
     * @return Binary representation (88 bytes)
     */
    std::vector<uint8_t> serialize() const;

    /**
     * @brief Deserialize from binary format
     * @param data Binary data (must be at least 88 bytes)
     * @param offset Offset in data to start reading
     * @return Deserialized entry, or empty optional on error
     */
    static std::optional<PasswordHistoryEntry> deserialize(
        const std::vector<uint8_t>& data, size_t offset);

    /**
     * @brief Get serialized size in bytes
     * @return 88 bytes (8 + 32 + 48)
     */
    static constexpr size_t SERIALIZED_SIZE = 88;
};

/**
 * @brief Vault-wide security policy (admin-controlled)
 *
 * Security settings defined at vault creation and applied uniformly
 * to all users. This ensures consistent security baseline across the
 * entire vault without per-user opt-outs.
 *
 * @section design_rationale Design Rationale
 * - **Vault-level YubiKey**: All users must use YubiKey or none do
 * - **Shared challenge**: Simplifies YubiKey deployment and backup
 * - **Uniform enforcement**: Prevents security downgrade attacks
 * - **Admin control**: IT/security team sets baseline policy
 *
 * @note All fields are FEC-protected (Reed-Solomon) when enabled
 */
struct VaultSecurityPolicy {
    /**
     * @brief YubiKey requirement flag
     *
     * If true, ALL users must provide YubiKey response during authentication.
     * If false, password-only authentication is used for all users.
     *
     * @note Cannot be changed after vault creation (prevents downgrade attacks)
     */
    bool require_yubikey = false;

    /**
     * @brief Minimum password length for all users
     *
     * Enforced during password creation and changes.
     * Default: 12 characters (NIST minimum recommendation)
     *
     * @note Range: 8-128 characters
     */
    uint32_t min_password_length = 12;

    /**
     * @brief PBKDF2 iteration count for key derivation
     *
     * Higher values increase security but slow down authentication.
     * Default: 100,000 (NIST SP 800-63B minimum recommendation)
     *
     * @note Range: 100,000-1,000,000 iterations
     */
    uint32_t pbkdf2_iterations = 100000;

    /**
     * @brief Password history depth for reuse prevention
     *
     * Number of previous passwords to remember per user.
     * When a user changes their password, the system checks against
     * this many previous passwords and rejects reuse.
     *
     * @section depth_behavior Behavior by Depth Value
     * - 0: Password history disabled (no checking)
     * - 1-24: Remember this many previous passwords
     * - Default: 5 (recommended for most use cases)
     *
     * @section storage_impact Storage Impact
     * Each entry: 88 bytes (timestamp + salt + hash)
     * - Depth 5: 440 bytes per user
     * - Depth 12: 1056 bytes per user
     * - Depth 24: 2112 bytes per user
     *
     * @note Range: 0-24 (enforced at API level)
     * @note Uses ring buffer (FIFO eviction when depth exceeded)
     */
    uint32_t password_history_depth = 5;

    /**
     * @brief Username hashing algorithm (Phase 2 - Username Hashing Security)
     *
     * Specifies the cryptographic algorithm used to hash usernames stored in KeySlots.
     * Prevents username enumeration attacks by storing hashed usernames instead of plaintext.
     *
     * @section algorithm_values Algorithm Values
     * - **0**: Plaintext (legacy mode, no hashing) - DEFAULT for backward compatibility
     * - **1**: SHA3-256 (recommended, FIPS-approved, 32-byte hash)
     * - **2**: SHA3-384 (FIPS-approved, 48-byte hash)
     * - **3**: SHA3-512 (FIPS-approved, 64-byte hash)
     * - **4**: PBKDF2-SHA256 (FIPS-approved, 32-byte hash, configurable iterations)
     * - **5**: Argon2id (NOT FIPS-approved, 32-byte hash, memory-hard)
     *
     * @section fips_compliance FIPS Mode Enforcement
     * - FIPS mode blocks Argon2id (value 5) and Plaintext (value 0)
     * - Enforced at SettingsValidator level before vault creation
     *
     * @note Cannot be changed after vault creation (prevents downgrade attacks)
     * @note Maps to UsernameHashService::Algorithm enum values
     * @note Default: 0 (plaintext) - **DEPRECATED**, use SHA3-256 or higher for new vaults
     * @note Vaults with username_hash_algorithm = 0 are NOT supported (breaking change in Phase 2)
     */
    uint8_t username_hash_algorithm = 0;

    /**
     * @brief Argon2id memory cost in kilobytes (V2 Format Extension - KEK Derivation Enhancement)
     *
     * Memory consumption for Argon2id key derivation.
     * Higher values increase security but consume more RAM.
     *
     * @section recommendations Memory Recommendations
     * - **8192 KB (8 MB)**: Minimum, fast unlock (~300ms)
     * - **65536 KB (64 MB)**: Default, balanced security/performance (~500ms)
     * - **262144 KB (256 MB)**: High security, slower unlock (~2s)
     * - **1048576 KB (1 GB)**: Maximum security, very slow (~8s)
     *
     * @note Only used if username_hash_algorithm = 5 (Argon2id) or KEK derivation uses Argon2id
     * @note Range: 8192-1048576 (enforced at API level)
     * @note Default: 65536 (64 MB)
     */
    uint32_t argon2_memory_kb = 65536;

    /**
     * @brief Argon2id time cost / iterations (V2 Format Extension - KEK Derivation Enhancement)
     *
     * Number of iterations for Argon2id algorithm.
     * Higher values increase security but slow down authentication.
     *
     * @section recommendations Time Cost Recommendations
     * - **1**: Minimum, very fast (~200ms with 64MB)
     * - **3**: Default, balanced (~500ms with 64MB)
     * - **5**: High security (~800ms with 64MB)
     * - **10**: Maximum security (~1.5s with 64MB)
     *
     * @note Only used if username_hash_algorithm = 5 (Argon2id) or KEK derivation uses Argon2id
     * @note Range: 1-10 (enforced at API level)
     * @note Default: 3
     */
    uint32_t argon2_iterations = 3;

    /**
     * @brief Argon2id parallelism / thread count (V2 Format Extension - KEK Derivation Enhancement)
     *
     * Number of parallel threads for Argon2id computation.
     * Higher values can improve performance on multi-core systems.
     *
     * @section recommendations Parallelism Recommendations
     * - **1**: Single-threaded (slower but deterministic)
     * - **4**: Default, good for most systems
     * - **8**: High-end systems with 8+ cores
     * - **16**: Server-class systems
     *
     * @note Only used if username_hash_algorithm = 5 (Argon2id) or KEK derivation uses Argon2id
     * @note Range: 1-16 (enforced at API level)
     * @note Default: 4
     */
    uint8_t argon2_parallelism = 4;

    /**
     * @brief YubiKey HMAC algorithm identifier (FIPS-140-3 compliant only)
     *
     * Specifies which hash algorithm to use for YubiKey challenge-response.
     * All enrolled YubiKeys must use the same algorithm.
     *
     * FIPS-140-3 Compliance:
     * - ✅ HMAC-SHA256 (0x02) - 32-byte response, FIPS-approved, minimum required
     * - ✅ HMAC-SHA512 (0x03) - 64-byte response, FIPS-approved
     * - ✅ HMAC-SHA3-256 (0x10) - 32-byte, Future YubiKey firmware
     * - ✅ HMAC-SHA3-512 (0x11) - 64-byte, Future YubiKey firmware
     *
     * @note SHA-1 (0x01) support completely removed for FIPS-140-3 compliance
     * @note New vaults default to SHA-256 (0x02)
     */
    uint8_t yubikey_algorithm = 0x02;  ///< YubiKeyAlgorithm enum value (0x02=SHA-256 minimum)

    /**
     * @brief YubiKey challenge data (size varies by algorithm)
     *
     * Random challenge generated at vault creation.
     * All users' YubiKeys are programmed with the SAME challenge-response secret.
     *
     * Challenge size: Fixed at 64 bytes (YUBIKEY_CHALLENGE_SIZE)
     * Response size: Depends on algorithm (20-64 bytes)
     *
     * @section shared_challenge Why Shared Challenge?
     * - Simpler YubiKey deployment (admin programs all keys identically)
     * - Easier backup YubiKeys (program with same secret)
     * - Matches LUKS/dm-crypt model (one challenge per vault)
     * - No per-user YubiKey slot management
     *
     * @note Only used if require_yubikey is true
     * @note Set to zero if YubiKey not required
     */
    std::array<uint8_t, 64> yubikey_challenge = {};

    /**
     * @brief Serialize to binary format for vault header
     * @return Binary representation (117 bytes)
     */
    std::vector<uint8_t> serialize() const;

    /**
     * @brief Deserialize from binary format
     * @param data Binary data (must be at least 117 bytes)
     * @return Deserialized policy, or empty optional on error
     */
    static std::optional<VaultSecurityPolicy> deserialize(const std::vector<uint8_t>& data);

    /**
     * @brief Get serialized size in bytes
     * @return 131 bytes (1 + 1 + 4 + 4 + 4 + 1 + 4 + 4 + 1 + 64 + 34)
     *
     * @section layout Serialization Layout (V2 Format)
     * - Byte 0: require_yubikey (bool)
     * - Byte 1: yubikey_algorithm (uint8_t)
     * - Bytes 2-5: min_password_length (uint32_t, big-endian)
     * - Bytes 6-9: pbkdf2_iterations (uint32_t, big-endian)
     * - Bytes 10-13: password_history_depth (uint32_t, big-endian)
     * - Byte 14: username_hash_algorithm (uint8_t) - V2 username hashing
     * - Bytes 15-18: argon2_memory_kb (uint32_t, big-endian) - V2 KEK derivation
     * - Bytes 19-22: argon2_iterations (uint32_t, big-endian) - V2 KEK derivation
     * - Byte 23: argon2_parallelism (uint8_t) - V2 KEK derivation
     * - Bytes 24-87: yubikey_challenge (64 bytes)
     * - Bytes 88-130: reserved (34 bytes - room for future V2 extensions)
     *
     * @note V2 format evolved from 121 bytes (pre-username-hashing) to 131 bytes
     * @note Backward compatibility maintained via size-based detection
     */
    static constexpr size_t SERIALIZED_SIZE = 131;

    /** @brief Reserved bytes for future expansion (first block) */
    static constexpr size_t RESERVED_BYTES_1 = 0;

    /** @brief Reserved bytes for future expansion (second block) - reduced from 52 to 43 */
    static constexpr size_t RESERVED_BYTES_2 = 43;
};

/**
 * @brief Key slot for LUKS-style multi-user authentication
 *
 * Each user has a key slot containing their credentials and wrapped DEK.
 * The DEK (Data Encryption Key) is the actual key used to encrypt vault data.
 * Each user's password derives a KEK (Key Encryption Key) that wraps the DEK.
 *
 * @section key_hierarchy Key Hierarchy
 * @code
 * User Password + Salt → PBKDF2 → KEK (Key Encryption Key)
 * KEK + YubiKey response → Final KEK
 * Final KEK → AES-256-KW → Unwraps → DEK (Data Encryption Key)
 * DEK → AES-256-GCM → Decrypts → Vault Data
 * @endcode
 *
 * @section benefits Key Slot Benefits
 * - **Independent passwords**: Each user has unique salt and KEK
 * - **Shared vault data**: All users unwrap to same DEK
 * - **Easy user management**: Add/remove users without re-encrypting vault
 * - **Password changes**: Re-wrap DEK with new KEK (fast operation)
 *
 * @note All fields are FEC-protected (Reed-Solomon) when enabled
 */
struct KeySlot {
    /**
     * @brief Key slot active flag
     *
     * If false, this slot is unused and can be recycled for new users.
     * Deletion sets this to false rather than removing the slot.
     */
    bool active = false;

    /**
     * @brief Username for this key slot (plaintext - for UI display only)
     *
     * Stored in plaintext for user identification and display in the UI.
     * Authentication uses username_hash, not this field.
     *
     * @note Stored as UTF-8 string
     * @note Max length: 255 characters (enforced at API level)
     * @note For display only - authentication uses username_hash
     */
    std::string username;

    /**
     * @brief KEK derivation algorithm for this key slot (V2 Format Extension - KEK Derivation Enhancement)
     *
     * Specifies which algorithm was used to derive the KEK (Key Encryption Key)
     * from the user's master password. This determines how to unlock the vault.
     *
     * @section kek_algorithms KEK Derivation Algorithms
     * - **0x04**: PBKDF2-HMAC-SHA256 (default, FIPS-approved)
     * - **0x05**: Argon2id (maximum security, memory-hard, NOT FIPS-approved)
     *
     * @section important_distinction CRITICAL Security Distinction
     * This field may DIFFER from username_hash_algorithm!
     *
     * **Example 1 - SHA3 Username, PBKDF2 Password:**
     * - username_hash_algorithm = 0x01 (SHA3-256 for username)
     * - kek_derivation_algorithm = 0x04 (PBKDF2 for password - automatic upgrade)
     * - Rationale: SHA3 is too fast for password protection (no brute-force resistance)
     *
     * **Example 2 - Argon2id for Both:**
     * - username_hash_algorithm = 0x05 (Argon2id for username)
     * - kek_derivation_algorithm = 0x05 (Argon2id for password - same algorithm)
     * - Rationale: Argon2id appropriate for both (slow KDF with memory hardness)
     *
     * @section sha3_prohibition SHA3 NEVER Used for KEK
     * SHA3 (0x01-0x03) is NEVER used for password → KEK derivation.
     * SHA3 is a cryptographic hash function, NOT a key derivation function.
     * It lacks computational work factor and memory hardness needed to
     * resist brute-force attacks on passwords.
     *
     * **Security Impact:**
     * - SHA3 password hashing: ~1,000,000 attempts/second (GPU)
     * - PBKDF2 600K iterations: ~1 attempt/second
     * - Argon2id 256MB: ~0.5 attempts/second + GPU resistance
     *
     * @note Cannot be 0x00-0x03 (SHA3 variants not allowed)
     * @note Default: 0x04 (PBKDF2-HMAC-SHA256)
     * @note Set at vault creation, immutable per key slot
     * @note Parameters (iterations, memory) stored in VaultSecurityPolicy
     */
    uint8_t kek_derivation_algorithm = 0x04;

    /**
     * @brief Cryptographic hash of username (up to 64 bytes)
     *
     * Stores the cryptographically hashed username to prevent username enumeration.
     * Hash algorithm is specified in VaultSecurityPolicy.username_hash_algorithm.
     *
     * @section hash_sizes Hash Sizes by Algorithm
     * - SHA3-256: 32 bytes
     * - SHA3-384: 48 bytes
     * - SHA3-512: 64 bytes
     * - PBKDF2-SHA256: 32 bytes
     * - Argon2id: 32 bytes
     *
     * @note Array size is 64 bytes (maximum for SHA3-512)
     * @note Actual used size indicated by username_hash_size
     * @note Authentication always uses this hash, never plaintext username
     */
    std::array<uint8_t, 64> username_hash = {};

    /**
     * @brief Actual size of username hash in bytes
     *
     * Indicates the number of valid bytes in username_hash array.
     *
     * @section size_interpretation Size Interpretation
     * - **32**: SHA3-256, PBKDF2-SHA256, or Argon2id
     * - **48**: SHA3-384
     * - **64**: SHA3-512
     *
     * @note Must match the output size of algorithm in VaultSecurityPolicy
     * @note Must be > 0 for all valid vaults (username hashing is mandatory)
     */
    uint8_t username_hash_size = 0;

    /**
     * @brief Random salt for username hashing (16 bytes)
     *
     * Unique per-user salt for username hashing to prevent rainbow table attacks.
     * Generated with RAND_bytes() (FIPS DRBG when FIPS mode enabled).
     *
     * @section purpose Why Salt Username Hashes?
     * - Prevents precomputed hash databases (rainbow tables)
     * - Ensures different hashes even if usernames match across vaults
     * - Adds additional entropy for PBKDF2/Argon2id algorithms
     *
     * @note Size: 16 bytes (sufficient for username hashing)
     * @note Different from password salt (32 bytes) - username salt is shorter
     */
    std::array<uint8_t, 16> username_salt = {};

    /**
     * @brief Random salt for PBKDF2 key derivation (32 bytes)
     *
     * Unique per-user salt ensures each user's KEK is different
     * even if they choose the same password.
     *
     * @note Generated with RAND_bytes() (FIPS DRBG when FIPS mode enabled)
     */
    std::array<uint8_t, 32> salt = {};

    /**
     * @brief AES-256-KW wrapped DEK (40 bytes)
     *
     * The Data Encryption Key (DEK) wrapped with the user's KEK.
     * Wrapping uses AES-256-KW (RFC 3394, NIST SP 800-38F).
     *
     * @section wrapping_process Wrapping Process
     * 1. Derive KEK from password: KEK = PBKDF2(password, salt, iterations)
     * 2. If YubiKey required: KEK = KEK XOR yubikey_response(challenge)
     * 3. Wrap DEK: wrapped_dek = AES_KW_encrypt(KEK, DEK)
     *
     * @note Wrapped DEK size: 40 bytes (32-byte DEK + 8-byte integrity tag)
     * @note Unwrapping verifies integrity (fails if KEK is incorrect)
     */
    std::array<uint8_t, 40> wrapped_dek = {};

    /**
     * @brief User's role (permissions level)
     *
     * - ADMINISTRATOR: Can manage users, change security policy (future), full vault access
     * - STANDARD_USER: Can view and edit accounts, no user management
     *
     * @note At least one ADMINISTRATOR must exist in vault (enforced at API level)
     */
    UserRole role = UserRole::STANDARD_USER;

    /**
     * @brief Force password change on next login
     *
     * Set to true when admin creates user with temporary password.
     * User MUST change password before accessing vault.
     * Set to false after successful password change.
     *
     * @section temporary_password_workflow Temporary Password Workflow
     * 1. Admin creates user with temporary password, must_change_password = true
     * 2. User logs in with temporary password
     * 3. Vault unlocks but access is blocked
     * 4. Password change dialog appears (forced)
     * 5. User sets new password, system re-wraps DEK
     * 6. must_change_password = false, user gains full access
     *
     * @note Admin never knows user's final password (user sets during first login)
     */
    bool must_change_password = false;

    /**
     * @brief Timestamp of last password change (Unix epoch seconds)
     *
     * Used for password age tracking and audit logging.
     * Set to 0 when user is created with temporary password.
     * Updated when user changes password.
     */
    int64_t password_changed_at = 0;

    /**
     * @brief Timestamp of last successful login (Unix epoch seconds)
     *
     * Used for audit logging and inactive user detection.
     * Updated on each successful vault unlock.
     */
    int64_t last_login_at = 0;

    /**
     * @brief YubiKey enrollment status for this user
     *
     * Indicates whether this user has enrolled their YubiKey for two-factor authentication.
     * Set to true after successful YubiKey enrollment.
     * Checked during authentication if VaultSecurityPolicy.require_yubikey is true.
     *
     * @note Each user has their own unique YubiKey challenge
     * @note Admin cannot enroll YubiKey for users (requires physical device)
     */
    bool yubikey_enrolled = false;

    /**
     * @brief User's unique YubiKey challenge (32 bytes for HMAC-SHA256)
     *
     * Random challenge used for HMAC challenge-response authentication.
     * Generated during YubiKey enrollment and remains constant for this user.
     * Used to derive KEK: KEK_final = KEK_password ⊕ HMAC(challenge)
     *
     * @section challenge_size Challenge Size
     * - **Size**: 32 bytes (matches HMAC-SHA256 output)
     * - **Format**: Raw binary challenge bytes
     * - **Generation**: Cryptographically secure random (RAND_bytes)
     *
     * @note Empty (all zeros) if yubikey_enrolled is false
     * @note Challenge is unique per user, not shared across users
     * @note Must be same size as algorithm response for proper XOR combining
     */
    std::array<uint8_t, 32> yubikey_challenge = {};

    /**
     * @brief YubiKey device serial number (optional)
     *
     * Serial number of the enrolled YubiKey device for audit logging.
     * Can be used for device verification warnings (not enforced).
     * Empty string if not available or YubiKey not enrolled.
     */
    std::string yubikey_serial;

    /**
     * @brief Timestamp of YubiKey enrollment (Unix epoch seconds)
     *
     * Records when this user enrolled their YubiKey.
     * Set to 0 if YubiKey not enrolled.
     * Used for audit logging and compliance reporting.
     */
    int64_t yubikey_enrolled_at = 0;

    /**
     * @brief Encrypted YubiKey FIDO2 PIN (variable length, max 64 bytes encrypted)
     *
     * User's YubiKey FIDO2 PIN encrypted with their KEK using AES-256-GCM.
     * Encrypted during vault creation or YubiKey enrollment.
     * Decrypted automatically when vault is opened with user's password.
     *
     * @section encryption_scheme PIN Encryption
     * - **Algorithm**: AES-256-GCM (authenticated encryption)
     * - **Key**: User's KEK (derived from password + salt)
     * - **IV**: 12 random bytes (prepended to ciphertext)
     * - **Tag**: 16 bytes (appended to ciphertext)
     * - **Format**: [IV(12) || ciphertext(PIN_LEN) || tag(16)]
     *
     * @section why_encrypt_pin Why Encrypt PIN?
     * - Each user has their own YubiKey with unique PIN
     * - PIN stored per-user (not shared via environment variable)
     * - User enters PIN only once during enrollment
     * - PIN automatically available when vault opens
     * - More secure than environment variable
     *
     * @note Empty (all zeros) if yubikey_enrolled is false
     * @note PIN length: 6-48 characters (FIDO2 spec)
     * @note Total encrypted size: 12 (IV) + PIN_LEN + 16 (tag)
     */
    std::vector<uint8_t> yubikey_encrypted_pin;

    /**
     * @brief FIDO2 credential ID for this user (48 bytes for YubiKey 5)
     *
     * Unique credential identifier created during makeCredential operation.
     * Must be provided during getAssertion (challenge-response) to identify
     * which credential to use for authentication.
     *
     * @section credential_lifecycle Credential Lifecycle
     * 1. Created during YubiKey enrollment with create_credential()
     * 2. Stored in user's KeySlot (persistent across vault open/close)
     * 3. Loaded and set with set_credential() when opening vault
     * 4. Used for all challenge-response operations for this user
     *
     * @note Empty (all zeros) if yubikey_enrolled is false
     * @note Size is typically 48 bytes for YubiKey 5 series
     * @note Required for FIDO2 hmac-secret extension
     */
    std::vector<uint8_t> yubikey_credential_id;

    /**
     * @brief Password history for reuse prevention
     *
     * Stores hashes of previous passwords to prevent password reuse.
     * Managed as a ring buffer with FIFO eviction when depth limit reached.
     *
     * @section usage_workflow Password Change Workflow
     * 1. User attempts to change password
     * 2. System checks new password against all entries in password_history
     * 3. If match found → Reject with "Password was used previously"
     * 4. If no match → Accept, hash new password, add to history
     * 5. If history.size() > policy.depth → Remove oldest entry (FIFO)
     *
     * @section depth_synchronization Depth Synchronization
     * - Max size governed by VaultSecurityPolicy.password_history_depth
     * - If admin decreases depth, oldest entries are trimmed on next write
     * - If admin increases depth, new entries are added normally
     * - If depth set to 0, password checking is disabled (history retained but not checked)
     *
     * @note Empty when user first created or when history disabled
     * @note Maximum size: 24 entries (configurable via policy)
     */
    std::vector<PasswordHistoryEntry> password_history;

    /**
     * @brief Serialize to binary format for vault header
     * @return Binary representation (variable length due to username)
     */
    std::vector<uint8_t> serialize() const;

    /**
     * @brief Deserialize from binary format
     * @param data Binary data
     * @param offset Offset in data to start reading
     * @return Pair of (deserialized slot, bytes consumed), or empty optional on error
     */
    static std::optional<std::pair<KeySlot, size_t>> deserialize(
        const std::vector<uint8_t>& data, size_t offset);

    /**
     * @brief Calculate serialized size for this key slot
     * @return Size in bytes (variable due to username, username hash fields, etc.)
     *
     * @section base_fields Base Fields (Phase 2 - with username hashing)
     * - 1 byte: active flag
     * - 1 byte: username length
     * - N bytes: username (UTF-8, 0-255 bytes)
     * - 64 bytes: username_hash (fixed array)
     * - 1 byte: username_hash_size
     * - 16 bytes: username_salt (fixed array)
     * - 32 bytes: salt (password derivation)
     * - 40 bytes: wrapped_dek
     * - ... (other fields follow)
     */
    size_t calculate_serialized_size() const;

    /**
     * @brief Minimum serialized size (empty username, no password history)
     * @return Base size: 221 bytes (V2 format with KEK derivation enhancement)
     *
     * @note Evolved from 220 bytes (added kek_derivation_algorithm field)
     * @note Backward compatible: Deserializer detects format via heuristic
     */
    static constexpr size_t MIN_SERIALIZED_SIZE = 221;
};

/**
 * @brief Current user session information
 *
 * Created after successful vault authentication.
 * Tracks current user's identity, role, and session state.
 */
struct UserSession {
    /**
     * @brief Authenticated username
     */
    std::string username;

    /**
     * @brief User's role (for permission checks)
     */
    UserRole role;

    /**
     * @brief Password change required before vault access
     *
     * If true, all vault operations are blocked until password is changed.
     * UI should immediately show password change dialog.
     */
    bool password_change_required = false;

    /**
     * @brief YubiKey enrollment required before vault access
     *
     * If true, user must enroll YubiKey before accessing vault.
     * Set when vault policy requires YubiKey but user doesn't have one enrolled.
     * UI should show YubiKey enrollment dialog after password change.
     */
    bool requires_yubikey_enrollment = false;

    /**
     * @brief Session creation timestamp (Unix epoch seconds)
     */
    int64_t session_started_at = 0;

    /**
     * @brief Check if user is administrator
     * @return true if user has admin role
     */
    bool is_admin() const {
        return role == UserRole::ADMINISTRATOR;
    }

    /**
     * @brief Check if vault access is allowed
     * @return true if user can access vault (no password change or YubiKey enrollment required)
     */
    bool can_access_vault() const {
        return !password_change_required && !requires_yubikey_enrollment;
    }
};

/**
 * @brief Vault header for multi-user (V2) format
 *
 * Contains security policy and all user key slots.
 * This entire structure is FEC-protected (Reed-Solomon) when enabled.
 *
 * @section header_layout Binary Layout
 * @code
 * +------------------+
 * | Magic: "VAUL"    | 4 bytes
 * | Version: 2       | 4 bytes
 * | PBKDF2 Iters     | 4 bytes
 * | Header Size      | 4 bytes (including this field)
 * +------------------+
 * | Security Policy  | 117 bytes
 * +------------------+
 * | Num Key Slots    | 1 byte (0-32)
 * +------------------+
 * | Key Slot 0       | Variable
 * | Key Slot 1       | Variable
 * | ...              |
 * +------------------+
 * | [FEC Parity]     | Optional (if RS enabled)
 * +------------------+
 * | Encrypted Data   | Variable
 * | (vault records)  |
 * +------------------+
 * @endcode
 */
struct VaultHeaderV2 {
    /**
     * @brief Vault security policy
     */
    VaultSecurityPolicy security_policy;

    /**
     * @brief Active key slots (up to 32)
     */
    std::vector<KeySlot> key_slots;

    /**
     * @brief Maximum number of key slots per vault
     *
     * Matches LUKS2 default (32 slots).
     * Can be increased if needed, but 32 is sufficient for most use cases.
     */
    static constexpr size_t MAX_KEY_SLOTS = 32;

    /**
     * @brief Serialize to binary format
     * @return Binary representation
     */
    std::vector<uint8_t> serialize() const;

    /**
     * @brief Deserialize from binary format
     * @param data Binary data
     * @return Deserialized header, or empty optional on error
     */
    static std::optional<VaultHeaderV2> deserialize(const std::vector<uint8_t>& data);

    /**
     * @brief Calculate total serialized size
     * @return Size in bytes (variable due to usernames)
     */
    size_t calculate_serialized_size() const;
};

} // namespace KeepTower

#endif // MULTIUSERTYPES_H
