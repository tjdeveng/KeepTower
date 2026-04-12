// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include <gtest/gtest.h>

#include "utils/ImportExport.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

class ImportExport1PifTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = fs::temp_directory_path() / "keeptower_import_export_1pif_tests";
        fs::create_directories(test_dir);
        pif_path = (test_dir / "in.1pif").string();
        out_path = (test_dir / "out.1pif").string();
        big_path = (test_dir / "big.1pif").string();
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
    std::string pif_path;
    std::string out_path;
    std::string big_path;
};

TEST_F(ImportExport1PifTest, ImportFrom1Password_ParsesCoreFieldsAndEmailFromNotes) {
    write_file(pif_path,
               "{\"uuid\":\"u1\",\"category\":\"001\",\"title\":\"My & Title\","
               "\"secureContents\":{\"fields\":["
               "{\"value\":\"user1\",\"name\":\"username\",\"type\":\"T\",\"designation\":\"username\"},"
               "{\"value\":\"p<ass&\",\"name\":\"password\",\"type\":\"P\",\"designation\":\"password\"}],"
               "\"URLs\":[{\"url\":\"https://example.com?a=1&b=2\"}],"
               "\"notesPlain\":\"Email: me@example.com\\n\\nhello\",\"htmlForm\":null}}\n"
               "***5642bee8-a5ff-11dc-8314-0800200c9a66***\n");

    auto imp = ImportExport::import_from_1password(pif_path);
    ASSERT_TRUE(imp.has_value());
    ASSERT_EQ(imp->size(), 1u);

    const auto& got = (*imp)[0];
    EXPECT_EQ(got.account_name(), "My & Title");
    EXPECT_EQ(got.user_name(), "user1");
    EXPECT_EQ(got.password(), "p<ass&");
    EXPECT_EQ(got.website(), "https://example.com?a=1&b=2");
    EXPECT_EQ(got.email(), "me@example.com");
    EXPECT_EQ(got.notes(), "hello");
}

TEST_F(ImportExport1PifTest, ImportFrom1Password_LegacyKeepTowerXmlEntities_AreUnescaped) {
    write_file(pif_path,
               "{\"uuid\":\"generated-uuid-123\",\"category\":\"001\",\"title\":\"My &amp; Title\","
               "\"secureContents\":{\"fields\":["
               "{\"value\":\"user1\",\"name\":\"username\",\"type\":\"T\",\"designation\":\"username\"},"
               "{\"value\":\"p&lt;ass&amp;\",\"name\":\"password\",\"type\":\"P\",\"designation\":\"password\"}],"
               "\"URLs\":[{\"url\":\"https://example.com?a=1&amp;b=2\"}],"
               "\"notesPlain\":\"Email: me@example.com\\n\\nhello &amp; bye\",\"htmlForm\":null}}\n"
               "***5642bee8-a5ff-11dc-8314-0800200c9a66***\n");

    auto imp = ImportExport::import_from_1password(pif_path);
    ASSERT_TRUE(imp.has_value());
    ASSERT_EQ(imp->size(), 1u);

    const auto& got = (*imp)[0];
    EXPECT_EQ(got.account_name(), "My & Title");
    EXPECT_EQ(got.password(), "p<ass&");
    EXPECT_EQ(got.website(), "https://example.com?a=1&b=2");
    EXPECT_EQ(got.notes(), "hello & bye");
}

TEST_F(ImportExport1PifTest, ExportTo1Password1pif_WritesSeparatorAndEscapesJsonStrings) {
    keeptower::AccountRecord r;
    r.set_account_name("My & <Title>");
    r.set_user_name("user\"name");
    r.set_password("p<>&\"'");
    r.set_website("https://example.com?a=1&b=2");
    r.set_email("me@example.com");
    r.set_notes("single-line");

    std::vector<keeptower::AccountRecord> accounts;
    accounts.push_back(r);

    auto exp = ImportExport::export_to_1password_1pif(out_path, accounts);
    ASSERT_TRUE(exp.has_value());

    std::ifstream f(out_path, std::ios::binary);
    ASSERT_TRUE(f.is_open());
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    EXPECT_NE(content.find("***5642bee8-a5ff-11dc-8314-0800200c9a66***"), std::string::npos);

    // JSON escaping expectations
    EXPECT_NE(content.find("user\\\"name"), std::string::npos);
    EXPECT_NE(content.find("p<>&\\\"'"), std::string::npos);
    EXPECT_NE(content.find("Email: me@example.com\\n\\nsingle-line"), std::string::npos);
}

TEST_F(ImportExport1PifTest, ExportThenImport_MultilineNotes_RoundTripsNotesAndEmail) {
    keeptower::AccountRecord r;
    r.set_account_name("Title");
    r.set_user_name("user");
    r.set_password("pass");
    r.set_website("https://example.com");
    r.set_email("me@example.com");
    r.set_notes("line1\nline2");

    std::vector<keeptower::AccountRecord> accounts;
    accounts.push_back(r);

    auto exp = ImportExport::export_to_1password_1pif(out_path, accounts);
    ASSERT_TRUE(exp.has_value());

    auto imp = ImportExport::import_from_1password(out_path);
    ASSERT_TRUE(imp.has_value());
    ASSERT_EQ(imp->size(), 1u);

    const auto& got = (*imp)[0];
    EXPECT_EQ(got.account_name(), r.account_name());
    EXPECT_EQ(got.user_name(), r.user_name());
    EXPECT_EQ(got.password(), r.password());
    EXPECT_EQ(got.website(), r.website());
    EXPECT_EQ(got.email(), r.email());
    EXPECT_EQ(got.notes(), r.notes());
}

TEST_F(ImportExport1PifTest, ImportFrom1Password_EmptyFile_ReturnsEmptyFileError) {
    write_file(pif_path, "");

    auto imp = ImportExport::import_from_1password(pif_path);
    ASSERT_FALSE(imp.has_value());
    EXPECT_EQ(imp.error(), ImportExport::ImportError::EMPTY_FILE);
}

TEST_F(ImportExport1PifTest, ImportFrom1Password_FileOver100MB_IsRejected) {
    write_sparse_file_over_100mb(big_path);

    auto imp = ImportExport::import_from_1password(big_path);
    ASSERT_FALSE(imp.has_value());
    EXPECT_EQ(imp.error(), ImportExport::ImportError::INVALID_FORMAT);
}

TEST_F(ImportExport1PifTest, ImportFrom1Password_FileNotFound_ReturnsNotFound) {
    auto imp = ImportExport::import_from_1password((test_dir / "missing.1pif").string());
    ASSERT_FALSE(imp.has_value());
    EXPECT_EQ(imp.error(), ImportExport::ImportError::FILE_NOT_FOUND);
}

TEST_F(ImportExport1PifTest, ExportTo1Password1pif_DirectoryPathReturnsWriteError) {
    std::vector<keeptower::AccountRecord> accounts;
    keeptower::AccountRecord record;
    record.set_account_name("Any");
    accounts.push_back(record);

    auto exp = ImportExport::export_to_1password_1pif(test_dir.string(), accounts);
    ASSERT_FALSE(exp.has_value());
    EXPECT_EQ(exp.error(), ImportExport::ExportError::FILE_WRITE_ERROR);
}

TEST_F(ImportExport1PifTest, ImportFrom1Password_ParsesUnicodeAndEscapedSequences) {
    write_file(pif_path,
               "{\"uuid\":\"u1\",\"title\":\"A \\u00DF \\u4F60\\u597D \\uD83D\\uDE00\","
               "\"secureContents\":{\"fields\":["
               "{\"value\":\"user\\tname\",\"designation\":\"username\"},"
               "{\"value\":\"p\\\\\\/word\",\"designation\":\"password\"}]}}\n"
               "***5642bee8-a5ff-11dc-8314-0800200c9a66***\n");

    auto imp = ImportExport::import_from_1password(pif_path);
    ASSERT_TRUE(imp.has_value());
    ASSERT_EQ(imp->size(), 1u);

    const std::string expected_title = std::string("A ") + "\xC3\x9F" + " " +
                                       "\xE4\xBD\xA0\xE5\xA5\xBD" + " " + "\xF0\x9F\x98\x80";
    EXPECT_EQ((*imp)[0].account_name(), expected_title);
    EXPECT_EQ((*imp)[0].user_name(), "user\tname");
    EXPECT_EQ((*imp)[0].password(), "p\\/word");
}

TEST_F(ImportExport1PifTest, ImportFrom1Password_ReplacesInvalidHighSurrogate) {
    write_file(pif_path,
               "{\"uuid\":\"u1\",\"title\":\"bad-\\uD83DX\","
               "\"secureContents\":{\"fields\":[]}}\n"
               "***5642bee8-a5ff-11dc-8314-0800200c9a66***\n");

    auto imp = ImportExport::import_from_1password(pif_path);
    ASSERT_TRUE(imp.has_value());
    ASSERT_EQ(imp->size(), 1u);

    const std::string expected = std::string("bad-") + "\xEF\xBF\xBD" + "X";
    EXPECT_EQ((*imp)[0].account_name(), expected);
}

TEST_F(ImportExport1PifTest, ImportFrom1Password_InvalidUnicodeEscapeYieldsEmptyFile) {
    write_file(pif_path,
               "{\"uuid\":\"u1\",\"title\":\"bad-\\u12G4\","
               "\"secureContents\":{\"fields\":[]}}\n"
               "***5642bee8-a5ff-11dc-8314-0800200c9a66***\n");

    auto imp = ImportExport::import_from_1password(pif_path);
    ASSERT_FALSE(imp.has_value());
    EXPECT_EQ(imp.error(), ImportExport::ImportError::EMPTY_FILE);
}

TEST_F(ImportExport1PifTest, ImportFrom1Password_UnknownEscapeIsKeptAsLiteralCharacter) {
    write_file(pif_path,
               "{\"uuid\":\"u1\",\"title\":\"name\\qvalue\","
               "\"secureContents\":{\"fields\":[]}}\n"
               "***5642bee8-a5ff-11dc-8314-0800200c9a66***\n");

    auto imp = ImportExport::import_from_1password(pif_path);
    ASSERT_TRUE(imp.has_value());
    ASSERT_EQ(imp->size(), 1u);
    EXPECT_EQ((*imp)[0].account_name(), "nameqvalue");
}

TEST_F(ImportExport1PifTest, ImportFrom1Password_HandlesSeparatorsWhitespaceAndMultipleRecords) {
    write_file(pif_path,
               "***5642bee8-a5ff-11dc-8314-0800200c9a66***\n"
               "   \n"
               "{\"uuid\":\"u1\",\"title\":\"One\",\"secureContents\":{\"fields\":[]}}\n"
               "***5642bee8-a5ff-11dc-8314-0800200c9a66***\n"
               "\n"
               "{\"uuid\":\"u2\",\"title\":\"Two\",\"secureContents\":{\"fields\":[]}}\n"
               "***5642bee8-a5ff-11dc-8314-0800200c9a66***\n");

    auto imp = ImportExport::import_from_1password(pif_path);
    ASSERT_TRUE(imp.has_value());
    ASSERT_EQ(imp->size(), 2u);
    EXPECT_EQ((*imp)[0].account_name(), "One");
    EXPECT_EQ((*imp)[1].account_name(), "Two");
}

TEST_F(ImportExport1PifTest, ImportFrom1Password_NotesEmailOnlyAndPlainNotesBranches) {
    write_file(pif_path,
               "{\"uuid\":\"u1\",\"title\":\"EmailOnly\","
               "\"secureContents\":{\"fields\":[],\"notesPlain\":\"Email: one@example.com\"}}\n"
               "***5642bee8-a5ff-11dc-8314-0800200c9a66***\n"
               "{\"uuid\":\"u2\",\"title\":\"PlainNotes\","
               "\"secureContents\":{\"fields\":[],\"notesPlain\":\"just-notes\"}}\n"
               "***5642bee8-a5ff-11dc-8314-0800200c9a66***\n");

    auto imp = ImportExport::import_from_1password(pif_path);
    ASSERT_TRUE(imp.has_value());
    ASSERT_EQ(imp->size(), 2u);
    EXPECT_EQ((*imp)[0].email(), "one@example.com");
    EXPECT_TRUE((*imp)[0].notes().empty());
    EXPECT_EQ((*imp)[1].notes(), "just-notes");
}

TEST_F(ImportExport1PifTest, ImportFrom1Password_NonLegacyUuidDoesNotUnescapeXmlEntities) {
    write_file(pif_path,
               "{\"uuid\":\"external-u1\",\"title\":\"A &amp; B\","
               "\"secureContents\":{\"fields\":["
               "{\"value\":\"u&amp;ser\",\"designation\":\"username\"}]}}\n"
               "***5642bee8-a5ff-11dc-8314-0800200c9a66***\n");

    auto imp = ImportExport::import_from_1password(pif_path);
    ASSERT_TRUE(imp.has_value());
    ASSERT_EQ(imp->size(), 1u);
    EXPECT_EQ((*imp)[0].account_name(), "A &amp; B");
    EXPECT_EQ((*imp)[0].user_name(), "u&amp;ser");
}

TEST_F(ImportExport1PifTest, ExportTo1Password1pif_EscapesControlCharactersAndOptionalFields) {
    keeptower::AccountRecord r;
    r.set_account_name("Ctrl");
    r.set_user_name(std::string("u") + '\b' + '\f' + '\n' + '\r' + '\t' + static_cast<char>(0x01));
    r.set_password("p");
    r.set_notes("n");

    std::vector<keeptower::AccountRecord> accounts;
    accounts.push_back(r);

    auto exp = ImportExport::export_to_1password_1pif(out_path, accounts);
    ASSERT_TRUE(exp.has_value());

    std::ifstream f(out_path, std::ios::binary);
    ASSERT_TRUE(f.is_open());
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    EXPECT_NE(content.find("\\b"), std::string::npos);
    EXPECT_NE(content.find("\\f"), std::string::npos);
    EXPECT_NE(content.find("\\n"), std::string::npos);
    EXPECT_NE(content.find("\\r"), std::string::npos);
    EXPECT_NE(content.find("\\t"), std::string::npos);
    EXPECT_NE(content.find("\\u0001"), std::string::npos);
    EXPECT_EQ(content.find("\"URLs\""), std::string::npos);
}

TEST_F(ImportExport1PifTest, ExportTo1Password1pif_EscapesBackslashAndQuote) {
    keeptower::AccountRecord r;
    r.set_account_name("Q\\\"Name");
    r.set_user_name("u");
    r.set_password("p");

    std::vector<keeptower::AccountRecord> accounts{r};
    auto exp = ImportExport::export_to_1password_1pif(out_path, accounts);
    ASSERT_TRUE(exp.has_value());

    std::ifstream f(out_path, std::ios::binary);
    ASSERT_TRUE(f.is_open());
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    EXPECT_NE(content.find("Q\\\\\\\"Name"), std::string::npos);
}

TEST_F(ImportExport1PifTest, ImportFrom1Password_DecodesQuoteBackspaceFormfeedAndCarriageReturnEscapes) {
    write_file(pif_path,
               "{\"uuid\":\"u1\",\"title\":\"a\\\"b\\b\\f\\r\","
               "\"secureContents\":{\"fields\":[]}}\n"
               "***5642bee8-a5ff-11dc-8314-0800200c9a66***\n");

    auto imp = ImportExport::import_from_1password(pif_path);
    ASSERT_TRUE(imp.has_value());
    ASSERT_EQ(imp->size(), 1u);

    const std::string expected = std::string("a\"b") + '\b' + '\f' + '\r';
    EXPECT_EQ((*imp)[0].account_name(), expected);
}

TEST_F(ImportExport1PifTest, ImportFrom1Password_LastRecordWithoutSeparatorIsAccepted) {
    write_file(pif_path,
               "{\"uuid\":\"u1\",\"title\":\"NoFinalSeparator\","
               "\"secureContents\":{\"fields\":[]}}\n");

    auto imp = ImportExport::import_from_1password(pif_path);
    ASSERT_TRUE(imp.has_value());
    ASSERT_EQ(imp->size(), 1u);
    EXPECT_EQ((*imp)[0].account_name(), "NoFinalSeparator");
}

TEST_F(ImportExport1PifTest, ImportFrom1Password_MalformedJsonStringValuesAreSkipped) {
    write_file(pif_path,
               "{\"uuid\":\"u1\",\"title\":\"bad\\\",\"secureContents\":{\"fields\":[]}}\n"
               "***5642bee8-a5ff-11dc-8314-0800200c9a66***\n"
               "{\"uuid\":\"u2\",\"title\":\"good\",\"secureContents\":{\"fields\":[]}}\n"
               "***5642bee8-a5ff-11dc-8314-0800200c9a66***\n");

    auto imp = ImportExport::import_from_1password(pif_path);
    ASSERT_TRUE(imp.has_value());
    EXPECT_TRUE(std::any_of(imp->begin(), imp->end(), [](const keeptower::AccountRecord& record) {
        return record.account_name() == "good";
    }));
}
