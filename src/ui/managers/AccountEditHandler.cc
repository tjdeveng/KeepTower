/**
 * @file AccountEditHandler.cc
 * @brief Implementation of account edit handler (Phase 5j)
 */

#include "../widgets/AccountDetailWidget.h"
#include "AccountEditHandler.h"
#include "DialogManager.h"
#include "../../core/VaultManager.h"
#include "../../core/commands/UndoManager.h"
#include "../../core/commands/AccountCommands.h"
#include "../../utils/Log.h"
#include "record.pb.h"

#include <gtkmm.h>
#include <random>
#include <format>
#include <ctime>

namespace UI {

AccountEditHandler::AccountEditHandler(Gtk::Window& window,
                                      VaultManager* vault_manager,
                                      UndoManager* undo_manager,
                                       DialogManager* dialog_manager,
                                       AccountDetailWidget* detail_widget,
                                      Gtk::SearchEntry* search_entry,
                                      StatusCallback status_callback,
                                      UpdateCallback update_callback,
                                      GetAccountIndexCallback get_account_index_callback,
                                      IsUndoRedoEnabledCallback is_undo_redo_enabled_callback,
                                      SelectAccountCallback select_account_callback)
    : m_window(window)
    , m_vault_manager(vault_manager)
    , m_undo_manager(undo_manager)
    , m_dialog_manager(dialog_manager)
    , m_detail_widget(detail_widget)
    , m_search_entry(search_entry)
    , m_status_callback(std::move(status_callback))
    , m_update_callback(std::move(update_callback))
    , m_get_account_index_callback(std::move(get_account_index_callback))
    , m_is_undo_redo_enabled_callback(std::move(is_undo_redo_enabled_callback))
    , m_select_account_callback(std::move(select_account_callback))
{
}

void AccountEditHandler::handle_add() {
    // Create new account record with current timestamp
    keeptower::AccountRecord new_account;
    new_account.set_id(std::to_string(std::time(nullptr)));  // Use timestamp as unique ID string
    new_account.set_created_at(std::time(nullptr));
    new_account.set_modified_at(std::time(nullptr));
    new_account.set_account_name("New Account");
    new_account.set_user_name("");
    new_account.set_password("");
    new_account.set_email("");
    new_account.set_website("");
    new_account.set_notes("");

    // Store the account ID for later selection
    const std::string new_account_id = new_account.id();

    // Create command with UI callback
    auto ui_callback = [this, new_account_id]() {
        // Clear search filter so new account is visible
        m_search_entry->set_text("");

        // Update the display (synchronous GTK operation - completes before returning)
        m_update_callback();

        // Select the newly created account (now safe - tree widget is fully rebuilt)
        if (m_select_account_callback) {
            m_select_account_callback(new_account_id);
        }

        // Focus the name field for immediate editing
        // Use idle callback here to allow the selection signal to propagate first
        Glib::signal_idle().connect_once([this]() {
            if (m_detail_widget) {
                m_detail_widget->focus_account_name_entry();
            }
        });

        m_status_callback("Account added");
    };

    auto command = std::make_unique<AddAccountCommand>(
        m_vault_manager,
        std::move(new_account),
        ui_callback
    );

    // Check if undo/redo is enabled
    if (m_is_undo_redo_enabled_callback()) {
        if (!m_undo_manager->execute_command(std::move(command))) {
            m_status_callback("Failed to add account");
        }
    } else {
        // Execute directly without undo history
        if (!command->execute()) {
            m_status_callback("Failed to add account");
        }
    }
}

void AccountEditHandler::handle_delete(const std::string& context_menu_account_id) {
    // Determine which account to delete: context menu or selected account
    int account_index = -1;

    if (!context_menu_account_id.empty()) {
        // Context menu delete
        account_index = find_account_index_by_id(context_menu_account_id);
    } else {
        // Button/keyboard delete
        account_index = m_get_account_index_callback();
    }

    if (account_index < 0) {
        return;
    }

    // Check delete permissions (V2 multi-user vaults)
    if (!m_vault_manager->can_delete_account(account_index)) {
        m_dialog_manager->show_error_dialog(
            "You do not have permission to delete this account.\n\n"
            "Only administrators can delete admin-protected accounts."
        );
        return;
    }

    // Get account name for confirmation dialog
    const auto* account = m_vault_manager->get_account(account_index);
    if (!account) {
        return;
    }

    const Glib::ustring account_name = account->account_name();

    // Adjust message based on whether undo/redo is enabled
    Glib::ustring message = "Are you sure you want to delete '" + account_name + "'?";
    if (!m_is_undo_redo_enabled_callback()) {
        message += "\nThis action cannot be undone.";
    }

    m_dialog_manager->show_confirmation_dialog(
        message.raw(),
        "Delete Account?",
        [this, account_index](bool confirmed) {
            if (confirmed) {
                // Create command with UI callback
                auto ui_callback = [this]() {
                    m_update_callback();
                };

                auto command = std::make_unique<DeleteAccountCommand>(
                    m_vault_manager,
                    account_index,
                    ui_callback
                );

                // Check if undo/redo is enabled
                if (m_is_undo_redo_enabled_callback()) {
                    if (!m_undo_manager->execute_command(std::move(command))) {
                        m_dialog_manager->show_error_dialog("Failed to delete account");
                    }
                } else {
                    // Execute directly without undo history
                    if (!command->execute()) {
                        m_dialog_manager->show_error_dialog("Failed to delete account");
                    }
                }
            }
        }
    );
}

void AccountEditHandler::handle_generate_password() {
    // Create password generator options dialog
    auto* dialog = Gtk::make_managed<Gtk::Dialog>("Generate Password", m_window, true);
    dialog->add_button("_Cancel", Gtk::ResponseType::CANCEL);
    dialog->add_button("_Generate", Gtk::ResponseType::OK);
    dialog->set_default_response(Gtk::ResponseType::OK);

    auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 12);
    box->set_margin(24);

    // Password length selector
    auto* length_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);
    auto* length_label = Gtk::make_managed<Gtk::Label>("Password Length:");
    length_label->set_xalign(0.0);
    auto* length_spin = Gtk::make_managed<Gtk::SpinButton>();
    length_spin->set_range(8, 64);
    length_spin->set_increments(1, 5);
    length_spin->set_value(20);
    length_spin->set_hexpand(true);
    length_box->append(*length_label);
    length_box->append(*length_spin);

    // Character type options
    auto* uppercase_check = Gtk::make_managed<Gtk::CheckButton>("Include Uppercase (A-Z)");
    uppercase_check->set_active(true);
    auto* lowercase_check = Gtk::make_managed<Gtk::CheckButton>("Include Lowercase (a-z)");
    lowercase_check->set_active(true);
    auto* digits_check = Gtk::make_managed<Gtk::CheckButton>("Include Digits (2-9)");
    digits_check->set_active(true);
    auto* symbols_check = Gtk::make_managed<Gtk::CheckButton>("Include Symbols (!@#$%...)");
    symbols_check->set_active(true);
    auto* ambiguous_check = Gtk::make_managed<Gtk::CheckButton>("Exclude ambiguous (0/O, 1/l/I)");
    ambiguous_check->set_active(true);

    box->append(*length_box);
    box->append(*uppercase_check);
    box->append(*lowercase_check);
    box->append(*digits_check);
    box->append(*symbols_check);
    box->append(*ambiguous_check);

    dialog->get_content_area()->append(*box);

    dialog->signal_response().connect([this, dialog, length_spin, uppercase_check, lowercase_check,
                                       digits_check, symbols_check, ambiguous_check](int response) {
        if (response == Gtk::ResponseType::OK) {
            const int length = static_cast<int>(length_spin->get_value());

            // Build charset based on options
            std::string charset;
            if (lowercase_check->get_active()) {
                charset += ambiguous_check->get_active() ? "abcdefghjkmnpqrstuvwxyz" : "abcdefghijklmnopqrstuvwxyz";
            }
            if (uppercase_check->get_active()) {
                charset += ambiguous_check->get_active() ? "ABCDEFGHJKMNPQRSTUVWXYZ" : "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
            }
            if (digits_check->get_active()) {
                charset += ambiguous_check->get_active() ? "23456789" : "0123456789";
            }
            if (symbols_check->get_active()) {
                charset += "!@#$%^&*()-_=+[]{}|;:,.<>?";
            }

            if (charset.empty()) {
                m_dialog_manager->show_error_dialog("Please select at least one character type.");
                dialog->hide();
                return;
            }

            // Generate password
            std::random_device rd;
            if (rd.entropy() == 0.0) {
                g_warning("std::random_device has zero entropy, password may be less secure");
            }

            std::mt19937 gen(rd());
            std::uniform_int_distribution<std::size_t> dis(0, charset.size() - 1);

            std::string password;
            password.reserve(length);

            for (int i = 0; i < length; ++i) {
                password += charset[dis(gen)];
            }

            m_detail_widget->set_password(password);
            m_status_callback(std::format("Generated {}-character password", length));
        }
        dialog->hide();
    });

    dialog->show();
}

int AccountEditHandler::find_account_index_by_id(const std::string& account_id) const {
    if (!m_vault_manager) return -1;
    const auto& accounts = m_vault_manager->get_all_accounts();
    for (size_t i = 0; i < accounts.size(); ++i) {
        if (accounts[i].id() == account_id) return static_cast<int>(i);
    }
    return -1;
}

} // namespace UI
