// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file SearchController.h
 * @brief Controller for account search and filtering logic
 *
 * This controller extracts search/filter logic from MainWindow,
 * providing a clean, testable interface for account filtering operations.
 */

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <optional>
#include "record.pb.h"

/**
 * @brief Field filter options for searching
 */
enum class SearchField {
    ALL = 0,           ///< Search all fields
    ACCOUNT_NAME = 1,  ///< Search account name only
    USERNAME = 2,      ///< Search username only
    EMAIL = 3,         ///< Search email only
    WEBSITE = 4,       ///< Search website only
    NOTES = 5,         ///< Search notes only
    TAGS = 6           ///< Search tags only
};

/**
 * @brief Sort direction for account list
 */
enum class SortOrder {
    ASCENDING,   ///< A-Z
    DESCENDING   ///< Z-A
};

/**
 * @brief Search criteria for filtering accounts
 */
struct SearchCriteria {
    std::string search_text;           ///< Text to search for
    std::string tag_filter;            ///< Tag to filter by (empty = all)
    SearchField field_filter = SearchField::ALL;  ///< Which field(s) to search
    SortOrder sort_order = SortOrder::ASCENDING;  ///< Sort direction
    int fuzzy_threshold = 30;          ///< Minimum fuzzy match score (0-100)
};

/**
 * @brief Controller for account search and filtering
 *
 * SearchController handles:
 * - Text search with fuzzy matching
 * - Field-specific filtering
 * - Tag filtering
 * - Sorting accounts
 * - Search result ranking
 *
 * This class separates search logic from MainWindow,
 * making it testable and reusable.
 *
 * @section usage Usage Example
 * @code
 * SearchController controller;
 *
 * SearchCriteria criteria;
 * criteria.search_text = "gmail";
 * criteria.field_filter = SearchField::ACCOUNT_NAME;
 * criteria.sort_order = SortOrder::ASCENDING;
 *
 * auto results = controller.filter_accounts(all_accounts, criteria);
 * @endcode
 */
class SearchController {
public:
    SearchController() = default;
    ~SearchController() = default;

    // Allow copy and move
    /** @brief Copy constructor - SearchController is stateless */
    SearchController(const SearchController&) = default;

    /** @brief Copy assignment operator */
    SearchController& operator=(const SearchController&) = default;

    /** @brief Move constructor */
    SearchController(SearchController&&) = default;

    /** @brief Move assignment operator */
    SearchController& operator=(SearchController&&) = default;

    /**
     * @brief Filter accounts based on search criteria
     *
     * Applies search text, tag filter, and field filter to the account list.
     * Returns filtered accounts sorted according to criteria.
     *
     * @param accounts All accounts to filter
     * @param criteria Search and filter criteria
     * @return Vector of accounts matching the criteria
     */
    [[nodiscard]] std::vector<keeptower::AccountRecord> filter_accounts(
        const std::vector<keeptower::AccountRecord>& accounts,
        const SearchCriteria& criteria) const;

    /**
     * @brief Check if an account matches search text
     *
     * Performs fuzzy matching against the specified field(s).
     *
     * @param account Account to check
     * @param search_text Text to search for
     * @param field Which field(s) to search
     * @param fuzzy_threshold Minimum score for fuzzy matches (0-100)
     * @return true if account matches search criteria
     */
    [[nodiscard]] bool matches_search(
        const keeptower::AccountRecord& account,
        const std::string& search_text,
        SearchField field = SearchField::ALL,
        int fuzzy_threshold = 30) const;

    /**
     * @brief Check if an account has a specific tag
     *
     * @param account Account to check
     * @param tag Tag to search for
     * @return true if account has the tag
     */
    [[nodiscard]] bool has_tag(
        const keeptower::AccountRecord& account,
        const std::string& tag) const;

    /**
     * @brief Sort accounts by name
     *
     * @param accounts Accounts to sort (modified in place)
     * @param order Sort order (ascending or descending)
     */
    void sort_accounts(
        std::vector<keeptower::AccountRecord>& accounts,
        SortOrder order) const;

    /**
     * @brief Get all unique tags from account list
     *
     * @param accounts Accounts to extract tags from
     * @return Sorted vector of unique tags
     */
    [[nodiscard]] std::vector<std::string> get_all_tags(
        const std::vector<keeptower::AccountRecord>& accounts) const;

    /**
     * @brief Calculate search relevance score
     *
     * Higher scores indicate better matches.
     * Used for ranking search results.
     *
     * @param account Account to score
     * @param search_text Search query
     * @param field Field filter applied
     * @return Score (0-100, higher = more relevant)
     */
    [[nodiscard]] int calculate_relevance_score(
        const keeptower::AccountRecord& account,
        const std::string& search_text,
        SearchField field = SearchField::ALL) const;

private:
    /**
     * @brief Check if text matches in a specific field
     *
     * @param field_value Field content to search
     * @param search_text Text to find
     * @param fuzzy_threshold Minimum fuzzy match score
     * @return true if field matches
     */
    [[nodiscard]] bool field_matches(
        const std::string& field_value,
        const std::string& search_text,
        int fuzzy_threshold) const;

    /**
     * @brief Get the searchable content for a field
     *
     * @param account Account to extract field from
     * @param field Which field to extract
     * @return Field content as string
     */
    [[nodiscard]] std::string get_field_content(
        const keeptower::AccountRecord& account,
        SearchField field) const;

    /**
     * @brief Compare two accounts by name (case-insensitive)
     *
     * @param a First account
     * @param b Second account
     * @return true if a should come before b
     */
    [[nodiscard]] static bool compare_accounts_asc(
        const keeptower::AccountRecord& a,
        const keeptower::AccountRecord& b);

    /**
     * @brief Compare two accounts by name descending (case-insensitive)
     *
     * @param a First account
     * @param b Second account
     * @return true if a should come after b
     */
    [[nodiscard]] static bool compare_accounts_desc(
        const keeptower::AccountRecord& a,
        const keeptower::AccountRecord& b);
};
