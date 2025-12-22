#include "AccountTreeWidget.h"
#include "GroupRowWidget.h"
#include "AccountRowWidget.h"
#include "record.pb.h"
#include <sigc++/signal.h>
#include <algorithm>

AccountTreeWidget::AccountTreeWidget()
    : Gtk::Box(Gtk::Orientation::VERTICAL, 0)
{
    // Make this widget expand to fill available space
    set_vexpand(true);
    set_hexpand(true);

    // Configure ListBox for proper display
    m_list_box.set_selection_mode(Gtk::SelectionMode::NONE);
    m_list_box.set_show_separators(false);
    m_list_box.add_css_class("navigation-sidebar");

    // Make ScrolledWindow expand to fill the parent
    m_scrolled_window.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    m_scrolled_window.set_vexpand(true);
    m_scrolled_window.set_hexpand(true);
    m_scrolled_window.set_child(m_list_box);
    append(m_scrolled_window);
}

AccountTreeWidget::~AccountTreeWidget() = default;

// Right-click signal accessors
sigc::signal<void(std::string, Gtk::Widget*, double, double)>& AccountTreeWidget::signal_account_right_click() {
    return m_signal_account_right_click;
}

sigc::signal<void(std::string, Gtk::Widget*, double, double)>& AccountTreeWidget::signal_group_right_click() {
    return m_signal_group_right_click;
}

void AccountTreeWidget::set_data(const std::vector<keeptower::AccountGroup>& groups,
                                 const std::vector<keeptower::AccountRecord>& accounts) {
    // Cache the data for filtering
    m_all_groups = groups;
    m_all_accounts = accounts;

    // Apply current filters and rebuild
    if (m_search_text.empty() && m_tag_filter.empty()) {
        rebuild_rows(groups, accounts);
    } else {
        set_filters(m_search_text, m_tag_filter, m_field_filter);
    }
}

sigc::signal<void(std::string)>& AccountTreeWidget::signal_account_selected() {
    return m_signal_account_selected;
}

sigc::signal<void(std::string)>& AccountTreeWidget::signal_group_selected() {
    return m_signal_group_selected;
}

sigc::signal<void(std::string)>& AccountTreeWidget::signal_favorite_toggled() {
    return m_signal_favorite_toggled;
}

sigc::signal<void(std::string, std::string, int)>& AccountTreeWidget::signal_account_reordered() {
    return m_signal_account_reordered;
}

sigc::signal<void(std::string, int)>& AccountTreeWidget::signal_group_reordered() {
    return m_signal_group_reordered;
}

void AccountTreeWidget::rebuild_rows(const std::vector<keeptower::AccountGroup>& groups,
                                     const std::vector<keeptower::AccountRecord>& accounts) {
    // Clear previous widgets
    m_list_box.remove_all();
    m_group_rows.clear();
    m_account_rows.clear();

    // Create "Favorites" system group with favorited accounts
    std::vector<size_t> favorite_indices;
    for (size_t i = 0; i < accounts.size(); ++i) {
        if (accounts[i].is_favorite()) {
            favorite_indices.push_back(i);
        }
    }

    if (!favorite_indices.empty()) {
        // Create a synthetic AccountGroup for favorites
        keeptower::AccountGroup favorites_group;
        favorites_group.set_group_id("favorites");
        favorites_group.set_group_name("⭐ Favorites");
        favorites_group.set_icon("starred-symbolic");

        auto group_row = Gtk::make_managed<GroupRowWidget>();
        group_row->set_group(favorites_group);
        group_row->set_visible(true);
        group_row->signal_selected().connect(
            sigc::mem_fun(*this, &AccountTreeWidget::on_group_row_selected));
        group_row->signal_right_clicked().connect(
            [this](const std::string& group_id, Gtk::Widget* widget, double x, double y) {
                m_signal_group_right_click.emit(group_id, widget, x, y);
            });
        group_row->signal_account_dropped().connect(
            [this](const std::string& account_id, const std::string& group_id) {
                m_signal_account_reordered.emit(account_id, group_id, 0);
            });

        // Wrap in ListBoxRow to control behavior
        auto list_row = Gtk::make_managed<Gtk::ListBoxRow>();
        list_row->set_activatable(false);
        list_row->set_selectable(false);
        list_row->set_child(*group_row);
        m_list_box.append(*list_row);

        m_group_rows.push_back(group_row);

        // Sort favorites alphabetically
        std::sort(favorite_indices.begin(), favorite_indices.end(),
            [&accounts](size_t a, size_t b) {
                return accounts[a].account_name() < accounts[b].account_name();
            });

        // Add favorite accounts as children of the group
        for (size_t index : favorite_indices) {
            auto account_row = Gtk::make_managed<AccountRowWidget>();
            account_row->set_account(accounts[index]);
            account_row->set_visible(true);
            account_row->signal_selected().connect(
                sigc::mem_fun(*this, &AccountTreeWidget::on_account_row_selected));
            account_row->signal_right_clicked().connect(
                [this](const std::string& account_id, Gtk::Widget* widget, double x, double y) {
                    m_signal_account_right_click.emit(account_id, widget, x, y);
                });
            account_row->signal_favorite_toggled().connect(
                [this](const std::string& account_id) {
                    m_signal_favorite_toggled.emit(account_id);
                });
            group_row->add_child(*account_row);  // Add as child of group, not sibling
            m_account_rows.push_back(account_row);
        }
    }

    // Add user-created groups with their accounts
    for (const auto& group : groups) {
        // Skip system group (favorites)
        if (group.group_id() == "favorites") {
            continue;
        }


        // Get accounts in this group
        std::vector<size_t> group_account_indices;
        for (size_t i = 0; i < accounts.size(); ++i) {
            for (int j = 0; j < accounts[i].groups_size(); ++j) {
                if (accounts[i].groups(j).group_id() == group.group_id()) {
                    group_account_indices.push_back(i);
                    break;
                }
            }
        }


        // Only show group if it has accounts
        if (group_account_indices.empty()) {
            continue;
        }

        auto group_row = Gtk::make_managed<GroupRowWidget>();
        group_row->set_group(group);
        group_row->set_visible(true);
        group_row->signal_selected().connect(
            sigc::mem_fun(*this, &AccountTreeWidget::on_group_row_selected));
        group_row->signal_right_clicked().connect(
            [this](const std::string& group_id, Gtk::Widget* widget, double x, double y) {
                m_signal_group_right_click.emit(group_id, widget, x, y);
            });
        group_row->signal_account_dropped().connect(
            [this](const std::string& account_id, const std::string& group_id) {
                m_signal_account_reordered.emit(account_id, group_id, 0);
            });

        // Wrap in ListBoxRow to control behavior
        auto list_row = Gtk::make_managed<Gtk::ListBoxRow>();
        list_row->set_activatable(false);
        list_row->set_selectable(false);
        list_row->set_child(*group_row);
        m_list_box.append(*list_row);

        m_group_rows.push_back(group_row);

        // Sort accounts alphabetically
        std::sort(group_account_indices.begin(), group_account_indices.end(),
            [&accounts](size_t a, size_t b) {
                return accounts[a].account_name() < accounts[b].account_name();
            });

        // Add accounts as children of this group
        for (size_t index : group_account_indices) {

            auto account_row = Gtk::make_managed<AccountRowWidget>();
            account_row->set_account(accounts[index]);
            account_row->set_visible(true);
            account_row->signal_selected().connect(
                sigc::mem_fun(*this, &AccountTreeWidget::on_account_row_selected));
            account_row->signal_right_clicked().connect(
                [this](const std::string& account_id, Gtk::Widget* widget, double x, double y) {
                    m_signal_account_right_click.emit(account_id, widget, x, y);
                });
            account_row->signal_favorite_toggled().connect(
                [this](const std::string& account_id) {
                    m_signal_favorite_toggled.emit(account_id);
                });
            account_row->signal_account_dropped_on_account().connect(
                [this, group_id = group.group_id()](const std::string& dragged_id, const std::string& /* target_id */) {
                    // Accounts are sorted alphabetically, so just emit with index 0
                    // The actual position doesn't matter since rebuild will re-sort
                    m_signal_account_reordered.emit(dragged_id, group_id, 0);
                });
            group_row->add_child(*account_row);  // Add as child of group, not sibling
            m_account_rows.push_back(account_row);
        }
    }

    // Create "All Accounts" system group - TEMPORARILY SHOW ALL to debug
    // Create a synthetic AccountGroup for all accounts
    keeptower::AccountGroup all_group;
    all_group.set_group_id("all");
    all_group.set_group_name("All Accounts");
    all_group.set_icon("folder-symbolic");

    auto all_group_row = Gtk::make_managed<GroupRowWidget>();
    all_group_row->set_group(all_group);
    all_group_row->set_visible(true);
    all_group_row->signal_selected().connect(
        sigc::mem_fun(*this, &AccountTreeWidget::on_group_row_selected));
    all_group_row->signal_right_clicked().connect(
        [this](const std::string& group_id, Gtk::Widget* widget, double x, double y) {
            m_signal_group_right_click.emit(group_id, widget, x, y);
        });
    all_group_row->signal_account_dropped().connect(
        [this](const std::string& account_id, [[maybe_unused]] const std::string& group_id) {
            // Dropping into 'All Accounts' means remove from any specific group
            // The account stays in the vault and remains visible in All Accounts
            // Emit with empty string to indicate "remove from current group"
            m_signal_account_reordered.emit(account_id, "", 0);
        });

    // Wrap in ListBoxRow to control behavior
    auto all_list_row = Gtk::make_managed<Gtk::ListBoxRow>();
    all_list_row->set_activatable(false);
    all_list_row->set_selectable(false);
    all_list_row->set_child(*all_group_row);
    m_list_box.append(*all_list_row);

    m_group_rows.push_back(all_group_row);

    // Temporarily show ALL accounts here to debug
    std::vector<size_t> all_indices;
    for (size_t i = 0; i < accounts.size(); ++i) {
        all_indices.push_back(i);
    }

    // Sort all accounts alphabetically
    std::sort(all_indices.begin(), all_indices.end(),
        [&accounts](size_t a, size_t b) {
            return accounts[a].account_name() < accounts[b].account_name();
        });

    // Add all accounts as children of the group
    for (size_t index : all_indices) {
        auto account_row = Gtk::make_managed<AccountRowWidget>();
        account_row->set_account(accounts[index]);
        account_row->set_visible(true);
        account_row->signal_selected().connect(
            sigc::mem_fun(*this, &AccountTreeWidget::on_account_row_selected));
        account_row->signal_right_clicked().connect(
            [this](const std::string& account_id, Gtk::Widget* widget, double x, double y) {
                m_signal_account_right_click.emit(account_id, widget, x, y);
            });
        account_row->signal_favorite_toggled().connect(
            [this](const std::string& account_id) {
                m_signal_favorite_toggled.emit(account_id);
            });
        account_row->signal_account_dropped_on_account().connect(
            [this](const std::string& dragged_id, const std::string& target_id) {
                // In 'All Accounts', just emit to remove from all groups
                // Don't calculate index since accounts are sorted alphabetically
                m_signal_account_reordered.emit(dragged_id, "", 0);
            });
        all_group_row->add_child(*account_row);  // Add as child of group, not sibling
        m_account_rows.push_back(account_row);
    }
}

void AccountTreeWidget::on_account_row_selected(const std::string& account_id) {
    m_signal_account_selected.emit(account_id);
}

void AccountTreeWidget::on_group_row_selected(const std::string& group_id) {
    m_signal_group_selected.emit(group_id);
}

void AccountTreeWidget::set_filters(const std::string& search_text, const std::string& tag_filter, int field_filter) {
    m_search_text = search_text;
    m_tag_filter = tag_filter;
    m_field_filter = field_filter;

    // If no filters active, show all accounts
    if (search_text.empty() && tag_filter.empty()) {
        rebuild_rows(m_all_groups, m_all_accounts);
        return;
    }

    // Filter accounts based on search text and tag
    std::vector<keeptower::AccountRecord> filtered_accounts;

    for (const auto& account : m_all_accounts) {
        // Check tag filter first
        bool tag_match = tag_filter.empty();
        if (!tag_filter.empty()) {
            for (int i = 0; i < account.tags_size(); ++i) {
                if (account.tags(i) == tag_filter) {
                    tag_match = true;
                    break;
                }
            }
        }

        if (!tag_match) continue;

        // Check search text filter
        if (!search_text.empty()) {
            std::string search_lower = search_text;
            std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::tolower);

            bool text_match = false;

            // Helper to check if a field matches the search text
            auto check_field = [&](const std::string& field_value) {
                std::string field_lower = field_value;
                std::transform(field_lower.begin(), field_lower.end(), field_lower.begin(), ::tolower);
                return field_lower.find(search_lower) != std::string::npos;
            };

            // Check based on field_filter
            // 0=All, 1=Account Name, 2=Username, 3=Email, 4=Website, 5=Notes, 6=Tags
            switch (field_filter) {
                case 0: // All fields
                    text_match = check_field(account.account_name()) ||
                                check_field(account.user_name()) ||
                                check_field(account.email()) ||
                                check_field(account.website()) ||
                                check_field(account.notes());
                    // Check tags
                    if (!text_match) {
                        for (int i = 0; i < account.tags_size(); ++i) {
                            if (check_field(account.tags(i))) {
                                text_match = true;
                                break;
                            }
                        }
                    }
                    break;
                case 1: // Account Name
                    text_match = check_field(account.account_name());
                    break;
                case 2: // Username
                    text_match = check_field(account.user_name());
                    break;
                case 3: // Email
                    text_match = check_field(account.email());
                    break;
                case 4: // Website
                    text_match = check_field(account.website());
                    break;
                case 5: // Notes
                    text_match = check_field(account.notes());
                    break;
                case 6: // Tags
                    for (int i = 0; i < account.tags_size(); ++i) {
                        if (check_field(account.tags(i))) {
                            text_match = true;
                            break;
                        }
                    }
                    break;
            }

            if (!text_match) continue;
        }

        // Account passed all filters
        filtered_accounts.push_back(account);
    }

    // Rebuild with filtered accounts
    rebuild_rows(m_all_groups, filtered_accounts);
}

void AccountTreeWidget::clear_filters() {
    m_search_text.clear();
    m_tag_filter.clear();
    m_field_filter = 0;
    rebuild_rows(m_all_groups, m_all_accounts);
}
