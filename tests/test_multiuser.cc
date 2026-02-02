// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file test_multiuser.cc
 * @brief Unit tests for multi-user vault infrastructure
 *
 * Tests key wrapping, key slot serialization, and V2 vault format.
 */

#include "../src/core/KeyWrapping.h"
#include "../src/core/MultiUserTypes.h"
#include "../src/core/VaultFormatV2.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <cstring>

using namespace KeepTower;

// Test result tracking
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "FAIL: " << message << std::endl; \
            tests_failed++; \
            return false; \
        } \
        tests_passed++; \
    } while (0)

#define RUN_TEST(test_func) \
    do { \
        std::cout << "Running " << #test_func << "..." << std::endl; \
        if (test_func()) { \
            std::cout << "  ✓ PASSED" << std::endl; \
        } else { \
            std::cout << "  ✗ FAILED" << std::endl; \
        } \
    } while (0)

// ============================================================================
// Key Wrapping Tests
// ============================================================================

bool test_key_wrapping_basic() {
    // Generate random KEK and DEK
    auto kek_result = KeyWrapping::generate_random_salt();
    TEST_ASSERT(kek_result.has_value(), "KEK generation failed");

    auto dek_result = KeyWrapping::generate_random_dek();
    TEST_ASSERT(dek_result.has_value(), "DEK generation failed");

    auto kek = kek_result.value();
    auto dek = dek_result.value();

    // Wrap the DEK
    auto wrap_result = KeyWrapping::wrap_key(kek, dek);
    TEST_ASSERT(wrap_result.has_value(), "Key wrapping failed");

    auto wrapped = wrap_result.value();
    TEST_ASSERT(wrapped.wrapped_key.size() == KeyWrapping::WRAPPED_KEY_SIZE,
                "Wrapped key has wrong size");

    // Unwrap the DEK
    auto unwrap_result = KeyWrapping::unwrap_key(kek, wrapped.wrapped_key);
    TEST_ASSERT(unwrap_result.has_value(), "Key unwrapping failed");

    auto unwrapped = unwrap_result.value();

    // Verify unwrapped DEK matches original
    TEST_ASSERT(unwrapped.dek == dek, "Unwrapped DEK does not match original");

    return true;
}

bool test_key_wrapping_wrong_password() {
    // Generate KEKs and DEK
    auto kek1_result = KeyWrapping::generate_random_salt();
    auto kek2_result = KeyWrapping::generate_random_salt();
    auto dek_result = KeyWrapping::generate_random_dek();

    TEST_ASSERT(kek1_result.has_value() && kek2_result.has_value() && dek_result.has_value(),
                "Key generation failed");

    auto kek1 = kek1_result.value();
    auto kek2 = kek2_result.value();
    auto dek = dek_result.value();

    // Wrap with KEK1
    auto wrap_result = KeyWrapping::wrap_key(kek1, dek);
    TEST_ASSERT(wrap_result.has_value(), "Key wrapping failed");

    // Try to unwrap with KEK2 (wrong password)
    auto unwrap_result = KeyWrapping::unwrap_key(kek2, wrap_result.value().wrapped_key);
    TEST_ASSERT(!unwrap_result.has_value(), "Unwrapping with wrong KEK should fail");
    TEST_ASSERT(unwrap_result.error() == KeyWrapping::Error::UNWRAP_FAILED,
                "Wrong error code for bad unwrap");

    return true;
}

bool test_pbkdf2_derivation() {
    Glib::ustring password = "test_password_123";
    auto salt_result = KeyWrapping::generate_random_salt();
    TEST_ASSERT(salt_result.has_value(), "Salt generation failed");

    auto salt = salt_result.value();

    // Derive KEK from password
    auto kek_result = KeyWrapping::derive_kek_from_password(password, salt, 100000);
    TEST_ASSERT(kek_result.has_value(), "PBKDF2 derivation failed");

    auto kek1 = kek_result.value();

    // Derive again with same password and salt (should be identical)
    auto kek2_result = KeyWrapping::derive_kek_from_password(password, salt, 100000);
    TEST_ASSERT(kek2_result.has_value(), "PBKDF2 derivation failed (second)");

    auto kek2 = kek2_result.value();
    TEST_ASSERT(kek1 == kek2, "PBKDF2 is not deterministic");

    // Derive with different password (should be different)
    auto kek3_result = KeyWrapping::derive_kek_from_password("different_password", salt, 100000);
    TEST_ASSERT(kek3_result.has_value(), "PBKDF2 derivation failed (third)");

    auto kek3 = kek3_result.value();
    TEST_ASSERT(kek1 != kek3, "Different passwords produce same KEK");

    return true;
}

bool test_yubikey_combination() {
    auto kek_result = KeyWrapping::generate_random_salt();
    TEST_ASSERT(kek_result.has_value(), "KEK generation failed");

    auto kek = kek_result.value();

    // Simulate YubiKey response (20 bytes)
    std::array<uint8_t, KeyWrapping::YUBIKEY_RESPONSE_SIZE> yk_response;
    for (size_t i = 0; i < yk_response.size(); ++i) {
        yk_response[i] = static_cast<uint8_t>(i * 7 % 256);
    }

    // Combine KEK with YubiKey response
    auto combined_kek = KeyWrapping::combine_with_yubikey(kek, yk_response);

    // Verify the first 20 bytes are XOR'd
    for (size_t i = 0; i < KeyWrapping::YUBIKEY_RESPONSE_SIZE; ++i) {
        TEST_ASSERT(combined_kek[i] == (kek[i] ^ yk_response[i]),
                    "YubiKey XOR incorrect");
    }

    // Verify the remaining 12 bytes are unchanged
    for (size_t i = KeyWrapping::YUBIKEY_RESPONSE_SIZE; i < KeyWrapping::KEK_SIZE; ++i) {
        TEST_ASSERT(combined_kek[i] == kek[i],
                    "KEK bytes should be unchanged after byte 20");
    }

    return true;
}

// ============================================================================
// Serialization Tests
// ============================================================================

bool test_vault_security_policy_serialization() {
    VaultSecurityPolicy policy;
    policy.require_yubikey = true;
    policy.min_password_length = 12;
    policy.pbkdf2_iterations = 100000;
    policy.username_hash_algorithm = 0;  // Legacy mode for this test

    // Set challenge
    for (size_t i = 0; i < policy.yubikey_challenge.size(); ++i) {
        policy.yubikey_challenge[i] = static_cast<uint8_t>(i % 256);
    }

    // Serialize
    auto serialized = policy.serialize();
    TEST_ASSERT(serialized.size() == VaultSecurityPolicy::SERIALIZED_SIZE,
                "Serialized size should be 131 bytes (current V2 format)");

    // Deserialize
    auto deserialized_opt = VaultSecurityPolicy::deserialize(serialized);
    TEST_ASSERT(deserialized_opt.has_value(), "Deserialization failed");

    auto& deserialized = deserialized_opt.value();
    TEST_ASSERT(deserialized.require_yubikey == policy.require_yubikey,
                "require_yubikey mismatch");
    TEST_ASSERT(deserialized.min_password_length == policy.min_password_length,
                "min_password_length mismatch");
    TEST_ASSERT(deserialized.pbkdf2_iterations == policy.pbkdf2_iterations,
                "pbkdf2_iterations mismatch");
    TEST_ASSERT(deserialized.username_hash_algorithm == policy.username_hash_algorithm,
                "username_hash_algorithm mismatch");
    TEST_ASSERT(deserialized.yubikey_challenge == policy.yubikey_challenge,
                "yubikey_challenge mismatch");

    return true;
}

bool test_key_slot_serialization() {
    KeySlot slot;
    slot.active = true;
    slot.username = "testuser@example.com";
    slot.role = UserRole::ADMINISTRATOR;
    slot.must_change_password = false;
    slot.password_changed_at = 1234567890;
    slot.last_login_at = 1234567900;

    // Legacy mode: no username hashing
    slot.username_hash_size = 0;
    slot.username_hash.fill(0);
    slot.username_salt.fill(0);

    // Set salt and wrapped_dek
    for (size_t i = 0; i < slot.salt.size(); ++i) {
        slot.salt[i] = static_cast<uint8_t>(i % 256);
    }
    for (size_t i = 0; i < slot.wrapped_dek.size(); ++i) {
        slot.wrapped_dek[i] = static_cast<uint8_t>((i * 3) % 256);
    }

    // Serialize
    auto serialized = slot.serialize();
    TEST_ASSERT(!serialized.empty(), "Serialization failed");

    // Deserialize
    auto deserialized_opt = KeySlot::deserialize(serialized, 0);
    TEST_ASSERT(deserialized_opt.has_value(), "Deserialization failed");

    auto& [deserialized, bytes_consumed] = deserialized_opt.value();
    TEST_ASSERT(bytes_consumed == serialized.size(), "Bytes consumed mismatch");
    TEST_ASSERT(deserialized.active == slot.active, "active mismatch");
    // Username is intentionally NOT serialized (security: USERNAME_HASHING_SECURITY_PLAN.md)
    TEST_ASSERT(deserialized.username.empty(), "username should be empty after deserialization");
    TEST_ASSERT(deserialized.role == slot.role, "role mismatch");
    TEST_ASSERT(deserialized.must_change_password == slot.must_change_password,
                "must_change_password mismatch");
    TEST_ASSERT(deserialized.password_changed_at == slot.password_changed_at,
                "password_changed_at mismatch");
    TEST_ASSERT(deserialized.last_login_at == slot.last_login_at,
                "last_login_at mismatch");
    TEST_ASSERT(deserialized.salt == slot.salt, "salt mismatch");
    TEST_ASSERT(deserialized.wrapped_dek == slot.wrapped_dek, "wrapped_dek mismatch");

    return true;
}

bool test_key_slot_deserialize_rejects_truncated_data() {
    KeySlot slot;
    slot.active = true;
    slot.username = "testuser@example.com";
    slot.kek_derivation_algorithm = 0x04;  // PBKDF2-HMAC-SHA256
    slot.role = UserRole::ADMINISTRATOR;
    slot.must_change_password = false;
    slot.password_changed_at = 1234567890;
    slot.last_login_at = 1234567900;

    // Legacy mode: no username hashing
    slot.username_hash_size = 0;
    slot.username_hash.fill(0);
    slot.username_salt.fill(0);

    for (size_t i = 0; i < slot.salt.size(); ++i) {
        slot.salt[i] = static_cast<uint8_t>(i % 256);
    }
    for (size_t i = 0; i < slot.wrapped_dek.size(); ++i) {
        slot.wrapped_dek[i] = static_cast<uint8_t>((i * 3) % 256);
    }

    auto serialized = slot.serialize();
    TEST_ASSERT(!serialized.empty(), "Serialization failed");

    // Offset at end must be rejected.
    TEST_ASSERT(!KeySlot::deserialize(serialized, serialized.size()).has_value(),
                "Deserialization should fail with offset==size");

    // Truncate into required core fields; must be rejected.
    const size_t core_min_size = 1 + 1 + 64 + 1 + 16 + 32 + 40 + 1 + 1 + 8 + 8;
    TEST_ASSERT(serialized.size() >= core_min_size, "Unexpected serialized KeySlot size");
    std::vector<uint8_t> truncated(serialized.begin(), serialized.begin() + (core_min_size - 1));
    TEST_ASSERT(!KeySlot::deserialize(truncated, 0).has_value(),
                "Deserialization should fail when core fields are truncated");

    // Minimal 1-byte buffer should be rejected safely.
    std::vector<uint8_t> one_byte{0x01};
    TEST_ASSERT(!KeySlot::deserialize(one_byte, 0).has_value(),
                "Deserialization should fail for 1-byte buffer");
    TEST_ASSERT(!KeySlot::deserialize(one_byte, 1).has_value(),
                "Deserialization should fail for offset==size on 1-byte buffer");

    return true;
}

bool test_vault_header_v2_serialization() {
    VaultHeaderV2 header;

    // Setup security policy
    header.security_policy.require_yubikey = false;
    header.security_policy.min_password_length = 12;
    header.security_policy.pbkdf2_iterations = 100000;
    header.security_policy.username_hash_algorithm = 0;  // Legacy mode

    // Add two key slots
    KeySlot slot1;
    slot1.active = true;
    slot1.username = "admin@example.com";
    slot1.role = UserRole::ADMINISTRATOR;
    slot1.must_change_password = false;
    slot1.username_hash_size = 0;  // Legacy mode
    for (size_t i = 0; i < slot1.salt.size(); ++i) {
        slot1.salt[i] = static_cast<uint8_t>(i % 256);
    }
    for (size_t i = 0; i < slot1.wrapped_dek.size(); ++i) {
        slot1.wrapped_dek[i] = static_cast<uint8_t>((i * 2) % 256);
    }

    KeySlot slot2;
    slot2.active = true;
    slot2.username = "user@example.com";
    slot2.role = UserRole::STANDARD_USER;
    slot2.must_change_password = true;
    for (size_t i = 0; i < slot2.salt.size(); ++i) {
        slot2.salt[i] = static_cast<uint8_t>((i + 100) % 256);
    }
    for (size_t i = 0; i < slot2.wrapped_dek.size(); ++i) {
        slot2.wrapped_dek[i] = static_cast<uint8_t>((i * 3) % 256);
    }

    header.key_slots.push_back(slot1);
    header.key_slots.push_back(slot2);

    // Serialize
    auto serialized = header.serialize();
    TEST_ASSERT(!serialized.empty(), "Serialization failed");

    // Deserialize
    auto deserialized_opt = VaultHeaderV2::deserialize(serialized);
    TEST_ASSERT(deserialized_opt.has_value(), "Deserialization failed");

    auto& deserialized = deserialized_opt.value();
    TEST_ASSERT(deserialized.key_slots.size() == 2, "Key slot count mismatch");
    // Usernames are intentionally NOT serialized (security: USERNAME_HASHING_SECURITY_PLAN.md)
    TEST_ASSERT(deserialized.key_slots[0].username.empty(),
                "First username should be empty after deserialization");
    TEST_ASSERT(deserialized.key_slots[1].username.empty(),
                "Second username should be empty after deserialization");
    TEST_ASSERT(deserialized.key_slots[0].role == UserRole::ADMINISTRATOR,
                "First role mismatch");
    TEST_ASSERT(deserialized.key_slots[1].role == UserRole::STANDARD_USER,
                "Second role mismatch");

    return true;
}

// ============================================================================
// V2 Format Tests
// ============================================================================

bool test_vault_format_v2_header_write_read() {
    VaultFormatV2::V2FileHeader header;
    header.pbkdf2_iterations = 100000;

    // Setup security policy
    header.vault_header.security_policy.require_yubikey = false;
    header.vault_header.security_policy.min_password_length = 12;
    header.vault_header.security_policy.pbkdf2_iterations = 100000;
    header.vault_header.security_policy.username_hash_algorithm = 0;  // Legacy mode

    // Add key slot
    KeySlot slot;
    slot.active = true;
    slot.username = "test@example.com";
    slot.role = UserRole::ADMINISTRATOR;
    slot.username_hash_size = 0;  // Legacy mode
    for (size_t i = 0; i < slot.salt.size(); ++i) {
        slot.salt[i] = static_cast<uint8_t>(i % 256);
    }
    for (size_t i = 0; i < slot.wrapped_dek.size(); ++i) {
        slot.wrapped_dek[i] = static_cast<uint8_t>((i * 5) % 256);
    }
    header.vault_header.key_slots.push_back(slot);

    // Set data salt and IV
    for (size_t i = 0; i < header.data_salt.size(); ++i) {
        header.data_salt[i] = static_cast<uint8_t>((i + 50) % 256);
    }
    for (size_t i = 0; i < header.data_iv.size(); ++i) {
        header.data_iv[i] = static_cast<uint8_t>((i + 100) % 256);
    }

    // Write header (with FEC)
    auto write_result = VaultFormatV2::write_header(header, true);
    TEST_ASSERT(write_result.has_value(), "Header write failed");

    auto file_data = write_result.value();

    // Read header back
    auto read_result = VaultFormatV2::read_header(file_data);
    TEST_ASSERT(read_result.has_value(), "Header read failed");

    auto& [read_header, data_offset] = read_result.value();

    // Verify header fields
    TEST_ASSERT(read_header.magic == VaultFormatV2::VAULT_MAGIC, "Magic mismatch");
    TEST_ASSERT(read_header.version == VaultFormatV2::VAULT_VERSION_V2, "Version mismatch");
    TEST_ASSERT(read_header.pbkdf2_iterations == 100000, "PBKDF2 iterations mismatch");
    TEST_ASSERT(read_header.vault_header.key_slots.size() == 1, "Key slot count mismatch");
    // Username is intentionally NOT serialized (security: USERNAME_HASHING_SECURITY_PLAN.md)
    TEST_ASSERT(read_header.vault_header.key_slots[0].username.empty(),
                "Username should be empty after deserialization");
    TEST_ASSERT(read_header.data_salt == header.data_salt, "Data salt mismatch");
    TEST_ASSERT(read_header.data_iv == header.data_iv, "Data IV mismatch");

    return true;
}

bool test_version_detection() {
    // Create a V2 header
    VaultFormatV2::V2FileHeader header;
    header.pbkdf2_iterations = 100000;
    header.vault_header.security_policy.min_password_length = 12;
    header.vault_header.security_policy.pbkdf2_iterations = 100000;
    header.vault_header.security_policy.username_hash_algorithm = 0;  // Legacy mode

    KeySlot slot;
    slot.active = true;
    slot.username = "test";
    slot.role = UserRole::ADMINISTRATOR;
    slot.username_hash_size = 0;  // Legacy mode
    header.vault_header.key_slots.push_back(slot);

    auto write_result = VaultFormatV2::write_header(header, false);
    TEST_ASSERT(write_result.has_value(), "Header write failed");

    auto file_data = write_result.value();

    // Detect version
    auto version_result = VaultFormatV2::detect_version(file_data);
    TEST_ASSERT(version_result.has_value(), "Version detection failed");
    TEST_ASSERT(version_result.value() == VaultFormatV2::VAULT_VERSION_V2,
                "Wrong version detected");

    // Check is_valid_v2_vault
    TEST_ASSERT(VaultFormatV2::is_valid_v2_vault(file_data),
                "Should be valid V2 vault");

    return true;
}

bool test_header_fec_redundancy_levels() {
    VaultFormatV2::V2FileHeader header;
    header.pbkdf2_iterations = 100000;
    header.vault_header.security_policy.min_password_length = 12;
    header.vault_header.security_policy.pbkdf2_iterations = 100000;

    KeySlot slot;
    slot.active = true;
    slot.username = "test";
    slot.role = UserRole::ADMINISTRATOR;
    slot.username_hash_size = 0;  // Legacy mode
    header.vault_header.key_slots.push_back(slot);

    // Test 1: Default (0) should use minimum 20%
    auto write_result1 = VaultFormatV2::write_header(header, true, 0);
    TEST_ASSERT(write_result1.has_value(), "Header write with default redundancy failed");

    // Test 2: Lower than minimum (10%) should use 20%
    auto write_result2 = VaultFormatV2::write_header(header, true, 10);
    TEST_ASSERT(write_result2.has_value(), "Header write with 10% redundancy failed");

    // Test 3: Higher than minimum (30%) should use 30%
    auto write_result3 = VaultFormatV2::write_header(header, true, 30);
    TEST_ASSERT(write_result3.has_value(), "Header write with 30% redundancy failed");

    // Test 4: Maximum (50%) should use 50%
    auto write_result4 = VaultFormatV2::write_header(header, true, 50);
    TEST_ASSERT(write_result4.has_value(), "Header write with 50% redundancy failed");

    // Note: For small headers (~214 bytes), Reed-Solomon block size constraints may
    // result in similar encoded sizes. The important verification is that all variants
    // can be written and read back correctly with their specified redundancy levels.

    // Verify all can be read back
    auto read_result1 = VaultFormatV2::read_header(write_result1.value());
    TEST_ASSERT(read_result1.has_value(), "Read back 20% redundancy header failed");

    auto read_result2 = VaultFormatV2::read_header(write_result2.value());
    TEST_ASSERT(read_result2.has_value(), "Read back 10%->20% redundancy header failed");

    auto read_result3 = VaultFormatV2::read_header(write_result3.value());
    TEST_ASSERT(read_result3.has_value(), "Read back 30% redundancy header failed");

    auto read_result4 = VaultFormatV2::read_header(write_result4.value());
    TEST_ASSERT(read_result4.has_value(), "Read back 50% redundancy header failed");

    // Verify size ordering: 10% parameter should produce same size as 0% (both use 20% minimum)
    size_t size_default = write_result1.value().size();
    size_t size_10_param = write_result2.value().size();
    TEST_ASSERT(size_default == size_10_param,
                "10% parameter should use 20% minimum (same as default)");

    return true;
}

// ============================================================================
// Phase 2: Username Hashing Tests
// ============================================================================

bool test_vault_security_policy_username_hash_algorithm_serialization() {
    VaultSecurityPolicy policy;
    policy.require_yubikey = false;
    policy.min_password_length = 12;
    policy.pbkdf2_iterations = 100000;
    policy.password_history_depth = 5;
    policy.username_hash_algorithm = 1;  // SHA3-256

    // Set challenge
    for (size_t i = 0; i < policy.yubikey_challenge.size(); ++i) {
        policy.yubikey_challenge[i] = static_cast<uint8_t>(i % 256);
    }

    // Serialize
    auto serialized = policy.serialize();
    TEST_ASSERT(serialized.size() == VaultSecurityPolicy::SERIALIZED_SIZE,
                "Serialized size should be 131 bytes (current V2 format)");

    // Deserialize
    auto deserialized_opt = VaultSecurityPolicy::deserialize(serialized);
    TEST_ASSERT(deserialized_opt.has_value(), "Deserialization failed");

    auto& deserialized = deserialized_opt.value();
    TEST_ASSERT(deserialized.require_yubikey == policy.require_yubikey,
                "require_yubikey mismatch");
    TEST_ASSERT(deserialized.min_password_length == policy.min_password_length,
                "min_password_length mismatch");
    TEST_ASSERT(deserialized.pbkdf2_iterations == policy.pbkdf2_iterations,
                "pbkdf2_iterations mismatch");
    TEST_ASSERT(deserialized.password_history_depth == policy.password_history_depth,
                "password_history_depth mismatch");
    TEST_ASSERT(deserialized.username_hash_algorithm == policy.username_hash_algorithm,
                "username_hash_algorithm mismatch");
    TEST_ASSERT(deserialized.yubikey_challenge == policy.yubikey_challenge,
                "yubikey_challenge mismatch");

    return true;
}

bool test_vault_security_policy_backward_compatibility() {
    // Create an old-format policy (122 bytes, no username_hash_algorithm)
    std::vector<uint8_t> old_format_data(122, 0);

    // Byte 0: require_yubikey = false
    old_format_data[0] = 0;

    // Byte 1: yubikey_algorithm = 0x02 (SHA-256)
    old_format_data[1] = 0x02;

    // Bytes 2-5: min_password_length = 12 (big-endian)
    old_format_data[2] = 0;
    old_format_data[3] = 0;
    old_format_data[4] = 0;
    old_format_data[5] = 12;

    // Bytes 6-9: pbkdf2_iterations = 100000
    uint32_t iterations = 100000;
    old_format_data[6] = (iterations >> 24) & 0xFF;
    old_format_data[7] = (iterations >> 16) & 0xFF;
    old_format_data[8] = (iterations >> 8) & 0xFF;
    old_format_data[9] = iterations & 0xFF;

    // Bytes 10-13: password_history_depth = 5
    old_format_data[10] = 0;
    old_format_data[11] = 0;
    old_format_data[12] = 0;
    old_format_data[13] = 5;

    // Bytes 14-77: yubikey_challenge (64 bytes of zeros)
    // Bytes 78-121: reserved (44 bytes of zeros)

    // Deserialize old format
    auto deserialized_opt = VaultSecurityPolicy::deserialize(old_format_data);
    TEST_ASSERT(deserialized_opt.has_value(), "Deserialization of old format failed");

    auto& policy = deserialized_opt.value();
    TEST_ASSERT(policy.require_yubikey == false, "require_yubikey mismatch");
    TEST_ASSERT(policy.min_password_length == 12, "min_password_length mismatch");
    TEST_ASSERT(policy.pbkdf2_iterations == 100000, "pbkdf2_iterations mismatch");
    TEST_ASSERT(policy.password_history_depth == 5, "password_history_depth mismatch");
    TEST_ASSERT(policy.username_hash_algorithm == 0,
                "username_hash_algorithm should default to 0 (plaintext) for old format");

    return true;
}

bool test_key_slot_username_hashing_serialization() {
    KeySlot slot;
    slot.active = true;
    slot.username = "";  // Empty for hashed mode
    slot.role = UserRole::ADMINISTRATOR;
    slot.must_change_password = false;
    slot.password_changed_at = 1234567890;
    slot.last_login_at = 1234567900;

    // Set username hashing fields (SHA3-256 example)
    slot.username_hash_size = 32;  // SHA3-256 produces 32 bytes
    for (size_t i = 0; i < 32; ++i) {
        slot.username_hash[i] = static_cast<uint8_t>((i * 7) % 256);
    }
    for (size_t i = 0; i < 16; ++i) {
        slot.username_salt[i] = static_cast<uint8_t>((i * 13) % 256);
    }

    // Set salt and wrapped_dek
    for (size_t i = 0; i < slot.salt.size(); ++i) {
        slot.salt[i] = static_cast<uint8_t>(i % 256);
    }
    for (size_t i = 0; i < slot.wrapped_dek.size(); ++i) {
        slot.wrapped_dek[i] = static_cast<uint8_t>((i * 3) % 256);
    }

    // Serialize
    auto serialized = slot.serialize();
    TEST_ASSERT(!serialized.empty(), "Serialization failed");

    // Deserialize
    auto deserialized_opt = KeySlot::deserialize(serialized, 0);
    TEST_ASSERT(deserialized_opt.has_value(), "Deserialization failed");

    auto& [deserialized, bytes_consumed] = deserialized_opt.value();
    TEST_ASSERT(bytes_consumed == serialized.size(), "Bytes consumed mismatch");
    TEST_ASSERT(deserialized.active == slot.active, "active mismatch");
    // Username is intentionally NOT serialized (security: USERNAME_HASHING_SECURITY_PLAN.md)
    TEST_ASSERT(deserialized.username.empty(), "username should be empty after deserialization");
    TEST_ASSERT(deserialized.username_hash_size == slot.username_hash_size,
                "username_hash_size mismatch");

    // Verify username_hash (first 32 bytes)
    for (size_t i = 0; i < 32; ++i) {
        TEST_ASSERT(deserialized.username_hash[i] == slot.username_hash[i],
                    "username_hash mismatch at byte " + std::to_string(i));
    }

    // Verify username_salt
    TEST_ASSERT(deserialized.username_salt == slot.username_salt, "username_salt mismatch");
    TEST_ASSERT(deserialized.role == slot.role, "role mismatch");
    TEST_ASSERT(deserialized.salt == slot.salt, "salt mismatch");
    TEST_ASSERT(deserialized.wrapped_dek == slot.wrapped_dek, "wrapped_dek mismatch");

    return true;
}

// Legacy format test removed - backward compatibility with pre-Phase 2 vaults
// no longer supported. All vaults now use username hashing for security.

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Multi-User Infrastructure Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    RUN_TEST(test_key_wrapping_basic);
    RUN_TEST(test_key_wrapping_wrong_password);
    RUN_TEST(test_pbkdf2_derivation);
    RUN_TEST(test_yubikey_combination);
    RUN_TEST(test_vault_security_policy_serialization);
    RUN_TEST(test_key_slot_serialization);
    RUN_TEST(test_key_slot_deserialize_rejects_truncated_data);
    RUN_TEST(test_vault_header_v2_serialization);
    RUN_TEST(test_vault_format_v2_header_write_read);
    RUN_TEST(test_version_detection);
    RUN_TEST(test_header_fec_redundancy_levels);

    // Phase 2: Username hashing tests
    RUN_TEST(test_vault_security_policy_username_hash_algorithm_serialization);
    RUN_TEST(test_vault_security_policy_backward_compatibility);
    RUN_TEST(test_key_slot_username_hashing_serialization);

    std::cout << "========================================" << std::endl;
    std::cout << "Results: " << tests_passed << " passed, " << tests_failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return (tests_failed == 0) ? 0 : 1;
}
