// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "YubiKeyPromptDialog.h"

YubiKeyPromptDialog::YubiKeyPromptDialog(Gtk::Window& parent, PromptType type, const std::string& serial)
    : Gtk::Dialog("YubiKey Required", parent, true)
    , m_content_box(Gtk::Orientation::VERTICAL, 12)
    , m_icon()
    , m_message_label()
    , m_spinner()
{
    set_default_size(400, 200);
    set_resizable(false);
    set_modal(true);

    // Add buttons FIRST (before content)
    if (type == PromptType::INSERT) {
        add_button("_Cancel", Gtk::ResponseType::CANCEL);
        add_button("_Retry", Gtk::ResponseType::OK);
    }
    // No buttons for touch prompt - it will be dismissed programmatically

    // Setup content box
    m_content_box.set_margin(24);
    m_content_box.set_halign(Gtk::Align::CENTER);
    m_content_box.set_valign(Gtk::Align::CENTER);

    // Add icon
    m_icon.set_from_icon_name("dialog-password");
    m_icon.set_pixel_size(48);
    m_content_box.append(m_icon);

    // Add message label
    m_message_label.set_wrap(true);
    m_message_label.set_max_width_chars(50);
    m_message_label.set_justify(Gtk::Justification::CENTER);
    m_content_box.append(m_message_label);

    // Setup based on prompt type
    if (type == PromptType::INSERT) {
        setup_insert_prompt(serial);
    } else {
        setup_touch_prompt();
    }

    // Add content to dialog
    get_content_area()->append(m_content_box);
}

void YubiKeyPromptDialog::setup_insert_prompt(const std::string& serial) {
    std::string message = "This vault requires a YubiKey for authentication.";

    if (!serial.empty()) {
        message += "\n\nExpected YubiKey serial: " + serial;
    }

    message += "\n\nPlease insert your YubiKey and click Retry.";
    m_message_label.set_markup("<big><b>YubiKey Not Detected</b></big>\n\n" + message);
}

void YubiKeyPromptDialog::setup_touch_prompt() {
    m_message_label.set_markup(
        "<big><b>Touch Your YubiKey</b></big>\n\n"
        "Please touch the button on your YubiKey to authenticate.\n\n"
        "The LED should be flashing..."
    );

    // Add and start spinner
    m_spinner.set_margin_top(12);
    m_spinner.start();
    m_content_box.append(m_spinner);
}
