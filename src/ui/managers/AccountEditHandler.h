/**
 * @file AccountEditHandler.h
 * @brief Handler for account editing operations (Phase 5j)
 *
 * Extracts account add, delete, and password generation functionality from MainWindow.
 */

#ifndef UI_MANAGERS_ACCOUNT_EDIT_HANDLER_H
#define UI_MANAGERS_ACCOUNT_EDIT_HANDLER_H

#include <memory>
#include <string>
#include <functional>
#include <glibmm/ustring.h>

#include "../widgets/AccountDetailWidget.h"

// Forward declarations
class VaultManager;
class UndoManager;
namespace UI {
    class DialogManager;
}
namespace Gtk {
    class Window;
    class SearchEntry;
    class Label;
}
namespace keeptower {
    class AccountRecord;
}

namespace UI {

// Forward declaration for DialogManager
class DialogManager;

/**
 * @class AccountEditHandler
 * @brief Handles account editing operations
 *
 * Phase 5j extraction: Manages account-related operations including:
 * - Adding new accounts with undo/redo support
 * - Deleting accounts with confirmation and permissions check
 * - Generating secure passwords with customizable options
 */
class AccountEditHandler {
public:
    /**
     * @brief Callback for UI updates after account operations
     */
    using UpdateCallback = std::function<void()>;

    /**
     * @brief Callback for status label updates
     */
    using StatusCallback = std::function<void(const std::string&)>;

    /**
     * @brief Callback for getting current account index
     */
    using GetAccountIndexCallback = std::function<int()>;

    /**
     * @brief Callback to check if undo/redo is enabled
     */
    using IsUndoRedoEnabledCallback = std::function<bool()>;

    /**
     * @brief Construct account edit handler
     * @param window Parent window for dialogs
     * @param vault_manager Vault manager for account operations
     * @param undo_manager Undo manager for command pattern
     * @param dialog_manager Dialog manager for confirmations/errors
     * @param detail_widget Account detail widget for display/editing
     * @param search_entry Search entry for filtering
     * @param status_callback Callback to update status label
     * @param update_callback Callback to refresh account list
     * @param get_account_index_callback Callback to get selected account index
     * @param is_undo_redo_enabled_callback Callback to check undo/redo status
     */
    AccountEditHandler(Gtk::Window& window,
                      VaultManager* vault_manager,
                      UndoManager* undo_manager,
                      DialogManager* dialog_manager,
                      AccountDetailWidget* detail_widget,
                      Gtk::SearchEntry* search_entry,
                      StatusCallback status_callback,
                      UpdateCallback update_callback,
                      GetAccountIndexCallback get_account_index_callback,
                      IsUndoRedoEnabledCallback is_undo_redo_enabled_callback);

    /**
     * @brief Add a new account
     *
     * Creates a new account with default values and focuses the name field.
     * Uses undo/redo system if enabled.
     * Clears search filter to show new account.
     */
    void handle_add();

    /**
     * @brief Delete an account
     * @param context_menu_account_id Account ID from context menu (empty if from button)
     *
     * Shows confirmation dialog and checks permissions (V2 multi-user).
     * Uses undo/redo system if enabled.
     * Adjusts confirmation message based on undo availability.
     */
    void handle_delete(const std::string& context_menu_account_id);

    /**
     * @brief Generate a secure password
     *
     * Shows password generation dialog with options:
     * - Length (8-64 characters)
     * - Uppercase/Lowercase/Digits/Symbols
     * - Exclude ambiguous characters
     *
     * Updates password field in detail widget.
     */
    void handle_generate_password();

private:
    /**
     * @brief Find account index by ID
     */
    int find_account_index_by_id(const std::string& account_id) const;

    Gtk::Window& m_window;
    VaultManager* m_vault_manager;
    UndoManager* m_undo_manager;
    DialogManager* m_dialog_manager;
    AccountDetailWidget* m_detail_widget;
    Gtk::SearchEntry* m_search_entry;
    StatusCallback m_status_callback;
    UpdateCallback m_update_callback;
    GetAccountIndexCallback m_get_account_index_callback;
    IsUndoRedoEnabledCallback m_is_undo_redo_enabled_callback;
};

} // namespace UI

#endif // UI_MANAGERS_ACCOUNT_EDIT_HANDLER_H
