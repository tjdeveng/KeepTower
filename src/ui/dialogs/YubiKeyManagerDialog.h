// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#ifndef YUBIKEY_MANAGER_DIALOG_H
#define YUBIKEY_MANAGER_DIALOG_H

#include <gtkmm.h>

#ifdef HAVE_YUBIKEY_SUPPORT

// Forward declaration
class VaultManager;

/**
 * @brief Dialog for managing multiple YubiKeys for a vault
 *
 * Allows users to add backup YubiKeys and remove existing keys
 * from the vault's authorized list.
 */
class YubiKeyManagerDialog : public Gtk::Dialog {
public:
    explicit YubiKeyManagerDialog(Gtk::Window& parent, VaultManager* vault_manager);
    virtual ~YubiKeyManagerDialog() = default;

private:
    void setup_ui();
    void refresh_key_list();
    void on_add_key();
    void on_remove_key();

    VaultManager* m_vault_manager;

    // UI Widgets
    Gtk::Box m_content_box;
    Gtk::Label m_info_label;
    Gtk::ScrolledWindow m_scrolled_window;
    Gtk::ListBox m_key_list;
    Gtk::Box m_button_box;
    Gtk::Button m_add_button;
    Gtk::Button m_remove_button;
    Gtk::Button m_close_button;

    std::string m_selected_serial;
};

#endif // HAVE_YUBIKEY_SUPPORT
#endif // YUBIKEY_MANAGER_DIALOG_H
