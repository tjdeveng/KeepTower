// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng
//
// CommonPasswords.h - Comprehensive common password list
// Based on industry breach data and NIST SP 800-63B guidelines
// Sources: Have I Been Pwned, SplashData, NordPass annual reports

#ifndef KEEPTOWER_COMMON_PASSWORDS_H
#define KEEPTOWER_COMMON_PASSWORDS_H

#include <array>
#include <string_view>

namespace KeepTower {

// Common passwords from real-world breaches and patterns
// This list includes:
// - Top breached passwords (from Have I Been Pwned)
// - Sequential numbers and keyboard patterns
// - Common words and names
// - Leet speak variations
// - Year patterns
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

// Helper function to check if a password is in the common list
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
