/**
 * @file GroupHandler.cc
 * @brief Implementation of group management handler (Phase 5i)
 */

#include "GroupHandler.h"
#include "DialogManager.h"
#include "../../core/VaultManager.h"
#include "../../core/services/GroupService.h"
#include "../dialogs/GroupCreateDialog.h"
#include "../dialogs/GroupRenameDialog.h"
#include "../../utils/StringHelpers.h"

#include <gtkmm.h>

using KeepTower::safe_ustring_to_string;

namespace UI {

GroupHandler::GroupHandler(Gtk::Window& window,
                           VaultManager* vault_manager,
                           KeepTower::IGroupService* group_service,
                           UI::DialogManager* dialog_manager,
                           StatusCallback status_callback,
                           UpdateCallback update_callback)
    : m_window(window)
    , m_vault_manager(vault_manager)
    , m_group_service(group_service)
    , m_dialog_manager(dialog_manager)
    , m_status_callback(std::move(status_callback))
    , m_update_callback(std::move(update_callback))
{
}

void GroupHandler::handle_create() {
    auto* dialog = Gtk::make_managed<GroupCreateDialog>(m_window);
    dialog->set_modal(true);
    dialog->set_hide_on_close(true);

    dialog->signal_response().connect([this, dialog](int result) {
        dialog->hide();

        if (result != static_cast<int>(Gtk::ResponseType::OK)) {
            return;
        }

        auto group_name = dialog->get_group_name();
        if (group_name.empty()) {
            return;
        }

        // Phase 3: Use GroupService for validation and creation
        if (m_group_service) {
            auto result = m_group_service->create_group(safe_ustring_to_string(group_name, "group_name"));
            if (!result) {
                // Convert service error to user-friendly message
                std::string error_msg;
                switch (result.error()) {
                    case KeepTower::ServiceError::VALIDATION_FAILED:
                        error_msg = "Group name cannot be empty.";
                        break;
                    case KeepTower::ServiceError::FIELD_TOO_LONG:
                        error_msg = "Group name is too long. Maximum length is 100 characters.";
                        break;
                    case KeepTower::ServiceError::DUPLICATE_NAME:
                        error_msg = "A group with this name already exists.";
                        break;
                    default:
                        error_msg = "Failed to create group: " + std::string(KeepTower::to_string(result.error()));
                        break;
                }
                m_dialog_manager->show_error_dialog(error_msg);
                return;
            }
            m_status_callback("Group created: " + group_name);
            m_update_callback();
        } else {
            // Fallback to direct VaultManager call if service not available
            std::string group_id = m_vault_manager->create_group(safe_ustring_to_string(group_name, "group_name"));
            if (group_id.empty()) {
                m_dialog_manager->show_error_dialog("Failed to create group. The name may already exist or be invalid.");
                return;
            }
            m_status_callback("Group created: " + group_name);
            m_update_callback();
        }
    });

    dialog->present();
}

void GroupHandler::handle_rename(const std::string& group_id, const Glib::ustring& current_name) {
    if (group_id.empty()) {
        return;
    }

    auto* dialog = Gtk::make_managed<GroupRenameDialog>(m_window, current_name);
    dialog->set_modal(true);
    dialog->set_hide_on_close(true);

    dialog->signal_response().connect([this, dialog, group_id](int result) {
        dialog->hide();

        if (result != static_cast<int>(Gtk::ResponseType::OK)) {
            return;
        }

        auto new_name = dialog->get_group_name();

        // Phase 3: Use GroupService for validation and renaming
        if (m_group_service) {
            auto result = m_group_service->rename_group(group_id, safe_ustring_to_string(new_name, "group_name"));
            if (!result) {
                // Convert service error to user-friendly message
                std::string error_msg;
                switch (result.error()) {
                    case KeepTower::ServiceError::VALIDATION_FAILED:
                        error_msg = "Group name cannot be empty.";
                        break;
                    case KeepTower::ServiceError::FIELD_TOO_LONG:
                        error_msg = "Group name is too long. Maximum length is 100 characters.";
                        break;
                    case KeepTower::ServiceError::DUPLICATE_NAME:
                        error_msg = "A group with this name already exists.";
                        break;
                    case KeepTower::ServiceError::ACCOUNT_NOT_FOUND:
                        error_msg = "Group not found.";
                        break;
                    default:
                        error_msg = "Failed to rename group: " + std::string(KeepTower::to_string(result.error()));
                        break;
                }
                m_dialog_manager->show_error_dialog(error_msg);
                return;
            }
            m_status_callback("Group renamed");
            m_update_callback();
        } else {
            // Fallback to direct VaultManager call if service not available
            if (m_vault_manager->rename_group(group_id, safe_ustring_to_string(new_name, "group_name"))) {
                m_status_callback("Group renamed");
                m_update_callback();
            } else {
                m_dialog_manager->show_error_dialog("Failed to rename group");
            }
        }
    });

    dialog->present();
}

void GroupHandler::handle_delete(const std::string& group_id) {
    if (group_id.empty()) {
        return;
    }

    // Confirm deletion
    m_dialog_manager->show_confirmation_dialog(
        "Accounts in this group will not be deleted.",
        "Delete this group?",
        [this, group_id](bool confirmed) {
            if (confirmed) {
                if (m_vault_manager->delete_group(group_id)) {
                    m_status_callback("Group deleted");
                    m_update_callback();
                } else {
                    m_dialog_manager->show_error_dialog("Failed to delete group");
                }
            }
        }
    );
}

} // namespace UI
