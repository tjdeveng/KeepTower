// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file test_fuzzy_match.cc
 * @brief Unit tests for fuzzy string matching functionality
 *
 * Tests the Levenshtein distance algorithm and fuzzy matching scoring system
 * used for advanced account search features.
 */

#include "../src/utils/helpers/FuzzyMatch.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <string>

using namespace KeepTower::FuzzyMatch;

/**
 * @brief Test Levenshtein distance calculations
 */
void test_levenshtein_distance() {
    std::cout << "Testing Levenshtein distance..." << std::endl;

    // Identical strings
    assert(levenshtein_distance("hello", "hello") == 0);
    assert(levenshtein_distance("", "") == 0);

    // Single character difference
    assert(levenshtein_distance("hello", "hallo") == 1);  // Substitution
    assert(levenshtein_distance("hello", "ello") == 1);   // Deletion
    assert(levenshtein_distance("hello", "helllo") == 1); // Insertion

    // Multiple differences
    assert(levenshtein_distance("kitten", "sitting") == 3);
    assert(levenshtein_distance("saturday", "sunday") == 3);

    // Empty string cases
    assert(levenshtein_distance("", "hello") == 5);
    assert(levenshtein_distance("hello", "") == 5);

    // Case sensitivity (should be case-insensitive)
    assert(levenshtein_distance("Hello", "hello") == 0);
    assert(levenshtein_distance("GITHUB", "github") == 0);

    std::cout << "✓ Levenshtein distance tests passed" << std::endl;
}

/**
 * @brief Test fuzzy score calculations
 */
void test_fuzzy_score() {
    std::cout << "Testing fuzzy score..." << std::endl;

    // Exact matches should score 100
    assert(fuzzy_score("github", "github") == 100);
    assert(fuzzy_score("GitHub", "github") == 100);  // Case insensitive

    // Starts with should score 90
    assert(fuzzy_score("git", "github") == 90);
    assert(fuzzy_score("face", "facebook") == 90);

    // Contains should score 80
    assert(fuzzy_score("hub", "github") == 80);
    assert(fuzzy_score("book", "facebook") == 80);

    // Similar strings should score > 30 (threshold)
    assert(fuzzy_score("googl", "google") >= 30);
    assert(fuzzy_score("amazn", "amazon") >= 30);

    // Very different strings should score low
    assert(fuzzy_score("xyz", "github") < 30);
    assert(fuzzy_score("abc", "twitter") < 30);

    // Empty strings
    assert(fuzzy_score("", "github") == 0);
    assert(fuzzy_score("github", "") == 0);

    std::cout << "✓ Fuzzy score tests passed" << std::endl;
}

/**
 * @brief Test fuzzy matches function
 */
void test_fuzzy_matches() {
    std::cout << "Testing fuzzy matches..." << std::endl;

    // Should match with default threshold (30)
    assert(fuzzy_matches("git", "github"));
    assert(fuzzy_matches("face", "facebook"));
    assert(fuzzy_matches("amaz", "amazon"));

    // Should not match (too different)
    assert(!fuzzy_matches("xyz", "github"));
    assert(!fuzzy_matches("abc", "twitter"));

    // Custom thresholds
    assert(fuzzy_matches("git", "github", 90));  // Starts with
    assert(!fuzzy_matches("hub", "github", 90)); // Contains only scores 80

    std::cout << "✓ Fuzzy matches tests passed" << std::endl;
}

/**
 * @brief Test realistic account search scenarios
 */
void test_realistic_searches() {
    std::cout << "Testing realistic search scenarios..." << std::endl;

    // Typos should still match
    assert(fuzzy_matches("gmai", "gmail"));
    assert(fuzzy_matches("gogle", "google"));
    assert(fuzzy_matches("facbook", "facebook"));

    // Partial matches
    assert(fuzzy_matches("amazon", "myamazon@email.com"));
    assert(fuzzy_matches("work", "work-account"));

    // Account name variations
    assert(fuzzy_score("github", "GitHub Account") >= 80);  // Contains
    assert(fuzzy_score("aws", "AWS Production") >= 80);

    // Website URLs
    assert(fuzzy_score("github", "https://github.com") >= 80);
    assert(fuzzy_score("google", "mail.google.com") >= 80);

    std::cout << "✓ Realistic search tests passed" << std::endl;
}

/**
 * @brief Test edge cases
 */
void test_edge_cases() {
    std::cout << "Testing edge cases..." << std::endl;

    // Very long strings
    std::string long_str(1000, 'a');
    std::string long_str2 = long_str + "b";
    assert(levenshtein_distance(long_str, long_str2) == 1);

    // Special characters
    assert(fuzzy_matches("user@", "user@example.com"));
    assert(fuzzy_matches("https://", "https://github.com"));

    // Unicode (basic ASCII only for now)
    assert(fuzzy_score("test", "test") == 100);

    // Single character
    assert(levenshtein_distance("a", "b") == 1);
    assert(levenshtein_distance("a", "a") == 0);

    std::cout << "✓ Edge case tests passed" << std::endl;
}

int main() {
    std::cout << "Running Fuzzy Match Tests\n";
    std::cout << "===========================\n\n";

    try {
        test_levenshtein_distance();
        test_fuzzy_score();
        test_fuzzy_matches();
        test_realistic_searches();
        test_edge_cases();

        std::cout << "\n===========================\n";
        std::cout << "✓ All fuzzy match tests passed!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "✗ Test failed with unknown exception" << std::endl;
        return 1;
    }
}
