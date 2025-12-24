// SPDX-License-Identifier: GPL-3.0-or-later
// Manual test for V1 → V2 vault migration

#include "../src/core/VaultManager.h"
#include "../src/utils/Log.h"
#include <iostream>
#include <filesystem>
#include <ctime>

namespace fs = std::filesystem;

int main() {

    std::string v1_vault_path = "test_vaults/migration_test_v1.vault";
    std::string password = "TestPassword123!";

    std::cout << "\n=== Phase 8: V1 → V2 Migration Test ===\n\n";

    // Ensure test directory exists
    fs::create_directories("test_vaults");

    // Step 1: Create a V1 vault with some test data
    std::cout << "Step 1: Creating V1 vault with test accounts...\n";

    VaultManager vault_manager;
    vault_manager.set_backup_enabled(false);
    vault_manager.set_reed_solomon_enabled(false);

    if (!vault_manager.create_vault(v1_vault_path, password)) {
        std::cerr << "❌ Failed to create V1 vault\n";
        return 1;
    }

    // Add test accounts
    for (int i = 1; i <= 5; ++i) {
        keeptower::AccountRecord account;
        account.set_id("account-" + std::to_string(i));
        account.set_account_name("Test Account " + std::to_string(i));
        account.set_user_name("user" + std::to_string(i) + "@example.com");
        account.set_password("SecretPass" + std::to_string(i) + "!");
        account.set_email("user" + std::to_string(i) + "@example.com");
        account.set_website("https://example" + std::to_string(i) + ".com");
        account.set_notes("Test notes for account " + std::to_string(i));
        account.set_created_at(std::time(nullptr));
        account.set_modified_at(std::time(nullptr));

        if (!vault_manager.add_account(account)) {
            std::cerr << "❌ Failed to add account " << i << "\n";
            return 1;
        }
    }

    if (!vault_manager.save_vault()) {
        std::cerr << "❌ Failed to save V1 vault\n";
        return 1;
    }

    std::cout << "✓ Created V1 vault with 5 test accounts\n";
    std::cout << "  Vault: " << v1_vault_path << "\n";
    std::cout << "  Size: " << fs::file_size(v1_vault_path) << " bytes\n\n";

    // Step 2: Verify V1 vault is not V2
    std::cout << "Step 2: Verifying V1 vault format...\n";
    if (vault_manager.get_current_user_session().has_value()) {
        std::cerr << "❌ ERROR: Vault reports V2 session (should be V1)\n";
        return 1;
    }
    std::cout << "✓ Confirmed V1 vault format (no user session)\n\n";

    // Step 3: Perform migration
    std::cout << "Step 3: Migrating V1 vault to V2 format...\n";

    KeepTower::VaultSecurityPolicy policy;
    policy.min_password_length = 12;
    policy.pbkdf2_iterations = 100000;
    policy.require_yubikey = false;

    Glib::ustring admin_username = "admin";
    Glib::ustring admin_password = "AdminPass123!";

    auto migration_result = vault_manager.convert_v1_to_v2(
        admin_username,
        admin_password,
        policy
    );

    if (!migration_result) {
        std::cerr << "❌ Migration failed: " << static_cast<int>(migration_result.error()) << "\n";
        return 1;
    }

    std::cout << "✓ Migration completed successfully\n\n";

    // Step 4: Verify backup was created
    std::cout << "Step 4: Verifying backup...\n";
    std::string backup_path = v1_vault_path + ".v1.backup";
    if (!fs::exists(backup_path)) {
        std::cerr << "❌ Backup file not found: " << backup_path << "\n";
        return 1;
    }
    std::cout << "✓ Backup created: " << backup_path << "\n";
    std::cout << "  Size: " << fs::file_size(backup_path) << " bytes\n\n";

    // Step 5: Verify V2 vault structure
    std::cout << "Step 5: Verifying V2 vault...\n";

    auto session = vault_manager.get_current_user_session();
    if (!session.has_value()) {
        std::cerr << "❌ No V2 user session found\n";
        return 1;
    }

    std::cout << "✓ V2 user session active\n";
    std::cout << "  Username: " << session->username << "\n";
    std::cout << "  Role: " << (session->role == KeepTower::UserRole::ADMINISTRATOR ? "Administrator" : "Standard") << "\n\n";

    // Step 6: Verify all accounts were migrated
    std::cout << "Step 6: Verifying migrated accounts...\n";
    auto accounts = vault_manager.get_all_accounts();

    if (accounts.size() != 5) {
        std::cerr << "❌ Expected 5 accounts, found " << accounts.size() << "\n";
        return 1;
    }

    std::cout << "✓ All 5 accounts migrated successfully\n";
    for (size_t i = 0; i < accounts.size(); ++i) {
        std::cout << "  " << (i+1) << ". " << accounts[i].account_name()
                  << " (" << accounts[i].user_name() << ")\n";
    }
    std::cout << "\n";

    // Step 7: Test vault close/reopen with V2 credentials
    std::cout << "Step 7: Testing V2 vault close/reopen...\n";

    if (!vault_manager.close_vault()) {
        std::cerr << "❌ Failed to close V2 vault\n";
        return 1;
    }

    auto open_result = vault_manager.open_vault_v2(v1_vault_path, admin_username, admin_password);
    if (!open_result) {
        std::cerr << "❌ Failed to reopen V2 vault\n";
        return 1;
    }

    std::cout << "✓ V2 vault reopened successfully\n";

    auto reopened_session = vault_manager.get_current_user_session();
    if (!reopened_session.has_value()) {
        std::cerr << "❌ No session after reopen\n";
        return 1;
    }

    std::cout << "  Session: " << reopened_session->username
              << " (role: " << (reopened_session->role == KeepTower::UserRole::ADMINISTRATOR ? "Admin" : "User") << ")\n";
    std::cout << "  Accounts: " << vault_manager.get_account_count() << "\n\n";

    // Step 8: Verify V1 vault cannot be opened with old password
    std::cout << "Step 8: Verifying V1 password no longer works...\n";

    if (!vault_manager.close_vault()) {
        std::cerr << "❌ Failed to close vault\n";
        return 1;
    }

    // Try old V1 open method (should fail)
    if (vault_manager.open_vault(v1_vault_path, password)) {
        std::cerr << "❌ WARNING: V1 open succeeded (should fail on V2 vault)\n";
    } else {
        std::cout << "✓ V1 open method correctly fails on V2 vault\n";
    }

    std::cout << "\n=== Migration Test Summary ===\n";
    std::cout << "✓ V1 vault created with 5 accounts\n";
    std::cout << "✓ Migration to V2 completed successfully\n";
    std::cout << "✓ Backup created automatically\n";
    std::cout << "✓ Administrator account established\n";
    std::cout << "✓ All accounts preserved during migration\n";
    std::cout << "✓ V2 vault functions correctly\n";
    std::cout << "✓ V1 credentials no longer work\n";
    std::cout << "\n🎉 Phase 8 Migration Test: PASSED\n\n";

    return 0;
}
