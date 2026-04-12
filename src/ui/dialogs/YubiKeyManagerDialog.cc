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
    // Determine vault type for UI adaptation
    bool is_v2_vault = m_vault_manager && m_vault_manager->is_v2_vault();

    // Info label
    if (is_v2_vault) {
        m_info_label.set_markup(
            "<b>YubiKey User Enrollment</b>\n\n"
            "YubiKeys are managed per-user during account setup and first login.\n"
            "Users can verify their YubiKeys during vault access steps.\n\n"
            "<i>To manage YubiKeys for specific users, use the Account Management UI.</i>"
        );
    } else {
        m_info_label.set_markup(
            "<b>Manage Authorized YubiKeys</b>\n\n"
            "Add backup YubiKeys to access this vault. All keys must be configured\n"
            "with FIPS-compliant HMAC-SHA256 challenge-response.\n\n"
            "<i>For complete setup instructions, see Help → Security or search for\n"
            "\"YubiKey FIPS Configuration\" in the help documentation.</i>"
        );
    }
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

    // Disable add/remove buttons for V2 vaults (not supported for backup keys)
    if (is_v2_vault) {
        m_add_button.set_sensitive(false);
        m_remove_button.set_sensitive(false);
        m_add_button.set_tooltip_text("YubiKey management for V2 vaults is done per-user");
        m_remove_button.set_tooltip_text("YubiKey management for V2 vaults is done per-user");
    }

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

    KeepTower::Log::info("YubiKeyManagerDialog: Calling get_yubikey_list_view()");
    auto keys = m_vault_manager->get_yubikey_list_view();
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
                i, key.name, key.serial);

            auto* row = Gtk::make_managed<Gtk::ListBoxRow>();
            auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
            box->set_margin(12);

            // Name label with safety check
            auto* name_label = Gtk::make_managed<Gtk::Label>();
            std::string name_text = !key.name.empty() ? key.name : "Unknown YubiKey";
            // GCC 13 compatibility: explicit string conversion required for Glib::Markup::escape_text
            // GCC 14+ will support direct usage: std::format("<b>{}</b>", Glib::Markup::escape_text(name_text))
            std::string escaped_name = Glib::Markup::escape_text(name_text);
            name_label->set_markup(std::format("<b>{}</b>", escaped_name));
            name_label->set_xalign(0.0);
            box->append(*name_label);

            // Info label with safety checks
            auto* info_label = Gtk::make_managed<Gtk::Label>();
            std::string serial_text = !key.serial.empty() ? key.serial : "Unknown";
            std::string time_text = "Unknown";

            if (key.added_at > 0) {
                std::time_t timestamp = key.added_at;
                const std::tm* tm_ptr = std::localtime(&timestamp);
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
    // V1 YubiKey backup key operations removed (Slice 4: #30)
    // V2 vaults manage YubiKeys per-user during enrollment
    if (m_vault_manager && m_vault_manager->is_v2_vault()) {
        auto* dialog = Gtk::make_managed<Gtk::MessageDialog>(*this,
            "YubiKey Management Not Available",
            false, Gtk::MessageType::INFO, Gtk::ButtonsType::OK, true);
        dialog->set_secondary_text(
            "YubiKeys for V2 vaults are managed per-user during account setup and first login.\n"
            "There is no vault-level backup key management for V2 vaults."
        );
        dialog->signal_response().connect([dialog](int) { dialog->hide(); });
        dialog->show();
        return;
    }
}

void YubiKeyManagerDialog::on_remove_key() {
    // V1 YubiKey backup key operations removed (Slice 4: #30)
    // V2 vaults manage YubiKeys per-user; removal is not applicable at vault level
    if (m_vault_manager && m_vault_manager->is_v2_vault()) {
        auto* dialog = Gtk::make_managed<Gtk::MessageDialog>(*this,
            "YubiKey Removal Not Available",
            false, Gtk::MessageType::INFO, Gtk::ButtonsType::OK, true);
        dialog->set_secondary_text(
            "YubiKey removal for V2 vaults must be done through the Account Management UI,\n"
            "as each user's YubiKey enrollment is managed independently."
        );
        dialog->signal_response().connect([dialog](int) { dialog->hide(); });
        dialog->show();
        return;
    }
}

#endif // HAVE_YUBIKEY_SUPPORT
