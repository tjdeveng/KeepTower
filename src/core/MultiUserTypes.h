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
     * @brief YubiKey HMAC-SHA1 challenge (shared by all users)
     *
     * Random 64-byte challenge generated at vault creation.
     * All users' YubiKeys are programmed with the SAME challenge-response secret.
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
     * @return 117 bytes (1 + 4 + 4 + 4 + 64 + alignment padding)
     */
    static constexpr size_t SERIALIZED_SIZE = 117;

    /** @brief Reserved bytes for future expansion (first block) */
    static constexpr size_t RESERVED_BYTES_1 = 4;

    /** @brief Reserved bytes for future expansion (second block) */
    static constexpr size_t RESERVED_BYTES_2 = 40;
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
     * @brief Username for this key slot (plaintext)
     *
     * Used for user selection during authentication.
     * Must be unique within vault.
     *
     * @note Stored as UTF-8 string
     * @note Max length: 255 characters (enforced at API level)
     */
    std::string username;

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
     * @return Size in bytes (variable due to username)
     */
    size_t calculate_serialized_size() const;

    /**
     * @brief Minimum serialized size (empty username)
     * @return 131 bytes (1 + 1 + 255 + 32 + 40 + 1 + 1 + 8 + 8)
     */
    static constexpr size_t MIN_SERIALIZED_SIZE = 131;
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
     * @return true if user can access vault (no password change required)
     */
    bool can_access_vault() const {
        return !password_change_required;
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
