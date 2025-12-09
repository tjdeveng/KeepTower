// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#ifndef SETTINGS_VALIDATOR_H
#define SETTINGS_VALIDATOR_H

#include <algorithm>
#include <giomm/settings.h>

/**
 * @brief Validates and enforces security constraints on GSettings values
 *
 * This class provides runtime validation to prevent tampering with the
 * GSettings schema file from bypassing security limits. Even if a user
 * modifies the schema file to allow insecure values, these validators
 * will clamp them to safe ranges at runtime.
 */
class SettingsValidator {
public:
    // Security constraint constants
    static constexpr int MIN_CLIPBOARD_TIMEOUT = 5;      // seconds
    static constexpr int MAX_CLIPBOARD_TIMEOUT = 300;    // 5 minutes
    static constexpr int DEFAULT_CLIPBOARD_TIMEOUT = 30;

    static constexpr int MIN_AUTO_LOCK_TIMEOUT = 60;     // 1 minute
    static constexpr int MAX_AUTO_LOCK_TIMEOUT = 3600;   // 1 hour
    static constexpr int DEFAULT_AUTO_LOCK_TIMEOUT = 300; // 5 minutes

    static constexpr int MIN_PASSWORD_HISTORY = 1;
    static constexpr int MAX_PASSWORD_HISTORY = 20;
    static constexpr int DEFAULT_PASSWORD_HISTORY = 5;

    /**
     * @brief Get clipboard timeout with validation
     * @param settings GSettings instance
     * @return Validated clipboard timeout in seconds (5-300)
     */
    static int get_clipboard_timeout(const Glib::RefPtr<Gio::Settings>& settings) {
        const int value = settings->get_int("clipboard-clear-timeout");
        return std::clamp(value, MIN_CLIPBOARD_TIMEOUT, MAX_CLIPBOARD_TIMEOUT);
    }

    /**
     * @brief Get auto-lock timeout with validation
     * @param settings GSettings instance
     * @return Validated auto-lock timeout in seconds (60-3600)
     */
    static int get_auto_lock_timeout(const Glib::RefPtr<Gio::Settings>& settings) {
        const int value = settings->get_int("auto-lock-timeout");
        return std::clamp(value, MIN_AUTO_LOCK_TIMEOUT, MAX_AUTO_LOCK_TIMEOUT);
    }

    /**
     * @brief Get password history limit with validation
     * @param settings GSettings instance
     * @return Validated password history limit (1-20)
     */
    static int get_password_history_limit(const Glib::RefPtr<Gio::Settings>& settings) {
        const int value = settings->get_int("password-history-limit");
        return std::clamp(value, MIN_PASSWORD_HISTORY, MAX_PASSWORD_HISTORY);
    }

    /**
     * @brief Check if auto-lock is enabled
     * @param settings GSettings instance
     * @return true if auto-lock is enabled
     */
    static bool is_auto_lock_enabled(const Glib::RefPtr<Gio::Settings>& settings) {
        return settings->get_boolean("auto-lock-enabled");
    }

    /**
     * @brief Check if password history is enabled
     * @param settings GSettings instance
     * @return true if password history tracking is enabled
     */
    static bool is_password_history_enabled(const Glib::RefPtr<Gio::Settings>& settings) {
        return settings->get_boolean("password-history-enabled");
    }

private:
    SettingsValidator() = delete;  // Static class, no instantiation
};

#endif // SETTINGS_VALIDATOR_H
