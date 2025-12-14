// FuzzyMatch.h - Fuzzy string matching utilities
// Copyright (C) 2025 KeepTower Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace KeepTower::FuzzyMatch {

/// @brief Calculate Levenshtein distance between two strings
/// @param s1 First string
/// @param s2 Second string
/// @return Edit distance (lower = more similar)
inline int levenshtein_distance(std::string_view s1, std::string_view s2) {
    const size_t len1 = s1.size();
    const size_t len2 = s2.size();

    // Optimization: if one string is empty, distance is the length of the other
    if (len1 == 0) return static_cast<int>(len2);
    if (len2 == 0) return static_cast<int>(len1);

    // Use two rows for space optimization (instead of full matrix)
    std::vector<int> prev_row(len2 + 1);
    std::vector<int> curr_row(len2 + 1);

    // Initialize first row
    for (size_t i = 0; i <= len2; ++i) {
        prev_row[i] = static_cast<int>(i);
    }

    // Calculate distances
    for (size_t i = 0; i < len1; ++i) {
        curr_row[0] = static_cast<int>(i + 1);

        for (size_t j = 0; j < len2; ++j) {
            const int cost = (std::tolower(s1[i]) == std::tolower(s2[j])) ? 0 : 1;

            curr_row[j + 1] = std::min({
                curr_row[j] + 1,      // Insertion
                prev_row[j + 1] + 1,  // Deletion
                prev_row[j] + cost    // Substitution
            });
        }

        std::swap(prev_row, curr_row);
    }

    return prev_row[len2];
}

/// @brief Calculate fuzzy match score (0-100, higher = better match)
/// @param query Search query
/// @param target Target string to match against
/// @return Match score (0 = no match, 100 = perfect match)
inline int fuzzy_score(std::string_view query, std::string_view target) {
    if (query.empty()) return 0;
    if (target.empty()) return 0;

    // Convert to lowercase for comparison
    std::string query_lower;
    std::string target_lower;
    query_lower.reserve(query.size());
    target_lower.reserve(target.size());

    for (char c : query) query_lower += std::tolower(c);
    for (char c : target) target_lower += std::tolower(c);

    // Exact match = 100 points
    if (query_lower == target_lower) {
        return 100;
    }

    // Starts with query = 90 points
    if (target_lower.starts_with(query_lower)) {
        return 90;
    }

    // Contains query as substring = 80 points
    if (target_lower.find(query_lower) != std::string::npos) {
        return 80;
    }

    // Calculate Levenshtein distance for fuzzy matching
    const int distance = levenshtein_distance(query_lower, target_lower);
    const int max_len = static_cast<int>(std::max(query_lower.size(), target_lower.size()));

    // Score based on similarity (inverse of edit distance ratio)
    // distance/max_len ranges from 0.0 (identical) to 1.0 (completely different)
    const double similarity = 1.0 - (static_cast<double>(distance) / max_len);

    // Map similarity to 0-70 range (fuzzy matches get lower scores than exact/substring)
    return static_cast<int>(similarity * 70);
}

/// @brief Check if a string fuzzy matches query with minimum score threshold
/// @param query Search query
/// @param target Target string
/// @param threshold Minimum score required (default: 30)
/// @return True if score >= threshold
inline bool fuzzy_matches(std::string_view query, std::string_view target, int threshold = 30) {
    return fuzzy_score(query, target) >= threshold;
}

} // namespace KeepTower::FuzzyMatch
