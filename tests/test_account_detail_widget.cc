// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include <gtest/gtest.h>
#include <gtkmm/application.h>
#include <gtkmm/window.h>
#include <glib-object.h>

#include "../src/ui/widgets/AccountDetailWidget.h"

#include <memory>
#include <string>
#include <vector>

namespace {

KeepTower::AccountDetail make_account_detail() {
    KeepTower::AccountDetail detail;
    detail.id = "account-1";
    detail.account_name = "Primary Account";
    detail.user_name = "alice";
    detail.password = "initial-password";
    detail.email = "alice@example.com";
    detail.website = "https://example.com";
    detail.notes = "seed data";
    detail.tags = {"existing"};
    return detail;
}

template <typename WidgetType, typename Predicate>
WidgetType* find_descendant(Gtk::Widget& root, Predicate&& predicate) {
    if (auto* match = dynamic_cast<WidgetType*>(&root)) {
        if (predicate(*match)) {
            return match;
        }
    }

    for (auto* child = root.get_first_child(); child; child = child->get_next_sibling()) {
        if (auto* match = find_descendant<WidgetType>(*child, predicate)) {
            return match;
        }
    }

    return nullptr;
}

class AccountDetailWidgetTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_app = Gtk::Application::create("com.test.account-detail-widget");
        m_window = std::make_unique<Gtk::Window>();
        m_widget = std::make_unique<AccountDetailWidget>();
        m_window->set_child(*m_widget);
    }

    Gtk::Entry* find_tags_entry() {
        return find_descendant<Gtk::Entry>(
            *m_widget,
            [](Gtk::Entry& entry) {
                return entry.get_placeholder_text() == "Add tag (press Enter)";
            }
        );
    }

    Glib::RefPtr<Gtk::Application> m_app;
    std::unique_ptr<Gtk::Window> m_window;
    std::unique_ptr<AccountDetailWidget> m_widget;
};

TEST_F(AccountDetailWidgetTest, DisplayAccountLeavesWidgetClean) {
    m_widget->display_account(make_account_detail());

    EXPECT_FALSE(m_widget->is_modified());
}

TEST_F(AccountDetailWidgetTest, ProgrammaticPasswordUpdateMarksWidgetModified) {
    m_widget->display_account(make_account_detail());

    m_widget->set_password("updated-password");

    EXPECT_TRUE(m_widget->is_modified());
}

TEST_F(AccountDetailWidgetTest, ResetModifiedFlagClearsProgrammaticEditState) {
    m_widget->display_account(make_account_detail());
    m_widget->set_password("updated-password");

    m_widget->reset_modified_flag();

    EXPECT_FALSE(m_widget->is_modified());
}

TEST_F(AccountDetailWidgetTest, TagEntryActivationMarksWidgetModifiedAndAddsTag) {
    m_widget->display_account(make_account_detail());

    auto* tags_entry = find_tags_entry();
    ASSERT_NE(tags_entry, nullptr);

    tags_entry->set_text("ops");
    g_signal_emit_by_name(tags_entry->gobj(), "activate");

    EXPECT_TRUE(m_widget->is_modified());

    const auto tags = m_widget->get_all_tags();
    EXPECT_NE(std::find(tags.begin(), tags.end(), "ops"), tags.end());
}

TEST_F(AccountDetailWidgetTest, ClearResetsModifiedState) {
    m_widget->display_account(make_account_detail());
    m_widget->set_password("updated-password");

    m_widget->clear();

    EXPECT_FALSE(m_widget->is_modified());
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    auto app = Gtk::Application::create("com.test.account-detail-widget");
    return RUN_ALL_TESTS();
}