// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file test_search_controller.cc
 * @brief Unit tests for SearchController
 */

#include <gtest/gtest.h>
#include "../src/ui/controllers/SearchController.h"
#include <vector>

/**
 * @class SearchControllerTest
 * @brief Test fixture for SearchController
 */
class SearchControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        controller = std::make_unique<SearchController>();

        // Create test accounts
        keeptower::AccountRecord account1;
        account1.set_id("1");
        account1.set_account_name("Gmail Personal");
        account1.set_user_name("john.doe@gmail.com");
        account1.set_email("john.doe@gmail.com");
        account1.set_website("https://gmail.com");
        account1.set_notes("Personal email account");
        account1.add_tags("email");
        account1.add_tags("personal");

        keeptower::AccountRecord account2;
        account2.set_id("2");
        account2.set_account_name("GitHub Work");
        account2.set_user_name("jdoe");
        account2.set_email("john@company.com");
        account2.set_website("https://github.com");
        account2.set_notes("Work repository access");
        account2.add_tags("development");
        account2.add_tags("work");

        keeptower::AccountRecord account3;
        account3.set_id("3");
        account3.set_account_name("AWS Console");
        account3.set_user_name("admin");
        account3.set_email("admin@company.com");
        account3.set_website("https://aws.amazon.com");
        account3.set_notes("Cloud infrastructure");
        account3.add_tags("cloud");
        account3.add_tags("work");

        keeptower::AccountRecord account4;
        account4.set_id("4");
        account4.set_account_name("Netflix");
        account4.set_email("john.doe@gmail.com");
        account4.set_website("https://netflix.com");
        account4.set_notes("Streaming service");
        account4.add_tags("entertainment");
        account4.add_tags("personal");

        test_accounts = {account1, account2, account3, account4};
    }

    std::unique_ptr<SearchController> controller;
    std::vector<keeptower::AccountRecord> test_accounts;
};

/**
 * @test Filter with empty criteria returns all accounts
 */
TEST_F(SearchControllerTest, FilterEmpty) {
    SearchCriteria criteria;
    // All fields empty/default

    auto results = controller->filter_accounts(test_accounts, criteria);

    EXPECT_EQ(results.size(), 4);
}

/**
 * @test Filter by account name
 */
TEST_F(SearchControllerTest, FilterByAccountName) {
    SearchCriteria criteria;
    criteria.search_text = "Gmail";
    criteria.field_filter = SearchField::ACCOUNT_NAME;

    auto results = controller->filter_accounts(test_accounts, criteria);

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].account_name(), "Gmail Personal");
}

/**
 * @test Filter by username
 */
TEST_F(SearchControllerTest, FilterByUsername) {
    SearchCriteria criteria;
    criteria.search_text = "jdoe";
    criteria.field_filter = SearchField::USERNAME;

    auto results = controller->filter_accounts(test_accounts, criteria);

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].account_name(), "GitHub Work");
}

/**
 * @test Filter by email
 */
TEST_F(SearchControllerTest, FilterByEmail) {
    SearchCriteria criteria;
    criteria.search_text = "gmail.com";
    criteria.field_filter = SearchField::EMAIL;

    auto results = controller->filter_accounts(test_accounts, criteria);

    // Should match 2 accounts with gmail.com email
    EXPECT_EQ(results.size(), 2);
}

/**
 * @test Filter by website
 */
TEST_F(SearchControllerTest, FilterByWebsite) {
    SearchCriteria criteria;
    criteria.search_text = "github";
    criteria.field_filter = SearchField::WEBSITE;

    auto results = controller->filter_accounts(test_accounts, criteria);

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].account_name(), "GitHub Work");
}

/**
 * @test Filter by notes
 */
TEST_F(SearchControllerTest, FilterByNotes) {
    SearchCriteria criteria;
    criteria.search_text = "infrastructure";
    criteria.field_filter = SearchField::NOTES;

    auto results = controller->filter_accounts(test_accounts, criteria);

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].account_name(), "AWS Console");
}

/**
 * @test Filter by tag
 */
TEST_F(SearchControllerTest, FilterByTag) {
    SearchCriteria criteria;
    criteria.tag_filter = "work";

    auto results = controller->filter_accounts(test_accounts, criteria);

    // Should match GitHub and AWS accounts
    EXPECT_EQ(results.size(), 2);
}

/**
 * @test Filter with search text in ALL fields
 */
TEST_F(SearchControllerTest, FilterAllFields) {
    SearchCriteria criteria;
    criteria.search_text = "john";
    criteria.field_filter = SearchField::ALL;

    auto results = controller->filter_accounts(test_accounts, criteria);

    // Should match accounts with "john" in any field
    // Gmail (username, email), Netflix (username, email)
    EXPECT_GE(results.size(), 2);
}

/**
 * @test Fuzzy matching works
 */
TEST_F(SearchControllerTest, FuzzyMatching) {
    SearchCriteria criteria;
    criteria.search_text = "gitub";  // Typo: should still match GitHub
    criteria.field_filter = SearchField::ACCOUNT_NAME;
    criteria.fuzzy_threshold = 20;  // Lower threshold for typos

    auto results = controller->filter_accounts(test_accounts, criteria);

    // Should find GitHub despite typo
    ASSERT_GE(results.size(), 1);
    bool found_github = false;
    for (const auto& acc : results) {
        if (acc.account_name() == "GitHub Work") {
            found_github = true;
            break;
        }
    }
    EXPECT_TRUE(found_github);
}

/**
 * @test Sort ascending (A-Z)
 */
TEST_F(SearchControllerTest, SortAscending) {
    SearchCriteria criteria;
    criteria.sort_order = SortOrder::ASCENDING;

    auto results = controller->filter_accounts(test_accounts, criteria);

    ASSERT_EQ(results.size(), 4);
    // Should be: AWS Console, GitHub Work, Gmail Personal, Netflix
    EXPECT_EQ(results[0].account_name(), "AWS Console");
    EXPECT_EQ(results[1].account_name(), "GitHub Work");
    EXPECT_EQ(results[2].account_name(), "Gmail Personal");
    EXPECT_EQ(results[3].account_name(), "Netflix");
}

/**
 * @test Sort descending (Z-A)
 */
TEST_F(SearchControllerTest, SortDescending) {
    SearchCriteria criteria;
    criteria.sort_order = SortOrder::DESCENDING;

    auto results = controller->filter_accounts(test_accounts, criteria);

    ASSERT_EQ(results.size(), 4);
    // Should be: Netflix, Gmail Personal, GitHub Work, AWS Console
    EXPECT_EQ(results[0].account_name(), "Netflix");
    EXPECT_EQ(results[1].account_name(), "Gmail Personal");
    EXPECT_EQ(results[2].account_name(), "GitHub Work");
    EXPECT_EQ(results[3].account_name(), "AWS Console");
}

/**
 * @test Combined search and tag filter
 */
TEST_F(SearchControllerTest, CombinedSearchAndTag) {
    SearchCriteria criteria;
    criteria.search_text = "com";
    criteria.tag_filter = "work";
    criteria.field_filter = SearchField::ALL;

    auto results = controller->filter_accounts(test_accounts, criteria);

    // Should match only work accounts with "com" in any field
    // GitHub (has "com" in email/website) and AWS (has "com" in email/website)
    EXPECT_EQ(results.size(), 2);
}

/**
 * @test Has tag check
 */
TEST_F(SearchControllerTest, HasTag) {
    EXPECT_TRUE(controller->has_tag(test_accounts[0], "email"));
    EXPECT_TRUE(controller->has_tag(test_accounts[0], "personal"));
    EXPECT_FALSE(controller->has_tag(test_accounts[0], "work"));

    EXPECT_TRUE(controller->has_tag(test_accounts[1], "development"));
    EXPECT_TRUE(controller->has_tag(test_accounts[1], "work"));
    EXPECT_FALSE(controller->has_tag(test_accounts[1], "personal"));
}

/**
 * @test Has tag is case-insensitive
 */
TEST_F(SearchControllerTest, HasTagCaseInsensitive) {
    EXPECT_TRUE(controller->has_tag(test_accounts[0], "EMAIL"));
    EXPECT_TRUE(controller->has_tag(test_accounts[0], "Personal"));
    EXPECT_TRUE(controller->has_tag(test_accounts[0], "PERSONAL"));
}

/**
 * @test Get all unique tags
 */
TEST_F(SearchControllerTest, GetAllTags) {
    auto tags = controller->get_all_tags(test_accounts);

    // Should have 6 unique tags, sorted alphabetically
    ASSERT_EQ(tags.size(), 6);
    EXPECT_EQ(tags[0], "cloud");
    EXPECT_EQ(tags[1], "development");
    EXPECT_EQ(tags[2], "email");
    EXPECT_EQ(tags[3], "entertainment");
    EXPECT_EQ(tags[4], "personal");
    EXPECT_EQ(tags[5], "work");
}

/**
 * @test Get tags from empty list
 */
TEST_F(SearchControllerTest, GetAllTagsEmpty) {
    std::vector<keeptower::AccountRecord> empty_accounts;
    auto tags = controller->get_all_tags(empty_accounts);

    EXPECT_EQ(tags.size(), 0);
}

/**
 * @test Calculate relevance score
 */
TEST_F(SearchControllerTest, RelevanceScore) {
    // Exact match should score high
    int score1 = controller->calculate_relevance_score(
        test_accounts[0], "Gmail", SearchField::ACCOUNT_NAME);
    EXPECT_GT(score1, 70);  // Should be high score

    // Partial match should score medium
    int score2 = controller->calculate_relevance_score(
        test_accounts[0], "mail", SearchField::ACCOUNT_NAME);
    EXPECT_GT(score2, 30);
    EXPECT_LT(score2, 100);

    // No match should score low
    int score3 = controller->calculate_relevance_score(
        test_accounts[0], "zzzzz", SearchField::ACCOUNT_NAME);
    EXPECT_LT(score3, 30);
}

/**
 * @test Matches search with exact match
 */
TEST_F(SearchControllerTest, MatchesSearchExact) {
    EXPECT_TRUE(controller->matches_search(
        test_accounts[0], "Gmail", SearchField::ACCOUNT_NAME, 30));
}

/**
 * @test Matches search with partial match
 */
TEST_F(SearchControllerTest, MatchesSearchPartial) {
    EXPECT_TRUE(controller->matches_search(
        test_accounts[0], "mail", SearchField::ACCOUNT_NAME, 30));
}

/**
 * @test Matches search case-insensitive
 */
TEST_F(SearchControllerTest, MatchesSearchCaseInsensitive) {
    EXPECT_TRUE(controller->matches_search(
        test_accounts[0], "gmail", SearchField::ACCOUNT_NAME, 30));
    EXPECT_TRUE(controller->matches_search(
        test_accounts[0], "GMAIL", SearchField::ACCOUNT_NAME, 30));
}

/**
 * @test Empty search text matches everything
 */
TEST_F(SearchControllerTest, EmptySearchMatchesAll) {
    EXPECT_TRUE(controller->matches_search(
        test_accounts[0], "", SearchField::ALL, 30));
    EXPECT_TRUE(controller->matches_search(
        test_accounts[1], "", SearchField::ACCOUNT_NAME, 30));
}

/**
 * @test Filter with no matches returns empty
 */
TEST_F(SearchControllerTest, NoMatches) {
    SearchCriteria criteria;
    criteria.search_text = "ThisDoesNotExistAnywhere";
    criteria.field_filter = SearchField::ALL;

    auto results = controller->filter_accounts(test_accounts, criteria);

    EXPECT_EQ(results.size(), 0);
}

/**
 * @test Tag search in tags field
 */
TEST_F(SearchControllerTest, SearchInTags) {
    SearchCriteria criteria;
    criteria.search_text = "entertainment";
    criteria.field_filter = SearchField::TAGS;

    auto results = controller->filter_accounts(test_accounts, criteria);

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].account_name(), "Netflix");
}

/**
 * @test Controller is copyable
 */
TEST_F(SearchControllerTest, Copyable) {
    SearchController controller1;
    SearchController controller2 = controller1;  // Copy

    // Both should work independently
    SearchCriteria criteria;
    criteria.search_text = "test";

    auto results1 = controller1.filter_accounts(test_accounts, criteria);
    auto results2 = controller2.filter_accounts(test_accounts, criteria);

    EXPECT_EQ(results1.size(), results2.size());
}
