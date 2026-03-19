// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "ImportExport.h"

namespace ImportExport {

std::string import_error_to_string(ImportError error) {
    switch (error) {
        case ImportError::FILE_NOT_FOUND:
            return "File not found";
        case ImportError::PARSE_ERROR:
            return "Failed to parse file format";
        case ImportError::INVALID_FORMAT:
            return "Invalid or corrupted file format";
        case ImportError::UNSUPPORTED_VERSION:
            return "Unsupported file version";
        case ImportError::EMPTY_FILE:
            return "File is empty";
        case ImportError::ENCRYPTION_ERROR:
            return "Failed to decrypt file";
        default:
            return "Unknown error";
    }
}

std::string export_error_to_string(ExportError error) {
    switch (error) {
        case ExportError::FILE_WRITE_ERROR:
            return "Failed to write file";
        case ExportError::INVALID_DATA:
            return "Invalid data to export";
        case ExportError::PERMISSION_DENIED:
            return "Permission denied";
        default:
            return "Unknown error";
    }
}
} // namespace ImportExport
