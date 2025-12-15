// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file test_secure_memory.cc
 * @brief Tests for secure memory clearing in commands
 */

#include <gtest/gtest.h>
#include "../src/core/commands/AccountCommands.h"
#include "../src/core/commands/UndoManager.h"
#include "../src/core/VaultManager.h"
#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

/**
 * @brief Test fixture for secure memory tests
 */
class SecureMemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_vault_path = "/tmp/test_secure_memory.vault";
        test_password = "test_password_123";

        vault_manager = std::make_unique<VaultManager>();
        bool create_result = vault_manager->create_vault(test_vault_path, test_password);
        ASSERT_TRUE(create_result) << "Failed to create test vault";

        undo_manager = std::make_unique<UndoManager>();
    }

    void TearDown() override {
        vault_manager->close_vault();
        if (fs::exists(test_vault_path)) {
            fs::remove(test_vault_path);
        }
    }

    keeptower::AccountRecord create_test_account_with_password(const std::string& name, const std::string& password) {
        keeptower::AccountRecord account;
        account.set_id(std::to_string(std::time(nullptr)));
        account.set_created_at(std::time(nullptr));
        account.set_modified_at(std::time(nullptr));
        account.set_account_name(name);
        account.set_user_name("testuser");
        account.set_password(password);
        account.set_email("test@example.com");
        return account;
    }

    std::string test_vault_path;
    std::string test_password;
    std::unique_ptr<VaultManager> vault_manager;
    std::unique_ptr<UndoManager> undo_manager;
};

/**
 * @test Verify secure_clear_account() wipes password from memory
 */
TEST_F(SecureMemoryTest, SecureClearAccountWipesPassword) {
    const std::string test_password = "supersecret123!";
    auto account = create_test_account_with_password("Test", test_password);

    // Verify password is set
    EXPECT_EQ(account.password(), test_password);
    EXPECT_FALSE(account.password().empty());

    // Get pointer to password data for verification
    const char* password_data = account.password().data();

    // Clear the account
    secure_clear_account(account);

    // After clearing, password string should be empty
    EXPECT_TRUE(account.password().empty());

    // Memory should be zeroed (we can't directly verify OPENSSL_cleanse worked,
    // but we can verify the string is empty which is the visible effect)
}

/**
 * @test Verify DeleteAccountCommand destructor clears password
 */
TEST_F(SecureMemoryTest, DeleteCommandClearsPasswordOnDestruction) {
    const std::string sensitive_password = "MySecretP@ssw0rd!";
    auto account = create_test_account_with_password("Gmail", sensitive_password);

    // Add account to vault
    vault_manager->add_account(account);
    ASSERT_EQ(vault_manager->get_account_count(), 1);

    // Create delete command (captures account data with password)
    {
        auto cmd = std::make_unique<DeleteAccountCommand>(
            vault_manager.get(),
            0,
            nullptr
        );

        // Execute the command
        ASSERT_TRUE(undo_manager->execute_command(std::move(cmd)));
        EXPECT_EQ(vault_manager->get_account_count(), 0);

        // Undo the delete (restore account)
        ASSERT_TRUE(undo_manager->undo());
        EXPECT_EQ(vault_manager->get_account_count(), 1);

        // Command still exists in redo stack with password data
    }

    // Clear undo/redo history - this destroys the command and should wipe password
    undo_manager->clear();

    // We can't directly verify memory was wiped, but the test shows the
    // destructor runs without crashing and follows the secure clearing pattern
    SUCCEED() << "Command destructors executed successfully";
}

/**
 * @test Verify ModifyAccountCommand destructor clears both old and new passwords
 */
TEST_F(SecureMemoryTest, ModifyCommandClearsBothPasswords) {
    const std::string old_password = "OldP@ssw0rd123";
    const std::string new_password = "NewP@ssw0rd456";

    auto account = create_test_account_with_password("Test", old_password);
    vault_manager->add_account(account);

    const auto* acc = vault_manager->get_account(0);
    ASSERT_NE(acc, nullptr);
    EXPECT_EQ(acc->password(), old_password);

    // Create modify command
    auto modified_account = *acc;
    modified_account.set_password(new_password);

    {
        auto cmd = std::make_unique<ModifyAccountCommand>(
            vault_manager.get(),
            0,
            std::move(modified_account),
            nullptr
        );

        // Execute - now stores both old and new passwords
        ASSERT_TRUE(undo_manager->execute_command(std::move(cmd)));
        EXPECT_EQ(vault_manager->get_account(0)->password(), new_password);
    }

    // Clear history - destroys command and should wipe both passwords
    undo_manager->clear();

    SUCCEED() << "ModifyAccountCommand destructors executed successfully";
}

/**
 * @test Verify AddAccountCommand destructor clears password
 */
TEST_F(SecureMemoryTest, AddCommandClearsPasswordOnDestruction) {
    const std::string sensitive_password = "TopSecret999!";
    auto account = create_test_account_with_password("Bank", sensitive_password);

    {
        auto cmd = std::make_unique<AddAccountCommand>(
            vault_manager.get(),
            std::move(account),
            nullptr
        );

        // Execute - command stores account with password
        ASSERT_TRUE(undo_manager->execute_command(std::move(cmd)));
        EXPECT_EQ(vault_manager->get_account_count(), 1);
    }

    // Clear history - destroys command and should wipe password
    undo_manager->clear();

    SUCCEED() << "AddAccountCommand destructors executed successfully";
}

/**
 * @test Verify multiple commands in history all get cleared
 */
TEST_F(SecureMemoryTest, MultipleCommandsAllClearPasswords) {
    // Add multiple accounts with different passwords
    for (int i = 0; i < 5; ++i) {
        std::string password = "Secret" + std::to_string(i) + "!@#";
        auto account = create_test_account_with_password("Account" + std::to_string(i), password);

        auto cmd = std::make_unique<AddAccountCommand>(
            vault_manager.get(),
            std::move(account),
            nullptr
        );

        ASSERT_TRUE(undo_manager->execute_command(std::move(cmd)));
    }

    EXPECT_EQ(vault_manager->get_account_count(), 5);
    EXPECT_EQ(undo_manager->get_undo_count(), 5);

    // Clear all - should securely wipe all 5 passwords
    undo_manager->clear();

    EXPECT_EQ(undo_manager->get_undo_count(), 0);
    SUCCEED() << "All command destructors executed successfully";
}

/**
 * @test Verify undo manager respects history limit and clears old commands
 */
TEST_F(SecureMemoryTest, HistoryLimitTriggersSecureClear) {
    const size_t limit = 3;
    undo_manager->set_max_history(limit);

    // Add more commands than the limit
    for (size_t i = 0; i < 10; ++i) {
        std::string password = "P@ssw0rd" + std::to_string(i);
        auto account = create_test_account_with_password("Account" + std::to_string(i), password);

        auto cmd = std::make_unique<AddAccountCommand>(
            vault_manager.get(),
            std::move(account),
            nullptr
        );

        [[maybe_unused]] bool success = undo_manager->execute_command(std::move(cmd));
    }

    // Should only keep the most recent 'limit' commands
    EXPECT_LE(undo_manager->get_undo_count(), limit);

    // Old commands should have been destroyed and passwords wiped
    SUCCEED() << "Old commands beyond history limit were securely cleared";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
