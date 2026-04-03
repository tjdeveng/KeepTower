// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include <gtest/gtest.h>

#include "../src/core/services/V2AuthService.h"
#include "../src/core/services/UsernameHashService.h"
#include "../src/lib/crypto/VaultCrypto.h"
#include "../src/lib/crypto/KeyWrapping.h"

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <vector>

using namespace KeepTower;

namespace {

KeySlot make_slot_for_username(
    const std::string& username,
    const std::array<uint8_t, 16>& username_salt,
    const VaultSecurityPolicy& policy) {

    KeySlot slot;
    slot.active = true;
    slot.username = username;
    slot.role = UserRole::STANDARD_USER;
    slot.kek_derivation_algorithm = 0x04; // PBKDF2-HMAC-SHA256
    slot.username_salt = username_salt;

    auto hash_result = UsernameHashService::hash_username(
        username,
        static_cast<UsernameHashService::Algorithm>(policy.username_hash_algorithm),
        std::span<const uint8_t, 16>(slot.username_salt),
        policy.pbkdf2_iterations);
    EXPECT_TRUE(hash_result.has_value());
    if (!hash_result) {
        return slot;
    }

    slot.username_hash_size = static_cast<uint8_t>(hash_result->size());
    std::copy(hash_result->begin(), hash_result->end(), slot.username_hash.begin());
    return slot;
}

} // namespace

TEST(V2AuthServiceUnitTests, ResolveUserSlotForOpenReturnsAuthenticationFailedWhenMissing) {
    std::vector<KeySlot> slots;
    VaultSecurityPolicy policy;
    policy.username_hash_algorithm = 0x01; // SHA3-256
    policy.pbkdf2_iterations = 10000;

    auto result = V2AuthService::resolve_user_slot_for_open(slots, "alice", policy);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::AuthenticationFailed);
}

TEST(V2AuthServiceUnitTests, ResolveUserSlotForOpenFindsExistingUser) {
    VaultSecurityPolicy policy;
    policy.username_hash_algorithm = 0x01; // SHA3-256
    policy.pbkdf2_iterations = 10000;

    std::array<uint8_t, 16> username_salt{};
    username_salt[0] = 0x2A;
    username_salt[1] = 0x7F;

    std::vector<KeySlot> slots;
    slots.push_back(make_slot_for_username("alice", username_salt, policy));

    auto result = V2AuthService::resolve_user_slot_for_open(slots, "alice", policy);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value(), nullptr);
    EXPECT_EQ(result.value()->username, "alice");
}

TEST(V2AuthServiceUnitTests, DerivePasswordKekForSlotReturnsCryptoErrorForUnsupportedAlgorithm) {
    KeySlot slot;
    slot.kek_derivation_algorithm = 0x03; // Unsupported for KEK derivation

    VaultSecurityPolicy policy;
    policy.argon2_memory_kb = 65536;
    policy.argon2_iterations = 3;
    policy.argon2_parallelism = 4;

    auto result = V2AuthService::derive_password_kek_for_slot(slot, "Password123!", 600000, policy);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::CryptoError);
}

TEST(V2AuthServiceUnitTests, DerivePasswordKekForSlotSucceedsForPbkdf2Slot) {
    KeySlot slot;
    slot.kek_derivation_algorithm = 0x04; // PBKDF2-HMAC-SHA256
    slot.salt.fill(0x5A);

    VaultSecurityPolicy policy;
    policy.argon2_memory_kb = 65536;
    policy.argon2_iterations = 3;
    policy.argon2_parallelism = 4;

    auto result = V2AuthService::derive_password_kek_for_slot(slot, "Password123!", 600000, policy);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 32u);
    const bool all_zero = std::all_of(result->begin(), result->end(), [](uint8_t b) { return b == 0; });
    EXPECT_FALSE(all_zero);
}

TEST(V2AuthServiceUnitTests, DecryptYubiKeyPinForOpenRejectsMissingEncryptedPin) {
    KeySlot slot;
    std::array<uint8_t, 32> kek{};

    auto result = V2AuthService::decrypt_yubikey_pin_for_open(slot, kek);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::YubiKeyError);
}

TEST(V2AuthServiceUnitTests, DecryptYubiKeyPinForOpenRoundTripsEncryptedPin) {
    const std::string pin = "123456";

    std::array<uint8_t, 32> kek{};
    for (size_t i = 0; i < kek.size(); ++i) {
        kek[i] = static_cast<uint8_t>(i + 1);
    }

    std::array<uint8_t, KeepTower::VaultCrypto::IV_LENGTH> iv{};
    for (size_t i = 0; i < iv.size(); ++i) {
        iv[i] = static_cast<uint8_t>(0xA0 + i);
    }

    std::vector<uint8_t> ciphertext;
    const std::vector<uint8_t> pin_bytes(pin.begin(), pin.end());
    ASSERT_TRUE(KeepTower::VaultCrypto::encrypt_data(pin_bytes, kek, ciphertext, iv));

    KeySlot slot;
    slot.yubikey_encrypted_pin.reserve(iv.size() + ciphertext.size());
    slot.yubikey_encrypted_pin.insert(slot.yubikey_encrypted_pin.end(), iv.begin(), iv.end());
    slot.yubikey_encrypted_pin.insert(slot.yubikey_encrypted_pin.end(), ciphertext.begin(), ciphertext.end());

    auto result = V2AuthService::decrypt_yubikey_pin_for_open(slot, kek);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), pin);
}

TEST(V2AuthServiceUnitTests, ResolveYubiKeyPinForAuthUsesFallbackWhenEncryptedPinMissing) {
    KeySlot slot;
    std::array<uint8_t, 32> kek{};
    const std::optional<std::string> fallback_pin = std::string("654321");

    auto result = V2AuthService::resolve_yubikey_pin_for_auth(slot, kek, fallback_pin);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "654321");
}

TEST(V2AuthServiceUnitTests, ResolveYubiKeyPinForAuthFailsWhenNoPinAvailable) {
    KeySlot slot;
    std::array<uint8_t, 32> kek{};

    auto result = V2AuthService::resolve_yubikey_pin_for_auth(slot, kek, std::nullopt);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::YubiKeyError);
}

TEST(V2AuthServiceUnitTests, CombineKekWithYubiKeyResponseMatchesKeyWrapping) {
    std::array<uint8_t, 32> password_kek{};
    for (size_t i = 0; i < password_kek.size(); ++i) {
        password_kek[i] = static_cast<uint8_t>(0x10 + i);
    }

    std::vector<uint8_t> yk_response(32);
    for (size_t i = 0; i < yk_response.size(); ++i) {
        yk_response[i] = static_cast<uint8_t>(0x80 + i);
    }

    auto expected = KeyWrapping::combine_with_yubikey_v2(password_kek, yk_response);
    auto actual = V2AuthService::combine_kek_with_yubikey_response_for_open(
        password_kek,
        std::span<const uint8_t>(yk_response));

    EXPECT_EQ(actual, expected);
}
