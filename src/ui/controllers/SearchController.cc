// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "SearchController.h"
#include "../../utils/helpers/FuzzyMatch.h"
#include <algorithm>
#include <cctype>
#include <set>

// Convert string to lowercase for case-insensitive comparison
static std::string to_lower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return str;
}

std::vector<keeptower::AccountRecord> SearchController::filter_accounts(
    const std::vector<keeptower::AccountRecord>& accounts,
    const SearchCriteria& criteria) const {

    std::vector<keeptower::AccountRecord> filtered;
    filtered.reserve(accounts.size());

    // Apply filters
    for (const auto& account : accounts) {
        bool matches = true;

        // Apply search text filter
        if (!criteria.search_text.empty()) {
            if (!matches_search(account, criteria.search_text,
                               criteria.field_filter, criteria.fuzzy_threshold)) {
                matches = false;
            }
        }

        // Apply tag filter
        if (matches && !criteria.tag_filter.empty()) {
            if (!has_tag(account, criteria.tag_filter)) {
                matches = false;
            }
        }

        if (matches) {
            filtered.push_back(account);
        }
    }

    // Sort results
    sort_accounts(filtered, criteria.sort_order);

    return filtered;
}

bool SearchController::matches_search(
    const keeptower::AccountRecord& account,
    const std::string& search_text,
    SearchField field,
    int fuzzy_threshold) const {

    if (search_text.empty()) {
        return true;
    }

    // Check all fields if field == ALL
    if (field == SearchField::ALL) {
        // Try all fields until one matches
        const SearchField all_fields[] = {
            SearchField::ACCOUNT_NAME,
            SearchField::USERNAME,
            SearchField::EMAIL,
            SearchField::WEBSITE,
            SearchField::NOTES,
            SearchField::TAGS
        };

        for (const auto& f : all_fields) {
            std::string field_content = get_field_content(account, f);
            if (field_matches(field_content, search_text, fuzzy_threshold)) {
                return true;
            }
        }
        return false;
    }

    // Check specific field
    std::string field_content = get_field_content(account, field);
    return field_matches(field_content, search_text, fuzzy_threshold);
}

bool SearchController::has_tag(
    const keeptower::AccountRecord& account,
    const std::string& tag) const {

    if (tag.empty()) {
        return true;  // Empty tag filter matches all
    }

    std::string tag_lower = to_lower(tag);

    for (int i = 0; i < account.tags_size(); ++i) {
        if (to_lower(account.tags(i)) == tag_lower) {
            return true;
        }
    }

    return false;
}

void SearchController::sort_accounts(
    std::vector<keeptower::AccountRecord>& accounts,
    SortOrder order) const {

    if (order == SortOrder::ASCENDING) {
        std::sort(accounts.begin(), accounts.end(), compare_accounts_asc);
    } else {
        std::sort(accounts.begin(), accounts.end(), compare_accounts_desc);
    }
}

std::vector<std::string> SearchController::get_all_tags(
    const std::vector<keeptower::AccountRecord>& accounts) const {

    std::set<std::string> unique_tags;

    for (const auto& account : accounts) {
        for (int i = 0; i < account.tags_size(); ++i) {
            if (!account.tags(i).empty()) {
                unique_tags.insert(account.tags(i));
            }
        }
    }

    // Convert to sorted vector
    std::vector<std::string> tags(unique_tags.begin(), unique_tags.end());
    std::sort(tags.begin(), tags.end());

    return tags;
}

int SearchController::calculate_relevance_score(
    const keeptower::AccountRecord& account,
    const std::string& search_text,
    SearchField field) const {

    if (search_text.empty()) {
        return 0;
    }

    int best_score = 0;

    if (field == SearchField::ALL) {
        // Check all fields and return highest score
        const SearchField all_fields[] = {
            SearchField::ACCOUNT_NAME,
            SearchField::USERNAME,
            SearchField::EMAIL,
            SearchField::WEBSITE,
            SearchField::NOTES,
            SearchField::TAGS
        };

        for (const auto& f : all_fields) {
            std::string field_content = get_field_content(account, f);
            int score = KeepTower::FuzzyMatch::fuzzy_score(search_text, field_content);

            // Boost scores for high-priority fields
            if (f == SearchField::ACCOUNT_NAME) {
                score = static_cast<int>(score * 1.3);  // 30% boost for account name
            } else if (f == SearchField::USERNAME) {
                score = static_cast<int>(score * 1.1);  // 10% boost for username
            }

            best_score = std::max(best_score, score);
        }
    } else {
        // Check specific field
        std::string field_content = get_field_content(account, field);
        best_score = KeepTower::FuzzyMatch::fuzzy_score(search_text, field_content);
    }

    // Cap at 100
    return std::min(best_score, 100);
}

bool SearchController::field_matches(
    const std::string& field_value,
    const std::string& search_text,
    int fuzzy_threshold) const {

    if (field_value.empty()) {
        return false;
    }

    // Use fuzzy matching from FuzzyMatch utility
    return KeepTower::FuzzyMatch::fuzzy_matches(search_text, field_value, fuzzy_threshold);
}

std::string SearchController::get_field_content(
    const keeptower::AccountRecord& account,
    SearchField field) const {

    switch (field) {
        case SearchField::ACCOUNT_NAME:
            return account.account_name();

        case SearchField::USERNAME:
            return account.user_name();

        case SearchField::EMAIL:
            return account.email();

        case SearchField::WEBSITE:
            return account.website();

        case SearchField::NOTES:
            return account.notes();

        case SearchField::TAGS: {
            // Concatenate all tags with spaces
            std::string all_tags;
            for (int i = 0; i < account.tags_size(); ++i) {
                if (i > 0) all_tags += " ";
                all_tags += account.tags(i);
            }
            return all_tags;
        }

        case SearchField::ALL:
        default:
            // Concatenate all fields
            return account.account_name() + " " +
                   account.user_name() + " " +
                   account.email() + " " +
                   account.website() + " " +
                   account.notes();
    }
}

bool SearchController::compare_accounts_asc(
    const keeptower::AccountRecord& a,
    const keeptower::AccountRecord& b) {

    // Case-insensitive comparison
    return to_lower(a.account_name()) < to_lower(b.account_name());
}

bool SearchController::compare_accounts_desc(
    const keeptower::AccountRecord& a,
    const keeptower::AccountRecord& b) {

    // Case-insensitive comparison (reversed)
    return to_lower(a.account_name()) > to_lower(b.account_name());
}
