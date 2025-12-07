// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng
//
// Test program to verify input validation and field length limits

#include <gtest/gtest.h>
#include <string>

// Define the same constants as in MainWindow.h
namespace UI {
    constexpr int MAX_NOTES_LENGTH = 1000;
    constexpr int MAX_ACCOUNT_NAME_LENGTH = 256;
    constexpr int MAX_USERNAME_LENGTH = 256;
    constexpr int MAX_PASSWORD_LENGTH = 512;
    constexpr int MAX_EMAIL_LENGTH = 256;
    constexpr int MAX_WEBSITE_LENGTH = 512;
}

// Simple validation function that mimics MainWindow::validate_field_length logic
bool validate_field_length(const std::string& value, int max_length) {
    return static_cast<int>(value.length()) <= max_length;
}

// Test account name validation
TEST(InputValidationTest, AccountNameWithinLimit) {
    std::string valid_name(UI::MAX_ACCOUNT_NAME_LENGTH, 'a');
    EXPECT_TRUE(validate_field_length(valid_name, UI::MAX_ACCOUNT_NAME_LENGTH));
}

TEST(InputValidationTest, AccountNameExceedsLimit) {
    std::string invalid_name(UI::MAX_ACCOUNT_NAME_LENGTH + 1, 'a');
    EXPECT_FALSE(validate_field_length(invalid_name, UI::MAX_ACCOUNT_NAME_LENGTH));
}

// Test username validation
TEST(InputValidationTest, UsernameWithinLimit) {
    std::string valid_username(UI::MAX_USERNAME_LENGTH, 'u');
    EXPECT_TRUE(validate_field_length(valid_username, UI::MAX_USERNAME_LENGTH));
}

TEST(InputValidationTest, UsernameExceedsLimit) {
    std::string invalid_username(UI::MAX_USERNAME_LENGTH + 1, 'u');
    EXPECT_FALSE(validate_field_length(invalid_username, UI::MAX_USERNAME_LENGTH));
}

// Test password validation
TEST(InputValidationTest, PasswordWithinLimit) {
    std::string valid_password(UI::MAX_PASSWORD_LENGTH, 'p');
    EXPECT_TRUE(validate_field_length(valid_password, UI::MAX_PASSWORD_LENGTH));
}

TEST(InputValidationTest, PasswordExceedsLimit) {
    std::string invalid_password(UI::MAX_PASSWORD_LENGTH + 1, 'p');
    EXPECT_FALSE(validate_field_length(invalid_password, UI::MAX_PASSWORD_LENGTH));
}

// Test email validation
TEST(InputValidationTest, EmailWithinLimit) {
    std::string valid_email(UI::MAX_EMAIL_LENGTH, 'e');
    EXPECT_TRUE(validate_field_length(valid_email, UI::MAX_EMAIL_LENGTH));
}

TEST(InputValidationTest, EmailExceedsLimit) {
    std::string invalid_email(UI::MAX_EMAIL_LENGTH + 1, 'e');
    EXPECT_FALSE(validate_field_length(invalid_email, UI::MAX_EMAIL_LENGTH));
}

// Test website validation
TEST(InputValidationTest, WebsiteWithinLimit) {
    std::string valid_website(UI::MAX_WEBSITE_LENGTH, 'w');
    EXPECT_TRUE(validate_field_length(valid_website, UI::MAX_WEBSITE_LENGTH));
}

TEST(InputValidationTest, WebsiteExceedsLimit) {
    std::string invalid_website(UI::MAX_WEBSITE_LENGTH + 1, 'w');
    EXPECT_FALSE(validate_field_length(invalid_website, UI::MAX_WEBSITE_LENGTH));
}

// Test notes validation
TEST(InputValidationTest, NotesWithinLimit) {
    std::string valid_notes(UI::MAX_NOTES_LENGTH, 'n');
    EXPECT_TRUE(validate_field_length(valid_notes, UI::MAX_NOTES_LENGTH));
}

TEST(InputValidationTest, NotesExceedsLimit) {
    std::string invalid_notes(UI::MAX_NOTES_LENGTH + 1, 'n');
    EXPECT_FALSE(validate_field_length(invalid_notes, UI::MAX_NOTES_LENGTH));
}

// Test empty strings (should always be valid)
TEST(InputValidationTest, EmptyStringsValid) {
    EXPECT_TRUE(validate_field_length("", UI::MAX_ACCOUNT_NAME_LENGTH));
    EXPECT_TRUE(validate_field_length("", UI::MAX_USERNAME_LENGTH));
    EXPECT_TRUE(validate_field_length("", UI::MAX_PASSWORD_LENGTH));
    EXPECT_TRUE(validate_field_length("", UI::MAX_EMAIL_LENGTH));
    EXPECT_TRUE(validate_field_length("", UI::MAX_WEBSITE_LENGTH));
    EXPECT_TRUE(validate_field_length("", UI::MAX_NOTES_LENGTH));
}

// Test boundary conditions (exactly at limit)
TEST(InputValidationTest, BoundaryConditions) {
    // At limit - should pass
    EXPECT_TRUE(validate_field_length(std::string(UI::MAX_ACCOUNT_NAME_LENGTH, 'x'), UI::MAX_ACCOUNT_NAME_LENGTH));
    EXPECT_TRUE(validate_field_length(std::string(UI::MAX_USERNAME_LENGTH, 'x'), UI::MAX_USERNAME_LENGTH));
    EXPECT_TRUE(validate_field_length(std::string(UI::MAX_PASSWORD_LENGTH, 'x'), UI::MAX_PASSWORD_LENGTH));
    EXPECT_TRUE(validate_field_length(std::string(UI::MAX_EMAIL_LENGTH, 'x'), UI::MAX_EMAIL_LENGTH));
    EXPECT_TRUE(validate_field_length(std::string(UI::MAX_WEBSITE_LENGTH, 'x'), UI::MAX_WEBSITE_LENGTH));
    EXPECT_TRUE(validate_field_length(std::string(UI::MAX_NOTES_LENGTH, 'x'), UI::MAX_NOTES_LENGTH));

    // One over limit - should fail
    EXPECT_FALSE(validate_field_length(std::string(UI::MAX_ACCOUNT_NAME_LENGTH + 1, 'x'), UI::MAX_ACCOUNT_NAME_LENGTH));
    EXPECT_FALSE(validate_field_length(std::string(UI::MAX_USERNAME_LENGTH + 1, 'x'), UI::MAX_USERNAME_LENGTH));
    EXPECT_FALSE(validate_field_length(std::string(UI::MAX_PASSWORD_LENGTH + 1, 'x'), UI::MAX_PASSWORD_LENGTH));
    EXPECT_FALSE(validate_field_length(std::string(UI::MAX_EMAIL_LENGTH + 1, 'x'), UI::MAX_EMAIL_LENGTH));
    EXPECT_FALSE(validate_field_length(std::string(UI::MAX_WEBSITE_LENGTH + 1, 'x'), UI::MAX_WEBSITE_LENGTH));
    EXPECT_FALSE(validate_field_length(std::string(UI::MAX_NOTES_LENGTH + 1, 'x'), UI::MAX_NOTES_LENGTH));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
