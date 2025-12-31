// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "config.h"

#ifdef HAVE_YUBIKEY_SUPPORT

#include "YubiKeyManagerDialog.h"
#include "../../core/VaultManager.h"
#include "../../utils/Log.h"
#include <format>

YubiKeyManagerDialog::YubiKeyManagerDialog(Gtk::Window& parent, VaultManager* vault_manager)
    : Gtk::Dialog("Manage YubiKeys", parent, true)
    , m_vault_manager(vault_manager)
    , m_content_box(Gtk::Orientation::VERTICAL, 12)
    , m_info_label()
    , m_scrolled_window()
    , m_key_list()
    , m_button_box(Gtk::Orientation::HORIZONTAL, 6)
    , m_add_button("Add Current YubiKey")
    , m_remove_button("Remove Selected")
    , m_close_button("Close")
{
    KeepTower::Log::info("YubiKeyManagerDialog: Constructor called");
    set_default_size(500, 400);
    set_modal(true);

    KeepTower::Log::info("YubiKeyManagerDialog: Calling setup_ui()");
    setup_ui();
    KeepTower::Log::info("YubiKeyManagerDialog: Calling refresh_key_list()");
    refresh_key_list();
    KeepTower::Log::info("YubiKeyManagerDialog: Constructor completed");
}

void YubiKeyManagerDialog::setup_ui() {
    // Info label
    m_info_label.set_markup(
        "<b>Manage Authorized YubiKeys</b>\n\n"
        "Add backup YubiKeys to access this vault. All keys must be programmed\n"
        "with the same HMAC-SHA1 secret using <tt>ykpersonalize -2</tt>."
    );
    m_info_label.set_wrap(true);
    m_info_label.set_margin(12);
    m_content_box.append(m_info_label);

    // Scrolled window with key list
    m_scrolled_window.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    m_scrolled_window.set_min_content_height(200);
    m_scrolled_window.set_child(m_key_list);
    m_scrolled_window.set_vexpand(true);
    m_content_box.append(m_scrolled_window);

    // Key list selection
    m_key_list.set_selection_mode(Gtk::SelectionMode::SINGLE);
    m_key_list.signal_row_selected().connect([this](Gtk::ListBoxRow* row) {
        m_remove_button.set_sensitive(row != nullptr);
    });

    // Button box
    m_button_box.set_halign(Gtk::Align::END);
    m_button_box.set_margin(12);
    m_button_box.append(m_add_button);
    m_button_box.append(m_remove_button);
    m_button_box.append(m_close_button);
    m_content_box.append(m_button_box);

    // Button signals
    m_add_button.signal_clicked().connect(sigc::mem_fun(*this, &YubiKeyManagerDialog::on_add_key));
    m_remove_button.signal_clicked().connect(sigc::mem_fun(*this, &YubiKeyManagerDialog::on_remove_key));
    m_close_button.signal_clicked().connect([this]() { hide(); });

    m_remove_button.set_sensitive(false);

    // Add content to dialog
    get_content_area()->append(m_content_box);
}

void YubiKeyManagerDialog::refresh_key_list() {
    // Clear existing rows
    while (auto child = m_key_list.get_first_child()) {
        m_key_list.remove(*child);
    }

    if (!m_vault_manager || !m_vault_manager->is_vault_open()) {
        return;
    }

    KeepTower::Log::info("YubiKeyManagerDialog: Calling get_yubikey_list()");
    auto keys = m_vault_manager->get_yubikey_list();
    KeepTower::Log::info("YubiKeyManagerDialog: Retrieved {} YubiKey entries", keys.size());

    if (keys.empty()) {
        auto* label = Gtk::make_managed<Gtk::Label>("No YubiKeys configured");
        label->set_margin(24);
        m_key_list.append(*label);
        return;
    }

    for (size_t i = 0; i < keys.size(); ++i) {
        try {
            const auto& key = keys[i];
            KeepTower::Log::info("YubiKeyManagerDialog: Processing key {}: name='{}', serial='{}'",
                i, key.name(), key.serial());

            auto* row = Gtk::make_managed<Gtk::ListBoxRow>();
            auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
            box->set_margin(12);

            // Name label with safety check
            auto* name_label = Gtk::make_managed<Gtk::Label>();
            std::string name_text = !key.name().empty() ? key.name() : "Unknown YubiKey";
            // GCC 13 compatibility: explicit string conversion required for Glib::Markup::escape_text
            // GCC 14+ will support direct usage: std::format("<b>{}</b>", Glib::Markup::escape_text(name_text))
            std::string escaped_name = Glib::Markup::escape_text(name_text);
            name_label->set_markup(std::format("<b>{}</b>", escaped_name));
            name_label->set_xalign(0.0);
            box->append(*name_label);

            // Info label with safety checks
            auto* info_label = Gtk::make_managed<Gtk::Label>();
            std::string serial_text = !key.serial().empty() ? key.serial() : "Unknown";
            std::string time_text = "Unknown";

            if (key.added_at() > 0) {
                std::time_t timestamp = key.added_at();
                std::tm* tm_ptr = std::localtime(&timestamp);
                if (tm_ptr) {
                    char time_buf[100];
                    if (std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M", tm_ptr) > 0) {
                        time_text = time_buf;
                    }
                }
            }

            // GCC 13 compatibility: explicit string conversion required for Glib::Markup::escape_text
            // GCC 14+ will support inline calls: std::format("...", Glib::Markup::escape_text(...))
            std::string escaped_serial = Glib::Markup::escape_text(serial_text);
            std::string escaped_time = Glib::Markup::escape_text(time_text);
            info_label->set_markup(std::format("<small>Serial: {} • Added: {}</small>",
                                              escaped_serial,
                                              escaped_time));
            info_label->set_xalign(0.0);
            box->append(*info_label);

            row->set_child(*box);
            row->set_data("serial", g_strdup(serial_text.c_str()), g_free);
            m_key_list.append(*row);

            KeepTower::Log::info("YubiKeyManagerDialog: Successfully added UI row for key {}", i);
        } catch (const std::exception& e) {
            KeepTower::Log::error("YubiKeyManagerDialog: Error processing YubiKey entry: {}", e.what());
            continue;
        }
    }

    KeepTower::Log::info("YubiKeyManagerDialog: refresh_key_list() completed");
}

void YubiKeyManagerDialog::on_add_key() {
    // Prompt for name
    auto* entry_dialog = Gtk::make_managed<Gtk::Dialog>("Add YubiKey", *this, true);
    entry_dialog->add_button("_Cancel", Gtk::ResponseType::CANCEL);
    entry_dialog->add_button("_Add", Gtk::ResponseType::OK);

    auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 12);
    box->set_margin(24);

    auto* label = Gtk::make_managed<Gtk::Label>("Enter a name for this YubiKey:");
    label->set_xalign(0.0);
    box->append(*label);

    auto* entry = Gtk::make_managed<Gtk::Entry>();
    entry->set_placeholder_text("e.g., Backup, Office Key");
    entry->set_activates_default(true);
    box->append(*entry);

    entry_dialog->get_content_area()->append(*box);
    entry_dialog->set_default_response(Gtk::ResponseType::OK);

    entry_dialog->signal_response().connect([this, entry_dialog, entry](int response) {
        if (response == Gtk::ResponseType::OK) {
            std::string name = entry->get_text();
            if (name.empty()) {
                name = "Backup";
            }

            if (m_vault_manager->add_backup_yubikey(name)) {
                refresh_key_list();

                // Show success message
                auto* success_dialog = Gtk::make_managed<Gtk::MessageDialog>(*this,
                    "YubiKey added successfully!",
                    false, Gtk::MessageType::INFO, Gtk::ButtonsType::OK, true);
                success_dialog->signal_response().connect([success_dialog](int) {
                    success_dialog->hide();
                });
                success_dialog->show();
            } else {
                // Show error message
                auto* error_dialog = Gtk::make_managed<Gtk::MessageDialog>(*this,
                    "Failed to add YubiKey. Make sure the key is connected and programmed with the same secret.",
                    false, Gtk::MessageType::ERROR, Gtk::ButtonsType::OK, true);
                error_dialog->signal_response().connect([error_dialog](int) {
                    error_dialog->hide();
                });
                error_dialog->show();
            }
        }
        entry_dialog->hide();
    });

    entry_dialog->show();
}

void YubiKeyManagerDialog::on_remove_key() {
    auto* row = m_key_list.get_selected_row();
    if (!row) {
        return;
    }

    const char* serial = static_cast<const char*>(row->get_data("serial"));
    if (!serial) {
        return;
    }

    // Confirm removal
    auto* confirm_dialog = Gtk::make_managed<Gtk::MessageDialog>(*this,
        std::format("Remove YubiKey with serial {}?", serial),
        false, Gtk::MessageType::QUESTION, Gtk::ButtonsType::YES_NO, true);

    confirm_dialog->signal_response().connect([this, confirm_dialog, serial](int response) {
        if (response == Gtk::ResponseType::YES) {
            if (m_vault_manager->remove_yubikey(serial)) {
                refresh_key_list();
            } else {
                auto* error_dialog = Gtk::make_managed<Gtk::MessageDialog>(*this,
                    "Failed to remove YubiKey. Cannot remove the last key.",
                    false, Gtk::MessageType::ERROR, Gtk::ButtonsType::OK, true);
                error_dialog->signal_response().connect([error_dialog](int) {
                    error_dialog->hide();
                });
                error_dialog->show();
            }
        }
        confirm_dialog->hide();
    });

    confirm_dialog->show();
}

#endif // HAVE_YUBIKEY_SUPPORT
