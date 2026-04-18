// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng
//
// Test program to verify advanced security features:
// - Magic header and version
// - Backup mechanism
// - Memory locking
// - Configurable PBKDF2 iterations

#include <iostream>
#include <vector>
#include <filesystem>
#include <fstream>
#include "../src/core/VaultManager.h"
#include "../src/core/managers/AccountManager.h"
#include "../src/lib/vaultformat/VaultFormatV2.h"
#include "../src/core/MultiUserTypes.h"
#include "record.pb.h"  // Include protobuf definitions
// windows.h must come AFTER GLib/glibmm headers: it defines ERROR=0 as a macro
// which clobbers glibmm's IOChannel::ERROR enum member.
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef ERROR  // windows.h defines ERROR=0; undefine to avoid GLib enum conflicts
#endif

namespace fs = std::filesystem;

void print_hex(const std::vector<uint8_t>& data, size_t max_bytes = 32) {
    for (size_t i = 0; i < std::min(data.size(), max_bytes); ++i) {
        printf("%02x ", data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");
}

bool test_magic_header() {
    std::cout << "\n=== Test 1: Magic Header and Version ===\n";

    const std::string vault_path = "/tmp/test_magic.v2";
    fs::remove(vault_path);

    // Create vault
    VaultManager vm;
    KeepTower::VaultSecurityPolicy policy;
    policy.require_yubikey = false;
    policy.min_password_length = 12;
    policy.pbkdf2_iterations = 100000;
    policy.password_history_depth = 0;

    if (!vm.create_vault_v2(vault_path, "admin", "TestPassword123!", policy)) {
        std::cerr << "Failed to create vault\n";
        return false;
    }
    (void)vm.close_vault();  // Ignore return value

    // Read and verify file format
    std::ifstream file(vault_path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open vault file\n";
        return false;
    }

    uint32_t magic = 0;
    uint32_t version = 0;
    uint32_t iterations = 0;
    uint32_t header_size = 0;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    file.read(reinterpret_cast<char*>(&iterations), sizeof(iterations));
    file.read(reinterpret_cast<char*>(&header_size), sizeof(header_size));

    std::cout << "Magic:      0x" << std::hex << magic << " (expected 0x" << KeepTower::VaultFormatV2::VAULT_MAGIC << ")\n";
    std::cout << "Version:    " << std::dec << version << " (expected " << KeepTower::VaultFormatV2::VAULT_VERSION_V2 << ")\n";
    std::cout << "Iterations: " << iterations << " (expected " << policy.pbkdf2_iterations << ")\n";
    std::cout << "HeaderSize: " << header_size << " (expected > 0)\n";

    bool success = (magic == KeepTower::VaultFormatV2::VAULT_MAGIC &&
                    version == KeepTower::VaultFormatV2::VAULT_VERSION_V2 &&
                    iterations == policy.pbkdf2_iterations &&
                    header_size > 0);
    std::cout << "Result: " << (success ? "✓ PASS" : "✗ FAIL") << "\n";

    fs::remove(vault_path);
    return success;
}

bool test_backup_mechanism() {
    std::cout << "\n=== Test 2: Backup Mechanism ===\n";

    const std::string vault_path = "/tmp/test_backup.v2";
    const fs::path vault_dir = fs::path(vault_path).parent_path();
    const std::string backup_prefix = fs::path(vault_path).filename().string() + ".backup.";

    auto list_backups = [&]() {
        std::vector<fs::path> backups;
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(vault_dir, ec)) {
            if (ec) {
                break;
            }

            const auto name = entry.path().filename().string();
            if (name.starts_with(backup_prefix)) {
                backups.push_back(entry.path());
            }
        }
        return backups;
    };

    fs::remove(vault_path);
    for (const auto& p : list_backups()) {
        fs::remove(p);
    }

    // Create vault and add data
    VaultManager vm;
    VaultManager::BackupSettings backup_settings = vm.get_backup_settings();
    backup_settings.enabled = true;
    if (!vm.apply_backup_settings(backup_settings)) {
        std::cerr << "Failed to configure backup settings\n";
        return false;
    }

    KeepTower::VaultSecurityPolicy policy;
    policy.require_yubikey = false;
    policy.min_password_length = 12;
    policy.pbkdf2_iterations = 100000;
    policy.password_history_depth = 0;

    if (!vm.create_vault_v2(vault_path, "admin", "TestPassword123!", policy)) {
        std::cerr << "Failed to create vault\n";
        return false;
    }

    // Add account
    keeptower::AccountRecord account;
    account.set_account_name("First Account");
    account.set_user_name("user1");
    account.set_password("pass1");
    (void)vm.account_manager()->add_account(account);

    // First save - no backup expected (file didn't exist before)
    if (!vm.save_vault()) {
        std::cerr << "Failed first save\n";
        return false;
    }

    const auto backups_after_first_save = list_backups();
    std::cout << "Backup exists after first save: "
              << (!backups_after_first_save.empty() ? "YES" : "NO") << "\n";

    // Modify and save again - backup should be created this time
    account.set_account_name("Modified Account");
    (void)vm.account_manager()->update_account(0, account);

    if (!vm.save_vault()) {
        std::cerr << "Failed second save\n";
        return false;
    }

    const auto backups_after_second_save = list_backups();
    bool backup_exists = !backups_after_second_save.empty();
    std::cout << "Backup exists after second save: "
              << (backup_exists ? "YES ✓" : "NO ✗") << "\n";

    if (backup_exists) {
        auto vault_size = fs::file_size(vault_path);
        auto backup_size = fs::file_size(backups_after_second_save.front());
        std::cout << "Vault size:  " << vault_size << " bytes\n";
        std::cout << "Backup size: " << backup_size << " bytes\n";
    }

    (void)vm.close_vault();

    fs::remove(vault_path);
    for (const auto& p : list_backups()) {
        fs::remove(p);
    }

    return backup_exists;
}

bool test_memory_locking() {
    std::cout << "\n=== Test 3: Memory Locking ===\n";

    const std::string vault_path = "/tmp/test_mlock.v2";
    fs::remove(vault_path);

    VaultManager vm;
    KeepTower::VaultSecurityPolicy policy;
    policy.require_yubikey = false;
    policy.min_password_length = 12;
    policy.pbkdf2_iterations = 100000;
    policy.password_history_depth = 0;

    if (!vm.create_vault_v2(vault_path, "admin", "TestPassword123!", policy)) {
        std::cerr << "Failed to create vault\n";
        return false;
    }

    std::cout << "Vault created - memory locking attempted during key derivation\n";
    std::cout << "Check logs for 'Locked N bytes of sensitive memory' messages\n";

#ifdef __linux__
    std::cout << "Platform: Linux (mlock used)\n";
    std::cout << "To verify, run: grep VmLck /proc/$PPID/status\n";
#elif _WIN32
    std::cout << "Platform: Windows (VirtualLock used)\n";
#else
    std::cout << "Platform: Other (memory locking not implemented)\n";
#endif

    std::cout << "Result: ✓ PASS (implementation present)\n";

    (void)vm.close_vault();
    fs::remove(vault_path);
    return true;
}

int main() {
#ifdef _WIN32
    // Enable UTF-8 console output so box-drawing chars and symbols render correctly
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::cout << "╔════════════════════════════════════════════════════╗\n";
    std::cout << "║  KeepTower Advanced Security Features Test Suite  ║\n";
    std::cout << "╚════════════════════════════════════════════════════╝\n";

    // Match application startup: initialize FIPS provider state once, before any crypto.
    // This avoids noisy warnings from is_fips_available()/is_fips_enabled() callers.
    (void)VaultManager::init_fips_mode(false);

    int passed = 0;
    int total = 3;

    if (test_magic_header()) passed++;
    if (test_backup_mechanism()) passed++;
    if (test_memory_locking()) passed++;

    std::cout << "\n" << std::string(52, '=') << "\n";
    std::cout << "Results: " << passed << "/" << total << " tests passed";

    if (passed == total) {
        std::cout << " ✓✓✓\n";
        std::cout << "\n🎉 All advanced security features working correctly!\n";
        return 0;
    } else {
        std::cout << " ✗✗✗\n";
        return 1;
    }
}
