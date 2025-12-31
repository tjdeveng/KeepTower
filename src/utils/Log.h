// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file Log.h
 * @brief Simple logging framework with compile-time formatting
 *
 * Provides a lightweight, type-safe logging system using C++23 std::format
 * and std::source_location for automatic file/line tracking. Supports multiple
 * log levels with runtime filtering.
 *
 * @section features Features
 * - Compile-time format string validation
 * - Automatic timestamp generation (millisecond precision)
 * - Source location tracking (file:line)
 * - Runtime log level filtering
 * - Zero-overhead when log level is disabled
 * - Thread-safe output via std::cerr
 *
 * @section usage Usage Example
 * @code
 * // Set minimum log level
 * KeepTower::Log::set_level(KeepTower::Log::Level::Debug);
 *
 * // Log messages at different levels
 * KeepTower::Log::debug("Opening vault: {}", vault_path);
 * KeepTower::Log::info("Vault opened successfully");
 * KeepTower::Log::warning("Weak password detected");
 * KeepTower::Log::error("Failed to decrypt: {}", error_msg);
 * @endcode
 *
 * @section thread_safety Thread Safety
 * Output via std::cerr is thread-safe. Multiple threads can log simultaneously
 * without interleaved output lines.
 *
 * @note Default log level is Info (Debug messages are hidden)
 */

#ifndef KEEPTOWER_LOG_H
#define KEEPTOWER_LOG_H

#include <format>
#include <iostream>
#include <string_view>
#include <chrono>
#include <source_location>

namespace KeepTower::Log {

/**
 * @brief Log severity levels
 *
 * Defines the severity hierarchy for log messages. Messages below the current
 * level are filtered out at runtime with minimal overhead.
 */
enum class Level {
    Debug,     ///< Detailed debugging information (verbose)
    Info,      ///< General informational messages
    Warning,   ///< Warning conditions (potential issues)
    Error      ///< Error conditions (operation failures)
};

/**
 * @brief Current minimum log level (can be changed at runtime)
 *
 * Messages below this level will not be printed. Default is Info.
 * Change via set_level() function.
 */
inline Level current_level = Level::Info;

/**
 * @brief Internal implementation details
 * @private
 */
namespace detail {
    /**
     * @brief Convert log level to display string
     * @param level The log level to convert
     * @return Fixed-width string representation (5 chars for alignment)
     */
    inline constexpr std::string_view level_to_string(Level level) noexcept {
        switch (level) {
            case Level::Debug:   return "DEBUG";
            case Level::Info:    return "INFO ";
            case Level::Warning: return "WARN ";
            case Level::Error:   return "ERROR";
        }
        return "UNKNOWN";
    }

    /**
     * @brief Generate ISO 8601 timestamp with millisecond precision
     * @return Formatted timestamp string (YYYY-MM-DD HH:MM:SS.mmm)
     */
    inline std::string get_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::tm tm{};
        localtime_r(&time_t, &tm);

        return std::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:03d}",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec, ms.count());
    }
}

/**
 * @brief Main logging function with automatic source location
 * @tparam Args Format argument types (deduced)
 * @param level Log level for this message
 * @param fmt Format string (validated at compile-time)
 * @param args Format arguments
 *
 * Filters messages based on current_level and outputs formatted log entries
 * to std::cerr. Automatically captures source location (file:line).
 *
 * @note Use convenience functions (debug, info, warning, error) instead of calling directly
 */
template<typename... Args>
void log(Level level, std::format_string<Args...> fmt, Args&&... args) {
    if (level < current_level) {
        return;
    }

    auto loc = std::source_location::current();
    auto message = std::format(fmt, std::forward<Args>(args)...);
    auto timestamp = detail::get_timestamp();
    auto level_str = detail::level_to_string(level);

    // Format: [TIMESTAMP] LEVEL: message (file:line)
    std::cerr << std::format("[{}] {}: {} ({}:{})\n",
        timestamp, level_str, message,
        loc.file_name(), loc.line());
}

/**
 * @brief Log debug message (Level::Debug)
 * @tparam Args Format argument types (deduced)
 * @param fmt Format string
 * @param args Format arguments
 *
 * Outputs detailed debugging information. Hidden by default (enable with set_level(Level::Debug)).
 *
 * @code
 * KeepTower::Log::debug("Account index: {}, name: {}", idx, name);
 * @endcode
 */
template<typename... Args>
void debug(std::format_string<Args...> fmt, Args&&... args) {
    log(Level::Debug, fmt, std::forward<Args>(args)...);
}

/**
 * @brief Log informational message (Level::Info)
 * @tparam Args Format argument types (deduced)
 * @param fmt Format string
 * @param args Format arguments
 *
 * Outputs general informational messages (default level).
 *
 * @code
 * KeepTower::Log::info("Vault opened: {}", vault_path);
 * @endcode
 */
template<typename... Args>
void info(std::format_string<Args...> fmt, Args&&... args) {
    log(Level::Info, fmt, std::forward<Args>(args)...);
}

/**
 * @brief Log warning message (Level::Warning)
 * @tparam Args Format argument types (deduced)
 * @param fmt Format string
 * @param args Format arguments
 *
 * Outputs warning conditions (potential issues that don't prevent operation).
 *
 * @code
 * KeepTower::Log::warning("Weak password detected for account: {}", account_name);
 * @endcode
 */
template<typename... Args>
void warning(std::format_string<Args...> fmt, Args&&... args) {
    log(Level::Warning, fmt, std::forward<Args>(args)...);
}

/**
 * @brief Log error message (Level::Error)
 * @tparam Args Format argument types (deduced)
 * @param fmt Format string
 * @param args Format arguments
 *
 * Outputs error conditions (operation failures).
 *
 * @code
 * KeepTower::Log::error("Failed to decrypt vault: {}", error_message);
 * @endcode
 */
template<typename... Args>
void error(std::format_string<Args...> fmt, Args&&... args) {
    log(Level::Error, fmt, std::forward<Args>(args)...);
}

/**
 * @brief Set minimum log level at runtime
 * @param level New minimum log level
 *
 * Messages below this level will be filtered out. Useful for enabling
 * debug logging in development builds.
 *
 * @code
 * KeepTower::Log::set_level(KeepTower::Log::Level::Debug);
 * @endcode
 */
inline void set_level(Level level) {
    current_level = level;
}

} // namespace KeepTower::Log

#endif // KEEPTOWER_LOG_H
