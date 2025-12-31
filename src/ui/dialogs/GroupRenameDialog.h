// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file GroupRenameDialog.h
 * @brief Dialog for renaming existing account groups
 */

#ifndef GROUP_RENAME_DIALOG_H
#define GROUP_RENAME_DIALOG_H

#include <gtkmm.h>

/**
 * @brief Dialog for renaming an existing account group
 *
 * Provides a simple interface for entering a new group name with validation.
 * Pre-populates with the current group name.
 */
class GroupRenameDialog : public Gtk::Dialog {
public:
    /** @brief Construct group rename dialog
     *  @param parent Parent window for modal dialog
     *  @param current_name Current group name to pre-populate */
    GroupRenameDialog(Gtk::Window& parent, const Glib::ustring& current_name);
    virtual ~GroupRenameDialog() = default;

    /**
     * @brief Get the entered group name
     * @return New group name entered by user
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

#endif // GROUP_RENAME_DIALOG_H
