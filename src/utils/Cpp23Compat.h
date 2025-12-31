// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 TJDev

#ifndef KEEPTOWER_CPP23_COMPAT_H
#define KEEPTOWER_CPP23_COMPAT_H

/**
 * @file Cpp23Compat.h
 * @brief C++23 compatibility layer for GCC 13/14+ differences
 *
 * Provides feature detection and compatibility helpers for C++23 features
 * that have varying support across compiler versions. Ubuntu 22.04/24.04 LTS
 * ships with GCC 13, while newer systems have GCC 14+ with better C++23 support.
 *
 * @section features Detected Features
 * - KEEPTOWER_HAS_RANGES: std::ranges support (GCC 13+)
 * - KEEPTOWER_HAS_FULL_FORMAT: Complete std::format support (GCC 14+)
 * - KEEPTOWER_HAS_CONSTEXPR_STRING: constexpr std::string (GCC 13+)
 */

#include <version>
#include <cstddef>  // For size_t

// Detect GCC version
#ifdef __GNUC__
#define KEEPTOWER_GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#else
#define KEEPTOWER_GCC_VERSION 0
#endif

// Feature detection macros

/**
 * @def KEEPTOWER_HAS_RANGES
 * @brief std::ranges support (available in GCC 13+)
 */
#if defined(__cpp_lib_ranges) && __cpp_lib_ranges >= 201911L
#define KEEPTOWER_HAS_RANGES 1
#else
#define KEEPTOWER_HAS_RANGES 0
#endif

/**
 * @def KEEPTOWER_HAS_FULL_FORMAT
 * @brief Complete std::format support for all types (GCC 14+)
 *
 * GCC 13 has partial std::format but struggles with Glib::ustring and
 * temporary conversions. GCC 14+ has more complete support.
 */
#if KEEPTOWER_GCC_VERSION >= 140000
#define KEEPTOWER_HAS_FULL_FORMAT 1
#else
#define KEEPTOWER_HAS_FULL_FORMAT 0
#endif

/**
 * @def KEEPTOWER_HAS_CONSTEXPR_STRING
 * @brief constexpr std::string support (GCC 13+)
 */
#if defined(__cpp_lib_constexpr_string) && __cpp_lib_constexpr_string >= 201907L
#define KEEPTOWER_HAS_CONSTEXPR_STRING 1
#else
#define KEEPTOWER_HAS_CONSTEXPR_STRING 0
#endif

/**
 * @def KEEPTOWER_HAS_CONSTEXPR_VECTOR
 * @brief constexpr std::vector support (GCC 13+)
 */
#if defined(__cpp_lib_constexpr_vector) && __cpp_lib_constexpr_vector >= 201907L
#define KEEPTOWER_HAS_CONSTEXPR_VECTOR 1
#else
#define KEEPTOWER_HAS_CONSTEXPR_VECTOR 0
#endif

// Compatibility helpers

/**
 * @namespace KeepTower::compat
 * @brief Compatibility utilities for C++23 features
 */
namespace KeepTower::compat {

/**
 * @brief Safe integer to size_t conversion with bounds checking
 *
 * Protobuf uses int for size(), but we prefer size_t for safety.
 * This helper provides explicit conversion with compile-time checks.
 *
 * @tparam T Integer type (typically int from protobuf)
 * @param value The integer value to convert
 * @return size_t The converted value
 * @note Returns 0 if value is negative
 */
template<typename T>
[[nodiscard]] constexpr size_t to_size(T value) noexcept {
    if (value < 0) return 0;
    return static_cast<size_t>(value);
}

/**
 * @brief Check if index is within bounds
 *
 * @tparam T Integer type (typically int from protobuf)
 * @param index Index to check
 * @param size Container size
 * @return true if index is valid, false otherwise
 */
template<typename T>
[[nodiscard]] constexpr bool is_valid_index(size_t index, T size) noexcept {
    if (size < 0) return false;
    return index < static_cast<size_t>(size);
}

} // namespace KeepTower::compat

#endif // KEEPTOWER_CPP23_COMPAT_H
