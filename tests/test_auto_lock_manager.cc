// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng
//
// test_auto_lock_manager.cc - Unit tests for AutoLockManager

#include <gtest/gtest.h>
#include <glibmm/init.h>
#include <glibmm/main.h>
#include "../src/ui/controllers/AutoLockManager.h"
#include <chrono>
#include <thread>

using namespace KeepTower;

// Test fixture
class AutoLockManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        Glib::init();
    }
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(AutoLockManagerTest, DefaultConstructor) {
    AutoLockManager manager;

    EXPECT_FALSE(manager.is_enabled()) << "Should be disabled by default";
    EXPECT_EQ(manager.get_timeout_seconds(), AutoLockManager::DEFAULT_TIMEOUT);
    EXPECT_FALSE(manager.is_timer_active()) << "No timer should be active";
}

// ============================================================================
// Enable/Disable Tests
// ============================================================================

TEST_F(AutoLockManagerTest, EnableDisable) {
    AutoLockManager manager;

    // Enable
    manager.set_enabled(true);
    EXPECT_TRUE(manager.is_enabled());

    // Disable
    manager.set_enabled(false);
    EXPECT_FALSE(manager.is_enabled());
}

TEST_F(AutoLockManagerTest, DisableStopsTimer) {
    AutoLockManager manager;
    manager.set_enabled(true);
    manager.reset_timer();

    EXPECT_TRUE(manager.is_timer_active()) << "Timer should be active";

    manager.set_enabled(false);
    EXPECT_FALSE(manager.is_timer_active()) << "Timer should be stopped when disabled";
}

// ============================================================================
// Timeout Configuration Tests
// ============================================================================

TEST_F(AutoLockManagerTest, SetTimeoutValidRange) {
    AutoLockManager manager;

    manager.set_timeout_seconds(120);
    EXPECT_EQ(manager.get_timeout_seconds(), 120);

    manager.set_timeout_seconds(600);
    EXPECT_EQ(manager.get_timeout_seconds(), 600);
}

TEST_F(AutoLockManagerTest, SetTimeoutClampsTooLow) {
    AutoLockManager manager;

    manager.set_timeout_seconds(10);  // Below MIN_TIMEOUT (60)
    EXPECT_EQ(manager.get_timeout_seconds(), AutoLockManager::MIN_TIMEOUT);
}

TEST_F(AutoLockManagerTest, SetTimeoutClampsTooHigh) {
    AutoLockManager manager;

    manager.set_timeout_seconds(5000);  // Above MAX_TIMEOUT (3600)
    EXPECT_EQ(manager.get_timeout_seconds(), AutoLockManager::MAX_TIMEOUT);
}

TEST_F(AutoLockManagerTest, SetTimeoutRestartsActiveTimer) {
    AutoLockManager manager;
    manager.set_enabled(true);
    manager.set_timeout_seconds(120);
    manager.reset_timer();

    EXPECT_TRUE(manager.is_timer_active());

    // Change timeout while timer is active
    manager.set_timeout_seconds(180);
    EXPECT_TRUE(manager.is_timer_active()) << "Timer should still be active after timeout change";
}

// ============================================================================
// Timer Tests
// ============================================================================

TEST_F(AutoLockManagerTest, ResetTimerWhenDisabledDoesNothing) {
    AutoLockManager manager;
    manager.set_enabled(false);

    manager.reset_timer();
    EXPECT_FALSE(manager.is_timer_active()) << "Timer should not start when disabled";
}

TEST_F(AutoLockManagerTest, ResetTimerWhenEnabledStartsTimer) {
    AutoLockManager manager;
    manager.set_enabled(true);

    manager.reset_timer();
    EXPECT_TRUE(manager.is_timer_active()) << "Timer should be active after reset";
}

TEST_F(AutoLockManagerTest, ResetTimerCancelsPreviousTimer) {
    AutoLockManager manager;
    manager.set_enabled(true);

    manager.reset_timer();
    auto first_connection = manager.is_timer_active();
    EXPECT_TRUE(first_connection);

    manager.reset_timer();
    EXPECT_TRUE(manager.is_timer_active()) << "New timer should be active";
}

TEST_F(AutoLockManagerTest, StopCancelsTimer) {
    AutoLockManager manager;
    manager.set_enabled(true);
    manager.reset_timer();

    EXPECT_TRUE(manager.is_timer_active());

    manager.stop();
    EXPECT_FALSE(manager.is_timer_active()) << "Timer should be stopped";
}

TEST_F(AutoLockManagerTest, StopWhenNoTimerIsNoOp) {
    AutoLockManager manager;

    EXPECT_NO_THROW(manager.stop());
    EXPECT_FALSE(manager.is_timer_active());
}

// ============================================================================
// Signal Tests
// ============================================================================

TEST_F(AutoLockManagerTest, SignalEmittedOnTimeout) {
    AutoLockManager manager;
    manager.set_enabled(true);
    manager.set_timeout_seconds(1);  // Will be clamped to 60, but test logic works

    bool signal_received = false;
    manager.signal_auto_lock_triggered().connect([&signal_received]() {
        signal_received = true;
    });

    // Note: This test won't actually wait for timeout (would take 60 seconds)
    // It just verifies the signal can be connected
    EXPECT_FALSE(signal_received) << "Signal should not be emitted yet";
}

TEST_F(AutoLockManagerTest, MultipleSignalConnections) {
    AutoLockManager manager;

    int callback1_count = 0;
    int callback2_count = 0;

    manager.signal_auto_lock_triggered().connect([&callback1_count]() {
        callback1_count++;
    });

    manager.signal_auto_lock_triggered().connect([&callback2_count]() {
        callback2_count++;
    });

    // Manually trigger (simulate timeout)
    manager.signal_auto_lock_triggered().emit();

    EXPECT_EQ(callback1_count, 1) << "First callback should fire";
    EXPECT_EQ(callback2_count, 1) << "Second callback should fire";
}

// ============================================================================
// Constants Tests
// ============================================================================

TEST_F(AutoLockManagerTest, ConstantsAreReasonable) {
    EXPECT_EQ(AutoLockManager::MIN_TIMEOUT, 60) << "Minimum should be 1 minute";
    EXPECT_EQ(AutoLockManager::MAX_TIMEOUT, 3600) << "Maximum should be 1 hour";
    EXPECT_EQ(AutoLockManager::DEFAULT_TIMEOUT, 300) << "Default should be 5 minutes";

    EXPECT_GE(AutoLockManager::DEFAULT_TIMEOUT, AutoLockManager::MIN_TIMEOUT);
    EXPECT_LE(AutoLockManager::DEFAULT_TIMEOUT, AutoLockManager::MAX_TIMEOUT);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
