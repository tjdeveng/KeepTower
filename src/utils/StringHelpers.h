// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file StringHelpers.h
 * @brief String conversion and validation utilities
 */

#ifndef STRING_HELPERS_H
#define STRING_HELPERS_H

#include <glibmm/ustring.h>
#include <string>
#include <string_view>

#include <glib.h>
#include "Log.h"

namespace KeepTower {

/**
 * @brief Safely convert Glib::ustring to std::string with UTF-8 validation
 *
 * @param ustr The Glib::ustring to convert
 * @param field_name Optional field name for error logging
 * @return std::string The converted string, or empty string if validation fails
 */
inline std::string safe_ustring_to_string(const Glib::ustring& ustr, const char* field_name = "field") {
    if (ustr.empty()) {
        return {};
    }

    // Validate UTF-8 encoding
    if (!ustr.validate()) {
        Log::warning("Invalid UTF-8 detected in {} - discarding invalid data", field_name);
        // Return empty to be safe
        return {};
    }

    return ustr.raw();
}

/**
 * @brief Ensure a string is valid UTF-8 for safe UI display
 *
 * Linux filenames and other external inputs can contain non-UTF-8 byte sequences.
 * gtkmm widgets expect valid UTF-8, and will error/warn on invalid sequences.
 *
 * This returns a valid UTF-8 string, replacing invalid sequences with U+FFFD.
 * @param text Source byte sequence to sanitize.
 * @param field_name Optional field label used in warning logs.
 * @return Valid UTF-8 string safe for UI display.
 */
inline std::string make_valid_utf8(std::string_view text, const char* field_name = "text") {
    if (text.empty()) {
        return {};
    }

    if (g_utf8_validate(text.data(), static_cast<gssize>(text.size()), nullptr)) {
        return std::string{text};
    }

    Log::warning("Invalid UTF-8 detected in {} - sanitizing for display", field_name);
    gchar* valid = g_utf8_make_valid(text.data(), static_cast<gssize>(text.size()));
    if (!valid) {
        return {};
    }

    std::string out{valid};
    g_free(valid);
    return out;
}

} // namespace KeepTower

#endif // STRING_HELPERS_H
