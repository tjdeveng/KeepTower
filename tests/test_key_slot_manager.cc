// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include <gtest/gtest.h>

#include "../src/core/services/KeySlotManager.h"
#include "../src/lib/crypto/UsernameHashService.h"

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <vector>

using namespace KeepTower;

namespace {

VaultSecurityPolicy make_policy(
    UsernameHashService::Algorithm current_algo,
    uint32_t pbkdf2_iterations = 1000) {
    VaultSecurityPolicy policy;
    policy.username_hash_algorithm = static_cast<uint8_t>(current_algo);
    policy.pbkdf2_iterations = pbkdf2_iterations;
    return policy;
}

KeySlot make_slot_for_username(
    const std::string& username,
    const VaultSecurityPolicy& policy,
    const std::array<uint8_t, 16>& username_salt,
    UsernameHashService::Algorithm hash_algo,
    UserRole role = UserRole::STANDARD_USER) {
    KeySlot slot;
    slot.active = true;
    slot.username = username;
    slot.role = role;
    slot.kek_derivation_algorithm = 0x04;
    slot.username_salt = username_salt;

    auto hash_result = UsernameHashService::hash_username(
        username,
        hash_algo,
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

PasswordHistoryEntry make_history_entry(uint8_t seed, int64_t timestamp) {
    PasswordHistoryEntry entry;
    entry.timestamp = timestamp;
    entry.salt.fill(seed);
    entry.hash.fill(static_cast<uint8_t>(seed + 1));
    return entry;
}

} // namespace

TEST(KeySlotManagerUnitTests, CreateUserSlotCopiesInputsAndInitializesDefaults) {
    const std::array<uint8_t, 16> username_salt = {
        1, 2, 3, 4, 5, 6, 7, 8,
        9, 10, 11, 12, 13, 14, 15, 16};
    const std::array<uint8_t, 32> salt = {
        10, 11, 12, 13, 14, 15, 16, 17,
        18, 19, 20, 21, 22, 23, 24, 25,
        26, 27, 28, 29, 30, 31, 32, 33,
        34, 35, 36, 37, 38, 39, 40, 41};
    const std::array<uint8_t, 40> wrapped_dek = {
        50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
        60, 61, 62, 63, 64, 65, 66, 67, 68, 69,
        70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
        80, 81, 82, 83, 84, 85, 86, 87, 88, 89};
    const std::array<uint8_t, 32> username_hash = {
        90, 91, 92, 93, 94, 95, 96, 97,
        98, 99, 100, 101, 102, 103, 104, 105,
        106, 107, 108, 109, 110, 111, 112, 113,
        114, 115, 116, 117, 118, 119, 120, 121};

    KeySlot slot = KeySlotManager::create_user_slot(
        "alice",
        0x04,
        std::span<const uint8_t>(username_hash),
        username_salt,
        salt,
        wrapped_dek,
        UserRole::ADMINISTRATOR,
        true);

    EXPECT_TRUE(slot.active);
    EXPECT_EQ(slot.username, "alice");
    EXPECT_EQ(slot.kek_derivation_algorithm, 0x04);
    EXPECT_EQ(slot.username_hash_size, username_hash.size());
    EXPECT_TRUE(std::equal(username_hash.begin(), username_hash.end(), slot.username_hash.begin()));
    EXPECT_EQ(slot.username_salt, username_salt);
    EXPECT_EQ(slot.salt, salt);
    EXPECT_EQ(slot.wrapped_dek, wrapped_dek);
    EXPECT_EQ(slot.role, UserRole::ADMINISTRATOR);
    EXPECT_TRUE(slot.must_change_password);
    EXPECT_EQ(slot.password_changed_at, 0);
    EXPECT_EQ(slot.last_login_at, 0);
}

TEST(KeySlotManagerUnitTests, FindSlotByUsernameHashMatchesCurrentAlgorithm) {
    VaultSecurityPolicy policy = make_policy(UsernameHashService::Algorithm::SHA3_256);
    const std::array<uint8_t, 16> username_salt = {
        1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4};

    std::vector<KeySlot> slots;
    slots.push_back(make_slot_for_username(
        "alice",
        policy,
        username_salt,
        UsernameHashService::Algorithm::SHA3_256));

    KeySlot* slot = KeySlotManager::find_slot_by_username_hash(slots, "alice", policy);

    ASSERT_NE(slot, nullptr);
    EXPECT_EQ(slot->username, "alice");
}

TEST(KeySlotManagerUnitTests, FindSlotByUsernameHashUsesFallbackDuringMigration) {
    VaultSecurityPolicy policy = make_policy(UsernameHashService::Algorithm::PBKDF2_SHA256);
    policy.username_hash_algorithm_previous = static_cast<uint8_t>(UsernameHashService::Algorithm::SHA3_256);
    policy.migration_flags = 0x01;

    const std::array<uint8_t, 16> username_salt = {
        5, 5, 5, 5, 6, 6, 6, 6,
        7, 7, 7, 7, 8, 8, 8, 8};

    std::vector<KeySlot> slots;
    slots.push_back(make_slot_for_username(
        "alice",
        policy,
        username_salt,
        UsernameHashService::Algorithm::SHA3_256));

    KeySlot* slot = KeySlotManager::find_slot_by_username_hash(slots, "alice", policy);

    ASSERT_NE(slot, nullptr);
    EXPECT_EQ(slot->username, "alice");
    EXPECT_EQ(slot->migration_status, 0xFF);
}

TEST(KeySlotManagerUnitTests, FindSlotByUsernameHashRescuesUnexpectedAlgorithmMatch) {
    VaultSecurityPolicy policy = make_policy(UsernameHashService::Algorithm::PBKDF2_SHA256);
    const std::array<uint8_t, 16> username_salt = {
        9, 9, 9, 9, 10, 10, 10, 10,
        11, 11, 11, 11, 12, 12, 12, 12};

    std::vector<KeySlot> slots;
    slots.push_back(make_slot_for_username(
        "alice",
        policy,
        username_salt,
        UsernameHashService::Algorithm::SHA3_512));

    KeySlot* slot = KeySlotManager::find_slot_by_username_hash(slots, "alice", policy);

    ASSERT_NE(slot, nullptr);
    EXPECT_EQ(slot->migration_status, 0xFF);
}

TEST(KeySlotManagerUnitTests, StoreUserSlotReusesInactiveEntry) {
    VaultSecurityPolicy policy = make_policy(UsernameHashService::Algorithm::SHA3_256);
    const std::array<uint8_t, 16> username_salt = {
        13, 13, 13, 13, 14, 14, 14, 14,
        15, 15, 15, 15, 16, 16, 16, 16};

    std::vector<KeySlot> slots;
    slots.push_back(make_slot_for_username(
        "old-user",
        policy,
        username_salt,
        UsernameHashService::Algorithm::SHA3_256));
    slots[0].active = false;

    auto result = KeySlotManager::store_user_slot(
        slots,
        make_slot_for_username(
            "alice",
            policy,
            username_salt,
            UsernameHashService::Algorithm::SHA3_256),
        4);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 0u);
    EXPECT_EQ(slots.size(), 1u);
    EXPECT_EQ(slots[0].username, "alice");
    EXPECT_TRUE(slots[0].active);
}

TEST(KeySlotManagerUnitTests, StoreUserSlotFailsAtConfiguredCapacity) {
    VaultSecurityPolicy policy = make_policy(UsernameHashService::Algorithm::SHA3_256);
    const std::array<uint8_t, 16> username_salt = {
        17, 17, 17, 17, 18, 18, 18, 18,
        19, 19, 19, 19, 20, 20, 20, 20};

    std::vector<KeySlot> slots;
    slots.push_back(make_slot_for_username(
        "alice",
        policy,
        username_salt,
        UsernameHashService::Algorithm::SHA3_256));

    auto result = KeySlotManager::store_user_slot(
        slots,
        make_slot_for_username(
            "bob",
            policy,
            username_salt,
            UsernameHashService::Algorithm::SHA3_256),
        1);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::MaxUsersReached);
}

TEST(KeySlotManagerUnitTests, DeactivateUserRejectsLastAdministrator) {
    VaultSecurityPolicy policy = make_policy(UsernameHashService::Algorithm::SHA3_256);
    const std::array<uint8_t, 16> username_salt = {
        21, 21, 21, 21, 22, 22, 22, 22,
        23, 23, 23, 23, 24, 24, 24, 24};

    std::vector<KeySlot> slots;
    slots.push_back(make_slot_for_username(
        "admin",
        policy,
        username_salt,
        UsernameHashService::Algorithm::SHA3_256,
        UserRole::ADMINISTRATOR));

    auto result = KeySlotManager::deactivate_user(slots, "admin", policy);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::LastAdministrator);
    EXPECT_TRUE(slots[0].active);
}

TEST(KeySlotManagerUnitTests, DeactivateUserAllowsRemovingNonFinalAdministrator) {
    VaultSecurityPolicy policy = make_policy(UsernameHashService::Algorithm::SHA3_256);
    const std::array<uint8_t, 16> username_salt = {
        25, 25, 25, 25, 26, 26, 26, 26,
        27, 27, 27, 27, 28, 28, 28, 28};

    std::vector<KeySlot> slots;
    slots.push_back(make_slot_for_username(
        "admin-a",
        policy,
        username_salt,
        UsernameHashService::Algorithm::SHA3_256,
        UserRole::ADMINISTRATOR));
    slots.push_back(make_slot_for_username(
        "admin-b",
        policy,
        username_salt,
        UsernameHashService::Algorithm::SHA3_256,
        UserRole::ADMINISTRATOR));

    auto result = KeySlotManager::deactivate_user(slots, "admin-a", policy);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(slots[0].active);
    EXPECT_TRUE(slots[1].active);
}

TEST(KeySlotManagerUnitTests, ApplyAndClearYubiKeyEnrollmentRoundTripsState) {
    KeySlot slot;
    const std::array<uint8_t, 32> challenge = {
        1, 2, 3, 4, 5, 6, 7, 8,
        9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24,
        25, 26, 27, 28, 29, 30, 31, 32};
    const std::vector<uint8_t> encrypted_pin = {1, 2, 3, 4};
    const std::vector<uint8_t> credential_id = {5, 6, 7, 8};

    KeySlotManager::apply_yubikey_enrollment(
        slot,
        true,
        challenge,
        "YK-123",
        123456789,
        encrypted_pin,
        credential_id);

    EXPECT_TRUE(slot.yubikey_enrolled);
    EXPECT_EQ(slot.yubikey_challenge, challenge);
    EXPECT_EQ(slot.yubikey_serial, "YK-123");
    EXPECT_EQ(slot.yubikey_enrolled_at, 123456789);
    EXPECT_EQ(slot.yubikey_encrypted_pin, encrypted_pin);
    EXPECT_EQ(slot.yubikey_credential_id, credential_id);

    KeySlotManager::clear_yubikey_enrollment(slot);

    EXPECT_FALSE(slot.yubikey_enrolled);
    EXPECT_TRUE(slot.yubikey_serial.empty());
    EXPECT_EQ(slot.yubikey_enrolled_at, 0);
    EXPECT_TRUE(std::all_of(slot.yubikey_challenge.begin(), slot.yubikey_challenge.end(), [](uint8_t b) {
        return b == 0;
    }));
    EXPECT_TRUE(slot.yubikey_encrypted_pin.empty());
    EXPECT_TRUE(slot.yubikey_credential_id.empty());
}

TEST(KeySlotManagerUnitTests, ClearPasswordHistoryReturnsRemovedCount) {
    KeySlot slot;
    slot.password_history.push_back(make_history_entry(1, 100));
    slot.password_history.push_back(make_history_entry(2, 200));

    const size_t removed = KeySlotManager::clear_password_history(slot);

    EXPECT_EQ(removed, 2u);
    EXPECT_TRUE(slot.password_history.empty());
}
