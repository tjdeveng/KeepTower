// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng
//
// ClipboardManager.cc - Implementation of clipboard manager with auto-clear

#include "ClipboardManager.h"
#include "../../utils/Log.h"
#include <stdexcept>

namespace KeepTower {

ClipboardManager::ClipboardManager(const Glib::RefPtr<Gdk::Clipboard>& clipboard)
    : m_clipboard(clipboard),
      m_clear_timeout_seconds(DEFAULT_CLEAR_TIMEOUT),
      m_clear_timeout_connection(),
      m_signal_copied(),
      m_signal_cleared() {
    if (!m_clipboard) {
        throw std::invalid_argument("ClipboardManager: clipboard cannot be null");
    }
    Log::debug("ClipboardManager: Constructed with default timeout {} seconds", DEFAULT_CLEAR_TIMEOUT);
}

ClipboardManager::~ClipboardManager() {
    // RAII cleanup: clear clipboard if auto-clear is pending
    if (is_clear_pending()) {
        Log::info("ClipboardManager: Clearing clipboard on destruction");
        m_clipboard->set_text("");
    }

    // Stop timer
    if (m_clear_timeout_connection.connected()) {
        m_clear_timeout_connection.disconnect();
    }

    Log::debug("ClipboardManager: Destroyed");
}

void ClipboardManager::copy_text(const std::string& text) {
    if (!m_clipboard) {
        Log::error("ClipboardManager: Cannot copy, clipboard is null");
        return;
    }

    // Copy to clipboard immediately
    m_clipboard->set_text(text);

    // Cancel previous clear timer if exists
    if (m_clear_timeout_connection.connected()) {
        m_clear_timeout_connection.disconnect();
    }

    // Schedule auto-clear
    m_clear_timeout_connection = Glib::signal_timeout().connect(
        sigc::mem_fun(*this, &ClipboardManager::on_clear_timeout),
        m_clear_timeout_seconds * 1000  // Convert seconds to milliseconds
    );

    Log::info("ClipboardManager: Text copied, will clear in {} seconds", m_clear_timeout_seconds);

    // Emit copied signal (do not expose clipboard contents)
    m_signal_copied.emit();
}

void ClipboardManager::clear_immediately() {
    if (!m_clipboard) {
        Log::error("ClipboardManager: Cannot clear, clipboard is null");
        return;
    }

    // Check preservation flag
    if (m_preserve_on_close) {
        Log::info("ClipboardManager: Skipping clear (preservation active)");
        m_preserve_on_close = false;  // One-time skip
        return;
    }

    // Stop timer
    if (m_clear_timeout_connection.connected()) {
        m_clear_timeout_connection.disconnect();
    }

    // Clear clipboard
    m_clipboard->set_text("");
    Log::info("ClipboardManager: Clipboard cleared immediately");

    // Emit cleared signal
    m_signal_cleared.emit();
}

void ClipboardManager::set_clear_timeout_seconds(int seconds) {
    // Clamp to valid range
    const int clamped = std::clamp(seconds, MIN_CLEAR_TIMEOUT, MAX_CLEAR_TIMEOUT);

    if (clamped != seconds) {
        Log::warning("ClipboardManager: Timeout {} seconds clamped to {} (valid range: {}-{})",
                  seconds, clamped, MIN_CLEAR_TIMEOUT, MAX_CLEAR_TIMEOUT);
    }

    m_clear_timeout_seconds = clamped;
    Log::info("ClipboardManager: Clear timeout set to {} seconds", m_clear_timeout_seconds);

    // If clear timer is active, restart with new timeout
    if (is_clear_pending()) {
        // Note: We don't restart automatically because that would extend the clear time
        // If user wants to restart, they should copy again
        Log::debug("ClipboardManager: Clear timer still active with old timeout");
    }
}

bool ClipboardManager::on_clear_timeout() {
    Log::info("ClipboardManager: Auto-clear timeout triggered after {} seconds", m_clear_timeout_seconds);

    if (!m_clipboard) {
        Log::error("ClipboardManager: Cannot clear, clipboard is null");
        return false;
    }

    // Clear clipboard
    m_clipboard->set_text("");

    // Emit cleared signal
    m_signal_cleared.emit();

    // Return false to stop repeating (one-shot timer)
    return false;
}

void ClipboardManager::enable_preservation() {
    if (m_preserve_on_close) {
        Log::warning("ClipboardManager: Preservation already enabled");
        return;
    }

    m_preserve_on_close = true;

    // Cancel any existing preservation timeout
    if (m_preservation_timeout.connected()) {
        m_preservation_timeout.disconnect();
    }

    // Set safety timeout using configured clear timeout
    m_preservation_timeout = Glib::signal_timeout().connect(
        [this]() -> bool {
            Log::warning("ClipboardManager: Preservation safety timeout expired after {} seconds",
                        m_clear_timeout_seconds);
            m_preserve_on_close = false;
            return false;  // One-shot
        },
        m_clear_timeout_seconds * 1000  // Use configured timeout
    );

    Log::info("ClipboardManager: Preservation enabled with {}s safety timeout", m_clear_timeout_seconds);
}

void ClipboardManager::disable_preservation() {
    if (!m_preserve_on_close) {
        Log::debug("ClipboardManager: Preservation already disabled");
        return;
    }

    m_preserve_on_close = false;

    // Cancel safety timeout
    if (m_preservation_timeout.connected()) {
        m_preservation_timeout.disconnect();
    }

    Log::info("ClipboardManager: Preservation disabled");
}

}  // namespace KeepTower
