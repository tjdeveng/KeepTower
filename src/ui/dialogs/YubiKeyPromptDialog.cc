// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "YubiKeyPromptDialog.h"

YubiKeyPromptDialog::YubiKeyPromptDialog(Gtk::Window& parent, PromptType type, const std::string& serial, const std::string& custom_message)
    : Gtk::Dialog("YubiKey Required", parent, true)
    , m_content_box(Gtk::Orientation::VERTICAL, 12)
    , m_icon()
    , m_message_label()
    , m_spinner()
{
    set_default_size(400, 200);
    set_resizable(false);

    // Only set modal for INSERT prompts (which have buttons)
    // Touch prompts should be non-modal so GTK main loop can process events
    set_modal(type == PromptType::INSERT);

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
    } else if (!custom_message.empty()) {
        // Use custom message if provided
        m_message_label.set_markup(custom_message);
        // Add progress bar with pulse animation (more reliable than spinner)
        m_progress.set_margin_top(12);
        m_progress.set_show_text(false);
        m_content_box.append(m_progress);
        // Start pulse timer (100ms intervals)
        m_pulse_timer = Glib::signal_timeout().connect(
            sigc::mem_fun(*this, &YubiKeyPromptDialog::on_pulse_timer), 100);
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

    // Add progress bar with pulse animation (more reliable than spinner)
    m_progress.set_margin_top(12);
    m_progress.set_show_text(false);
    m_content_box.append(m_progress);
    // Start pulse timer (100ms intervals)
    m_pulse_timer = Glib::signal_timeout().connect(
        sigc::mem_fun(*this, &YubiKeyPromptDialog::on_pulse_timer), 100);
}

void YubiKeyPromptDialog::update_message(const std::string& message) {
    m_message_label.set_markup(message);

    // Restart spinner if it's already added to the UI
    if (m_spinner.get_parent()) {
        m_spinner.start();
    }

    // Restart pulse timer if using progress bar
    if (m_progress.get_parent() && !m_pulse_timer.connected()) {
        m_pulse_timer = Glib::signal_timeout().connect(
            sigc::mem_fun(*this, &YubiKeyPromptDialog::on_pulse_timer), 100);
    }
}

bool YubiKeyPromptDialog::on_pulse_timer() {
    m_progress.pulse();
    return true;  // Keep timer running
}
