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

} // namespace KeepTower

#endif // STRING_HELPERS_H
