// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "UserManagementDialog.h"
#include "../../utils/SettingsValidator.h"
#include "../../utils/SecureMemory.h"
#include <gtkmm/messagedialog.h>
#include <gtkmm/grid.h>
#include <gtkmm/separator.h>
#include <giomm/settings.h>
#include <glibmm/markup.h>
#include <openssl/rand.h>
#include <algorithm>
#include <array>
#include <format>
#include <functional>
#include <memory>

UserManagementDialog::UserManagementDialog(
    Gtk::Window& parent,
    VaultManager& vault_manager,
    std::string_view current_username
) : Gtk::Dialog("Manage Users", parent, true),  // Modal dialog
    m_content_box(Gtk::Orientation::VERTICAL, 12),
    m_button_box(Gtk::Orientation::HORIZONTAL, 6),
    m_vault_manager(vault_manager),
    m_current_username(current_username)
{
    // Dialog setup
    set_default_size(600, 400);
    set_modal(true);

    // Content box setup
    m_content_box.set_margin_start(12);
    m_content_box.set_margin_end(12);
    m_content_box.set_margin_top(12);
    m_content_box.set_margin_bottom(12);

    // Header label
    auto* header_label = Gtk::make_managed<Gtk::Label>();
    header_label->set_markup("<b>Vault Users</b>");
    header_label->set_halign(Gtk::Align::START);
    m_content_box.append(*header_label);

    // Scrolled window for user list
    m_scrolled_window.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    m_scrolled_window.set_vexpand(true);
    m_scrolled_window.set_has_frame(true);
    m_scrolled_window.set_child(m_user_list);
    m_content_box.append(m_scrolled_window);

    // User list setup
    m_user_list.set_selection_mode(Gtk::SelectionMode::NONE);
    m_user_list.add_css_class("boxed-list");

    // Button box
    m_button_box.set_halign(Gtk::Align::END);
    m_add_user_button.set_label("Add User");
    m_add_user_button.add_css_class("suggested-action");
    m_add_user_button.signal_clicked().connect(
        sigc::mem_fun(*this, &UserManagementDialog::on_add_user)
    );
    m_button_box.append(m_add_user_button);

    m_close_button.set_label("Close");
    m_close_button.signal_clicked().connect([this]() {
        response(Gtk::ResponseType::CLOSE);
    });
    m_button_box.append(m_close_button);

    m_content_box.append(m_button_box);

    // Set content box as dialog child
    set_child(m_content_box);

    // Initial population
    refresh_user_list();
}

void UserManagementDialog::refresh_user_list() {
    // Clear existing rows
    while (auto* child = m_user_list.get_first_child()) {
        m_user_list.remove(*child);
    }

    // Get all users from vault
    auto users = m_vault_manager.list_users();

    if (users.empty()) {
        auto* error_label = Gtk::make_managed<Gtk::Label>("No users found");
        error_label->add_css_class("dim-label");
        m_user_list.append(*error_label);
        return;
    }

    // Add row for each user
    for (const auto& user : users) {
        auto* row = create_user_row(user);
        m_user_list.append(*row);
    }
}

Gtk::Widget* UserManagementDialog::create_user_row(const KeepTower::KeySlot& user) {
    auto* row_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);
    row_box->set_margin_start(12);
    row_box->set_margin_end(12);
    row_box->set_margin_top(8);
    row_box->set_margin_bottom(8);

    // User info (username and role)
    auto* info_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 2);

    auto* username_label = Gtk::make_managed<Gtk::Label>(user.username);
    username_label->set_halign(Gtk::Align::START);
    username_label->add_css_class("title-4");
    info_box->append(*username_label);

    auto* role_label = Gtk::make_managed<Gtk::Label>(
        get_role_display_name(user.role)
    );
    role_label->set_halign(Gtk::Align::START);
    role_label->add_css_class("caption");
    role_label->add_css_class("dim-label");
    info_box->append(*role_label);

    // Add password change indicator if needed
    if (user.must_change_password) {
        auto* status_label = Gtk::make_managed<Gtk::Label>("⚠ Must change password");
        status_label->set_halign(Gtk::Align::START);
        status_label->add_css_class("caption");
        status_label->add_css_class("warning");
        info_box->append(*status_label);
    }

    // Current user indicator
    if (user.username == m_current_username) {
        auto* current_label = Gtk::make_managed<Gtk::Label>("(You)");
        current_label->set_halign(Gtk::Align::START);
        current_label->add_css_class("caption");
        current_label->add_css_class("accent");
        info_box->append(*current_label);
    }

    row_box->append(*info_box);

    // Spacer
    auto* spacer = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    spacer->set_hexpand(true);
    row_box->append(*spacer);

    // Action buttons
    auto* button_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);

    // Remove User button (with safety checks)
    auto* remove_button = Gtk::make_managed<Gtk::Button>("Remove");
    remove_button->add_css_class("destructive-action");

    // Disable remove button if user cannot be safely removed
    if (!can_remove_user(user.username, user.role)) {
        remove_button->set_sensitive(false);
        if (user.username == m_current_username) {
            remove_button->set_tooltip_text("Cannot remove yourself");
        } else {
            remove_button->set_tooltip_text("Cannot remove last administrator");
        }
    } else {
        remove_button->signal_clicked().connect([this, username = user.username]() {
            on_remove_user(username);
        });
    }

    button_box->append(*remove_button);

    // Reset Password button (admin-only, cannot reset own password via this method)
    auto* reset_button = Gtk::make_managed<Gtk::Button>("Reset Password");

    // Disable reset button if user is trying to reset own password
    if (user.username == m_current_username) {
        reset_button->set_sensitive(false);
        reset_button->set_tooltip_text("Use 'Change My Password' to change your own password");
    } else {
        reset_button->signal_clicked().connect([this, username = user.username]() {
            on_reset_password(username);
        });
    }

    button_box->append(*reset_button);

    row_box->append(*button_box);

    return row_box;
}

void UserManagementDialog::on_add_user() {
    // Create input dialog
    auto* dialog = new Gtk::Dialog("Add User", *this, true);
    dialog->set_default_size(400, 200);

    // Buttons must be added BEFORE setting content
    dialog->add_button("_Cancel", Gtk::ResponseType::CANCEL);
    dialog->add_button("_Add", Gtk::ResponseType::OK);

    auto* content = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 12);
    content->set_margin_start(12);
    content->set_margin_end(12);
    content->set_margin_top(12);
    content->set_margin_bottom(12);

    // Username input
    auto* username_label = Gtk::make_managed<Gtk::Label>("Username:");
    username_label->set_halign(Gtk::Align::START);
    content->append(*username_label);

    auto* username_entry = Gtk::make_managed<Gtk::Entry>();
    username_entry->set_placeholder_text("Enter username");
    username_entry->set_max_length(64);
    content->append(*username_entry);

    // Role selection
    auto* role_label = Gtk::make_managed<Gtk::Label>("Role:");
    role_label->set_halign(Gtk::Align::START);
    content->append(*role_label);

    auto role_model = Gtk::StringList::create({"Standard User", "Administrator"});
    auto* role_dropdown = Gtk::make_managed<Gtk::DropDown>(role_model);
    role_dropdown->set_selected(0);  // Default to Standard User
    content->append(*role_dropdown);

    // Append content to dialog's content area
    dialog->get_content_area()->append(*content);

    dialog->signal_response().connect([this, dialog, username_entry, role_dropdown](int response_id) {
        if (response_id == Gtk::ResponseType::OK) {
            std::string username = username_entry->get_text();

            // Validate username
            if (username.empty() || username.length() < 3) {
                auto* error_dlg = new Gtk::MessageDialog(
                    *this,
                    "Username must be at least 3 characters",
                    false,
                    Gtk::MessageType::ERROR
                );
                error_dlg->set_modal(true);
                error_dlg->signal_response().connect([error_dlg](int) {
                    error_dlg->hide();
                    delete error_dlg;
                });
                error_dlg->show();
                return;
            }

            // Determine role
            KeepTower::UserRole role = (role_dropdown->get_selected() == 1)
                ? KeepTower::UserRole::ADMINISTRATOR
                : KeepTower::UserRole::STANDARD_USER;

            // Generate temporary password (use Glib::ustring to preserve encoding)
            Glib::ustring temp_password = generate_temporary_password();

            // Add user to vault
            auto result = m_vault_manager.add_user(username, temp_password, role);

            if (!result) {
                auto* error_dlg = new Gtk::MessageDialog(
                    *this,
                    "Failed to add user: " + std::string(KeepTower::to_string(result.error())),
                    false,
                    Gtk::MessageType::ERROR
                );
                error_dlg->set_modal(true);
                error_dlg->signal_response().connect([error_dlg](int) {
                    error_dlg->hide();
                    delete error_dlg;
                });
                error_dlg->show();

                // Securely clear temporary password (Glib::ustring)
                KeepTower::secure_clear_ustring(temp_password);
                return;
            }

            // Success - show temporary password to admin
            // Chain the dialogs: show switch dialog AFTER password dialog closes
            show_temporary_password(username, temp_password, [this, username]() {
                // Offer to logout and login as new user
                auto* switch_dialog = new Gtk::MessageDialog(
                    *this,
                    "User Created Successfully",
                    false,
                    Gtk::MessageType::QUESTION,
                    Gtk::ButtonsType::YES_NO
                );
                switch_dialog->set_secondary_text(
                    "Do you want to close this vault and login as the new user '" + username + "'?\n\n"
                    "Note: You will need to enter the temporary password to login."
                );
                switch_dialog->set_modal(true);

                switch_dialog->signal_response().connect([this, switch_dialog, username](int switch_response) {
                    if (switch_response == Gtk::ResponseType::YES) {
                        // Signal parent window to logout and reopen vault
                        m_signal_request_relogin.emit(username);
                        // Close user management dialog
                        response(Gtk::ResponseType::CLOSE);
                    }
                    switch_dialog->hide();
                    delete switch_dialog;
                });

                switch_dialog->show();
            });

            // Securely clear temporary password
            KeepTower::secure_clear_ustring(temp_password);

            // Refresh user list
            refresh_user_list();
        }

        dialog->hide();
        delete dialog;
    });

    dialog->show();
}

void UserManagementDialog::on_remove_user(std::string_view username) {
    // Confirmation dialog
    auto* confirm_dlg = new Gtk::MessageDialog(
        *this,
        "Are you sure you want to remove user \"" + std::string(username) + "\"?",
        false,
        Gtk::MessageType::WARNING,
        Gtk::ButtonsType::YES_NO
    );
    confirm_dlg->set_secondary_text("This action cannot be undone.");
    confirm_dlg->set_modal(true);

    confirm_dlg->signal_response().connect([this, confirm_dlg, username = std::string(username)](int response) {
        if (response == Gtk::ResponseType::YES) {
            // Remove user
            auto result = m_vault_manager.remove_user(username);

            if (!result) {
                auto* error_dlg = new Gtk::MessageDialog(
                    *this,
                    "Failed to remove user: " + std::string(KeepTower::to_string(result.error())),
                    false,
                    Gtk::MessageType::ERROR
                );
                error_dlg->set_modal(true);
                error_dlg->signal_response().connect([error_dlg](int) {
                    error_dlg->hide();
                    delete error_dlg;
                });
                error_dlg->show();
                confirm_dlg->hide();
                delete confirm_dlg;
                return;
            }

            // Success
            auto* success_dlg = new Gtk::MessageDialog(
                *this,
                "User removed successfully",
                false,
                Gtk::MessageType::INFO
            );
            success_dlg->set_modal(true);
            success_dlg->signal_response().connect([success_dlg](int) {
                success_dlg->hide();
                delete success_dlg;
            });
            success_dlg->show();

            // Refresh user list
            refresh_user_list();
        }

        confirm_dlg->hide();
        delete confirm_dlg;
    });

    confirm_dlg->show();
}

void UserManagementDialog::on_reset_password(std::string_view username) {
    // Confirmation dialog
    auto* confirm_dlg = new Gtk::MessageDialog(
        *this,
        "Reset password for user \"" + std::string(username) + "\"?",
        false,
        Gtk::MessageType::QUESTION,
        Gtk::ButtonsType::YES_NO
    );
    confirm_dlg->set_secondary_text("A temporary password will be generated. The user must change it on next login.");
    confirm_dlg->set_modal(true);

    confirm_dlg->signal_response().connect([this, confirm_dlg, username = std::string(username)](int response) {
        if (response == Gtk::ResponseType::YES) {
            // Generate temporary password (use Glib::ustring to preserve encoding)
            Glib::ustring temp_password = generate_temporary_password();

            // Reset user password
            auto result = m_vault_manager.admin_reset_user_password(username, temp_password);

            if (!result) {
                auto* error_dlg = new Gtk::MessageDialog(
                    *this,
                    "Failed to reset password: " + std::string(KeepTower::to_string(result.error())),
                    false,
                    Gtk::MessageType::ERROR
                );
                error_dlg->set_modal(true);
                error_dlg->signal_response().connect([error_dlg](int) {
                    error_dlg->hide();
                    delete error_dlg;
                });
                error_dlg->show();

                // Securely clear temporary password (Glib::ustring)
                KeepTower::secure_clear_ustring(temp_password);

                confirm_dlg->hide();
                delete confirm_dlg;
                return;
            }

            // Success - show temporary password to admin
            show_temporary_password(username, temp_password);

            // Securely clear temporary password (Glib::ustring)
            KeepTower::secure_clear_ustring(temp_password);

            // Refresh user list
            refresh_user_list();
        }

        confirm_dlg->hide();
        delete confirm_dlg;
    });

    confirm_dlg->show();
}

void UserManagementDialog::show_temporary_password(std::string_view username, const Glib::ustring& temp_password, std::function<void()> on_closed) {
    auto* dialog = new Gtk::Dialog("Temporary Password Generated", *this, true);
    dialog->set_default_size(500, 250);

    // Add buttons
    auto* copy_button = dialog->add_button("_Copy to Clipboard", Gtk::ResponseType::APPLY);
    copy_button->add_css_class("suggested-action");
    dialog->add_button("_Close", Gtk::ResponseType::OK);

    // Content box
    auto* content = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 12);
    content->set_margin_start(12);
    content->set_margin_end(12);
    content->set_margin_top(12);
    content->set_margin_bottom(12);

    // Message label
    auto* message_label = Gtk::make_managed<Gtk::Label>();
    message_label->set_markup(
        "Temporary password for user \"<b>" + Glib::Markup::escape_text(std::string(username)) + "</b>\":"
    );
    message_label->set_halign(Gtk::Align::START);
    content->append(*message_label);

    // Password display (selectable label) - convert to std::string for display
    auto* password_label = Gtk::make_managed<Gtk::Label>();
    password_label->set_markup(
        "<span font_family='monospace' size='x-large'><b>" +
        Glib::Markup::escape_text(temp_password.raw()) +
        "</b></span>"
    );
    password_label->set_selectable(true);
    password_label->set_halign(Gtk::Align::CENTER);
    password_label->set_margin_top(12);
    password_label->set_margin_bottom(12);
    content->append(*password_label);

    // Warning label
    auto* warning_label = Gtk::make_managed<Gtk::Label>();
    warning_label->set_markup(
        std::string("⚠ <b>Important:</b> Save this password now. You will not be able to view it again.\n") +
        std::string("The user will be required to change this password on their next login.")
    );
    warning_label->set_wrap(true);
    warning_label->set_halign(Gtk::Align::START);
    content->append(*warning_label);

    // Add content to dialog
    dialog->get_content_area()->append(*content);

    // Track clipboard timeout connection (shared_ptr to keep it alive in lambda)
    auto clipboard_timeout = std::make_shared<sigc::connection>();

    // Capture temp_password as Glib::ustring to preserve encoding
    dialog->signal_response().connect([dialog, temp_password,
                                       clipboard_timeout, warning_label, on_closed](int response) {
        if (response == Gtk::ResponseType::APPLY) {
            // Copy to clipboard (raw() converts to std::string for clipboard)
            auto clipboard = dialog->get_clipboard();
            clipboard->set_text(temp_password.raw());

            // Get validated clipboard timeout from settings
            auto settings = Gio::Settings::create("com.tjdeveng.keeptower");
            const int timeout_seconds = SettingsValidator::get_clipboard_timeout(settings);

            // Update warning to show clipboard status
            warning_label->set_markup(
                std::format(
                    "✓ <b>Password copied to clipboard</b> (will clear in {} seconds)\n\n"
                    "⚠ <b>Important:</b> The user will be required to change this password on their next login.",
                    timeout_seconds
                )
            );

            // Cancel previous timeout if exists
            if (clipboard_timeout->connected()) {
                clipboard_timeout->disconnect();
            }

            // Schedule clipboard clear
            *clipboard_timeout = Glib::signal_timeout().connect(
                [clipboard, warning_label]() {
                    clipboard->set_text("");
                    warning_label->set_markup(
                        "🔒 <b>Clipboard cleared for security</b>\n\n"
                        "⚠ <b>Important:</b> Make sure you saved the password before closing this dialog."
                    );
                    return false;  // Don't repeat
                },
                timeout_seconds * 1000  // Convert to milliseconds
            );

            // Don't close dialog - return to allow viewing password again
            return;
        }

        // Close button or dialog closed
        if (clipboard_timeout->connected()) {
            clipboard_timeout->disconnect();
        }
        dialog->hide();
        delete dialog;

        // Invoke callback after dialog is closed
        if (on_closed) {
            on_closed();
        }
    });

    dialog->show();
}

Glib::ustring UserManagementDialog::generate_temporary_password() {
    // Get vault security policy for password requirements
    auto policy_opt = m_vault_manager.get_vault_security_policy();
    const uint32_t min_required = policy_opt ? policy_opt->min_password_length : 12;
    const uint32_t password_length = std::max(16u, min_required);  // Use max(16, policy min)

    // Character sets for password generation
    constexpr std::string_view uppercase = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    constexpr std::string_view lowercase = "abcdefghijklmnopqrstuvwxyz";
    constexpr std::string_view digits = "0123456789";
    constexpr std::string_view symbols = "!@#$%^&*-_=+";
    constexpr std::string_view all_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*-_=+";

    std::string password;
    password.reserve(password_length);

    // Ensure at least one character from each set
    std::array<std::string_view, 4> required_sets = {uppercase, lowercase, digits, symbols};
    std::array<unsigned char, 4> random_bytes;

    for (auto charset : required_sets) {
        if (RAND_bytes(random_bytes.data(), 1) != 1) {
            throw std::runtime_error("Failed to generate random bytes");
        }
        size_t index = random_bytes[0] % charset.size();
        password += charset[index];
    }

    // Fill remaining characters randomly
    for (uint32_t i = password.size(); i < password_length; ++i) {
        if (RAND_bytes(random_bytes.data(), 1) != 1) {
            throw std::runtime_error("Failed to generate random bytes");
        }
        size_t index = random_bytes[0] % all_chars.size();
        password += all_chars[index];
    }

    // Shuffle password to mix required characters
    for (size_t i = password.size() - 1; i > 0; --i) {
        if (RAND_bytes(random_bytes.data(), 1) != 1) {
            throw std::runtime_error("Failed to generate random bytes");
        }
        size_t j = random_bytes[0] % (i + 1);
        std::swap(password[i], password[j]);
    }

    // Return as Glib::ustring to preserve encoding
    return Glib::ustring(password);
}

std::string UserManagementDialog::get_role_display_name(KeepTower::UserRole role) noexcept {
    switch (role) {
        case KeepTower::UserRole::ADMINISTRATOR:
            return "Administrator";
        case KeepTower::UserRole::STANDARD_USER:
            return "Standard User";
        default:
            return "Unknown";
    }
}

bool UserManagementDialog::can_remove_user(std::string_view username, KeepTower::UserRole user_role) const noexcept {
    // Cannot remove self
    if (username == m_current_username) {
        return false;
    }

    // If removing an admin, ensure at least one other admin exists
    if (user_role == KeepTower::UserRole::ADMINISTRATOR) {
        auto users = m_vault_manager.list_users();

        // Count total administrators
        int admin_count = 0;
        for (const auto& user : users) {
            if (user.role == KeepTower::UserRole::ADMINISTRATOR) {
                ++admin_count;
            }
        }

        // Must have at least 2 admins to remove one (keep at least 1)
        if (admin_count < 2) {
            return false;
        }
    }

    return true;
}
