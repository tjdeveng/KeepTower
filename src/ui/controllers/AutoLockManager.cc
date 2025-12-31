// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng
//
// AutoLockManager.cc - Implementation of automatic vault locking manager

#include "AutoLockManager.h"
#include "../../utils/Log.h"

namespace KeepTower {

AutoLockManager::AutoLockManager()
    : m_enabled(false),
      m_timeout_seconds(DEFAULT_TIMEOUT),
      m_timeout_connection(),
      m_signal_auto_lock() {
    Log::debug("AutoLockManager: Constructed with default timeout {} seconds", DEFAULT_TIMEOUT);
}

AutoLockManager::~AutoLockManager() {
    stop();
    Log::debug("AutoLockManager: Destroyed");
}

void AutoLockManager::set_enabled(bool enabled) {
    if (m_enabled == enabled) {
        return;  // No change
    }

    m_enabled = enabled;

    if (!m_enabled) {
        // Disable: stop any active timer
        stop();
        Log::info("AutoLockManager: Disabled");
    } else {
        Log::info("AutoLockManager: Enabled with timeout {} seconds", m_timeout_seconds);
    }
}

void AutoLockManager::set_timeout_seconds(int seconds) {
    // Clamp to valid range
    const int clamped = std::clamp(seconds, MIN_TIMEOUT, MAX_TIMEOUT);

    if (clamped != seconds) {
        Log::warning("AutoLockManager: Timeout {} seconds clamped to {} (valid range: {}-{})",
                  seconds, clamped, MIN_TIMEOUT, MAX_TIMEOUT);
    }

    // Only log if timeout actually changed
    if (m_timeout_seconds != clamped) {
        m_timeout_seconds = clamped;
        Log::info("AutoLockManager: Timeout changed to {} seconds", m_timeout_seconds);

        // If timer is active, restart with new timeout
        if (is_timer_active()) {
            reset_timer();
        }
    }
}

void AutoLockManager::reset_timer() {
    if (!m_enabled) {
        return;  // No-op when disabled
    }

    // Cancel previous timeout if exists
    if (m_timeout_connection.connected()) {
        m_timeout_connection.disconnect();
    }

    // Schedule new timeout
    m_timeout_connection = Glib::signal_timeout().connect(
        sigc::mem_fun(*this, &AutoLockManager::on_timeout),
        m_timeout_seconds * 1000  // Convert seconds to milliseconds
    );

    Log::debug("AutoLockManager: Timer reset, will trigger in {} seconds", m_timeout_seconds);
}

void AutoLockManager::stop() {
    if (m_timeout_connection.connected()) {
        m_timeout_connection.disconnect();
        Log::debug("AutoLockManager: Timer stopped");
    }
}

bool AutoLockManager::on_timeout() {
    Log::info("AutoLockManager: Auto-lock timeout triggered after {} seconds", m_timeout_seconds);

    // Emit signal to notify listeners
    m_signal_auto_lock.emit();

    // Return false to stop repeating (one-shot timer)
    return false;
}

}  // namespace KeepTower
