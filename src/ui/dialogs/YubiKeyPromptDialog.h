// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#ifndef YUBIKEYPROMPTDIALOG_H
#define YUBIKEYPROMPTDIALOG_H

#include <gtkmm.h>

class YubiKeyPromptDialog : public Gtk::Dialog {
public:
    enum class PromptType {
        INSERT,  // Prompt to insert YubiKey
        TOUCH    // Prompt to touch YubiKey
    };

    YubiKeyPromptDialog(Gtk::Window& parent, PromptType type, const std::string& serial = "");
    virtual ~YubiKeyPromptDialog() = default;

private:
    void setup_insert_prompt(const std::string& serial);
    void setup_touch_prompt();

    Gtk::Box m_content_box;
    Gtk::Image m_icon;
    Gtk::Label m_message_label;
    Gtk::Spinner m_spinner;
};

#endif // YUBIKEYPROMPTDIALOG_H
