// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng
//
// AutoLockManager.h - Manages automatic vault locking after inactivity
//
// Phase 1.3 Controller - Extracted from MainWindow

#pragma once

#include <glibmm/main.h>
#include <sigc++/sigc++.h>
#include <algorithm>
#include <cstdint>

namespace KeepTower {

/**
 * @brief Manages automatic vault locking after configurable inactivity timeout
 *
 * Responsibilities:
 * - Track user activity and reset inactivity timer
 * - Schedule auto-lock after configured timeout
 * - Support configurable timeouts (60-3600 seconds)
 * - Signal-based notification for auto-lock events
 * - Can be enabled/disabled dynamically
 *
 * Design Philosophy:
 * - Security-first: Conservative defaults, strict validation
 * - Stateless: No dependency on UI widgets
 * - Signal-based: Loose coupling with MainWindow
 * - Testable: Can be tested without GTK event loop
 *
 * Usage Example:
 * @code
 * auto manager = std::make_unique<AutoLockManager>();
 * manager->set_enabled(true);
 * manager->set_timeout_seconds(300);  // 5 minutes
 *
 * manager->signal_auto_lock_triggered().connect([]() {
 *     // Lock the vault
 * });
 *
 * // On user activity (clicks, typing, etc.):
 * manager->reset_timer();
 * @endcode
 *
 * Thread Safety:
 * - All methods must be called from the GTK main thread
 * - Uses Glib::signal_timeout() which is thread-safe
 * - Signals are emitted on the main thread
 *
 * @since v0.3.0-beta (Phase 1.3)
 */
class AutoLockManager {
public:
    /// Minimum allowed timeout in seconds (1 minute)
    static constexpr int MIN_TIMEOUT = 60;

    /// Maximum allowed timeout in seconds (1 hour)
    static constexpr int MAX_TIMEOUT = 3600;

    /// Default timeout in seconds (5 minutes)
    static constexpr int DEFAULT_TIMEOUT = 300;

    /**
     * @brief Construct AutoLockManager with default settings
     *
     * Initial state:
     * - Disabled (must call set_enabled(true))
     * - Timeout: 300 seconds (5 minutes)
     * - No active timer
     */
    AutoLockManager();

    /**
     * @brief Destructor - stops any active timer
     */
    ~AutoLockManager();

    // Non-copyable, non-moveable (holds active timer connection)
    AutoLockManager(const AutoLockManager&) = delete;
    AutoLockManager& operator=(const AutoLockManager&) = delete;
    AutoLockManager(AutoLockManager&&) = delete;
    AutoLockManager& operator=(AutoLockManager&&) = delete;

    /**
     * @brief Enable or disable auto-lock functionality
     * @param enabled true to enable, false to disable
     *
     * When disabled:
     * - Stops any active timer
     * - reset_timer() becomes a no-op
     * - No auto-lock signals will be emitted
     *
     * @post If disabled, active timer is disconnected
     */
    void set_enabled(bool enabled);

    /**
     * @brief Check if auto-lock is currently enabled
     * @return true if enabled, false otherwise
     */
    [[nodiscard]] bool is_enabled() const noexcept { return m_enabled; }

    /**
     * @brief Set the inactivity timeout duration
     * @param seconds Timeout in seconds (will be clamped to MIN_TIMEOUT..MAX_TIMEOUT)
     *
     * If a timer is active, it will be restarted with the new timeout.
     *
     * @post Timeout is set to clamped value
     * @post Active timer (if any) is restarted with new timeout
     */
    void set_timeout_seconds(int seconds);

    /**
     * @brief Get the current timeout setting
     * @return Timeout in seconds (60-3600)
     */
    [[nodiscard]] int get_timeout_seconds() const noexcept { return m_timeout_seconds; }

    /**
     * @brief Reset the inactivity timer (call on user activity)
     *
     * Should be called whenever the user interacts with the application:
     * - Mouse clicks
     * - Keyboard input
     * - Scrolling
     *
     * If auto-lock is disabled, this is a no-op.
     *
     * @post If enabled, timer is restarted from zero
     * @post Previous timer (if any) is cancelled
     */
    void reset_timer();

    /**
     * @brief Stop the auto-lock timer
     *
     * Use when:
     * - Vault is closed
     * - Vault is already locked
     * - Application is being destroyed
     *
     * @post Active timer (if any) is disconnected
     */
    void stop();

    /**
     * @brief Check if a timer is currently active
     * @return true if countdown is active, false otherwise
     */
    [[nodiscard]] bool is_timer_active() const noexcept { return m_timeout_connection.connected(); }

    /**
     * @brief Signal emitted when auto-lock timeout expires
     *
     * The connected handler should:
     * 1. Save any unsaved changes
     * 2. Lock the vault (or logout for V2)
     * 3. Stop this timer (by calling stop())
     *
     * Signal Signature: void()
     *
     * @return Signal reference for connection
     */
    [[nodiscard]] sigc::signal<void()>& signal_auto_lock_triggered() { return m_signal_auto_lock; }

private:
    /**
     * @brief Callback when timeout expires
     * @return false to stop repeating (one-shot timer)
     *
     * Internal implementation:
     * - Emits signal_auto_lock_triggered()
     * - Returns false to disconnect
     */
    bool on_timeout();

    bool m_enabled;                     ///< Whether auto-lock is enabled
    int m_timeout_seconds;              ///< Timeout duration in seconds
    sigc::connection m_timeout_connection;  ///< Active timer connection
    sigc::signal<void()> m_signal_auto_lock;  ///< Signal when auto-lock triggers
};

}  // namespace KeepTower
