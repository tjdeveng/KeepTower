// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file CommonPasswords.h
 * @brief Comprehensive common password blacklist for strength validation
 *
 * Contains a curated list of 227 commonly used passwords compiled from real-world
 * data breaches and security research. Used to prevent users from selecting weak
 * passwords that appear in breach databases.
 *
 * @section sources Data Sources
 * - **Have I Been Pwned** (Troy Hunt's breach database)
 * - **SplashData** annual worst passwords report
 * - **NordPass** annual most common passwords report
 * - **NIST SP 800-63B** guidelines
 *
 * @section categories Password Categories
 * - Top 20 most breached passwords
 * - Sequential numbers and patterns
 * - Keyboard walking patterns
 * - Common words and phrases
 * - Sports teams and names
 * - Profanity and slang
 * - Leet speak variations
 * - Date and year patterns
 *
 * @section security Security Considerations
 * This list is intentionally kept in memory (not loaded from file) to ensure
 * password checks work even if the filesystem is compromised. The list is
 * compiled into the binary at build time.
 *
 * @warning Do not remove passwords from this list without security review
 *
 * @section usage Usage
 * Used by PasswordStrengthValidator during password creation and change operations.
 * Passwords matching any entry in this list are rejected as too weak.
 */

#ifndef KEEPTOWER_COMMON_PASSWORDS_H
#define KEEPTOWER_COMMON_PASSWORDS_H

#include <array>
#include <string_view>

namespace KeepTower {

/**
 * @brief Common password blacklist from real-world breaches
 *
 * Array of 227 common passwords that should never be accepted. Passwords are
 * stored as string_view for zero-copy, compile-time initialization.
 *
 * @note Case-insensitive comparison should be used when checking passwords
 */
inline constexpr std::array<std::string_view, 227> COMMON_PASSWORDS = {
    // Top 20 most common from breaches
    "password",
    "123456",
    "12345678",
    "1234",
    "qwerty",
    "12345",
    "dragon",
    "pussy",
    "baseball",
    "football",
    "letmein",
    "monkey",
    "696969",
    "abc123",
    "mustang",
    "michael",
    "shadow",
    "master",
    "jennifer",
    "111111",

    // Sequential numbers
    "123456789",
    "1234567890",
    "123123",
    "1234567",
    "123321",
    "654321",
    "0123456789",
    "987654321",
    "1111111",
    "11111111",
    "222222",
    "333333",
    "444444",
    "555555",
    "666666",
    "777777",
    "888888",
    "999999",
    "000000",
    "1234554321",

    // Keyboard patterns
    "qwertyuiop",
    "asdfghjkl",
    "zxcvbnm",
    "qwerty123",
    "1qaz2wsx",
    "qazwsx",
    "qweasd",
    "zxcvbn",
    "asdfgh",
    "qwertz",
    "azerty",
    "1qazxsw2",
    "zaq12wsx",
    "qwaszx",

    // Common words and phrases
    "welcome",
    "login",
    "admin",
    "adminadmin",
    "password1",
    "password123",
    "passw0rd",
    "p@ssw0rd",
    "p@ssword",
    "pass",
    "mypassword",
    "test",
    "testing",
    "guest",
    "user",
    "root",
    "default",
    "changeme",
    "secret",
    "trustno1",
    "iloveyou",

    // Sports and teams
    "football1",
    "baseball1",
    "basketball",
    "soccer",
    "hockey",
    "tennis",
    "golf",
    "swimming",
    "yankees",
    "cowboys",
    "lakers",
    "eagles",

    // Popular names
    "ashley",
    "bailey",
    "charlie",
    "daniel",
    "jessica",
    "matthew",
    "nicole",
    "robert",
    "amanda",
    "anthony",
    "justin",
    "melissa",
    "sarah",
    "andrew",
    "joshua",
    "brandon",
    "christopher",

    // Common animals
    "dolphin",
    "tigger",
    "butterfly",
    "purple",
    "maggie",
    "ranger",
    "buster",
    "sophie",
    "charlie1",
    "max",
    "tiger",
    "buddy",
    "pepper",
    "ginger",

    // Technology terms
    "computer",
    "internet",
    "windows",
    "samsung",
    "laptop",
    "android",
    "iphone",
    "google",
    "princess",
    "ninja",

    // Years (common patterns)
    "2000",
    "2001",
    "2002",
    "2003",
    "2004",
    "2005",
    "2006",
    "2007",
    "2008",
    "2009",
    "2010",
    "2011",
    "2012",
    "2013",
    "2014",
    "2015",
    "2016",
    "2017",
    "2018",
    "2019",
    "2020",
    "2021",
    "2022",
    "2023",
    "2024",
    "1990",
    "1991",
    "1992",
    "1993",
    "1994",
    "1995",
    "1996",
    "1997",
    "1998",
    "1999",

    // Leet speak variations
    "p4ssw0rd",
    "passw0rd1",
    "adm1n",
    "l3tm31n",
    "p455w0rd",
    "w3lc0m3",
    "h3ll0",
    "l0v3",

    // Simple phrases and patterns
    "sunshine",
    "princess1",
    "freedom",
    "whatever",
    "lovely",
    "incorrect",
    "flower",
    "cookie",
    "summer",
    "winter",
    "starwars",
    "superman",
    "batman",
    "spiderman",
    "pokemon",

    // Repeated characters
    "aaaaaaaa",
    "bbbbbbbb",
    "cccccccc",
    "dddddddd",
    "eeeeeeee",
    "ffffffff",
    "gggggggg",
    "hhhhhhhh",

    // Other common patterns
    "abcd1234",
    "1q2w3e4r",
    "1q2w3e4r5t",
    "q1w2e3r4",
    "password12",
    "welcome1",
    "welcome123",
    "monkey123",
    "dragon123",
};

/** @brief Check if password is in common passwords list
 *  @param password Password to check (case-insensitive)
 *  @return true if password is in the common passwords list */
inline bool is_common_password(std::string_view password) {
    // Convert to lowercase for case-insensitive comparison
    std::string lower_pass;
    lower_pass.reserve(password.length());
    for (char c : password) {
        lower_pass += std::tolower(static_cast<unsigned char>(c));
    }

    // Check exact match
    for (const auto& common : COMMON_PASSWORDS) {
        if (lower_pass == common) {
            return true;
        }
    }

    // Check if common password is contained within the password
    // Only for patterns >= 6 chars that are not single-character repetitions
    for (const auto& common : COMMON_PASSWORDS) {
        if (common.length() >= 6) {
            // Skip if common password is just repeating single character
            bool is_repetition = true;
            char first = common[0];
            for (size_t i = 1; i < common.length() && is_repetition; ++i) {
                if (common[i] != first) {
                    is_repetition = false;
                }
            }

            // Only do substring match for non-repetitions
            if (!is_repetition && lower_pass.find(common) != std::string::npos) {
                return true;
            }
        }
    }

    return false;
}} // namespace KeepTower

#endif // KEEPTOWER_COMMON_PASSWORDS_H
