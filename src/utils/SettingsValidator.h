// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#ifndef SETTINGS_VALIDATOR_H
#define SETTINGS_VALIDATOR_H

#include <algorithm>
#include <string_view>
#include <giomm/settings.h>

/**
 * @brief Validates and enforces security constraints on GSettings values
 *
 * This class provides runtime validation to prevent tampering with the
 * GSettings schema file from bypassing security limits. Even if a user
 * modifies the schema file to allow insecure values, these validators
 * will clamp them to safe ranges at runtime.
 *
 * @note This is a static utility class and cannot be instantiated.
 */
class SettingsValidator final {
public:
    // Security constraint constants (C++23: inline static constexpr)
    static inline constexpr int MIN_CLIPBOARD_TIMEOUT{5};      // seconds
    static inline constexpr int MAX_CLIPBOARD_TIMEOUT{300};    // 5 minutes
    static inline constexpr int DEFAULT_CLIPBOARD_TIMEOUT{30};

    static inline constexpr int MIN_AUTO_LOCK_TIMEOUT{60};     // 1 minute
    static inline constexpr int MAX_AUTO_LOCK_TIMEOUT{3600};   // 1 hour
    static inline constexpr int DEFAULT_AUTO_LOCK_TIMEOUT{300}; // 5 minutes

    static inline constexpr int MIN_PASSWORD_HISTORY{1};
    static inline constexpr int MAX_PASSWORD_HISTORY{20};
    static inline constexpr int DEFAULT_PASSWORD_HISTORY{5};

    /**
     * @brief Get clipboard timeout with validation
     * @param settings GSettings instance (must not be null)
     * @return Validated clipboard timeout in seconds (5-300)
     * @note Thread-safe as it only reads from GSettings
     */
    [[nodiscard]] static int get_clipboard_timeout(const Glib::RefPtr<Gio::Settings>& settings) noexcept {
        const int value{settings->get_int("clipboard-clear-timeout")};
        return std::clamp(value, MIN_CLIPBOARD_TIMEOUT, MAX_CLIPBOARD_TIMEOUT);
    }

    /**
     * @brief Get auto-lock timeout with validation
     * @param settings GSettings instance (must not be null)
     * @return Validated auto-lock timeout in seconds (60-3600)
     * @note Thread-safe as it only reads from GSettings
     */
    [[nodiscard]] static int get_auto_lock_timeout(const Glib::RefPtr<Gio::Settings>& settings) noexcept {
        const int value{settings->get_int("auto-lock-timeout")};
        return std::clamp(value, MIN_AUTO_LOCK_TIMEOUT, MAX_AUTO_LOCK_TIMEOUT);
    }

    /**
     * @brief Get password history limit with validation
     * @param settings GSettings instance (must not be null)
     * @return Validated password history limit (1-20)
     * @note Thread-safe as it only reads from GSettings
     */
    [[nodiscard]] static int get_password_history_limit(const Glib::RefPtr<Gio::Settings>& settings) noexcept {
        const int value{settings->get_int("password-history-limit")};
        return std::clamp(value, MIN_PASSWORD_HISTORY, MAX_PASSWORD_HISTORY);
    }

    /**
     * @brief Check if auto-lock is enabled
     * @param settings GSettings instance (must not be null)
     * @return true if auto-lock is enabled
     * @note Thread-safe as it only reads from GSettings
     */
    [[nodiscard]] static bool is_auto_lock_enabled(const Glib::RefPtr<Gio::Settings>& settings) noexcept {
        return settings->get_boolean("auto-lock-enabled");
    }

    /**
     * @brief Check if password history is enabled
     * @param settings GSettings instance (must not be null)
     * @return true if password history tracking is enabled
     * @note Thread-safe as it only reads from GSettings
     */
    [[nodiscard]] static bool is_password_history_enabled(const Glib::RefPtr<Gio::Settings>& settings) noexcept {
        return settings->get_boolean("password-history-enabled");
    }

private:
    SettingsValidator() = delete;                                    // No instantiation
    ~SettingsValidator() = delete;                                   // No destruction
    SettingsValidator(const SettingsValidator&) = delete;            // No copy
    SettingsValidator& operator=(const SettingsValidator&) = delete; // No copy assignment
    SettingsValidator(SettingsValidator&&) = delete;                 // No move
    SettingsValidator& operator=(SettingsValidator&&) = delete;      // No move assignment
};

#endif // SETTINGS_VALIDATOR_H
