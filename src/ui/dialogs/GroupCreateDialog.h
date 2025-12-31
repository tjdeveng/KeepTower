// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file GroupCreateDialog.h
 * @brief Dialog for creating new account groups
 */

#ifndef GROUP_CREATE_DIALOG_H
#define GROUP_CREATE_DIALOG_H

#include <gtkmm.h>

/**
 * @brief Dialog for creating a new account group
 *
 * Provides a simple interface for entering a group name with validation.
 */
class GroupCreateDialog : public Gtk::Dialog {
public:
    /** @brief Construct group creation dialog
     *  @param parent Parent window for modal dialog */
    GroupCreateDialog(Gtk::Window& parent);
    virtual ~GroupCreateDialog() = default;

    /**
     * @brief Get the entered group name
     * @return Group name entered by user
     */
    [[nodiscard]] Glib::ustring get_group_name() const;

private:
    void on_entry_changed();
    void on_entry_activate();

    Gtk::Box m_content_box;
    Gtk::Label m_label;
    Gtk::Entry m_name_entry;
    Gtk::Label m_hint_label;
};

#endif // GROUP_CREATE_DIALOG_H
