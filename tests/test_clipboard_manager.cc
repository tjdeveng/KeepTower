// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng
//
// test_clipboard_manager.cc - Unit tests for ClipboardManager

#include <gtest/gtest.h>
#include <glibmm/init.h>
#include <gtkmm/application.h>
#include <gtkmm/window.h>
#include "../src/ui/controllers/ClipboardManager.h"

using namespace KeepTower;

// Test fixture
class ClipboardManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create GTK application for clipboard access
        int argc = 0;
        char** argv = nullptr;
        m_app = Gtk::Application::create("com.test.clipboard");
        m_window = std::make_unique<Gtk::Window>();
        m_clipboard = m_window->get_clipboard();
    }

    void TearDown() override {
        m_window.reset();
    }

    Glib::RefPtr<Gtk::Application> m_app;
    std::unique_ptr<Gtk::Window> m_window;
    Glib::RefPtr<Gdk::Clipboard> m_clipboard;
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(ClipboardManagerTest, ConstructorWithValidClipboard) {
    EXPECT_NO_THROW({
        ClipboardManager manager(m_clipboard);
        EXPECT_EQ(manager.get_clear_timeout_seconds(), ClipboardManager::DEFAULT_CLEAR_TIMEOUT);
        EXPECT_FALSE(manager.is_clear_pending());
    });
}

TEST_F(ClipboardManagerTest, ConstructorThrowsOnNullClipboard) {
    Glib::RefPtr<Gdk::Clipboard> null_clipboard;
    EXPECT_THROW({
        ClipboardManager manager(null_clipboard);
    }, std::invalid_argument);
}

// ============================================================================
// Timeout Configuration Tests
// ============================================================================

TEST_F(ClipboardManagerTest, SetTimeoutValidRange) {
    ClipboardManager manager(m_clipboard);

    manager.set_clear_timeout_seconds(15);
    EXPECT_EQ(manager.get_clear_timeout_seconds(), 15);

    manager.set_clear_timeout_seconds(60);
    EXPECT_EQ(manager.get_clear_timeout_seconds(), 60);
}

TEST_F(ClipboardManagerTest, SetTimeoutClampsTooLow) {
    ClipboardManager manager(m_clipboard);

    manager.set_clear_timeout_seconds(1);  // Below MIN_CLEAR_TIMEOUT (5)
    EXPECT_EQ(manager.get_clear_timeout_seconds(), ClipboardManager::MIN_CLEAR_TIMEOUT);
}

TEST_F(ClipboardManagerTest, SetTimeoutClampsTooHigh) {
    ClipboardManager manager(m_clipboard);

    manager.set_clear_timeout_seconds(500);  // Above MAX_CLEAR_TIMEOUT (300)
    EXPECT_EQ(manager.get_clear_timeout_seconds(), ClipboardManager::MAX_CLEAR_TIMEOUT);
}

// ============================================================================
// Copy and Clear Tests
// ============================================================================

TEST_F(ClipboardManagerTest, CopyTextStartsClearTimer) {
    ClipboardManager manager(m_clipboard);

    manager.copy_text("TestPassword123");
    EXPECT_TRUE(manager.is_clear_pending()) << "Clear timer should be active after copy";
}

TEST_F(ClipboardManagerTest, CopyTextEmitsSignal) {
    ClipboardManager manager(m_clipboard);

    bool signal_received = false;
    std::string received_text;

    manager.signal_copied().connect([&signal_received, &received_text](const std::string& text) {
        signal_received = true;
        received_text = text;
    });

    manager.copy_text("MySecretPassword");
    EXPECT_TRUE(signal_received);
    EXPECT_EQ(received_text, "MySecretPassword");
}

TEST_F(ClipboardManagerTest, CopyTextCancelsPreviousTimer) {
    ClipboardManager manager(m_clipboard);

    manager.copy_text("First");
    EXPECT_TRUE(manager.is_clear_pending());

    manager.copy_text("Second");
    EXPECT_TRUE(manager.is_clear_pending()) << "New clear timer should be active";
}

TEST_F(ClipboardManagerTest, ClearImmediatelyStopsTimer) {
    ClipboardManager manager(m_clipboard);

    manager.copy_text("SomePassword");
    EXPECT_TRUE(manager.is_clear_pending());

    manager.clear_immediately();
    EXPECT_FALSE(manager.is_clear_pending()) << "Timer should be stopped after clear";
}

TEST_F(ClipboardManagerTest, ClearImmediatelyEmitsSignal) {
    ClipboardManager manager(m_clipboard);

    bool signal_received = false;
    manager.signal_cleared().connect([&signal_received]() {
        signal_received = true;
    });

    manager.copy_text("Password");
    manager.clear_immediately();
    EXPECT_TRUE(signal_received);
}

TEST_F(ClipboardManagerTest, ClearWithoutCopyIsNoOp) {
    ClipboardManager manager(m_clipboard);

    EXPECT_NO_THROW(manager.clear_immediately());
    EXPECT_FALSE(manager.is_clear_pending());
}

// ============================================================================
// Signal Tests
// ============================================================================

TEST_F(ClipboardManagerTest, MultipleSignalConnections) {
    ClipboardManager manager(m_clipboard);

    int callback1_count = 0;
    int callback2_count = 0;

    manager.signal_copied().connect([&callback1_count](const std::string&) {
        callback1_count++;
    });

    manager.signal_copied().connect([&callback2_count](const std::string&) {
        callback2_count++;
    });

    manager.copy_text("Test");

    EXPECT_EQ(callback1_count, 1);
    EXPECT_EQ(callback2_count, 1);
}

TEST_F(ClipboardManagerTest, ClearedSignalMultipleConnections) {
    ClipboardManager manager(m_clipboard);

    int callback1_count = 0;
    int callback2_count = 0;

    manager.signal_cleared().connect([&callback1_count]() {
        callback1_count++;
    });

    manager.signal_cleared().connect([&callback2_count]() {
        callback2_count++;
    });

    manager.copy_text("Test");
    manager.clear_immediately();

    EXPECT_EQ(callback1_count, 1);
    EXPECT_EQ(callback2_count, 1);
}

// ============================================================================
// Destructor Tests
// ============================================================================

TEST_F(ClipboardManagerTest, DestructorClearsPendingClipboard) {
    {
        ClipboardManager manager(m_clipboard);
        manager.copy_text("SensitiveData");
        EXPECT_TRUE(manager.is_clear_pending());
        // Manager destroyed here, should clear clipboard
    }

    // Clipboard should be cleared by destructor
    // (Cannot easily verify without async clipboard read)
}

// ============================================================================
// Constants Tests
// ============================================================================

TEST_F(ClipboardManagerTest, ConstantsAreReasonable) {
    EXPECT_EQ(ClipboardManager::MIN_CLEAR_TIMEOUT, 5) << "Minimum should be 5 seconds";
    EXPECT_EQ(ClipboardManager::MAX_CLEAR_TIMEOUT, 300) << "Maximum should be 5 minutes";
    EXPECT_EQ(ClipboardManager::DEFAULT_CLEAR_TIMEOUT, 30) << "Default should be 30 seconds";

    EXPECT_GE(ClipboardManager::DEFAULT_CLEAR_TIMEOUT, ClipboardManager::MIN_CLEAR_TIMEOUT);
    EXPECT_LE(ClipboardManager::DEFAULT_CLEAR_TIMEOUT, ClipboardManager::MAX_CLEAR_TIMEOUT);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    // Initialize GTK
    auto app = Gtk::Application::create("com.test.clipboard");

    return RUN_ALL_TESTS();
}
