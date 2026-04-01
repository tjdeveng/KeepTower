// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include <gtest/gtest.h>

#include "utils/ImportExport.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

class ImportExportCsvTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = fs::temp_directory_path() / "keeptower_import_export_csv_tests";
        fs::create_directories(test_dir);
        csv_path = (test_dir / "in.csv").string();
        out_path = (test_dir / "out.csv").string();
        big_path = (test_dir / "big.csv").string();
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
    std::string csv_path;
    std::string out_path;
    std::string big_path;
};

TEST_F(ImportExportCsvTest, ImportFromCsv_SkipsHeaderAndParsesQuotedFields) {
    // Contains commas and embedded quotes.
    write_file(csv_path,
               "Account Name,Username,Password,Email,Website,Notes\n"
               "\"My,Account\",\"user\"\"name\",\"p@ss,word\",e@x.com,https://example.com,\"note with \"\"quotes\"\"\"\n");

    auto result = ImportExport::import_from_csv(csv_path);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 1u);

    const auto& r = (*result)[0];
    EXPECT_EQ(r.account_name(), "My,Account");
    EXPECT_EQ(r.user_name(), "user\"name");
    EXPECT_EQ(r.password(), "p@ss,word");
    EXPECT_EQ(r.email(), "e@x.com");
    EXPECT_EQ(r.website(), "https://example.com");
    EXPECT_EQ(r.notes(), "note with \"quotes\"");
}

TEST_F(ImportExportCsvTest, ImportFromCsv_HandlesCRLF) {
    write_file(csv_path,
               "Account Name,Username,Password,Email,Website,Notes\r\n"
               "acc,user,pass,,,,\r\n");

    auto result = ImportExport::import_from_csv(csv_path);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 1u);
    EXPECT_EQ((*result)[0].account_name(), "acc");
    EXPECT_EQ((*result)[0].user_name(), "user");
    EXPECT_EQ((*result)[0].password(), "pass");
}

TEST_F(ImportExportCsvTest, ImportFromCsv_StripsUtf8BomOnFirstLine) {
    const std::string bom = std::string("\xEF\xBB\xBF", 3);

    write_file(csv_path,
               bom + "Account Name,Username,Password,Email,Website,Notes\n" +
                   "acc,user,pass,,,,\n");

    auto result = ImportExport::import_from_csv(csv_path);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 1u);
    EXPECT_EQ((*result)[0].account_name(), "acc");
}

TEST_F(ImportExportCsvTest, ImportFromCsv_SkipsRowsMissingAccountOrPassword) {
    write_file(csv_path,
               "Account Name,Username,Password,Email,Website,Notes\n"
               ",user,pass,,,,\n"
               "acc,user,,,,,\n"
               "acc2,user2,pass2,,,,\n");

    auto result = ImportExport::import_from_csv(csv_path);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 1u);
    EXPECT_EQ((*result)[0].account_name(), "acc2");
    EXPECT_EQ((*result)[0].password(), "pass2");
}

TEST_F(ImportExportCsvTest, ExportToCsv_EscapesCommaAndQuotes) {
    keeptower::AccountRecord r;
    r.set_account_name("My,Account");
    r.set_user_name("user\"name");
    r.set_password("p@ss");

    std::vector<keeptower::AccountRecord> accounts;
    accounts.push_back(r);

    auto exp = ImportExport::export_to_csv(out_path, accounts);
    ASSERT_TRUE(exp.has_value());

    std::ifstream f(out_path, std::ios::binary);
    ASSERT_TRUE(f.is_open());

    std::string header;
    std::getline(f, header);
    EXPECT_EQ(header, "Account Name,Username,Password,Email,Website,Notes");

    std::string line;
    std::getline(f, line);
    EXPECT_EQ(line, "\"My,Account\",\"user\"\"name\",p@ss,,,");
}

TEST_F(ImportExportCsvTest, ImportFromCsv_FileOver100MB_IsRejected) {
    write_sparse_file_over_100mb(big_path);

    auto imp = ImportExport::import_from_csv(big_path);
    ASSERT_FALSE(imp.has_value());
    EXPECT_EQ(imp.error(), ImportExport::ImportError::INVALID_FORMAT);
}
