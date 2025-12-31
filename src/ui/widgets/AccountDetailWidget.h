// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file AccountDetailWidget.h
 * @brief Widget for displaying and editing account details
 */

#pragma once
#include <gtkmm.h>
#include <string>
#include <vector>

// Forward declaration
namespace keeptower {
class AccountRecord;
}

/**
 * @class AccountDetailWidget
 * @brief Custom widget for account detail editing with password security
 *
 * Provides a split pane view with account fields on the left and notes on the right.
 * Implements secure password clearing on destruction and account switching.
 */
class AccountDetailWidget : public Gtk::ScrolledWindow {
public:
    AccountDetailWidget();
    virtual ~AccountDetailWidget();

    // Public API to display account details

    /** @brief Display account record in widget
     *  @param account Pointer to AccountRecord to display (nullptr to clear) */
    void display_account(const keeptower::AccountRecord* account);

    /** @brief Clear all fields and reset widget state */
    void clear();

    // Getters for edited values

    /** @brief Get edited account name
     *  @return Account name from entry field */
    std::string get_account_name() const;

    /** @brief Get edited username
     *  @return Username from entry field */
    std::string get_user_name() const;

    /** @brief Get edited password
     *  @return Password from entry field */
    std::string get_password() const;

    /** @brief Get edited email address
     *  @return Email from entry field */
    std::string get_email() const;

    /** @brief Get edited website URL
     *  @return Website from entry field */
    std::string get_website() const;

    /** @brief Get edited notes
     *  @return Notes from text view */
    std::string get_notes() const;

    /** @brief Get tags as comma-separated string
     *  @return Tags from entry field */
    std::string get_tags() const;

    /** @brief Get all tags as vector
     *  @return Vector of individual tags */
    std::vector<std::string> get_all_tags() const;

    // Privacy controls (V2 multi-user)

    /** @brief Get admin-only viewable flag state
     *  @return true if "Admin Only Viewable" is checked */
    [[nodiscard]] bool get_admin_only_viewable() const;

    /** @brief Get admin-only deletable flag state
     *  @return true if "Admin Only Deletable" is checked */
    [[nodiscard]] bool get_admin_only_deletable() const;

    // Setters for enabling/disabling editing

    /** @brief Enable or disable all field editing
     *  @param editable true to allow editing, false for read-only */
    void set_editable(bool editable);

    /** @brief Programmatically set password field
     *  @param password Password string (e.g., from generator) */
    void set_password(const std::string& password);

    /** @brief Enable or disable privacy checkbox editing
     *  @param editable true to allow editing privacy flags */
    void set_privacy_controls_editable(bool editable);

    /** @brief Enable or disable delete button
     *  @param sensitive true to enable delete button */
    void set_delete_button_sensitive(bool sensitive);

    // Track if account has been modified

    /** @brief Check if any field has been modified
     *  @return true if account has unsaved changes */
    [[nodiscard]] bool is_modified() const { return m_is_modified; }

    /** @brief Reset modification flag (after save) */
    void reset_modified_flag() { m_is_modified = false; }

    // Focus management

    /** @brief Set keyboard focus to account name entry field */
    void focus_account_name_entry();

    // Signals

    /** @brief Signal emitted when any field is modified
     *  @return Signal with no parameters */
    sigc::signal<void()> signal_modified();

    /** @brief Signal emitted when delete button is clicked
     *  @return Signal with no parameters */
    sigc::signal<void()> signal_delete_requested();

    /** @brief Signal emitted when generate password button is clicked
     *  @return Signal with no parameters */
    sigc::signal<void()> signal_generate_password();

    /** @brief Signal emitted when copy password button is clicked
     *  @return Signal with no parameters */
    sigc::signal<void()> signal_copy_password();

private:
    // UI Layout containers
    Gtk::Box m_details_box;
    Gtk::Paned m_details_paned;  // Horizontal resizable split for fields + notes
    Gtk::Box m_details_fields_box;  // Left side: all input fields

    // Account field widgets
    Gtk::Label m_account_name_label;
    Gtk::Entry m_account_name_entry;

    Gtk::Label m_user_name_label;
    Gtk::Entry m_user_name_entry;

    Gtk::Label m_password_label;
    Gtk::Entry m_password_entry;
    Gtk::Button m_show_password_button;
    Gtk::Button m_copy_password_button;
    Gtk::Button m_generate_password_button;

    Gtk::Label m_email_label;
    Gtk::Entry m_email_entry;

    Gtk::Label m_website_label;
    Gtk::Entry m_website_entry;

    Gtk::Label m_notes_label;
    Gtk::TextView m_notes_view;
    Gtk::ScrolledWindow m_notes_scrolled;

    // Tags
    Gtk::Label m_tags_label;
    Gtk::Entry m_tags_entry;
    Gtk::FlowBox m_tags_flowbox;
    Gtk::ScrolledWindow m_tags_scrolled;

    // Privacy controls (V2 multi-user vaults)
    Gtk::Label m_privacy_label;
    Gtk::CheckButton m_admin_only_viewable_check;
    Gtk::CheckButton m_admin_only_deletable_check;

    // Delete button
    Gtk::Button m_delete_account_button;

    // Signals
    sigc::signal<void()> m_signal_modified;
    sigc::signal<void()> m_signal_delete_requested;
    sigc::signal<void()> m_signal_generate_password;
    sigc::signal<void()> m_signal_copy_password;

    // Internal helpers
    void on_show_password_clicked();
    void on_entry_changed();
    void on_tag_entry_activate();
    void add_tag_chip(const std::string& tag);
    void remove_tag_chip(const std::string& tag);
    /**
     * @brief Securely clear password entry widget
     *
     * Uses triple-overwrite pattern (0x00, 0xFF, 0xAA) to clear password memory.
     * Note: This is best-effort as GTK4 Entry doesn't expose underlying buffer.
     */
    void secure_clear_password();  // Secure password clearing

    bool m_password_visible;
    bool m_is_modified;  // Track if account has been edited
};
