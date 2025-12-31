// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file AccountTreeWidget.h
 * @brief Hierarchical account/group tree view widget
 *
 * Provides a complete tree-based view of accounts and groups with:
 * - Hierarchical group/account display
 * - Search and tag filtering
 * - Configurable sorting (A-Z, Z-A)
 * - Drag-and-drop reordering
 * - Favorite toggling
 */

#pragma once
#include <sigc++/sigc++.h>
#include <gtkmm/box.h>
#include <gtkmm/listbox.h>
#include <gtkmm/listboxrow.h>
#include <gtkmm/scrolledwindow.h>
#include <string>
#include <vector>

#include "GroupRowWidget.h"
#include "AccountRowWidget.h"
#include "record.pb.h"

/**
 * @brief Sort direction for account/group display
 */
enum class SortDirection {
    ASCENDING,   ///< A-Z alphabetical
    DESCENDING   ///< Z-A reverse alphabetical
};

/**
 * @class AccountTreeWidget
 * @brief Complete hierarchical view of accounts and groups
 *
 * Main tree widget that orchestrates the display of all accounts and groups.
 * Manages filtering, sorting, drag-and-drop, and event propagation.
 *
 * @section features Features
 * - Group-based hierarchical organization
 * - Real-time search filtering (name, username, email, website, tags)
 * - Tag-based filtering
 * - Sortable (A-Z, Z-A)
 * - Drag-and-drop account reordering
 * - Favorite management
 * - Context menu integration
 */
class AccountTreeWidget : public Gtk::Box {
public:
    /** @brief Construct empty tree widget */
    AccountTreeWidget();

    /** @brief Destructor */
    ~AccountTreeWidget() override;

    /**
     * @brief Set data to display in tree
     * @param groups Vector of account groups
     * @param accounts Vector of account records
     */
    void set_data(const std::vector<keeptower::AccountGroup>& groups,
                 const std::vector<keeptower::AccountRecord>& accounts);

    /**
     * @brief Apply search and tag filters
     * @param search_text Text to search for in account fields
     * @param tag_filter Tag to filter by (empty for all)
     * @param field_filter Bitmask of fields to search
     */
    void set_filters(const std::string& search_text, const std::string& tag_filter, int field_filter);

    /** @brief Clear all active filters */
    void clear_filters();

    /**
     * @brief Set sort direction
     * @param direction ASCENDING or DESCENDING
     */
    void set_sort_direction(SortDirection direction);

    /**
     * @brief Get current sort direction
     * @return Current sort direction
     */
    SortDirection get_sort_direction() const;

    /** @brief Toggle between A-Z and Z-A */
    void toggle_sort_direction();

    /** @brief Signal emitted on account right-click */
    sigc::signal<void(std::string, Gtk::Widget*, double, double)>& signal_account_right_click();

    /** @brief Signal emitted on group right-click */
    sigc::signal<void(std::string, Gtk::Widget*, double, double)>& signal_group_right_click();

    /** @brief Signal emitted when account is selected */
    sigc::signal<void(std::string)>& signal_account_selected();

    /** @brief Signal emitted when group is selected */
    sigc::signal<void(std::string)>& signal_group_selected();

    /** @brief Signal emitted when favorite is toggled */
    sigc::signal<void(std::string)>& signal_favorite_toggled();

    /** @brief Signal emitted when account is reordered via drag-and-drop */
    sigc::signal<void(std::string, std::string, int)>& signal_account_reordered();

    /** @brief Signal emitted when group is reordered via drag-and-drop */
    sigc::signal<void(std::string, int)>& signal_group_reordered();

    /** @brief Signal emitted when sort direction changes */
    sigc::signal<void(SortDirection)>& signal_sort_direction_changed();

private:
    // Internal widgets
    Gtk::ScrolledWindow m_scrolled_window;
    Gtk::ListBox m_list_box; // ListBox for group/account rows

    // Store row widgets for lookup
    std::vector<GroupRowWidget*> m_group_rows;
    std::vector<AccountRowWidget*> m_account_rows;

    // Filter state
    std::string m_search_text;
    std::string m_tag_filter;
    int m_field_filter = 0; // 0=All, 1=Account Name, 2=Username, 3=Email, 4=Website, 5=Notes, 6=Tags

    // Sort state
    SortDirection m_sort_direction = SortDirection::ASCENDING;

    // Cached data for filtering
    std::vector<keeptower::AccountGroup> m_all_groups;
    std::vector<keeptower::AccountRecord> m_all_accounts;

    // Internal: clear and rebuild rows
    void rebuild_rows(const std::vector<keeptower::AccountGroup>& groups,
                     const std::vector<keeptower::AccountRecord>& accounts);

    // Internal: handle row selection
    void on_account_row_selected(const std::string& account_id);
    void on_group_row_selected(const std::string& group_id);

    // Signals
    sigc::signal<void(std::string, Gtk::Widget*, double, double)> m_signal_account_right_click;
    sigc::signal<void(std::string, Gtk::Widget*, double, double)> m_signal_group_right_click;
    sigc::signal<void(std::string)> m_signal_account_selected;
    sigc::signal<void(std::string)> m_signal_group_selected;
    sigc::signal<void(std::string)> m_signal_favorite_toggled;
    sigc::signal<void(std::string, std::string, int)> m_signal_account_reordered;
    sigc::signal<void(std::string, int)> m_signal_group_reordered;
    sigc::signal<void(SortDirection)> m_signal_sort_direction_changed;
};
