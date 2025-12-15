// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "GroupRenameDialog.h"

GroupRenameDialog::GroupRenameDialog(Gtk::Window& parent, const Glib::ustring& current_name)
    : Gtk::Dialog("Rename Group", parent, true)
    , m_content_box(Gtk::Orientation::VERTICAL, 18)  // HIG: 18px spacing between sections
    , m_label("New group name")  // HIG: sentence case for labels
    , m_hint_label()
{
    set_default_size(400, -1);

    // Setup content - HIG: 18px margins for dialog content
    m_content_box.set_margin(18);
    m_name_entry.set_text(current_name);  // Pre-populate with current name
    m_name_entry.set_placeholder_text("e.g., Work, Personal, Banking");
    m_name_entry.set_max_length(100);
    m_hint_label.set_text("Enter a new name for this group");
    m_hint_label.add_css_class("dim-label");
    m_hint_label.set_wrap(true);

    m_content_box.append(m_label);
    m_content_box.append(m_name_entry);
    m_content_box.append(m_hint_label);

    set_child(m_content_box);

    // Add buttons
    add_button("Cancel", Gtk::ResponseType::CANCEL);
    auto* rename_button = add_button("Rename", Gtk::ResponseType::OK);
    rename_button->add_css_class("suggested-action");
    set_default_response(Gtk::ResponseType::OK);

    // Connect signals
    m_name_entry.signal_changed().connect(
        sigc::mem_fun(*this, &GroupRenameDialog::on_entry_changed)
    );
    m_name_entry.signal_activate().connect(
        sigc::mem_fun(*this, &GroupRenameDialog::on_entry_activate)
    );

    // Select all text for easy replacement
    m_name_entry.select_region(0, -1);

    // Focus the entry
    m_name_entry.grab_focus();
}

Glib::ustring GroupRenameDialog::get_group_name() const {
    return m_name_entry.get_text();
}

void GroupRenameDialog::on_entry_changed() {
    auto text = m_name_entry.get_text();
    bool valid = !text.empty() && text.length() <= 100;
    set_response_sensitive(Gtk::ResponseType::OK, valid);
}

void GroupRenameDialog::on_entry_activate() {
    if (!m_name_entry.get_text().empty()) {
        response(Gtk::ResponseType::OK);
    }
}
