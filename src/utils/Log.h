// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng
//
// Log.h - Simple logging framework with levels
// C++23 std::format-based logging

#ifndef KEEPTOWER_LOG_H
#define KEEPTOWER_LOG_H

#include <format>
#include <iostream>
#include <string_view>
#include <chrono>
#include <source_location>

namespace KeepTower::Log {

enum class Level {
    Debug,
    Info,
    Warning,
    Error
};

// Current log level (can be changed at runtime)
inline Level current_level = Level::Info;

namespace detail {
    inline constexpr std::string_view level_to_string(Level level) noexcept {
        switch (level) {
            case Level::Debug:   return "DEBUG";
            case Level::Info:    return "INFO ";
            case Level::Warning: return "WARN ";
            case Level::Error:   return "ERROR";
        }
        return "UNKNOWN";
    }

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

// Main logging function
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

// Convenience functions for different log levels
template<typename... Args>
void debug(std::format_string<Args...> fmt, Args&&... args) {
    log(Level::Debug, fmt, std::forward<Args>(args)...);
}

template<typename... Args>
void info(std::format_string<Args...> fmt, Args&&... args) {
    log(Level::Info, fmt, std::forward<Args>(args)...);
}

template<typename... Args>
void warning(std::format_string<Args...> fmt, Args&&... args) {
    log(Level::Warning, fmt, std::forward<Args>(args)...);
}

template<typename... Args>
void error(std::format_string<Args...> fmt, Args&&... args) {
    log(Level::Error, fmt, std::forward<Args>(args)...);
}// Set log level at runtime
inline void set_level(Level level) {
    current_level = level;
}

} // namespace KeepTower::Log

#endif // KEEPTOWER_LOG_H
