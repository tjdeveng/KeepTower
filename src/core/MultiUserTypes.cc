// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "MultiUserTypes.h"
#include "../utils/Log.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/kdf.h>
#include <cstring>
#include <algorithm>

namespace KeepTower {

// ============================================================================
// PasswordHistoryEntry Secure Destruction and Serialization
// ============================================================================

PasswordHistoryEntry::~PasswordHistoryEntry() {
    // Securely clear the password hash to prevent memory dumps
    // Salt is not sensitive (it's stored in plaintext in vault)
    OPENSSL_cleanse(hash.data(), hash.size());
}

std::vector<uint8_t> PasswordHistoryEntry::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(SERIALIZED_SIZE);

    // Bytes 0-7: timestamp (big-endian int64_t)
    for (unsigned int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((timestamp >> ((7 - i) * 8)) & 0xFF));
    }

    // Bytes 8-39: salt (32 bytes)
    result.insert(result.end(), salt.begin(), salt.end());

    // Bytes 40-87: hash (48 bytes)
    result.insert(result.end(), hash.begin(), hash.end());

    return result;
}

std::optional<PasswordHistoryEntry> PasswordHistoryEntry::deserialize(
    const std::vector<uint8_t>& data, size_t offset) {

    if (offset + SERIALIZED_SIZE > data.size()) {
        Log::error("PasswordHistoryEntry: Insufficient data at offset {} (need {}, have {})",
                   offset, SERIALIZED_SIZE, data.size() - offset);
        return std::nullopt;
    }

    PasswordHistoryEntry entry;
    size_t pos = offset;

    // Bytes 0-7: timestamp (big-endian)
    entry.timestamp = 0;
    for (int i = 0; i < 8; ++i) {
        entry.timestamp = (entry.timestamp << 8) | data[pos++];
    }

    // Bytes 8-39: salt
    std::copy(data.begin() + pos, data.begin() + pos + 32, entry.salt.begin());
    pos += 32;

    // Bytes 40-87: hash
    std::copy(data.begin() + pos, data.begin() + pos + 48, entry.hash.begin());

    return entry;
}

// ============================================================================
// VaultSecurityPolicy Serialization
// ============================================================================

std::vector<uint8_t> VaultSecurityPolicy::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(SERIALIZED_SIZE);

    // Byte 0: require_yubikey flag
    result.push_back(require_yubikey ? 1 : 0);

    // Byte 1: yubikey_algorithm (YubiKeyAlgorithm enum)
    result.push_back(yubikey_algorithm);

    // Bytes 2-5: min_password_length (big-endian)
    result.push_back(static_cast<uint8_t>((min_password_length >> 24) & 0xFF));
    result.push_back(static_cast<uint8_t>((min_password_length >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((min_password_length >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(min_password_length & 0xFF));

    // Bytes 6-9: pbkdf2_iterations (big-endian)
    result.push_back(static_cast<uint8_t>((pbkdf2_iterations >> 24) & 0xFF));
    result.push_back(static_cast<uint8_t>((pbkdf2_iterations >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((pbkdf2_iterations >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(pbkdf2_iterations & 0xFF));

    // Bytes 10-13: password_history_depth (big-endian)
    result.push_back(static_cast<uint8_t>((password_history_depth >> 24) & 0xFF));
    result.push_back(static_cast<uint8_t>((password_history_depth >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((password_history_depth >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(password_history_depth & 0xFF));

    // Byte 14: username_hash_algorithm (V2 username hashing extension)
    result.push_back(username_hash_algorithm);

    // Bytes 15-18: argon2_memory_kb (V2 KEK derivation extension)
    result.push_back(static_cast<uint8_t>((argon2_memory_kb >> 24) & 0xFF));
    result.push_back(static_cast<uint8_t>((argon2_memory_kb >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((argon2_memory_kb >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(argon2_memory_kb & 0xFF));

    // Bytes 19-22: argon2_iterations (V2 KEK derivation extension)
    result.push_back(static_cast<uint8_t>((argon2_iterations >> 24) & 0xFF));
    result.push_back(static_cast<uint8_t>((argon2_iterations >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((argon2_iterations >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(argon2_iterations & 0xFF));

    // Byte 23: argon2_parallelism (V2 KEK derivation extension)
    result.push_back(argon2_parallelism);

    // Bytes 24-87: yubikey_challenge (64 bytes)
    result.insert(result.end(), yubikey_challenge.begin(), yubikey_challenge.end());

    // Byte 88: username_hash_algorithm_previous (migration support)
    result.push_back(username_hash_algorithm_previous);

    // Bytes 89-96: migration_started_at (big-endian uint64_t)
    result.push_back(static_cast<uint8_t>((migration_started_at >> 56) & 0xFF));
    result.push_back(static_cast<uint8_t>((migration_started_at >> 48) & 0xFF));
    result.push_back(static_cast<uint8_t>((migration_started_at >> 40) & 0xFF));
    result.push_back(static_cast<uint8_t>((migration_started_at >> 32) & 0xFF));
    result.push_back(static_cast<uint8_t>((migration_started_at >> 24) & 0xFF));
    result.push_back(static_cast<uint8_t>((migration_started_at >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((migration_started_at >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(migration_started_at & 0xFF));

    // Byte 97: migration_flags
    result.push_back(migration_flags);

    // Bytes 98-140: reserved for future use (43 bytes)
    for (size_t i = 0; i < RESERVED_BYTES_2; ++i) {
        result.push_back(0);
    }

    return result;
}

std::optional<VaultSecurityPolicy> VaultSecurityPolicy::deserialize(const std::vector<uint8_t>& data) {
    // V2 format evolved over development (no production vaults exist):
    // - Early V2 (121 bytes): Basic multi-user, no username hashing
    // - Mid V2 (122 bytes): Added username_hash_algorithm field
    // - Current V2 (131 bytes): Added Argon2id parameters for KEK derivation
    const size_t EARLY_V2_SIZE = 121;    // Pre-username-hashing
    const size_t MID_V2_SIZE = 122;      // Username hashing only
    const size_t CURRENT_V2_SIZE = 131;  // Full Argon2id support

    if (data.size() < EARLY_V2_SIZE) {
        Log::error("VaultSecurityPolicy: Insufficient data for deserialization (need at least {}, got {})",
                   EARLY_V2_SIZE, data.size());
        return std::nullopt;
    }

    VaultSecurityPolicy policy;
    size_t offset = 0;

    // Byte 0: require_yubikey
    policy.require_yubikey = (data[0] != 0);

    // Byte 1: yubikey_algorithm (must be FIPS-approved: 0x02=SHA256, 0x03=SHA512, etc.)
    policy.yubikey_algorithm = data[1];

    // Validate algorithm is FIPS-approved (SHA-256 minimum)
    if (policy.yubikey_algorithm < 0x02) {
        Log::error("VaultSecurityPolicy: Invalid or deprecated algorithm: 0x{:02X} (SHA-256 minimum required for FIPS-140-3)",
                   policy.yubikey_algorithm);
        return std::nullopt;
    }

    // Bytes 2-5: min_password_length (big-endian)
    policy.min_password_length = (static_cast<uint32_t>(data[2]) << 24) |
                                 (static_cast<uint32_t>(data[3]) << 16) |
                                 (static_cast<uint32_t>(data[4]) << 8) |
                                 static_cast<uint32_t>(data[5]);
    offset = 6;

    // Validate min_password_length
    if (policy.min_password_length < 8 || policy.min_password_length > 128) {
        Log::error("VaultSecurityPolicy: Invalid min_password_length: {}",
                   policy.min_password_length);
        return std::nullopt;
    }

    // Next 4 bytes: pbkdf2_iterations
    policy.pbkdf2_iterations = (static_cast<uint32_t>(data[offset]) << 24) |
                               (static_cast<uint32_t>(data[offset + 1]) << 16) |
                               (static_cast<uint32_t>(data[offset + 2]) << 8) |
                               static_cast<uint32_t>(data[offset + 3]);
    offset += 4;

    // Next 4 bytes: password_history_depth
    policy.password_history_depth = (static_cast<uint32_t>(data[offset]) << 24) |
                                    (static_cast<uint32_t>(data[offset + 1]) << 16) |
                                    (static_cast<uint32_t>(data[offset + 2]) << 8) |
                                    static_cast<uint32_t>(data[offset + 3]);
    offset += 4;

    // Backward compatibility: Check if this is mid V2 format (with username hashing) or later
    if (data.size() >= MID_V2_SIZE) {
        // Byte 14: username_hash_algorithm
        policy.username_hash_algorithm = data[offset];
        offset += 1;

        // Validate algorithm (0-5 are valid values)
        if (policy.username_hash_algorithm > 5) {
            Log::error("VaultSecurityPolicy: Invalid username_hash_algorithm: {}",
                       policy.username_hash_algorithm);
            return std::nullopt;
        }
    } else {
        // Early V2 format: default to plaintext (0) for backward compatibility
        policy.username_hash_algorithm = 0;
        Log::info("VaultSecurityPolicy: Early V2 format detected (no username hashing), defaulting username_hash_algorithm to 0");
    }

    // Check for current V2 format (with Argon2id KEK parameters)
    if (data.size() >= CURRENT_V2_SIZE) {
        // Bytes 15-18: argon2_memory_kb
        policy.argon2_memory_kb = (static_cast<uint32_t>(data[offset]) << 24) |
                                  (static_cast<uint32_t>(data[offset + 1]) << 16) |
                                  (static_cast<uint32_t>(data[offset + 2]) << 8) |
                                  static_cast<uint32_t>(data[offset + 3]);
        offset += 4;

        // Bytes 19-22: argon2_iterations
        policy.argon2_iterations = (static_cast<uint32_t>(data[offset]) << 24) |
                                   (static_cast<uint32_t>(data[offset + 1]) << 16) |
                                   (static_cast<uint32_t>(data[offset + 2]) << 8) |
                                   static_cast<uint32_t>(data[offset + 3]);
        offset += 4;

        // Byte 23: argon2_parallelism
        policy.argon2_parallelism = data[offset];
        offset += 1;

        // Validate Argon2 parameters
        if (policy.argon2_memory_kb < 8192 || policy.argon2_memory_kb > 1048576) {
            Log::error("VaultSecurityPolicy: Invalid argon2_memory_kb: {} (range: 8192-1048576)",
                       policy.argon2_memory_kb);
            return std::nullopt;
        }
        if (policy.argon2_iterations < 1 || policy.argon2_iterations > 10) {
            Log::error("VaultSecurityPolicy: Invalid argon2_iterations: {} (range: 1-10)",
                       policy.argon2_iterations);
            return std::nullopt;
        }
        if (policy.argon2_parallelism < 1 || policy.argon2_parallelism > 16) {
            Log::error("VaultSecurityPolicy: Invalid argon2_parallelism: {} (range: 1-16)",
                       policy.argon2_parallelism);
            return std::nullopt;
        }
    } else {
        // Mid V2 format: use default Argon2id parameters
        policy.argon2_memory_kb = 65536;  // 64 MB default
        policy.argon2_iterations = 3;     // Time cost default
        policy.argon2_parallelism = 4;    // Thread count default
        if (data.size() >= MID_V2_SIZE) {
            Log::info("VaultSecurityPolicy: Mid V2 format detected (username hashing only), using default Argon2id parameters");
        }
    }

    // Next 64 bytes: yubikey_challenge
    std::copy(data.begin() + offset, data.begin() + offset + 64, policy.yubikey_challenge.begin());
    offset += 64;

    // Check for migration support format (141 bytes)
    const size_t MIGRATION_V2_SIZE = 141;
    if (data.size() >= MIGRATION_V2_SIZE) {
        // Byte 88: username_hash_algorithm_previous
        policy.username_hash_algorithm_previous = data[offset];
        offset += 1;

        // Validate previous algorithm (0-5 are valid, or 0 for no migration)
        if (policy.username_hash_algorithm_previous > 5) {
            Log::error("VaultSecurityPolicy: Invalid username_hash_algorithm_previous: {}",
                       policy.username_hash_algorithm_previous);
            return std::nullopt;
        }

        // Bytes 89-96: migration_started_at (big-endian uint64_t)
        policy.migration_started_at = 0;
        for (int i = 0; i < 8; ++i) {
            policy.migration_started_at = (policy.migration_started_at << 8) | data[offset++];
        }

        // Byte 97: migration_flags
        policy.migration_flags = data[offset];
        offset += 1;

        // Validate migration flags (reserved bits must be 0)
        if ((policy.migration_flags & 0xFC) != 0) {  // Bits 2-7 must be 0
            Log::warning("VaultSecurityPolicy: Reserved migration flag bits are set: 0x{:02X}",
                        policy.migration_flags);
            // Don't fail - just clear reserved bits for forward compatibility
            policy.migration_flags &= 0x03;
        }

        // Validation: If migration is not active, previous algo and timestamp should be 0
        bool migration_active = (policy.migration_flags & 0x01) != 0;
        if (!migration_active && (policy.username_hash_algorithm_previous != 0 || policy.migration_started_at != 0)) {
            Log::warning("VaultSecurityPolicy: Migration not active but previous algo or timestamp set - clearing");
            policy.username_hash_algorithm_previous = 0;
            policy.migration_started_at = 0;
        }
    } else {
        // Pre-migration format: use defaults (no migration active)
        policy.username_hash_algorithm_previous = 0;
        policy.migration_started_at = 0;
        policy.migration_flags = 0;
        if (data.size() >= CURRENT_V2_SIZE) {
            Log::info("VaultSecurityPolicy: Pre-migration V2 format detected, using migration defaults");
        }
    }

    // Remaining bytes: reserved (skip)

    // Validation

    if (policy.pbkdf2_iterations < 100000 || policy.pbkdf2_iterations > 1000000) {
        Log::error("VaultSecurityPolicy: Invalid pbkdf2_iterations: {}", policy.pbkdf2_iterations);
        return std::nullopt;
    }

    if (policy.password_history_depth > 24) {
        Log::error("VaultSecurityPolicy: Invalid password_history_depth: {}", policy.password_history_depth);
        return std::nullopt;
    }

    return policy;
}

// ============================================================================
// KeySlot Serialization
// ============================================================================

size_t KeySlot::calculate_serialized_size() const {
    // KeySlot format (V2 with KEK derivation enhancement):
    // 1 byte: active flag
    // 1 byte: kek_derivation_algorithm (V2 KEK derivation extension)
    // 64 bytes: username_hash (fixed array)
    // 1 byte: username_hash_size
    // 16 bytes: username_salt (fixed array)
    // 32 bytes: salt
    // 40 bytes: wrapped_dek
    // 1 byte: role
    // 1 byte: must_change_password
    // 8 bytes: password_changed_at
    // 8 bytes: last_login_at
    // 1 byte: yubikey_enrolled
    // 32 bytes: yubikey_challenge
    // 1 byte: yubikey_serial length
    // N bytes: yubikey_serial
    // 8 bytes: yubikey_enrolled_at
    // 2 bytes: yubikey_encrypted_pin length (uint16_t)
    // N bytes: yubikey_encrypted_pin
    // 2 bytes: yubikey_credential_id length (uint16_t)
    // N bytes: yubikey_credential_id
    // 1 byte: password_history count
    // N * 88 bytes: password_history entries
    // 1 byte: migration_status (migration support)
    // 8 bytes: migrated_at (migration support)

    size_t base_size = 1 + 1 + 64 + 1 + 16 + 32 + 40 + 1 + 1 + 8 + 8 + 1 + 32 + 1 + yubikey_serial.size() + 8 + 2 + yubikey_encrypted_pin.size() + 2 + yubikey_credential_id.size() + 1 + 1 + 8;
    size_t history_size = password_history.size() * PasswordHistoryEntry::SERIALIZED_SIZE;
    return base_size + history_size;
}

std::vector<uint8_t> KeySlot::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(calculate_serialized_size());

    Log::debug("KeySlot::serialize: active={}, hash_size={}, kek_algo=0x{:02X}, yubikey_enrolled={}",
              active, username_hash_size, kek_derivation_algorithm, yubikey_enrolled);

    // Byte 0: active flag
    result.push_back(active ? 1 : 0);

    // Byte 1: kek_derivation_algorithm (V2 KEK derivation extension)
    result.push_back(kek_derivation_algorithm);

    // Next 64 bytes: username_hash (for secure authentication, no plaintext username stored)
    result.insert(result.end(), username_hash.begin(), username_hash.end());

    // Next byte: username_hash_size (Phase 2)
    result.push_back(username_hash_size);

    // Next 16 bytes: username_salt (Phase 2)
    result.insert(result.end(), username_salt.begin(), username_salt.end());

    // Next 32 bytes: salt (password derivation)
    result.insert(result.end(), salt.begin(), salt.end());

    // Next 40 bytes: wrapped_dek
    result.insert(result.end(), wrapped_dek.begin(), wrapped_dek.end());

    // Next byte: role
    result.push_back(static_cast<uint8_t>(role));

    // Next byte: must_change_password
    result.push_back(must_change_password ? 1 : 0);

    // Next 8 bytes: password_changed_at (big-endian)
    for (unsigned int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((password_changed_at >> ((7 - i) * 8)) & 0xFF));
    }

    // Next 8 bytes: last_login_at (big-endian)
    for (unsigned int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((last_login_at >> ((7 - i) * 8)) & 0xFF));
    }

    // Next byte: yubikey_enrolled
    result.push_back(yubikey_enrolled ? 1 : 0);

    // Next 32 bytes: yubikey_challenge
    result.insert(result.end(), yubikey_challenge.begin(), yubikey_challenge.end());

    // Next byte: yubikey_serial length
    if (yubikey_serial.size() > 255) {
        Log::error("KeySlot: YubiKey serial too long (max 255 bytes): {}", yubikey_serial.size());
        return {};
    }
    result.push_back(static_cast<uint8_t>(yubikey_serial.size()));

    // Next N bytes: yubikey_serial
    result.insert(result.end(), yubikey_serial.begin(), yubikey_serial.end());

    // Next 8 bytes: yubikey_enrolled_at (big-endian)
    for (unsigned int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((yubikey_enrolled_at >> ((7 - i) * 8)) & 0xFF));
    }

    // Next 2 bytes: yubikey_encrypted_pin length (big-endian uint16_t)
    uint16_t encrypted_pin_len = static_cast<uint16_t>(yubikey_encrypted_pin.size());
    result.push_back(static_cast<uint8_t>((encrypted_pin_len >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(encrypted_pin_len & 0xFF));

    // Next N bytes: yubikey_encrypted_pin
    result.insert(result.end(), yubikey_encrypted_pin.begin(), yubikey_encrypted_pin.end());

    // Next 2 bytes: yubikey_credential_id length (big-endian uint16_t)
    uint16_t credential_id_len = static_cast<uint16_t>(yubikey_credential_id.size());
    result.push_back(static_cast<uint8_t>((credential_id_len >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(credential_id_len & 0xFF));

    // Next N bytes: yubikey_credential_id
    result.insert(result.end(), yubikey_credential_id.begin(), yubikey_credential_id.end());

    // Next byte: password_history count
    if (password_history.size() > 255) {
        Log::error("KeySlot: Password history too long (max 255 entries): {}", password_history.size());
        return {};
    }
    result.push_back(static_cast<uint8_t>(password_history.size()));

    // Next N * 88 bytes: password_history entries
    for (const auto& entry : password_history) {
        auto entry_data = entry.serialize();
        result.insert(result.end(), entry_data.begin(), entry_data.end());
    }

    // Next byte: migration_status (migration support)
    result.push_back(migration_status);

    // Next 8 bytes: migrated_at (big-endian uint64_t)
    for (unsigned int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((migrated_at >> ((7 - i) * 8)) & 0xFF));
    }

    return result;
}

std::optional<std::pair<KeySlot, size_t>> KeySlot::deserialize(
    const std::vector<uint8_t>& data, size_t offset) {

    if (offset >= data.size()) {
        Log::error("KeySlot: Insufficient data for header at offset {}", offset);
        return std::nullopt;
    }

    KeySlot slot;
    size_t pos = offset;

    auto it = [&data](size_t idx) {
        return data.begin() + static_cast<std::vector<uint8_t>::difference_type>(idx);
    };

    // Byte 0: active flag
    slot.active = (data[pos] != 0);
    ++pos;

    // Username not stored on disk (security) - will be populated in-memory after authentication
    slot.username.clear();

    // Detect if this is current V2 format (with kek_derivation_algorithm field)
    // Heuristic: If next byte is 0x04 or 0x05, it's likely kek_derivation_algorithm
    // Otherwise, it's the first byte of username_hash (older V2 format)
    if (pos < data.size() && (data[pos] == 0x04 || data[pos] == 0x05)) {
        // Current V2 format with kek_derivation_algorithm
        slot.kek_derivation_algorithm = data[pos++];
    } else {
        // Older V2 format: default to PBKDF2
        slot.kek_derivation_algorithm = 0x04;  // PBKDF2-HMAC-SHA256 default
    }

    // Next 64 bytes: username_hash
    if (pos + 64 > data.size()) {
        Log::error("KeySlot: Insufficient data for username_hash");
        return std::nullopt;
    }
    std::copy(it(pos), it(pos + 64), slot.username_hash.begin());
    pos += 64;

    // Next byte: username_hash_size
    if (pos + 1 > data.size()) {
        Log::error("KeySlot: Insufficient data for username_hash_size");
        return std::nullopt;
    }
    slot.username_hash_size = data[pos++];
    if (slot.username_hash_size > 64) {
        Log::error("KeySlot: Invalid username_hash_size: {}", slot.username_hash_size);
        return std::nullopt;
    }

    // Next 16 bytes: username_salt
    if (pos + 16 > data.size()) {
        Log::error("KeySlot: Insufficient data for username_salt");
        return std::nullopt;
    }
    std::copy(it(pos), it(pos + 16), slot.username_salt.begin());
    pos += 16;

    // Check remaining data for core fields
    if (pos + 32 + 40 + 1 + 1 + 8 + 8 > data.size()) {
        Log::error("KeySlot: Insufficient data for core fields");
        return std::nullopt;
    }

    // Salt (32 bytes - password derivation)
    std::copy(it(pos), it(pos + 32), slot.salt.begin());
    pos += 32;

    // Wrapped DEK (40 bytes)
    std::copy(it(pos), it(pos + 40), slot.wrapped_dek.begin());
    pos += 40;

    // Role
    uint8_t role_byte = data[pos++];
    if (role_byte > 1) {
        Log::error("KeySlot: Invalid role value: {}", role_byte);
        return std::nullopt;
    }
    slot.role = static_cast<UserRole>(role_byte);

    // must_change_password
    slot.must_change_password = (data[pos++] != 0);

    // password_changed_at (8 bytes, big-endian)
    slot.password_changed_at = 0;
    for (int i = 0; i < 8; ++i) {
        slot.password_changed_at = (slot.password_changed_at << 8) | data[pos++];
    }

    // last_login_at (8 bytes, big-endian)
    slot.last_login_at = 0;
    for (int i = 0; i < 8; ++i) {
        slot.last_login_at = (slot.last_login_at << 8) | data[pos++];
    }

    // Check if we have YubiKey fields (backward compatibility)
    // If not enough data, treat as old format (no YubiKey fields)
    if (pos + 1 + 32 + 1 > data.size()) {
        // Old format without YubiKey fields - use defaults
        slot.yubikey_enrolled = false;
        slot.yubikey_challenge = {};
        slot.yubikey_serial = "";
        slot.yubikey_enrolled_at = 0;
        size_t bytes_consumed = pos - offset;
        return std::make_pair(slot, bytes_consumed);
    }

    // yubikey_enrolled (1 byte)
    slot.yubikey_enrolled = (data[pos++] != 0);

    // yubikey_challenge (32 bytes)
    std::copy(it(pos), it(pos + 32), slot.yubikey_challenge.begin());
    pos += 32;

    // yubikey_serial length (1 byte)
    uint8_t yubikey_serial_len = data[pos++];

    // Check if we have enough data for serial + timestamp
    if (pos + yubikey_serial_len + 8 > data.size()) {
        Log::error("KeySlot: Insufficient data for YubiKey serial and timestamp");
        return std::nullopt;
    }

    // yubikey_serial (N bytes)
    slot.yubikey_serial.assign(it(pos), it(pos + yubikey_serial_len));
    pos += yubikey_serial_len;

    // yubikey_enrolled_at (8 bytes, big-endian)
    slot.yubikey_enrolled_at = 0;
    for (int i = 0; i < 8; ++i) {
        slot.yubikey_enrolled_at = (slot.yubikey_enrolled_at << 8) | data[pos++];
    }

    // Check if we have yubikey_encrypted_pin field (backward compatibility)
    // If not enough data, treat as old format (no encrypted PIN)
    if (pos + 2 > data.size()) {
        // Old format without encrypted PIN - use empty vectors
        slot.yubikey_encrypted_pin.clear();
        slot.yubikey_credential_id.clear();
        slot.password_history.clear();
        size_t bytes_consumed = pos - offset;
        return std::make_pair(slot, bytes_consumed);
    }

    // yubikey_encrypted_pin length (2 bytes, big-endian uint16_t)
    uint16_t encrypted_pin_len = (static_cast<uint16_t>(data[pos]) << 8) | data[pos + 1];
    pos += 2;

    // Check if we have enough data for encrypted PIN
    if (pos + encrypted_pin_len > data.size()) {
        Log::error("KeySlot: Insufficient data for YubiKey encrypted PIN");
        return std::nullopt;
    }

    // yubikey_encrypted_pin (N bytes)
    slot.yubikey_encrypted_pin.assign(it(pos), it(pos + encrypted_pin_len));
    pos += encrypted_pin_len;

    // Check if we have yubikey_credential_id field (backward compatibility)
    if (pos + 2 > data.size()) {
        // Old format without credential ID - use empty vector
        slot.yubikey_credential_id.clear();
        slot.password_history.clear();
        size_t bytes_consumed = pos - offset;
        return std::make_pair(slot, bytes_consumed);
    }

    // yubikey_credential_id length (2 bytes, big-endian uint16_t)
    uint16_t credential_id_len = (static_cast<uint16_t>(data[pos]) << 8) | data[pos + 1];
    pos += 2;

    // Check if we have enough data for credential ID
    if (pos + credential_id_len > data.size()) {
        Log::error("KeySlot: Insufficient data for YubiKey credential ID");
        return std::nullopt;
    }

    // yubikey_credential_id (N bytes)
    slot.yubikey_credential_id.assign(it(pos), it(pos + credential_id_len));
    pos += credential_id_len;

    // Check if we have password_history field (backward compatibility)
    // If not enough data, treat as old format (no password history)
    if (pos + 1 > data.size()) {
        // Old format without password history - use empty vector
        slot.password_history.clear();
        size_t bytes_consumed = pos - offset;
        return std::make_pair(slot, bytes_consumed);
    }

    // password_history count (1 byte)
    uint8_t history_count = data[pos++];

    // Check if we have enough data for all history entries
    size_t history_bytes_needed = history_count * PasswordHistoryEntry::SERIALIZED_SIZE;
    if (pos + history_bytes_needed > data.size()) {
        Log::error("KeySlot: Insufficient data for password history (need {}, have {})",
                   history_bytes_needed, data.size() - pos);
        return std::nullopt;
    }

    // Deserialize password_history entries
    slot.password_history.clear();
    slot.password_history.reserve(history_count);
    for (uint8_t i = 0; i < history_count; ++i) {
        auto entry_opt = PasswordHistoryEntry::deserialize(data, pos);
        if (!entry_opt) {
            Log::error("KeySlot: Failed to deserialize password history entry {}", i);
            return std::nullopt;
        }
        slot.password_history.push_back(*entry_opt);
        pos += PasswordHistoryEntry::SERIALIZED_SIZE;
    }

    // Check if we have migration fields (backward compatibility)
    // Old format (pre-migration) won't have these fields
    if (pos + 1 + 8 > data.size()) {
        // Old format without migration fields - use defaults
        slot.migration_status = 0x00;
        slot.migrated_at = 0;
        size_t bytes_consumed = pos - offset;
        return std::make_pair(slot, bytes_consumed);
    }

    // migration_status (1 byte)
    slot.migration_status = data[pos++];

    // Validate migration_status (only 0x00, 0x01, 0xFF are valid)
    if (slot.migration_status != 0x00 && slot.migration_status != 0x01 && slot.migration_status != 0xFF) {
        Log::warning("KeySlot: Invalid migration_status: 0x{:02X}, defaulting to 0x00", slot.migration_status);
        slot.migration_status = 0x00;
    }

    // migrated_at (8 bytes, big-endian uint64_t)
    slot.migrated_at = 0;
    for (int i = 0; i < 8; ++i) {
        slot.migrated_at = (slot.migrated_at << 8) | data[pos++];
    }

    size_t bytes_consumed = pos - offset;
    return std::make_pair(slot, bytes_consumed);
}

// ============================================================================
// VaultHeaderV2 Serialization
// ============================================================================

size_t VaultHeaderV2::calculate_serialized_size() const {
    size_t size = VaultSecurityPolicy::SERIALIZED_SIZE; // Security policy
    size += 1; // Number of key slots

    for (const auto& slot : key_slots) {
        size += slot.calculate_serialized_size();
    }

    return size;
}

std::vector<uint8_t> VaultHeaderV2::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(calculate_serialized_size());

    // Serialize security policy
    auto policy_data = security_policy.serialize();
    result.insert(result.end(), policy_data.begin(), policy_data.end());

    // Number of key slots
    if (key_slots.size() > MAX_KEY_SLOTS) {
        Log::error("VaultHeaderV2: Too many key slots (max {}): {}", MAX_KEY_SLOTS, key_slots.size());
        return {};
    }
    result.push_back(static_cast<uint8_t>(key_slots.size()));

    // Serialize each key slot
    for (const auto& slot : key_slots) {
        auto slot_data = slot.serialize();
        if (slot_data.empty()) {
            Log::error("VaultHeaderV2: Failed to serialize key slot");
            return {};
        }
        result.insert(result.end(), slot_data.begin(), slot_data.end());
    }

    return result;
}

std::optional<VaultHeaderV2> VaultHeaderV2::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < VaultSecurityPolicy::SERIALIZED_SIZE + 1) {
        Log::error("VaultHeaderV2: Insufficient data for header");
        return std::nullopt;
    }

    VaultHeaderV2 header;
    size_t pos = 0;

    // Deserialize security policy
    std::vector<uint8_t> policy_data(data.begin(), data.begin() + VaultSecurityPolicy::SERIALIZED_SIZE);
    auto policy_opt = VaultSecurityPolicy::deserialize(policy_data);
    if (!policy_opt) {
        Log::error("VaultHeaderV2: Failed to deserialize security policy");
        return std::nullopt;
    }
    header.security_policy = *policy_opt;
    pos += VaultSecurityPolicy::SERIALIZED_SIZE;

    // Number of key slots
    uint8_t num_slots = data[pos++];
    if (num_slots > MAX_KEY_SLOTS) {
        Log::error("VaultHeaderV2: Too many key slots in header: {}", num_slots);
        return std::nullopt;
    }

    // Deserialize each key slot
    header.key_slots.reserve(num_slots);
    for (uint8_t i = 0; i < num_slots; ++i) {
        auto slot_opt = KeySlot::deserialize(data, pos);
        if (!slot_opt) {
            Log::error("VaultHeaderV2: Failed to deserialize key slot {}", i);
            return std::nullopt;
        }
        header.key_slots.push_back(slot_opt->first);
        pos += slot_opt->second;
    }

    return header;
}

} // namespace KeepTower
