// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include <gtest/gtest.h>
#include <gtkmm/application.h>
#include <gtkmm/window.h>

#include "../src/ui/widgets/AccountTreeWidget.h"

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
                                     const std::string& group_id) {
    keeptower::AccountRecord account;
    account.set_id(id);
    account.set_account_name(name);

    auto* membership = account.add_groups();
    membership->set_group_id(group_id);
    membership->set_display_order(0);

    return account;
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

}  // namespace