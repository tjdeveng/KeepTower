// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file test_undo_redo.cc
 * @brief Unit tests for undo/redo functionality
 */

#include <gtest/gtest.h>
#include "../src/core/commands/Command.h"
#include "../src/core/commands/AccountCommands.h"
#include "../src/core/commands/UndoManager.h"
#include "../src/core/VaultManager.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

/**
 * @brief Test fixture for undo/redo tests
 *
 * Creates a temporary vault for testing command operations.
 */
class UndoRedoTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary test vault
        test_vault_path = "/tmp/test_undo_vault.vault";
        test_password = "test_password_123";

        vault_manager = std::make_unique<VaultManager>();

        // Create new vault
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

    keeptower::AccountRecord create_test_account(const std::string& name) {
        keeptower::AccountRecord account;
        account.set_id(std::to_string(std::time(nullptr)));
        account.set_created_at(std::time(nullptr));
        account.set_modified_at(std::time(nullptr));
        account.set_account_name(name);
        account.set_user_name("testuser");
        account.set_password("testpass");
        account.set_email("test@example.com");
        return account;
    }

    std::string test_vault_path;
    std::string test_password;
    std::unique_ptr<VaultManager> vault_manager;
    std::unique_ptr<UndoManager> undo_manager;
};

/**
 * @test Test basic undo/redo with AddAccountCommand
 */
TEST_F(UndoRedoTest, AddAccountUndoRedo) {
    auto account = create_test_account("Test Account");

    // Execute add command
    auto command = std::make_unique<AddAccountCommand>(
        vault_manager.get(),
        std::move(account),
        nullptr
    );

    ASSERT_TRUE(undo_manager->execute_command(std::move(command)));
    EXPECT_EQ(vault_manager->get_account_count(), 1);
    EXPECT_TRUE(undo_manager->can_undo());
    EXPECT_FALSE(undo_manager->can_redo());

    // Undo add
    ASSERT_TRUE(undo_manager->undo());
    EXPECT_EQ(vault_manager->get_account_count(), 0);
    EXPECT_FALSE(undo_manager->can_undo());
    EXPECT_TRUE(undo_manager->can_redo());

    // Redo add
    ASSERT_TRUE(undo_manager->redo());
    EXPECT_EQ(vault_manager->get_account_count(), 1);
    EXPECT_TRUE(undo_manager->can_undo());
    EXPECT_FALSE(undo_manager->can_redo());
}

/**
 * @test Test DeleteAccountCommand undo/redo
 */
TEST_F(UndoRedoTest, DeleteAccountUndoRedo) {
    // Add an account first
    auto account = create_test_account("To Delete");
    vault_manager->add_account(account);
    ASSERT_EQ(vault_manager->get_account_count(), 1);

    // Delete the account
    auto delete_cmd = std::make_unique<DeleteAccountCommand>(
        vault_manager.get(),
        0,
        nullptr
    );

    ASSERT_TRUE(undo_manager->execute_command(std::move(delete_cmd)));
    EXPECT_EQ(vault_manager->get_account_count(), 0);

    // Undo delete (restore account)
    ASSERT_TRUE(undo_manager->undo());
    EXPECT_EQ(vault_manager->get_account_count(), 1);
    const auto* restored = vault_manager->get_account(0);
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->account_name(), "To Delete");

    // Redo delete
    ASSERT_TRUE(undo_manager->redo());
    EXPECT_EQ(vault_manager->get_account_count(), 0);
}

/**
 * @test Test ToggleFavoriteCommand undo/redo
 */
TEST_F(UndoRedoTest, ToggleFavoriteUndoRedo) {
    // Add an account
    auto account = create_test_account("Favorite Test");
    account.set_is_favorite(false);
    vault_manager->add_account(account);

    const auto* acc = vault_manager->get_account(0);
    ASSERT_NE(acc, nullptr);
    EXPECT_FALSE(acc->is_favorite());

    // Toggle favorite on
    auto toggle_cmd = std::make_unique<ToggleFavoriteCommand>(
        vault_manager.get(),
        0,
        nullptr
    );

    ASSERT_TRUE(undo_manager->execute_command(std::move(toggle_cmd)));
    EXPECT_TRUE(vault_manager->get_account(0)->is_favorite());

    // Undo toggle (back to non-favorite)
    ASSERT_TRUE(undo_manager->undo());
    EXPECT_FALSE(vault_manager->get_account(0)->is_favorite());

    // Redo toggle (back to favorite)
    ASSERT_TRUE(undo_manager->redo());
    EXPECT_TRUE(vault_manager->get_account(0)->is_favorite());
}

/**
 * @test Test ModifyAccountCommand undo/redo
 */
TEST_F(UndoRedoTest, ModifyAccountUndoRedo) {
    // Add an account
    auto account = create_test_account("Original Name");
    vault_manager->add_account(account);

    const auto* acc = vault_manager->get_account(0);
    ASSERT_NE(acc, nullptr);
    EXPECT_EQ(acc->account_name(), "Original Name");

    // Modify the account
    auto modified_account = *acc;
    modified_account.set_account_name("Modified Name");
    modified_account.set_email("modified@example.com");

    auto modify_cmd = std::make_unique<ModifyAccountCommand>(
        vault_manager.get(),
        0,
        std::move(modified_account),
        nullptr
    );

    ASSERT_TRUE(undo_manager->execute_command(std::move(modify_cmd)));
    EXPECT_EQ(vault_manager->get_account(0)->account_name(), "Modified Name");
    EXPECT_EQ(vault_manager->get_account(0)->email(), "modified@example.com");

    // Undo modification
    ASSERT_TRUE(undo_manager->undo());
    EXPECT_EQ(vault_manager->get_account(0)->account_name(), "Original Name");
    EXPECT_EQ(vault_manager->get_account(0)->email(), "test@example.com");

    // Redo modification
    ASSERT_TRUE(undo_manager->redo());
    EXPECT_EQ(vault_manager->get_account(0)->account_name(), "Modified Name");
}

/**
 * @test Test multiple operations with proper history
 */
TEST_F(UndoRedoTest, MultipleOperations) {
    // Add three accounts
    for (int i = 1; i <= 3; ++i) {
        auto account = create_test_account("Account " + std::to_string(i));
        auto cmd = std::make_unique<AddAccountCommand>(
            vault_manager.get(),
            std::move(account),
            nullptr
        );
        ASSERT_TRUE(undo_manager->execute_command(std::move(cmd)));
    }

    EXPECT_EQ(vault_manager->get_account_count(), 3);
    EXPECT_EQ(undo_manager->get_undo_count(), 3);

    // Undo all three
    ASSERT_TRUE(undo_manager->undo());
    EXPECT_EQ(vault_manager->get_account_count(), 2);

    ASSERT_TRUE(undo_manager->undo());
    EXPECT_EQ(vault_manager->get_account_count(), 1);

    ASSERT_TRUE(undo_manager->undo());
    EXPECT_EQ(vault_manager->get_account_count(), 0);

    EXPECT_FALSE(undo_manager->can_undo());

    // Redo all three
    ASSERT_TRUE(undo_manager->redo());
    EXPECT_EQ(vault_manager->get_account_count(), 1);

    ASSERT_TRUE(undo_manager->redo());
    EXPECT_EQ(vault_manager->get_account_count(), 2);

    ASSERT_TRUE(undo_manager->redo());
    EXPECT_EQ(vault_manager->get_account_count(), 3);

    EXPECT_FALSE(undo_manager->can_redo());
}

/**
 * @test Test that new command clears redo stack
 */
TEST_F(UndoRedoTest, NewCommandClearsRedoStack) {
    // Add two accounts
    auto account1 = create_test_account("Account 1");
    auto cmd1 = std::make_unique<AddAccountCommand>(
        vault_manager.get(),
        std::move(account1),
        nullptr
    );
    undo_manager->execute_command(std::move(cmd1));

    auto account2 = create_test_account("Account 2");
    auto cmd2 = std::make_unique<AddAccountCommand>(
        vault_manager.get(),
        std::move(account2),
        nullptr
    );
    undo_manager->execute_command(std::move(cmd2));

    EXPECT_EQ(vault_manager->get_account_count(), 2);

    // Undo last add
    ASSERT_TRUE(undo_manager->undo());
    EXPECT_EQ(vault_manager->get_account_count(), 1);
    EXPECT_TRUE(undo_manager->can_redo());

    // Add a new account - should clear redo stack
    auto account3 = create_test_account("Account 3");
    auto cmd3 = std::make_unique<AddAccountCommand>(
        vault_manager.get(),
        std::move(account3),
        nullptr
    );
    undo_manager->execute_command(std::move(cmd3));

    EXPECT_EQ(vault_manager->get_account_count(), 2);
    EXPECT_FALSE(undo_manager->can_redo()) << "Redo stack should be cleared after new command";
}

/**
 * @test Test history limit enforcement
 */
TEST_F(UndoRedoTest, HistoryLimit) {
    const size_t limit = 5;
    undo_manager->set_max_history(limit);

    // Add more commands than the limit
    for (size_t i = 1; i <= 10; ++i) {
        auto account = create_test_account("Account " + std::to_string(i));
        auto cmd = std::make_unique<AddAccountCommand>(
            vault_manager.get(),
            std::move(account),
            nullptr
        );
        undo_manager->execute_command(std::move(cmd));
    }

    EXPECT_EQ(vault_manager->get_account_count(), 10);
    EXPECT_LE(undo_manager->get_undo_count(), limit) << "History should not exceed limit";

    // Can only undo up to the limit
    size_t undo_count = 0;
    while (undo_manager->can_undo()) {
        [[maybe_unused]] bool undo_success = undo_manager->undo();
        ++undo_count;
    }

    EXPECT_LE(undo_count, limit);
    EXPECT_EQ(vault_manager->get_account_count(), 10 - undo_count);
}

/**
 * @test Test clear() removes all history
 */
TEST_F(UndoRedoTest, ClearHistory) {
    // Add some accounts
    for (int i = 1; i <= 3; ++i) {
        auto account = create_test_account("Account " + std::to_string(i));
        auto cmd = std::make_unique<AddAccountCommand>(
            vault_manager.get(),
            std::move(account),
            nullptr
        );
        [[maybe_unused]] bool exec_success = undo_manager->execute_command(std::move(cmd));
    }

    EXPECT_TRUE(undo_manager->can_undo());

    // Clear history
    undo_manager->clear();

    EXPECT_FALSE(undo_manager->can_undo());
    EXPECT_FALSE(undo_manager->can_redo());
    EXPECT_EQ(undo_manager->get_undo_count(), 0);
    EXPECT_EQ(undo_manager->get_redo_count(), 0);
}

/**
 * @test Test command descriptions
 */
TEST_F(UndoRedoTest, CommandDescriptions) {
    auto account = create_test_account("Gmail");
    auto cmd = std::make_unique<AddAccountCommand>(
        vault_manager.get(),
        std::move(account),
        nullptr
    );

    undo_manager->execute_command(std::move(cmd));

    std::string undo_desc = undo_manager->get_undo_description();
    EXPECT_FALSE(undo_desc.empty());
    EXPECT_NE(undo_desc.find("Gmail"), std::string::npos) << "Description should contain account name";

    [[maybe_unused]] bool undo_success = undo_manager->undo();

    std::string redo_desc = undo_manager->get_redo_description();
    EXPECT_FALSE(redo_desc.empty());
    EXPECT_NE(redo_desc.find("Gmail"), std::string::npos);
}

/**
 * @test Test UI callback invocation
 */
TEST_F(UndoRedoTest, UICallbackInvoked) {
    bool callback_invoked = false;
    auto callback = [&callback_invoked]() {
        callback_invoked = true;
    };

    auto account = create_test_account("Test");
    auto cmd = std::make_unique<AddAccountCommand>(
        vault_manager.get(),
        std::move(account),
        callback
    );

    ASSERT_TRUE(undo_manager->execute_command(std::move(cmd)));
    EXPECT_TRUE(callback_invoked) << "Callback should be invoked on execute";

    callback_invoked = false;
    ASSERT_TRUE(undo_manager->undo());
    EXPECT_TRUE(callback_invoked) << "Callback should be invoked on undo";

    callback_invoked = false;
    ASSERT_TRUE(undo_manager->redo());
    EXPECT_TRUE(callback_invoked) << "Callback should be invoked on redo";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
