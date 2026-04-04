// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include "../src/core/services/VaultDataService.h"

using namespace KeepTower;

class VaultDataServiceTest : public ::testing::Test {
protected:
    keeptower::VaultData make_vault() {
        keeptower::VaultData vault;
        auto* metadata = vault.mutable_metadata();
        metadata->set_schema_version(2);
        metadata->set_name("Test Vault");
        metadata->set_access_count(3);

        auto* account = vault.add_accounts();
        account->set_account_name("Email");
        account->set_user_name("user@example.com");
        account->set_password("secret");

        return vault;
    }
};

TEST_F(VaultDataServiceTest, SerializeDeserializeRoundTrip) {
    auto vault = make_vault();

    auto serialized = VaultDataService::serialize_vault_data(vault);
    ASSERT_TRUE(serialized.has_value());

    auto deserialized = VaultDataService::deserialize_vault_data(*serialized);
    ASSERT_TRUE(deserialized.has_value());

    EXPECT_EQ(deserialized->metadata().name(), "Test Vault");
    ASSERT_EQ(deserialized->accounts_size(), 1);
    EXPECT_EQ(deserialized->accounts(0).account_name(), "Email");
    EXPECT_EQ(deserialized->accounts(0).user_name(), "user@example.com");
}

TEST_F(VaultDataServiceTest, MigrateSchemaUpdatesLegacyVault) {
    keeptower::VaultData vault;
    vault.add_accounts()->set_account_name("Legacy");

    bool modified = false;
    EXPECT_TRUE(VaultDataService::migrate_vault_schema(vault, modified));
    EXPECT_TRUE(modified);
    EXPECT_EQ(vault.metadata().schema_version(), 2);
    EXPECT_EQ(vault.metadata().access_count(), 1);
}
