// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include <gtest/gtest.h>
#include <gtkmm/application.h>
#include <gtkmm/window.h>

#include "../src/ui/widgets/AccountTreeWidget.h"

#include <algorithm>
#include <functional>
#include <vector>

namespace {

keeptower::AccountGroup make_group(const std::string& id, const std::string& name) {
    keeptower::AccountGroup group;
    group.set_group_id(id);
    group.set_group_name(name);
    group.set_icon("folder-symbolic");
    return group;
}

keeptower::AccountRecord make_account(const std::string& id,
                                     const std::string& name,
                                     const std::string& group_id,
                                     bool favorite = false) {
    keeptower::AccountRecord account;
    account.set_id(id);
    account.set_account_name(name);
    account.set_user_name(id + "-user");
    account.set_email(id + "@example.com");
    account.set_website("https://" + id + ".example.com");
    account.set_notes("notes-" + id);
    account.set_is_favorite(favorite);

    auto* membership = account.add_groups();
    membership->set_group_id(group_id);
    membership->set_display_order(0);

    return account;
}

template <typename WidgetT>
std::vector<WidgetT*> collect_descendants(Gtk::Widget& root) {
    std::vector<WidgetT*> matches;

    std::function<void(Gtk::Widget&)> visit = [&](Gtk::Widget& widget) {
        if (auto* typed = dynamic_cast<WidgetT*>(&widget)) {
            matches.push_back(typed);
        }

        for (auto* child = widget.get_first_child(); child; child = child->get_next_sibling()) {
            visit(*child);
        }
    };

    visit(root);
    return matches;
}

std::vector<std::string> account_row_ids(AccountTreeWidget& tree_widget) {
    std::vector<std::string> ids;
    for (auto* row : collect_descendants<AccountRowWidget>(tree_widget)) {
        ids.push_back(row->account_id());
    }
    return ids;
}

int count_account_id(AccountTreeWidget& tree_widget, const std::string& id) {
    const auto ids = account_row_ids(tree_widget);
    return static_cast<int>(std::count(ids.begin(), ids.end(), id));
}

std::vector<std::string> group_row_ids(AccountTreeWidget& tree_widget) {
    std::vector<std::string> ids;
    for (auto* row : collect_descendants<GroupRowWidget>(tree_widget)) {
        ids.push_back(row->group_id());
    }
    return ids;
}

class AccountTreeWidgetTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_app = Gtk::Application::create("com.test.account-tree-widget");
        m_window = std::make_unique<Gtk::Window>();
        m_tree_widget = std::make_unique<AccountTreeWidget>();
        m_window->set_child(*m_tree_widget);
    }

    Glib::RefPtr<Gtk::Application> m_app;
    std::unique_ptr<Gtk::Window> m_window;
    std::unique_ptr<AccountTreeWidget> m_tree_widget;
};

TEST_F(AccountTreeWidgetTest, SelectAccountByIdSurvivesSynchronousRebuild) {
    std::vector<keeptower::AccountGroup> groups;
    groups.push_back(make_group("group-1", "Work"));

    std::vector<keeptower::AccountRecord> accounts;
    accounts.push_back(make_account("account-1", "Account 1", "group-1"));
    accounts.push_back(make_account("account-2", "Account 2", "group-1"));
    accounts.push_back(make_account("account-3", "Account 3", "group-1"));

    m_tree_widget->set_data(groups, accounts);

    int callback_count = 0;
    std::string selected_account_id;
    m_tree_widget->signal_account_selected().connect([&](const std::string& account_id) {
        ++callback_count;
        selected_account_id = account_id;

        // Reproduce the add-account path's synchronous tree rebuild during selection.
        m_tree_widget->set_data(groups, accounts);
    });

    EXPECT_NO_FATAL_FAILURE(m_tree_widget->select_account_by_id("account-3"));
    EXPECT_EQ(callback_count, 1);
    EXPECT_EQ(selected_account_id, "account-3");
}

TEST_F(AccountTreeWidgetTest, BoundaryTypeOverloadBuildsRowsAndSupportsSelection) {
    std::vector<KeepTower::GroupView> groups = {
        {.group_id = "group-1",
         .group_name = "Work",
         .description = "",
         .color = "",
         .icon = "folder-symbolic",
         .display_order = 0,
         .is_expanded = true,
         .is_system_group = false},
    };

    std::vector<KeepTower::AccountListItem> accounts = {
        {.id = "account-1",
         .account_name = "Email",
         .user_name = "alice",
         .email = "alice@example.com",
         .website = "https://email.example.com",
         .notes = "primary notes",
         .tags = {"ops", "urgent"},
         .groups = {{"group-1", 0}},
         .is_favorite = true,
         .is_archived = false,
         .global_display_order = 3},
    };

    m_tree_widget->set_data(groups, accounts);

    const auto group_ids = group_row_ids(*m_tree_widget);
    EXPECT_NE(std::find(group_ids.begin(), group_ids.end(), "group-1"), group_ids.end());
    EXPECT_NE(std::find(group_ids.begin(), group_ids.end(), "favorites"), group_ids.end());
    EXPECT_NE(std::find(group_ids.begin(), group_ids.end(), "all"), group_ids.end());

    int selected_calls = 0;
    std::string selected_account_id;
    m_tree_widget->signal_account_selected().connect([&](const std::string& account_id) {
        selected_calls++;
        selected_account_id = account_id;
    });

    m_tree_widget->select_account_by_id("account-1");

    EXPECT_EQ(selected_calls, 1);
    EXPECT_EQ(selected_account_id, "account-1");
    EXPECT_GE(count_account_id(*m_tree_widget, "account-1"), 2);
}

TEST_F(AccountTreeWidgetTest, SortDirectionSignalAndToggleRebuildVisibleOrder) {
    std::vector<keeptower::AccountGroup> groups;
    groups.push_back(make_group("group-1", "Work"));

    std::vector<keeptower::AccountRecord> accounts;
    accounts.push_back(make_account("account-b", "Beta", "group-1"));
    accounts.push_back(make_account("account-a", "Alpha", "group-1"));

    m_tree_widget->set_data(groups, accounts);

    int signal_count = 0;
    SortDirection last_direction = SortDirection::ASCENDING;
    m_tree_widget->signal_sort_direction_changed().connect([&](SortDirection direction) {
        signal_count++;
        last_direction = direction;
    });

    EXPECT_EQ(account_row_ids(*m_tree_widget),
              (std::vector<std::string>{"account-a", "account-b", "account-a", "account-b"}));

    m_tree_widget->set_sort_direction(SortDirection::ASCENDING);
    EXPECT_EQ(signal_count, 0);

    m_tree_widget->toggle_sort_direction();

    EXPECT_EQ(signal_count, 1);
    EXPECT_EQ(last_direction, SortDirection::DESCENDING);
    EXPECT_EQ(m_tree_widget->get_sort_direction(), SortDirection::DESCENDING);
    EXPECT_EQ(account_row_ids(*m_tree_widget),
              (std::vector<std::string>{"account-b", "account-a", "account-b", "account-a"}));
}

TEST_F(AccountTreeWidgetTest, FiltersLimitRowsAndClearFiltersRestoresData) {
    std::vector<keeptower::AccountGroup> groups;
    groups.push_back(make_group("group-1", "Work"));

    auto alpha = make_account("alpha-id", "Alpha", "group-1");
    alpha.add_tags("ops");
    alpha.add_tags("blue");

    auto beta = make_account("beta-id", "Beta", "group-1");
    beta.add_tags("sales");
    beta.set_notes("contains special note");

    std::vector<keeptower::AccountRecord> accounts{alpha, beta};
    m_tree_widget->set_data(groups, accounts);

    EXPECT_EQ(count_account_id(*m_tree_widget, "alpha-id"), 2);
    EXPECT_EQ(count_account_id(*m_tree_widget, "beta-id"), 2);

    m_tree_widget->set_filters("ops", "", 6);
    EXPECT_EQ(count_account_id(*m_tree_widget, "alpha-id"), 2);
    EXPECT_EQ(count_account_id(*m_tree_widget, "beta-id"), 0);

    int selected_calls = 0;
    m_tree_widget->signal_account_selected().connect([&](const std::string&) {
        selected_calls++;
    });
    m_tree_widget->select_account_by_id("beta-id");
    EXPECT_EQ(selected_calls, 0);

    m_tree_widget->set_filters("", "sales", 0);
    EXPECT_EQ(count_account_id(*m_tree_widget, "alpha-id"), 0);
    EXPECT_EQ(count_account_id(*m_tree_widget, "beta-id"), 2);

    m_tree_widget->set_filters("special", "", 5);
    EXPECT_EQ(count_account_id(*m_tree_widget, "alpha-id"), 0);
    EXPECT_EQ(count_account_id(*m_tree_widget, "beta-id"), 2);

    m_tree_widget->clear_filters();
    EXPECT_EQ(count_account_id(*m_tree_widget, "alpha-id"), 2);
    EXPECT_EQ(count_account_id(*m_tree_widget, "beta-id"), 2);
}

}  // namespace