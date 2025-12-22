// SPDX-License-Identifier: GPL-3.0-or-later


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

enum class SortDirection {
    ASCENDING,  // A-Z
    DESCENDING  // Z-A
};

class AccountTreeWidget : public Gtk::Box {
public:
    AccountTreeWidget();
    ~AccountTreeWidget() override;

    // Set the groups and accounts to display
    void set_data(const std::vector<keeptower::AccountGroup>& groups,
                 const std::vector<keeptower::AccountRecord>& accounts);

    // Filtering: apply search text and tag filter
    void set_filters(const std::string& search_text, const std::string& tag_filter, int field_filter);
    void clear_filters();

    // Sorting: set sort direction and toggle
    void set_sort_direction(SortDirection direction);
    SortDirection get_sort_direction() const;
    void toggle_sort_direction();

    // Signal: emitted when an account row is right-clicked
    sigc::signal<void(std::string, Gtk::Widget*, double, double)>& signal_account_right_click();
    sigc::signal<void(std::string, Gtk::Widget*, double, double)>& signal_group_right_click();
    sigc::signal<void(std::string)>& signal_account_selected();
    sigc::signal<void(std::string)>& signal_group_selected();
    sigc::signal<void(std::string)>& signal_favorite_toggled();
    sigc::signal<void(std::string, std::string, int)>& signal_account_reordered();
    sigc::signal<void(std::string, int)>& signal_group_reordered();
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
