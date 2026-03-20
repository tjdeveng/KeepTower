// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include <gtest/gtest.h>

#include "utils/FileDialogExtension.h"

#include <string>
#include <vector>

namespace {

TEST(FileDialogExtension, ExtFromGlobPattern) {
    EXPECT_EQ(KeepTower::FileDialogs::ext_from_glob_pattern("*.csv"), ".csv");
    EXPECT_EQ(KeepTower::FileDialogs::ext_from_glob_pattern("*.xml"), ".xml");
    EXPECT_EQ(KeepTower::FileDialogs::ext_from_glob_pattern("*.1pif"), ".1pif");
    EXPECT_EQ(KeepTower::FileDialogs::ext_from_glob_pattern("*"), "");
    EXPECT_EQ(KeepTower::FileDialogs::ext_from_glob_pattern("*.*"), "");
}

TEST(FileDialogExtension, EnsureFilenameExtension_ReplacesKnownExt) {
    const std::vector<std::string> known = {".csv", ".xml", ".1pif"};

    EXPECT_EQ(
        KeepTower::FileDialogs::ensure_filename_extension("passwords_export.csv", ".xml", known),
        "passwords_export.xml");

    EXPECT_EQ(
        KeepTower::FileDialogs::ensure_filename_extension("passwords_export.xml", ".1pif", known),
        "passwords_export.1pif");
}

TEST(FileDialogExtension, EnsureFilenameExtension_AppendsWhenNoExt) {
    const std::vector<std::string> known = {".csv", ".xml", ".1pif"};

    EXPECT_EQ(
        KeepTower::FileDialogs::ensure_filename_extension("passwords_export", ".csv", known),
        "passwords_export.csv");
}

TEST(FileDialogExtension, EnsureFilenameExtension_DoesNotOverrideUnknownExt) {
    const std::vector<std::string> known = {".csv", ".xml", ".1pif"};

    // If the user typed a different extension explicitly, keep it.
    EXPECT_EQ(
        KeepTower::FileDialogs::ensure_filename_extension("passwords_export.txt", ".xml", known),
        "passwords_export.txt");
}

TEST(FileDialogExtension, EnsurePathExtension_ReplacesKnownExt) {
    const std::vector<std::string> known = {".csv", ".xml", ".1pif"};

    EXPECT_EQ(
        KeepTower::FileDialogs::ensure_path_extension("/tmp/passwords_export.csv", ".xml", known),
        "/tmp/passwords_export.xml");
}

TEST(FileDialogExtension, EnsurePathExtension_AppendsWhenNoExt) {
    const std::vector<std::string> known = {".csv", ".xml", ".1pif"};

    EXPECT_EQ(
        KeepTower::FileDialogs::ensure_path_extension("/tmp/passwords_export", ".1pif", known),
        "/tmp/passwords_export.1pif");
}

TEST(FileDialogExtension, EnsurePathExtension_EmptyDesiredIsNoop) {
    const std::vector<std::string> known = {".csv", ".xml", ".1pif"};

    EXPECT_EQ(
        KeepTower::FileDialogs::ensure_path_extension("/tmp/passwords_export.csv", "", known),
        "/tmp/passwords_export.csv");
}

}  // namespace
