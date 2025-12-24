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
// VaultSecurityPolicy Serialization
// ============================================================================

std::vector<uint8_t> VaultSecurityPolicy::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(SERIALIZED_SIZE);

    // Byte 0: require_yubikey flag
    result.push_back(require_yubikey ? 1 : 0);

    // Bytes 1-4: min_password_length (big-endian)
    result.push_back(static_cast<uint8_t>((min_password_length >> 24) & 0xFF));
    result.push_back(static_cast<uint8_t>((min_password_length >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((min_password_length >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(min_password_length & 0xFF));

    // Bytes 5-8: pbkdf2_iterations (big-endian)
    result.push_back(static_cast<uint8_t>((pbkdf2_iterations >> 24) & 0xFF));
    result.push_back(static_cast<uint8_t>((pbkdf2_iterations >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((pbkdf2_iterations >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(pbkdf2_iterations & 0xFF));

    // Bytes 9-12: reserved for future use
    for (size_t i = 0; i < RESERVED_BYTES_1; ++i) {
        result.push_back(0);
    }

    // Bytes 13-76: yubikey_challenge (64 bytes)
    result.insert(result.end(), yubikey_challenge.begin(), yubikey_challenge.end());

    // Bytes 77-116: reserved for future use
    for (size_t i = 0; i < RESERVED_BYTES_2; ++i) {
        result.push_back(0);
    }

    return result;
}

std::optional<VaultSecurityPolicy> VaultSecurityPolicy::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < SERIALIZED_SIZE) {
        Log::error("VaultSecurityPolicy: Insufficient data for deserialization (need {}, got {})",
                   SERIALIZED_SIZE, data.size());
        return std::nullopt;
    }

    VaultSecurityPolicy policy;

    // Byte 0: require_yubikey
    policy.require_yubikey = (data[0] != 0);

    // Bytes 1-4: min_password_length
    policy.min_password_length = (static_cast<uint32_t>(data[1]) << 24) |
                                 (static_cast<uint32_t>(data[2]) << 16) |
                                 (static_cast<uint32_t>(data[3]) << 8) |
                                 static_cast<uint32_t>(data[4]);

    // Bytes 5-8: pbkdf2_iterations
    policy.pbkdf2_iterations = (static_cast<uint32_t>(data[5]) << 24) |
                               (static_cast<uint32_t>(data[6]) << 16) |
                               (static_cast<uint32_t>(data[7]) << 8) |
                               static_cast<uint32_t>(data[8]);

    // Bytes 9-12: reserved (skip)

    // Bytes 13-76: yubikey_challenge
    std::copy(data.begin() + 13, data.begin() + 77, policy.yubikey_challenge.begin());

    // Bytes 77-116: reserved (skip)

    // Validation
    if (policy.min_password_length < 8 || policy.min_password_length > 128) {
        Log::error("VaultSecurityPolicy: Invalid min_password_length: {}", policy.min_password_length);
        return std::nullopt;
    }

    if (policy.pbkdf2_iterations < 100000 || policy.pbkdf2_iterations > 1000000) {
        Log::error("VaultSecurityPolicy: Invalid pbkdf2_iterations: {}", policy.pbkdf2_iterations);
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
    // 20 bytes: yubikey_challenge
    // 1 byte: yubikey_serial length
    // N bytes: yubikey_serial
    // 8 bytes: yubikey_enrolled_at
    return 1 + 1 + username.size() + 32 + 40 + 1 + 1 + 8 + 8 + 1 + 20 + 1 + yubikey_serial.size() + 8;
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

    // Next 20 bytes: yubikey_challenge
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
    if (pos + 1 + 20 + 1 > data.size()) {
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

    // yubikey_challenge (20 bytes)
    std::copy(data.begin() + pos, data.begin() + pos + 20, slot.yubikey_challenge.begin());
    pos += 20;

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
