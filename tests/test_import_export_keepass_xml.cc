// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include <gtest/gtest.h>

#include "utils/ImportExport.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

class ImportExportKeePassXmlTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = fs::temp_directory_path() / "keeptower_import_export_keepass_xml_tests";
        fs::create_directories(test_dir);
        xml_path = (test_dir / "in.xml").string();
        out_path = (test_dir / "out.xml").string();
        big_path = (test_dir / "big.xml").string();
    }

    void TearDown() override {
        try {
            fs::remove_all(test_dir);
        } catch (...) {
        }
    }

    static void write_file(const std::string& path, const std::string& contents) {
        std::ofstream f(path, std::ios::binary);
        ASSERT_TRUE(f.is_open());
        f << contents;
        f.close();
    }

    static void write_sparse_file_over_100mb(const std::string& path) {
        std::ofstream f(path, std::ios::binary);
        ASSERT_TRUE(f.is_open());
        // Create a (likely sparse) file with apparent size > 100MB.
        f.seekp((100U * 1024U * 1024U) + 1U);
        f.put('x');
        f.close();
    }

    fs::path test_dir;
    std::string xml_path;
    std::string out_path;
    std::string big_path;
};

TEST_F(ImportExportKeePassXmlTest, ExportThenImport_RoundTripsCoreFields) {
    keeptower::AccountRecord r;
    r.set_account_name("Title & <tag> \"quoted\"");
    r.set_user_name("user&name");
    r.set_password("p<>&\"'");
    r.set_email("me@example.com");
    r.set_website("https://example.com?a=1&b=2");
    r.set_notes("note line 1\nnote line 2");

    std::vector<keeptower::AccountRecord> accounts;
    accounts.push_back(r);

    auto exp = ImportExport::export_to_keepass_xml(out_path, accounts);
    ASSERT_TRUE(exp.has_value());

    // Spot-check escaping is present in the output.
    {
        std::ifstream f(out_path, std::ios::binary);
        ASSERT_TRUE(f.is_open());
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        EXPECT_NE(content.find("&lt;tag&gt;"), std::string::npos);
        EXPECT_NE(content.find("&amp;"), std::string::npos);
        EXPECT_NE(content.find("&quot;"), std::string::npos);
        EXPECT_NE(content.find("&apos;"), std::string::npos);
    }

    auto imp = ImportExport::import_from_keepass_xml(out_path);
    ASSERT_TRUE(imp.has_value());
    ASSERT_EQ(imp->size(), 1u);

    const auto& got = (*imp)[0];
    EXPECT_EQ(got.account_name(), r.account_name());
    EXPECT_EQ(got.user_name(), r.user_name());
    EXPECT_EQ(got.password(), r.password());
    EXPECT_EQ(got.website(), r.website());

    // Email is embedded into Notes as "Email: ..." on export and re-extracted on import.
    EXPECT_EQ(got.email(), r.email());
    EXPECT_EQ(got.notes(), r.notes());
}

TEST_F(ImportExportKeePassXmlTest, ImportFromKeePassXml_EmptyFile_ReturnsEmptyFileError) {
    write_file(xml_path, "");

    auto imp = ImportExport::import_from_keepass_xml(xml_path);
    ASSERT_FALSE(imp.has_value());
    EXPECT_EQ(imp.error(), ImportExport::ImportError::EMPTY_FILE);
}

TEST_F(ImportExportKeePassXmlTest, ImportFromKeePassXml_FileOver100MB_IsRejected) {
    write_sparse_file_over_100mb(big_path);

    auto imp = ImportExport::import_from_keepass_xml(big_path);
    ASSERT_FALSE(imp.has_value());
    EXPECT_EQ(imp.error(), ImportExport::ImportError::INVALID_FORMAT);
}

TEST_F(ImportExportKeePassXmlTest, ImportFromKeePassXml_MalformedEntry_ReturnsEmptyFileError) {
    write_file(xml_path, "<KeePassFile><Root><Entry><String></String>");

    auto imp = ImportExport::import_from_keepass_xml(xml_path);
    ASSERT_FALSE(imp.has_value());
    EXPECT_EQ(imp.error(), ImportExport::ImportError::EMPTY_FILE);
}
