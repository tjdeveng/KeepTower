// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace KeepTower::FileDialogs {

inline std::string ext_from_glob_pattern(const std::string& pattern) {
    // Expected patterns are like "*.csv" or "*".
    if (pattern == "*" || pattern == "*.*") {
        return {};
    }
    if (pattern.starts_with("*.") && pattern.size() > 2) {
        return pattern.substr(1);  // ".csv"
    }
    return {};
}

inline bool ends_with_any(std::string_view text,
                          const std::vector<std::string>& suffixes,
                          std::string_view* matched_suffix = nullptr) {
    for (const auto& suffix : suffixes) {
        if (!suffix.empty() && text.size() >= suffix.size() && text.ends_with(suffix)) {
            if (matched_suffix) {
                *matched_suffix = suffix;
            }
            return true;
        }
    }
    return false;
}

inline bool last_segment_has_extension(std::string_view filename) {
    // Filename (no directory) heuristic: treat any '.' not at position 0 as an extension marker.
    const auto pos = filename.rfind('.');
    return pos != std::string_view::npos && pos != 0 && pos + 1 < filename.size();
}

inline std::string ensure_filename_extension(std::string filename,
                                            const std::string& desired_ext,
                                            const std::vector<std::string>& known_exts) {
    if (desired_ext.empty() || filename.empty()) {
        return filename;
    }

    if (std::string_view{filename}.ends_with(desired_ext)) {
        return filename;
    }

    std::string_view matched;
    if (ends_with_any(filename, known_exts, &matched)) {
        filename.resize(filename.size() - matched.size());
        filename += desired_ext;
        return filename;
    }

    if (!last_segment_has_extension(filename)) {
        filename += desired_ext;
    }

    return filename;
}

inline std::string ensure_path_extension(std::string path,
                                        const std::string& desired_ext,
                                        const std::vector<std::string>& known_exts) {
    if (desired_ext.empty() || path.empty()) {
        return path;
    }

    if (std::string_view{path}.ends_with(desired_ext)) {
        return path;
    }

    std::string_view matched;
    if (ends_with_any(path, known_exts, &matched)) {
        path.resize(path.size() - matched.size());
        path += desired_ext;
        return path;
    }

    // Append extension if path has no extension on the final segment.
    const auto slash = path.find_last_of('/');
    const std::string_view filename = (slash == std::string::npos)
                                          ? std::string_view{path}
                                          : std::string_view{path}.substr(static_cast<size_t>(slash) + 1);
    if (!last_segment_has_extension(filename)) {
        path += desired_ext;
    }

    return path;
}

}  // namespace KeepTower::FileDialogs
