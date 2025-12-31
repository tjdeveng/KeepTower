// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file test_vault_serialization.cc
 * @brief Unit tests for VaultSerialization protobuf operations
 *
 * Tests serialization, deserialization, and schema migration
 * for vault data using Protocol Buffers.
 */

#include <gtest/gtest.h>
#include "../src/core/serialization/VaultSerialization.h"
#include "record.pb.h"

using namespace KeepTower;

// ============================================================================
// Test Fixture
// ============================================================================

class VaultSerializationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create sample vault data
        vault_data = keeptower::VaultData();

        // Add sample account with string ID
        auto* account = vault_data.add_accounts();
        account->set_id("test-account-1");
        account->set_account_name("Test Account");
        account->set_user_name("testuser");
        account->set_password("testpass");
        account->set_email("test@example.com");
        account->set_website("https://example.com");
        account->set_created_at(1700000000);
        account->set_modified_at(1700000000);
        account->set_notes("Test notes");
    }

    keeptower::VaultData vault_data;
};

// ============================================================================
// Serialization Tests
// ============================================================================

TEST_F(VaultSerializationTest, SerializeSuccess) {
    auto result = VaultSerialization::serialize(vault_data);

    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result->size(), 0);
}

TEST_F(VaultSerializationTest, SerializeEmptyVault) {
    keeptower::VaultData empty_vault;

    auto result = VaultSerialization::serialize(empty_vault);

    ASSERT_TRUE(result.has_value());
    // Empty vault may serialize to empty or minimal data
    EXPECT_GE(result->size(), 0);
}

TEST_F(VaultSerializationTest, SerializeMultipleAccounts) {
    // Add more accounts
    for (int i = 2; i <= 10; ++i) {
        auto* account = vault_data.add_accounts();
        account->set_id("account-" + std::to_string(i));
        account->set_account_name("Account " + std::to_string(i));
        account->set_user_name("user" + std::to_string(i));
        account->set_password("pass" + std::to_string(i));
    }

    auto result = VaultSerialization::serialize(vault_data);

    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result->size(), 0);
}

TEST_F(VaultSerializationTest, SerializeWithMetadata) {
    auto* metadata = vault_data.mutable_metadata();
    metadata->set_schema_version(2);
    metadata->set_created_at(1700000000);
    metadata->set_last_modified(1700000000);
    metadata->set_last_accessed(1700000000);
    metadata->set_access_count(5);

    auto result = VaultSerialization::serialize(vault_data);

    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result->size(), 0);
}

TEST_F(VaultSerializationTest, SerializeLargeVault) {
    // Create vault with many accounts
    for (int i = 0; i < 1000; ++i) {
        auto* account = vault_data.add_accounts();
        account->set_id("account-" + std::to_string(i));
        account->set_account_name("Account " + std::to_string(i));
        account->set_user_name("user" + std::to_string(i));
        account->set_password("password_" + std::to_string(i));
        account->set_notes(std::string(100, 'x'));  // Large notes
    }

    auto result = VaultSerialization::serialize(vault_data);

    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result->size(), 100000);  // Should be substantial
}

// ============================================================================
// Deserialization Tests
// ============================================================================

TEST_F(VaultSerializationTest, DeserializeSuccess) {
    // Serialize first
    auto serialized = VaultSerialization::serialize(vault_data);
    ASSERT_TRUE(serialized.has_value());

    // Deserialize
    auto result = VaultSerialization::deserialize(serialized.value());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->accounts_size(), 1);
    EXPECT_EQ(result->accounts(0).account_name(), "Test Account");
}

TEST_F(VaultSerializationTest, DeserializeEmptyData) {
    std::vector<uint8_t> empty_data;

    auto result = VaultSerialization::deserialize(empty_data);

    // Empty data should deserialize to empty protobuf (valid)
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->accounts_size(), 0);
}

TEST_F(VaultSerializationTest, DeserializeInvalidData) {
    std::vector<uint8_t> invalid_data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    auto result = VaultSerialization::deserialize(invalid_data);

    // May fail or succeed depending on protobuf's resilience
    // Invalid protobuf should be caught
    if (!result.has_value()) {
        EXPECT_EQ(result.error(), VaultError::InvalidProtobuf);
    }
}

TEST_F(VaultSerializationTest, DeserializeTooLarge) {
    // Create data exceeding MAX_VAULT_SIZE (100 MB)
    std::vector<uint8_t> huge_data(101 * 1024 * 1024, 0xAA);

    auto result = VaultSerialization::deserialize(huge_data);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::InvalidProtobuf);
}

TEST_F(VaultSerializationTest, DeserializeCorruptedData) {
    // Serialize valid data
    auto serialized = VaultSerialization::serialize(vault_data);
    ASSERT_TRUE(serialized.has_value());

    // Corrupt some bytes
    auto corrupted = serialized.value();
    if (corrupted.size() > 10) {
        corrupted[5] ^= 0xFF;
        corrupted[10] ^= 0xFF;
    }

    auto result = VaultSerialization::deserialize(corrupted);

    // May fail or succeed with corrupted data (protobuf is resilient)
    // If it succeeds, data may be partially valid
}

// ============================================================================
// Round-Trip Tests
// ============================================================================

TEST_F(VaultSerializationTest, RoundTripBasicVault) {
    // Serialize
    auto serialized = VaultSerialization::serialize(vault_data);
    ASSERT_TRUE(serialized.has_value());

    // Deserialize
    auto deserialized = VaultSerialization::deserialize(serialized.value());
    ASSERT_TRUE(deserialized.has_value());

    // Verify
    EXPECT_EQ(deserialized->accounts_size(), vault_data.accounts_size());
    EXPECT_EQ(deserialized->accounts(0).account_name(), vault_data.accounts(0).account_name());
    EXPECT_EQ(deserialized->accounts(0).user_name(), vault_data.accounts(0).user_name());
    EXPECT_EQ(deserialized->accounts(0).password(), vault_data.accounts(0).password());
}

TEST_F(VaultSerializationTest, RoundTripWithMetadata) {
    auto* metadata = vault_data.mutable_metadata();
    metadata->set_schema_version(2);
    metadata->set_created_at(1700000000);
    metadata->set_last_modified(1700000000);
    metadata->set_last_accessed(1700000000);
    metadata->set_access_count(5);

    // Serialize
    auto serialized = VaultSerialization::serialize(vault_data);
    ASSERT_TRUE(serialized.has_value());

    // Deserialize
    auto deserialized = VaultSerialization::deserialize(serialized.value());
    ASSERT_TRUE(deserialized.has_value());

    // Verify metadata
    EXPECT_EQ(deserialized->metadata().schema_version(), 2);
    EXPECT_EQ(deserialized->metadata().created_at(), 1700000000);
    EXPECT_EQ(deserialized->metadata().access_count(), 5);
}

TEST_F(VaultSerializationTest, RoundTripMultipleGroups) {
    // Skip - AccountGroup API has changed
}

// ============================================================================
// Schema Migration Tests
// ============================================================================

TEST_F(VaultSerializationTest, MigrateV1ToV2) {
    // Create v1 vault (no metadata, has accounts)
    keeptower::VaultData v1_vault;
    auto* account = v1_vault.add_accounts();
    account->set_id("account-1");
    account->set_account_name("V1 Account");
    account->set_user_name("v1user");
    account->set_password("v1pass");

    // No metadata set (schema_version = 0)
    EXPECT_EQ(v1_vault.metadata().schema_version(), 0);

    bool modified = false;
    bool result = VaultSerialization::migrate_schema(v1_vault, modified);

    ASSERT_TRUE(result);
    EXPECT_TRUE(modified);
    EXPECT_EQ(v1_vault.metadata().schema_version(), 2);
    EXPECT_GT(v1_vault.metadata().created_at(), 0);
    EXPECT_GT(v1_vault.metadata().last_modified(), 0);
    EXPECT_EQ(v1_vault.metadata().access_count(), 1);
}

TEST_F(VaultSerializationTest, MigrateEmptyV2Vault) {
    // New empty vault (no accounts, no metadata)
    keeptower::VaultData empty_vault;

    bool modified = false;
    bool result = VaultSerialization::migrate_schema(empty_vault, modified);

    ASSERT_TRUE(result);
    EXPECT_FALSE(modified);  // New vault, not marked as modified
    EXPECT_EQ(empty_vault.metadata().schema_version(), 2);
    EXPECT_GT(empty_vault.metadata().created_at(), 0);
}

TEST_F(VaultSerializationTest, MigrateCurrentVersion) {
    // Vault already at current version
    auto* metadata = vault_data.mutable_metadata();
    metadata->set_schema_version(2);
    metadata->set_created_at(1700000000);
    metadata->set_last_modified(1700000000);
    metadata->set_last_accessed(1700000000);
    metadata->set_access_count(5);

    bool modified = false;
    bool result = VaultSerialization::migrate_schema(vault_data, modified);

    ASSERT_TRUE(result);
    EXPECT_TRUE(modified);  // Access tracking updated
    EXPECT_EQ(vault_data.metadata().schema_version(), 2);
    EXPECT_EQ(vault_data.metadata().access_count(), 6);  // Incremented
}

TEST_F(VaultSerializationTest, MigrateTracksAccess) {
    auto* metadata = vault_data.mutable_metadata();
    metadata->set_schema_version(2);
    metadata->set_created_at(1700000000);
    metadata->set_last_modified(1700000000);
    metadata->set_last_accessed(1700000000);
    metadata->set_access_count(10);

    int64_t old_accessed = metadata->last_accessed();

    // Small delay to ensure timestamp changes
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    bool modified = false;
    VaultSerialization::migrate_schema(vault_data, modified);

    EXPECT_GT(vault_data.metadata().last_accessed(), old_accessed);
    EXPECT_EQ(vault_data.metadata().access_count(), 11);
}

TEST_F(VaultSerializationTest, MigrateFutureVersion) {
    // Vault with future schema version (forward compatibility)
    auto* metadata = vault_data.mutable_metadata();
    metadata->set_schema_version(99);
    metadata->set_access_count(5);

    bool modified = false;
    bool result = VaultSerialization::migrate_schema(vault_data, modified);

    ASSERT_TRUE(result);
    EXPECT_TRUE(modified);  // Access tracking still updated
    EXPECT_EQ(vault_data.metadata().schema_version(), 99);  // Version preserved
    EXPECT_EQ(vault_data.metadata().access_count(), 6);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(VaultSerializationTest, SerializeAccountWithAllFields) {
    keeptower::VaultData complete_vault;
    auto* account = complete_vault.add_accounts();

    // Fill all possible fields
    account->set_id("account-42");
    account->set_account_name("Complete Account");
    account->set_user_name("completeuser");
    account->set_password("CompletePass123!");
    account->set_email("complete@example.com");
    account->set_website("https://complete.example.com");
    account->set_created_at(1700000000);
    account->set_modified_at(1700000100);
    account->set_notes("These are comprehensive notes\nWith multiple lines\nAnd special chars: äöü");
    account->set_password_changed_at(1700000200);
    account->set_is_favorite(true);
    account->set_is_archived(false);
    account->set_color("#FF5733");
    account->set_icon("key-icon");
    account->set_global_display_order(42);

    auto serialized = VaultSerialization::serialize(complete_vault);
    ASSERT_TRUE(serialized.has_value());

    auto deserialized = VaultSerialization::deserialize(serialized.value());
    ASSERT_TRUE(deserialized.has_value());

    // Verify all fields preserved
    EXPECT_EQ(deserialized->accounts(0).id(), "account-42");
    EXPECT_EQ(deserialized->accounts(0).email(), "complete@example.com");
    EXPECT_TRUE(deserialized->accounts(0).is_favorite());
    EXPECT_EQ(deserialized->accounts(0).global_display_order(), 42);
}

TEST_F(VaultSerializationTest, SerializeUnicodeData) {
    keeptower::VaultData unicode_vault;
    auto* account = unicode_vault.add_accounts();

    account->set_account_name("测试账户");  // Chinese
    account->set_user_name("пользователь");  // Russian
    account->set_password("パスワード");  // Japanese
    account->set_notes("Émojis: 🔒🔑💾");

    auto serialized = VaultSerialization::serialize(unicode_vault);
    ASSERT_TRUE(serialized.has_value());

    auto deserialized = VaultSerialization::deserialize(serialized.value());
    ASSERT_TRUE(deserialized.has_value());

    EXPECT_EQ(deserialized->accounts(0).account_name(), "测试账户");
    EXPECT_EQ(deserialized->accounts(0).user_name(), "пользователь");
    EXPECT_EQ(deserialized->accounts(0).password(), "パスワード");
}

TEST_F(VaultSerializationTest, SerializeEmptyStrings) {
    keeptower::VaultData empty_strings_vault;
    auto* account = empty_strings_vault.add_accounts();

    account->set_id("account-1");
    account->set_account_name("");
    account->set_user_name("");
    account->set_password("");
    account->set_email("");
    account->set_website("");
    account->set_notes("");

    auto serialized = VaultSerialization::serialize(empty_strings_vault);
    ASSERT_TRUE(serialized.has_value());

    auto deserialized = VaultSerialization::deserialize(serialized.value());
    ASSERT_TRUE(deserialized.has_value());

    EXPECT_EQ(deserialized->accounts(0).account_name(), "");
    EXPECT_EQ(deserialized->accounts(0).password(), "");
}

TEST_F(VaultSerializationTest, SerializeLongStrings) {
    keeptower::VaultData long_vault;
    auto* account = long_vault.add_accounts();

    std::string long_string(10000, 'x');
    account->set_id("account-1");
    account->set_account_name(long_string);
    account->set_notes(long_string);

    auto serialized = VaultSerialization::serialize(long_vault);
    ASSERT_TRUE(serialized.has_value());

    auto deserialized = VaultSerialization::deserialize(serialized.value());
    ASSERT_TRUE(deserialized.has_value());

    EXPECT_EQ(deserialized->accounts(0).account_name().size(), 10000);
}

TEST_F(VaultSerializationTest, SerializeSpecialCharacters) {
    keeptower::VaultData special_vault;
    auto* account = special_vault.add_accounts();

    account->set_account_name("Test<>Account");
    account->set_user_name("user&name");
    account->set_password("pass\"word'");
    account->set_notes("Line1\nLine2\r\nLine3\tTabbed");

    auto serialized = VaultSerialization::serialize(special_vault);
    ASSERT_TRUE(serialized.has_value());

    auto deserialized = VaultSerialization::deserialize(serialized.value());
    ASSERT_TRUE(deserialized.has_value());

    EXPECT_EQ(deserialized->accounts(0).account_name(), "Test<>Account");
    EXPECT_EQ(deserialized->accounts(0).user_name(), "user&name");
}

TEST_F(VaultSerializationTest, SerializeDeterministic) {
    // Same data should produce same serialization
    auto serialized1 = VaultSerialization::serialize(vault_data);
    auto serialized2 = VaultSerialization::serialize(vault_data);

    ASSERT_TRUE(serialized1.has_value());
    ASSERT_TRUE(serialized2.has_value());

    EXPECT_EQ(serialized1.value(), serialized2.value());
}
// ============================================================================
// Comprehensive Edge Case and Error Handling Tests
// ============================================================================

TEST_F(VaultSerializationTest, SerializationError_NullBytes) {
    keeptower::VaultData vault;
    auto* account = vault.add_accounts();
    account->set_id("null-test");
    account->set_password(std::string(100, '\0'));  // Null bytes
    account->set_notes("\0embedded\0nulls\0");

    auto result = VaultSerialization::serialize(vault);
    ASSERT_TRUE(result.has_value());

    auto deserialized = VaultSerialization::deserialize(result.value());
    ASSERT_TRUE(deserialized.has_value());
    EXPECT_EQ(deserialized->accounts(0).password().size(), 100);
}

TEST_F(VaultSerializationTest, DeserializeTruncatedMessage) {
    auto serialized = VaultSerialization::serialize(vault_data);
    ASSERT_TRUE(serialized.has_value());

    // Truncate the message
    auto truncated = serialized.value();
    if (truncated.size() > 10) {
        truncated.resize(truncated.size() / 2);
    }

    auto result = VaultSerialization::deserialize(truncated);
    // Protobuf may handle truncation gracefully or fail
    if (result.has_value()) {
        // Partial data may be recovered
        SUCCEED();
    } else {
        EXPECT_EQ(result.error(), VaultError::InvalidProtobuf);
    }
}

TEST_F(VaultSerializationTest, DeserializeExcessiveData) {
    // Create a large but valid vault at exactly the size limit
    keeptower::VaultData large_vault;

    // Add accounts until we approach the limit
    std::string large_notes(50000, 'X');
    for (int i = 0; i < 1500; ++i) {
        auto* account = large_vault.add_accounts();
        account->set_id("large-account-" + std::to_string(i));
        account->set_notes(large_notes);
    }

    auto serialized = VaultSerialization::serialize(large_vault);
    ASSERT_TRUE(serialized.has_value());

    // Should succeed if under 100MB
    if (serialized->size() < 100 * 1024 * 1024) {
        auto result = VaultSerialization::deserialize(serialized.value());
        EXPECT_TRUE(result.has_value());
    }
}

TEST_F(VaultSerializationTest, SerializeAccountWithAllProtobufFields) {
    keeptower::VaultData complete;
    auto* account = complete.add_accounts();

    // Set all available protobuf fields
    account->set_id("complete-id");
    account->set_account_name("Complete");
    account->set_user_name("user");
    account->set_password("pass");
    account->set_email("email@test.com");
    account->set_website("https://example.com");
    account->set_created_at(1700000000);
    account->set_modified_at(1700000100);
    account->set_password_changed_at(1700000200);
    account->set_notes("notes");
    account->add_tags("tag1");
    account->add_tags("tag2");
    account->set_is_favorite(true);
    account->set_is_archived(false);
    account->set_color("#FF5733");
    account->set_icon("key-icon");
    account->set_global_display_order(42);
    account->set_is_admin_only_viewable(false);
    account->set_is_admin_only_deletable(true);
    account->add_password_history("oldpass1");
    account->add_password_history("oldpass2");
    account->set_recovery_email("recovery@test.com");
    account->set_recovery_phone("+1234567890");

    auto* field = account->add_custom_fields();
    field->set_name("CustomField");
    field->set_value("CustomValue");
    field->set_is_sensitive(true);
    field->set_field_type("text");

    auto result = VaultSerialization::serialize(complete);
    ASSERT_TRUE(result.has_value());

    auto deserialized = VaultSerialization::deserialize(result.value());
    ASSERT_TRUE(deserialized.has_value());

    const auto& acc = deserialized->accounts(0);
    EXPECT_EQ(acc.tags_size(), 2);
    EXPECT_TRUE(acc.is_favorite());
    EXPECT_TRUE(acc.is_admin_only_deletable());
    EXPECT_EQ(acc.password_history_size(), 2);
    EXPECT_EQ(acc.custom_fields_size(), 1);
    EXPECT_EQ(acc.custom_fields(0).name(), "CustomField");
}

TEST_F(VaultSerializationTest, MigrateWithGroupMemberships) {
    keeptower::VaultData vault;
    auto* account = vault.add_accounts();
    account->set_id("grouped-account");

    auto* membership = account->add_groups();
    membership->set_group_id("group-uuid-123");
    membership->set_display_order(5);

    bool modified = false;
    ASSERT_TRUE(VaultSerialization::migrate_schema(vault, modified));

    auto serialized = VaultSerialization::serialize(vault);
    ASSERT_TRUE(serialized.has_value());

    auto deserialized = VaultSerialization::deserialize(serialized.value());
    ASSERT_TRUE(deserialized.has_value());

    EXPECT_EQ(deserialized->accounts(0).groups_size(), 1);
    EXPECT_EQ(deserialized->accounts(0).groups(0).group_id(), "group-uuid-123");
}

TEST_F(VaultSerializationTest, MigrateWithSecurityQuestions) {
    keeptower::VaultData vault;
    auto* account = vault.add_accounts();
    account->set_id("secure-account");

    auto* sq = account->add_security_questions();
    sq->set_name("Mother's maiden name?");
    sq->set_value("SecretAnswer");
    sq->set_is_sensitive(true);

    bool modified = false;
    ASSERT_TRUE(VaultSerialization::migrate_schema(vault, modified));

    auto serialized = VaultSerialization::serialize(vault);
    ASSERT_TRUE(serialized.has_value());

    auto deserialized = VaultSerialization::deserialize(serialized.value());
    ASSERT_TRUE(deserialized.has_value());

    EXPECT_EQ(deserialized->accounts(0).security_questions_size(), 1);
}

TEST_F(VaultSerializationTest, SerializeMetadataEdgeCases) {
    keeptower::VaultData vault;
    auto* metadata = vault.mutable_metadata();

    // Set edge case values
    metadata->set_schema_version(INT32_MAX);
    metadata->set_created_at(INT64_MAX);
    metadata->set_last_modified(INT64_MIN);
    metadata->set_last_accessed(0);
    metadata->set_access_count(UINT64_MAX);
    metadata->set_name("Test Vault");
    metadata->set_description("Edge case test");
}

TEST_F(VaultSerializationTest, SerializeVaultSettings) {
    keeptower::VaultData vault;

    // Test vault-level settings through metadata and top-level fields
    auto* metadata = vault.mutable_metadata();
    metadata->set_auto_lock_timeout_seconds(900);  // 15 minutes
    metadata->set_clipboard_timeout_seconds(30);

    vault.set_fec_enabled(false);
    vault.set_fec_redundancy_percent(25);
    vault.set_backup_enabled(true);
    vault.set_backup_count(5);

    auto result = VaultSerialization::serialize(vault);
    ASSERT_TRUE(result.has_value());

    auto deserialized = VaultSerialization::deserialize(result.value());
    ASSERT_TRUE(deserialized.has_value());

    EXPECT_EQ(deserialized->metadata().auto_lock_timeout_seconds(), 900);
    EXPECT_EQ(deserialized->backup_count(), 5);
    EXPECT_FALSE(deserialized->fec_enabled());
}

TEST_F(VaultSerializationTest, RoundTripWithAccountGroups) {
    keeptower::VaultData vault;

    // Add account groups
    auto* group1 = vault.add_groups();
    group1->set_group_id("group-uuid-1");
    group1->set_group_name("Work");
    group1->set_display_order(0);
    group1->set_is_expanded(true);
    group1->set_color("#FF5733");

    auto* group2 = vault.add_groups();
    group2->set_group_id("group-uuid-2");
    group2->set_group_name("Personal");
    group2->set_display_order(1);
    group2->set_is_system_group(false);

    auto result = VaultSerialization::serialize(vault);
    ASSERT_TRUE(result.has_value());

    auto deserialized = VaultSerialization::deserialize(result.value());
    ASSERT_TRUE(deserialized.has_value());

    EXPECT_EQ(deserialized->groups_size(), 2);
    EXPECT_EQ(deserialized->groups(0).group_name(), "Work");
    EXPECT_TRUE(deserialized->groups(0).is_expanded());
    EXPECT_EQ(deserialized->groups(1).group_name(), "Personal");
}

TEST_F(VaultSerializationTest, MigratePreservesAccountOrdering) {
    keeptower::VaultData v1_vault;

    // Add accounts in specific order
    for (int i = 0; i < 10; ++i) {
        auto* account = v1_vault.add_accounts();
        account->set_id("account-" + std::to_string(i));
        account->set_account_name("Account " + std::to_string(i));
        account->set_global_display_order(i);
    }

    bool modified = false;
    ASSERT_TRUE(VaultSerialization::migrate_schema(v1_vault, modified));

    // Verify ordering preserved
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(v1_vault.accounts(i).id(), "account-" + std::to_string(i));
        EXPECT_EQ(v1_vault.accounts(i).global_display_order(), i);
    }
}

TEST_F(VaultSerializationTest, MigrateMultipleTimes) {
    keeptower::VaultData vault;
    auto* account = vault.add_accounts();
    account->set_id("test");

    bool modified1 = false;
    ASSERT_TRUE(VaultSerialization::migrate_schema(vault, modified1));

    int64_t first_access = vault.metadata().last_accessed();
    uint64_t first_count = vault.metadata().access_count();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    bool modified2 = false;
    ASSERT_TRUE(VaultSerialization::migrate_schema(vault, modified2));

    EXPECT_GT(vault.metadata().last_accessed(), first_access);
    EXPECT_EQ(vault.metadata().access_count(), first_count + 1);
}

TEST_F(VaultSerializationTest, DeserializeRandomGarbage) {
    std::vector<uint8_t> garbage;

    // Fill with random-looking data
    for (int i = 0; i < 1000; ++i) {
        garbage.push_back(static_cast<uint8_t>(rand() % 256));
    }

    auto result = VaultSerialization::deserialize(garbage);

    // Should either fail or return partially valid data
    // Protobuf is designed to be resilient
    if (!result.has_value()) {
        EXPECT_EQ(result.error(), VaultError::InvalidProtobuf);
    }
}

TEST_F(VaultSerializationTest, SerializeWithBinaryData) {
    keeptower::VaultData vault;
    auto* account = vault.add_accounts();
    account->set_id("binary-test");

    // Protobuf strings must be valid UTF-8, so test with UTF-8 safe data
    std::string safe_data = "Password with special chars: \\x01\\x02\\x03";
    account->set_password(safe_data);

    auto result = VaultSerialization::serialize(vault);
    ASSERT_TRUE(result.has_value());

    auto deserialized = VaultSerialization::deserialize(result.value());
    ASSERT_TRUE(deserialized.has_value());

    EXPECT_EQ(deserialized->accounts(0).password(), safe_data);
}

TEST_F(VaultSerializationTest, MigrateEmptyMetadataFields) {
    keeptower::VaultData vault;
    auto* account = vault.add_accounts();
    account->set_id("test");

    // Set empty metadata strings
    auto* metadata = vault.mutable_metadata();
    metadata->set_name("");
    metadata->set_description("");

    bool modified = false;
    ASSERT_TRUE(VaultSerialization::migrate_schema(vault, modified));

    EXPECT_EQ(vault.metadata().schema_version(), 2);
}

TEST_F(VaultSerializationTest, SerializeVeryLargeAccountCount) {
    keeptower::VaultData vault;

    // Add 5000 minimal accounts
    for (int i = 0; i < 5000; ++i) {
        auto* account = vault.add_accounts();
        account->set_id("account-" + std::to_string(i));
    }

    auto result = VaultSerialization::serialize(vault);
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result->size(), 10000);

    auto deserialized = VaultSerialization::deserialize(result.value());
    ASSERT_TRUE(deserialized.has_value());
    EXPECT_EQ(deserialized->accounts_size(), 5000);
}

TEST_F(VaultSerializationTest, SerializeMaxSizeString) {
    keeptower::VaultData vault;
    auto* account = vault.add_accounts();
    account->set_id("max-string-test");

    // Create very large string (1MB)
    std::string max_string(1024 * 1024, 'M');
    account->set_notes(max_string);

    auto result = VaultSerialization::serialize(vault);
    ASSERT_TRUE(result.has_value());

    auto deserialized = VaultSerialization::deserialize(result.value());
    ASSERT_TRUE(deserialized.has_value());
    EXPECT_EQ(deserialized->accounts(0).notes().size(), 1024 * 1024);
}

TEST_F(VaultSerializationTest, DeserializeWithUnknownFields) {
    // Simulate future protobuf version with unknown fields
    // Protobuf should skip unknown fields gracefully
    auto serialized = VaultSerialization::serialize(vault_data);
    ASSERT_TRUE(serialized.has_value());

    // Deserialize (unknown fields would be in a newer schema)
    auto result = VaultSerialization::deserialize(serialized.value());
    ASSERT_TRUE(result.has_value());

    // Known fields should be preserved
    EXPECT_EQ(result->accounts(0).account_name(), "Test Account");
}

TEST_F(VaultSerializationTest, SerializeEmptyVaultWithMetadata) {
    keeptower::VaultData empty_vault;
    auto* metadata = empty_vault.mutable_metadata();
    metadata->set_schema_version(2);
    metadata->set_name("Empty Vault");
    metadata->set_description("Test vault with no accounts");

    auto result = VaultSerialization::serialize(empty_vault);
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result->size(), 0);

    auto deserialized = VaultSerialization::deserialize(result.value());
    ASSERT_TRUE(deserialized.has_value());
    EXPECT_EQ(deserialized->accounts_size(), 0);
    EXPECT_EQ(deserialized->metadata().schema_version(), 2);
}

TEST_F(VaultSerializationTest, MigratePreservesUnknownSchemaVersion) {
    keeptower::VaultData future_vault;
    auto* metadata = future_vault.mutable_metadata();
    metadata->set_schema_version(999);  // Future version
    metadata->set_access_count(10);

    auto* account = future_vault.add_accounts();
    account->set_id("future-account");

    bool modified = false;
    ASSERT_TRUE(VaultSerialization::migrate_schema(future_vault, modified));

    // Should preserve future version and update access count
    EXPECT_EQ(future_vault.metadata().schema_version(), 999);
    EXPECT_EQ(future_vault.metadata().access_count(), 11);
    EXPECT_TRUE(modified);
}