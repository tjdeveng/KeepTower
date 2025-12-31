// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file AccountCommands.h
 * @brief Concrete commands for account operations
 */

#ifndef ACCOUNT_COMMANDS_H
#define ACCOUNT_COMMANDS_H

#include "Command.h"
#include "../VaultManager.h"
#include "record.pb.h"
#include <functional>
#include <string>

/** @brief OpenSSL secure memory wipe function
 *  @param ptr Pointer to memory to clear
 *  @param len Length of memory region in bytes
 *  @note Forward declaration to avoid OpenSSL header conflicts */
extern "C" void OPENSSL_cleanse(void *ptr, size_t len);

/**
 * @brief Securely clear password data from an AccountRecord
 * @param account Account whose password field should be wiped from memory
 *
 * Uses OPENSSL_cleanse to prevent compiler optimization from removing
 * the memory clearing operation. This ensures passwords don't linger
 * in memory after commands are removed from undo history.
 */
inline void secure_clear_account(keeptower::AccountRecord& account) {
    // Get mutable password string and wipe it
    std::string* password = account.mutable_password();
    if (password && !password->empty()) {
        OPENSSL_cleanse(const_cast<char*>(password->data()), password->size());
        password->clear();
    }
}

/**
 * @brief Command for adding a new account
 *
 * Stores the account data to enable undo/redo. On undo, removes the
 * account by index. On redo, adds it back at the same position.
 *
 * @warning Security: Destructor securely wipes password from memory.
 */
class AddAccountCommand final : public Command {
public:
    /**
     * @brief Construct command to add an account
     * @param vault_manager Vault manager instance
     * @param account Account data to add
     * @param ui_callback Optional callback to update UI after operation
     */
    AddAccountCommand(
        VaultManager* vault_manager,
        keeptower::AccountRecord account,
        std::function<void()> ui_callback = nullptr
    ) : m_vault_manager(vault_manager),
        m_account(std::move(account)),
        m_ui_callback(std::move(ui_callback)),
        m_added_index(-1) {}

    /**
     * @brief Destructor - securely wipes password from memory
     */
    ~AddAccountCommand() override {
        secure_clear_account(m_account);
    }

    [[nodiscard]] bool execute() override {
        if (!m_vault_manager || !m_vault_manager->is_vault_open()) {
            return false;
        }

        // Add account to vault
        auto result = m_vault_manager->add_account(m_account);
        if (!result) {
            return false;
        }

        // Store the index where it was added (should be last)
        m_added_index = static_cast<int>(m_vault_manager->get_account_count()) - 1;

        if (m_ui_callback) {
            m_ui_callback();
        }

        return true;
    }

    [[nodiscard]] bool undo() override {
        if (!m_vault_manager || m_added_index < 0) {
            return false;
        }

        // Before deleting, save the current state of the account (in case user edited it)
        const auto* current_account = m_vault_manager->get_account(m_added_index);
        if (current_account) {
            // Update our stored account with current state for proper redo
            m_account.CopyFrom(*current_account);
        }

        // Delete the account we added
        bool success = m_vault_manager->delete_account(m_added_index);

        if (success && m_ui_callback) {
            m_ui_callback();
        }

        return success;
    }

    [[nodiscard]] std::string get_description() const override {
        return "Add Account '" + m_account.account_name() + "'";
    }

private:
    VaultManager* m_vault_manager;
    keeptower::AccountRecord m_account;
    std::function<void()> m_ui_callback;
    int m_added_index;
};

/**
 * @brief Command for deleting an account
 *
 * Stores the complete account data and its original index to enable
 * restoration on undo.
 *
 * @warning Security: Destructor securely wipes password from memory.
 */
class DeleteAccountCommand final : public Command {
public:
    /**
     * @brief Construct command to delete an account
     * @param vault_manager Vault manager instance
     * @param account_index Index of account to delete
     * @param ui_callback Optional callback to update UI after operation
     */
    DeleteAccountCommand(
        VaultManager* vault_manager,
        int account_index,
        std::function<void()> ui_callback = nullptr
    ) : m_vault_manager(vault_manager),
        m_account_index(account_index),
        m_ui_callback(std::move(ui_callback)) {

        // Capture account data before deletion
        if (vault_manager && account_index >= 0) {
            const auto* account = vault_manager->get_account(account_index);
            if (account) {
                m_deleted_account = *account;
                m_account_name = account->account_name();
            }
        }
    }

    /**
     * @brief Destructor - securely wipes password from memory
     */
    ~DeleteAccountCommand() override {
        secure_clear_account(m_deleted_account);
    }

    [[nodiscard]] bool execute() override {
        if (!m_vault_manager || m_account_index < 0) {
            return false;
        }

        bool success = m_vault_manager->delete_account(m_account_index);

        if (success && m_ui_callback) {
            m_ui_callback();
        }

        return success;
    }

    [[nodiscard]] bool undo() override {
        if (!m_vault_manager) {
            return false;
        }

        // Re-add the deleted account
        // Note: It will be added at the end, not at original position
        // This is acceptable as account order isn't semantically meaningful
        bool result = m_vault_manager->add_account(m_deleted_account);

        if (result && m_ui_callback) {
            m_ui_callback();
        }

        return result;
    }

    [[nodiscard]] std::string get_description() const override {
        return "Delete Account '" + m_account_name + "'";
    }

private:
    VaultManager* m_vault_manager;
    int m_account_index;
    std::function<void()> m_ui_callback;
    keeptower::AccountRecord m_deleted_account;
    std::string m_account_name;
};

/**
 * @brief Command for modifying an account
 *
 * Stores both old and new states to enable undo/redo.
 */
class ModifyAccountCommand final : public Command {
public:
    /**
     * @brief Construct command to modify an account
     * @param vault_manager Vault manager instance
     * @param account_index Index of account to modify
     * @param new_account New account data
     * @param ui_callback Optional callback to update UI after operation
     */
    ModifyAccountCommand(
        VaultManager* vault_manager,
        int account_index,
        keeptower::AccountRecord new_account,
        std::function<void()> ui_callback = nullptr
    ) : m_vault_manager(vault_manager),
        m_account_index(account_index),
        m_new_account(std::move(new_account)),
        m_ui_callback(std::move(ui_callback)) {

        // Capture current state before modification
        if (vault_manager && account_index >= 0) {
            const auto* account = vault_manager->get_account(account_index);
            if (account) {
                m_old_account = *account;
            }
        }
    }

    /**
     * @brief Destructor - securely wipes passwords from memory
     */
    ~ModifyAccountCommand() override {
        secure_clear_account(m_old_account);
        secure_clear_account(m_new_account);
    }

    [[nodiscard]] bool execute() override {
        if (!m_vault_manager || m_account_index < 0) {
            return false;
        }

        auto* account = m_vault_manager->get_account_mutable(m_account_index);
        if (!account) {
            return false;
        }

        *account = m_new_account;
        account->set_modified_at(std::time(nullptr));

        if (m_ui_callback) {
            m_ui_callback();
        }

        return true;
    }

    [[nodiscard]] bool undo() override {
        if (!m_vault_manager || m_account_index < 0) {
            return false;
        }

        auto* account = m_vault_manager->get_account_mutable(m_account_index);
        if (!account) {
            return false;
        }

        *account = m_old_account;

        if (m_ui_callback) {
            m_ui_callback();
        }

        return true;
    }

    [[nodiscard]] std::string get_description() const override {
        return "Modify Account '" + m_new_account.account_name() + "'";
    }

private:
    VaultManager* m_vault_manager;
    int m_account_index;
    keeptower::AccountRecord m_old_account;
    keeptower::AccountRecord m_new_account;
    std::function<void()> m_ui_callback;
};

/**
 * @brief Command for toggling favorite status
 *
 * Lightweight command that only stores the account index and toggles
 * the favorite flag.
 */
class ToggleFavoriteCommand final : public Command {
public:
    /**
     * @brief Construct command to toggle favorite status
     * @param vault_manager Vault manager instance
     * @param account_index Index of account to toggle
     * @param ui_callback Optional callback to update UI after operation
     */
    ToggleFavoriteCommand(
        VaultManager* vault_manager,
        int account_index,
        std::function<void()> ui_callback = nullptr
    ) : m_vault_manager(vault_manager),
        m_account_index(account_index),
        m_ui_callback(std::move(ui_callback)) {}

    [[nodiscard]] bool execute() override {
        return toggle();
    }

    [[nodiscard]] bool undo() override {
        // Toggling is its own inverse
        return toggle();
    }

    [[nodiscard]] std::string get_description() const override {
        return "Toggle Favorite";
    }

private:
    bool toggle() {
        if (!m_vault_manager || m_account_index < 0) {
            return false;
        }

        auto* account = m_vault_manager->get_account_mutable(m_account_index);
        if (!account) {
            return false;
        }

        account->set_is_favorite(!account->is_favorite());
        account->set_modified_at(std::time(nullptr));

        if (m_ui_callback) {
            m_ui_callback();
        }

        return true;
    }

    VaultManager* m_vault_manager;
    int m_account_index;
    std::function<void()> m_ui_callback;
};

#endif // ACCOUNT_COMMANDS_H
