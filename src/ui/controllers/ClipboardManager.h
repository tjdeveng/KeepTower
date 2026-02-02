// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng
//
// ClipboardManager.h - Manages clipboard operations with auto-clear security
//
// Phase 1.3 Controller - Extracted from MainWindow

#pragma once

#include <gdkmm/clipboard.h>
#include <glibmm/main.h>
#include <sigc++/sigc++.h>
#include <string>
#include <algorithm>

namespace KeepTower {

/**
 * @brief Manages clipboard operations with automatic security clearing
 *
 * Responsibilities:
 * - Copy sensitive data (passwords, etc.) to system clipboard
 * - Automatically clear clipboard after configurable timeout
 * - Support configurable timeouts (5-300 seconds)
 * - Signal-based notification for clipboard events
 *
 * Design Philosophy:
 * - Security-first: Auto-clear prevents password lingering in clipboard
 * - User-friendly: Configurable timeout balances security vs convenience
 * - Signal-based: Loose coupling with UI
 * - Platform-agnostic: Uses Gdk::Clipboard abstraction
 *
 * Usage Example:
 * @code
 * auto clipboard_mgr = std::make_unique<ClipboardManager>(window.get_clipboard());
 * clipboard_mgr->set_clear_timeout_seconds(30);  // 30 seconds
 *
 * clipboard_mgr->signal_cleared().connect([]() {
 *     // Update status bar
 * });
 *
 * clipboard_mgr->copy_text("MySecurePassword123");
 * // Password will be cleared after 30 seconds
 * @endcode
 *
 * Security Considerations:
 * - Clipboard cleared on timeout (prevents password exposure)
 * - Cleared when manager is destroyed (RAII cleanup)
 * - Cleared when vault is closed/locked
 * - Does NOT prevent other applications from reading clipboard before timeout
 *
 * Thread Safety:
 * - All methods must be called from the GTK main thread
 * - Gdk::Clipboard is not thread-safe
 *
 * @since v0.3.0-beta (Phase 1.3)
 */
class ClipboardManager {
public:
    /// Minimum allowed clear timeout in seconds (5 seconds)
    static constexpr int MIN_CLEAR_TIMEOUT = 5;

    /// Maximum allowed clear timeout in seconds (5 minutes)
    static constexpr int MAX_CLEAR_TIMEOUT = 300;

    /// Default clear timeout in seconds (30 seconds)
    static constexpr int DEFAULT_CLEAR_TIMEOUT = 30;

    /**
     * @brief Construct ClipboardManager with Gdk::Clipboard
     * @param clipboard Clipboard instance from window (must not be null)
     *
     * @throws std::invalid_argument if clipboard is null
     * @post Timeout is set to DEFAULT_CLEAR_TIMEOUT
     * @post No active clear timer
     */
    explicit ClipboardManager(const Glib::RefPtr<Gdk::Clipboard>& clipboard);

    /**
     * @brief Destructor - clears clipboard and stops timer
     *
     * RAII cleanup:
     * - Clears clipboard if auto-clear is pending
     * - Stops clear timer
     */
    ~ClipboardManager();

    // Non-copyable, non-moveable (holds clipboard reference and timer)
    ClipboardManager(const ClipboardManager&) = delete;
    ClipboardManager& operator=(const ClipboardManager&) = delete;
    ClipboardManager(ClipboardManager&&) = delete;
    ClipboardManager& operator=(ClipboardManager&&) = delete;

    /**
     * @brief Copy text to clipboard with auto-clear
     * @param text Text to copy (typically password or sensitive data)
     *
     * Behavior:
     * 1. Copies text to system clipboard immediately
     * 2. Cancels any previous clear timer
     * 3. Schedules new clear timer based on current timeout
     * 4. Emits signal_copied()
     *
     * @post Text is in system clipboard
     * @post Clear timer is active
     */
    void copy_text(const std::string& text);

    /**
     * @brief Immediately clear clipboard
     *
     * Use when:
     * - Vault is closed
     * - Vault is locked
     * - User manually clears
     * - Application exits
     *
     * @post Clipboard is empty
     * @post Clear timer is stopped
     * @post Emits signal_cleared()
     */
    void clear_immediately();

    /**
     * @brief Set the auto-clear timeout duration
     * @param seconds Timeout in seconds (will be clamped to MIN_CLEAR_TIMEOUT..MAX_CLEAR_TIMEOUT)
     *
    * If a clear timer is active, it is not restarted (to avoid extending the clear time).
    * Copy again to start a new timer using the updated timeout.
     *
     * @post Timeout is set to clamped value
    * @post Active clear timer (if any) continues with its original timeout
     */
    void set_clear_timeout_seconds(int seconds);

    /**
     * @brief Get the current auto-clear timeout
     * @return Timeout in seconds (5-300)
     */
    [[nodiscard]] int get_clear_timeout_seconds() const noexcept { return m_clear_timeout_seconds; }

    /**
     * @brief Check if auto-clear timer is currently active
     * @return true if clipboard will be cleared automatically, false otherwise
     */
    [[nodiscard]] bool is_clear_pending() const noexcept { return m_clear_timeout_connection.connected(); }

    /**
     * @brief Signal emitted after text is copied to clipboard
     *
     * Signal Signature: void()
     *
     * This signal intentionally does not expose the copied text to
     * listeners to reduce the chance of sensitive data propagation.
     */
    [[nodiscard]] sigc::signal<void()>& signal_copied() { return m_signal_copied; }

    /**
     * @brief Signal emitted after clipboard is cleared
     *
     * Signal Signature: void()
     *
     * Use for:
     * - Updating status bar ("Clipboard cleared")
     * - Logging security event
     *
     * @return Signal reference for connection
     */
    [[nodiscard]] sigc::signal<void()>& signal_cleared() { return m_signal_cleared; }

    /**
     * @brief Enable clipboard preservation
     *
     * When enabled, the next call to clear_immediately() will be skipped,
     * allowing clipboard content to persist through vault close events.
     *
     * Use case: Preserve temporary password after copying so admin can
     * paste it when logging in as the new user.
     *
     * Preservation automatically disables when:
     * - User explicitly calls disable_preservation()
     * - Safety timeout expires (uses configured clipboard-timeout setting)
     *
     * The safety timeout uses the same timeout value as the normal clipboard
     * auto-clear (configured via GSettings/vault preferences). This ensures
     * consistent behavior and respects user preferences.
     *
     * @post Preservation is enabled
     * @post Safety timeout is scheduled
     * @note Does not affect auto-clear timer - that continues normally
     */
    void enable_preservation();

    /**
     * @brief Disable clipboard preservation
     *
     * Resumes normal clearing behavior. Call this after the preserved
     * content is no longer needed (e.g., after successful login).
     *
     * @post Preservation is disabled
     * @post Safety timeout is cancelled
     */
    void disable_preservation();

    /**
     * @brief Check if preservation is active
     * @return true if clear_immediately() will be skipped
     */
    [[nodiscard]] bool is_preservation_active() const noexcept { return m_preserve_on_close; }

private:
    /**
     * @brief Callback when clear timeout expires
     * @return false to stop repeating (one-shot timer)
     *
     * Internal implementation:
     * - Clears clipboard
     * - Emits signal_cleared()
     * - Returns false to disconnect
     */
    bool on_clear_timeout();

    Glib::RefPtr<Gdk::Clipboard> m_clipboard;  ///< System clipboard reference
    int m_clear_timeout_seconds;               ///< Clear timeout in seconds
    sigc::connection m_clear_timeout_connection;  ///< Active clear timer
    sigc::signal<void()> m_signal_copied;      ///< Copied signal
    sigc::signal<void()> m_signal_cleared;     ///< Cleared signal

    // Preservation state
    bool m_preserve_on_close{false};           ///< Skip next clear_immediately() call
    sigc::connection m_preservation_timeout;   ///< Safety timeout for preservation
};

}  // namespace KeepTower
