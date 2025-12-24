// Quick test for V2 vault creation in new vault flow
#include "../src/core/VaultManager.h"
#include <iostream>

int main() {
    VaultManager vm;
    vm.set_backup_enabled(false);
    vm.set_reed_solomon_enabled(false);

    std::string vault_path = "test_vaults/quick_v2_test.vault";
    Glib::ustring admin_user = "admin";
    Glib::ustring admin_pass = "TestPassword123!";

    KeepTower::VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    policy.pbkdf2_iterations = 100000;
    policy.require_yubikey = false;

    std::cout << "Creating V2 vault...\n";
    auto result = vm.create_vault_v2(vault_path, admin_user, admin_pass, policy);

    if (!result) {
        std::cerr << "Failed to create V2 vault\n";
        return 1;
    }

    std::cout << "✓ V2 vault created successfully\n";

    // Check if we have a session
    auto session = vm.get_current_user_session();
    if (!session.has_value()) {
        std::cerr << "✗ No user session found\n";
        return 1;
    }

    std::cout << "✓ User session active: " << session->username << "\n";
    std::cout << "✓ Role: " << (session->role == KeepTower::UserRole::ADMINISTRATOR ? "Administrator" : "Standard") << "\n";

    // Add a test account
    keeptower::AccountRecord account;
    account.set_account_name("Test Account");
    account.set_user_name("testuser");
    account.set_password("password123");

    if (!vm.add_account(account)) {
        std::cerr << "✗ Failed to add account\n";
        return 1;
    }

    std::cout << "✓ Account added\n";

    if (!vm.save_vault()) {
        std::cerr << "✗ Failed to save vault\n";
        return 1;
    }

    std::cout << "✓ Vault saved\n";

    if (!vm.close_vault()) {
        std::cerr << "✗ Failed to close vault\n";
        return 1;
    }

    std::cout << "✓ Vault closed\n";

    // Reopen as V2
    std::cout << "\nReopening as V2 vault...\n";
    auto open_result = vm.open_vault_v2(vault_path, admin_user, admin_pass);

    if (!open_result) {
        std::cerr << "✗ Failed to reopen V2 vault\n";
        return 1;
    }

    std::cout << "✓ V2 vault reopened\n";

    auto reopened_session = vm.get_current_user_session();
    if (!reopened_session.has_value()) {
        std::cerr << "✗ No session after reopen\n";
        return 1;
    }

    std::cout << "✓ Session: " << reopened_session->username
              << " (role: " << (reopened_session->role == KeepTower::UserRole::ADMINISTRATOR ? "Admin" : "User") << ")\n";
    std::cout << "✓ Accounts: " << vm.get_account_count() << "\n";

    std::cout << "\n🎉 V2 vault creation and reopening works correctly!\n";

    return 0;
}
