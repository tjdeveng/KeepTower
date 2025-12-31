/**
 * @file GroupHandler.h
 * @brief Handler for group management operations (Phase 5i)
 *
 * Extracts group creation, renaming, and deletion functionality from MainWindow.
 */

#ifndef UI_MANAGERS_GROUP_HANDLER_H
#define UI_MANAGERS_GROUP_HANDLER_H

#include <memory>
#include <string>
#include <functional>
#include <glibmm/ustring.h>

// Forward declarations
class VaultManager;
namespace KeepTower {
    class IGroupService;
}
namespace UI {
    class DialogManager;
}

namespace Gtk {
    class Window;
    class Label;
}

namespace UI {

/**
 * @class GroupHandler
 * @brief Handles account group management operations
 *
 * Phase 5i extraction: Manages group-related operations including:
 * - Creating new groups with validation
 * - Renaming existing groups
 * - Deleting groups with confirmation
 *
 * Uses GroupService for business logic validation when available,
 * falls back to VaultManager for direct operations.
 */
class GroupHandler {
public:
    /**
     * @brief Callback for UI updates after group operations
     */
    using UpdateCallback = std::function<void()>;

    /**
     * @brief Callback for status label updates
     */
    using StatusCallback = std::function<void(const std::string&)>;

    /**
     * @brief Construct group handler
     * @param window Parent window for dialogs
     * @param vault_manager Vault manager for group operations
     * @param group_service Group service for validation (may be null)
     * @param dialog_manager Dialog manager for error/confirmation dialogs
     * @param status_callback Callback to update status label
     * @param update_callback Callback to refresh account list
     */
    GroupHandler(Gtk::Window& window,
                 VaultManager* vault_manager,
                 KeepTower::IGroupService* group_service,
                 UI::DialogManager* dialog_manager,
                 StatusCallback status_callback,
                 UpdateCallback update_callback);

    /**
     * @brief Create a new group
     *
     * Shows GroupCreateDialog and creates group with validation:
     * - Empty name check
     * - Length limit (100 characters)
     * - Duplicate name detection
     */
    void handle_create();

    /**
     * @brief Rename an existing group
     * @param group_id Unique identifier of the group
     * @param current_name Current name (for dialog display)
     *
     * Shows GroupRenameDialog and renames with validation:
     * - Empty name check
     * - Length limit (100 characters)
     * - Duplicate name detection
     * - Group existence verification
     */
    void handle_rename(const std::string& group_id, const Glib::ustring& current_name);

    /**
     * @brief Delete a group
     * @param group_id Unique identifier of the group
     *
     * Shows confirmation dialog and deletes group.
     * Accounts in the group are not deleted.
     */
    void handle_delete(const std::string& group_id);

private:
    Gtk::Window& m_window;
    VaultManager* m_vault_manager;
    KeepTower::IGroupService* m_group_service;
    UI::DialogManager* m_dialog_manager;
    StatusCallback m_status_callback;
    UpdateCallback m_update_callback;
};

} // namespace UI

#endif // UI_MANAGERS_GROUP_HANDLER_H
