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
    pos += 48;

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

    // Bytes 14-77: yubikey_challenge (64 bytes)
    result.insert(result.end(), yubikey_challenge.begin(), yubikey_challenge.end());

    // Bytes 78-121: reserved for future use (43 bytes, reduced from 44)
    for (size_t i = 0; i < RESERVED_BYTES_2; ++i) {
        result.push_back(0);
    }

    return result;
}

std::optional<VaultSecurityPolicy> VaultSecurityPolicy::deserialize(const std::vector<uint8_t>& data) {
    // Only support new format (122 bytes) - FIPS-140-3 compliant vaults only
    if (data.size() < SERIALIZED_SIZE) {
        Log::error("VaultSecurityPolicy: Insufficient data for deserialization (need {}, got {})",
                   SERIALIZED_SIZE, data.size());
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

    // Next 64 bytes: yubikey_challenge
    std::copy(data.begin() + offset, data.begin() + offset + 64, policy.yubikey_challenge.begin());
    offset += 64;

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
    // 1 byte: active flag
    // 1 byte: username length
    // N bytes: username (UTF-8)
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
    size_t base_size = 1 + 1 + username.size() + 32 + 40 + 1 + 1 + 8 + 8 + 1 + 32 + 1 + yubikey_serial.size() + 8 + 2 + yubikey_encrypted_pin.size() + 2 + yubikey_credential_id.size() + 1;
    size_t history_size = password_history.size() * PasswordHistoryEntry::SERIALIZED_SIZE;
    return base_size + history_size;
}

std::vector<uint8_t> KeySlot::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(calculate_serialized_size());

    // Byte 0: active flag
    result.push_back(active ? 1 : 0);

    // Byte 1: username length
    if (username.size() > 255) {
        Log::error("KeySlot: Username too long (max 255 bytes): {}", username.size());
        return {}; // Return empty vector on error
    }
    result.push_back(static_cast<uint8_t>(username.size()));

    // Bytes 2..N: username (UTF-8)
    result.insert(result.end(), username.begin(), username.end());

    // Next 32 bytes: salt
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

    return result;
}

std::optional<std::pair<KeySlot, size_t>> KeySlot::deserialize(
    const std::vector<uint8_t>& data, size_t offset) {

    if (offset + 2 > data.size()) {
        Log::error("KeySlot: Insufficient data for header at offset {}", offset);
        return std::nullopt;
    }

    KeySlot slot;
    size_t pos = offset;

    // Byte 0: active flag
    slot.active = (data[pos++] != 0);

    // Byte 1: username length
    uint8_t username_len = data[pos++];

    // Check if we have enough data for username + fixed fields (minimum without YubiKey fields)
    if (pos + username_len + 32 + 40 + 1 + 1 + 8 + 8 > data.size()) {
        Log::error("KeySlot: Insufficient data for slot (need {}, have {})",
                   username_len + 32 + 40 + 1 + 1 + 8 + 8, data.size() - pos);
        return std::nullopt;
    }

    // Username
    slot.username.assign(data.begin() + pos, data.begin() + pos + username_len);
    pos += username_len;

    // Salt (32 bytes)
    std::copy(data.begin() + pos, data.begin() + pos + 32, slot.salt.begin());
    pos += 32;

    // Wrapped DEK (40 bytes)
    std::copy(data.begin() + pos, data.begin() + pos + 40, slot.wrapped_dek.begin());
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
    std::copy(data.begin() + pos, data.begin() + pos + 32, slot.yubikey_challenge.begin());
    pos += 32;

    // yubikey_serial length (1 byte)
    uint8_t yubikey_serial_len = data[pos++];

    // Check if we have enough data for serial + timestamp
    if (pos + yubikey_serial_len + 8 > data.size()) {
        Log::error("KeySlot: Insufficient data for YubiKey serial and timestamp");
        return std::nullopt;
    }

    // yubikey_serial (N bytes)
    slot.yubikey_serial.assign(data.begin() + pos, data.begin() + pos + yubikey_serial_len);
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
    slot.yubikey_encrypted_pin.assign(data.begin() + pos, data.begin() + pos + encrypted_pin_len);
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
    slot.yubikey_credential_id.assign(data.begin() + pos, data.begin() + pos + credential_id_len);
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
