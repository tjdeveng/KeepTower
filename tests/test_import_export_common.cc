// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include <gtest/gtest.h>

#include "utils/ImportExport.h"

TEST(ImportExportCommonTest, ImportErrorToStringReturnsStableMessages) {
    EXPECT_EQ(ImportExport::import_error_to_string(ImportExport::ImportError::FILE_NOT_FOUND),
              "File not found");
    EXPECT_EQ(ImportExport::import_error_to_string(ImportExport::ImportError::PARSE_ERROR),
              "Failed to parse file format");
    EXPECT_EQ(ImportExport::import_error_to_string(ImportExport::ImportError::INVALID_FORMAT),
              "Invalid or corrupted file format");
    EXPECT_EQ(ImportExport::import_error_to_string(ImportExport::ImportError::UNSUPPORTED_VERSION),
              "Unsupported file version");
    EXPECT_EQ(ImportExport::import_error_to_string(ImportExport::ImportError::EMPTY_FILE),
              "File is empty");
    EXPECT_EQ(ImportExport::import_error_to_string(ImportExport::ImportError::ENCRYPTION_ERROR),
              "Failed to decrypt file");
}

TEST(ImportExportCommonTest, ImportErrorToStringFallsBackForUnknownValue) {
    const auto unknown = static_cast<ImportExport::ImportError>(999);
    EXPECT_EQ(ImportExport::import_error_to_string(unknown), "Unknown error");
}

TEST(ImportExportCommonTest, ExportErrorToStringReturnsStableMessages) {
    EXPECT_EQ(ImportExport::export_error_to_string(ImportExport::ExportError::FILE_WRITE_ERROR),
              "Failed to write file");
    EXPECT_EQ(ImportExport::export_error_to_string(ImportExport::ExportError::INVALID_DATA),
              "Invalid data to export");
    EXPECT_EQ(ImportExport::export_error_to_string(ImportExport::ExportError::PERMISSION_DENIED),
              "Permission denied");
}

TEST(ImportExportCommonTest, ExportErrorToStringFallsBackForUnknownValue) {
    const auto unknown = static_cast<ImportExport::ExportError>(999);
    EXPECT_EQ(ImportExport::export_error_to_string(unknown), "Unknown error");
}