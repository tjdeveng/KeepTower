// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#ifndef IMPORT_EXPORT_H
#define IMPORT_EXPORT_H

#include "record.pb.h"
#include <vector>
#include <string>
#include <expected>

/**
 * @brief Import and export utilities for password data
 *
 * Supports multiple formats for data portability while maintaining security.
 */
namespace ImportExport {

/**
 * @brief Import error types
 */
enum class ImportError {
    FILE_NOT_FOUND,
    PARSE_ERROR,
    INVALID_FORMAT,
    UNSUPPORTED_VERSION,
    EMPTY_FILE,
    ENCRYPTION_ERROR
};

/**
 * @brief Export error types
 */
enum class ExportError {
    FILE_WRITE_ERROR,
    INVALID_DATA,
    PERMISSION_DENIED
};

/**
 * @brief Convert import error to human-readable string
 */
std::string import_error_to_string(ImportError error);

/**
 * @brief Convert export error to human-readable string
 */
std::string export_error_to_string(ExportError error);

/**
 * @brief Import accounts from CSV format
 * @param filepath Path to CSV file
 * @return Vector of account records or error
 *
 * Expected CSV format:
 * Account Name,Username,Password,Email,Website,Notes
 */
std::expected<std::vector<keeptower::AccountRecord>, ImportError>
import_from_csv(const std::string& filepath);

/**
 * @brief Export accounts to CSV format
 * @param filepath Path to output CSV file
 * @param accounts Vector of accounts to export
 * @return true on success, error on failure
 *
 * WARNING: CSV export is unencrypted. Use with caution.
 */
std::expected<void, ExportError>
export_to_csv(const std::string& filepath,
              const std::vector<keeptower::AccountRecord>& accounts);

/**
 * @brief Export accounts to KeePass 2.x XML format
 * @param filepath Path to output XML file
 * @param accounts Vector of accounts to export
 * @return void on success, error on failure
 *
 * WARNING: XML export is unencrypted. Use with caution.
 * NOTE: Not fully tested - KeePass import compatibility unverified.
 */
std::expected<void, ExportError>
export_to_keepass_xml(const std::string& filepath,
                      const std::vector<keeptower::AccountRecord>& accounts);

/**
 * @brief Export accounts to 1Password 1PIF format
 * @param filepath Path to output 1PIF file
 * @param accounts Vector of accounts to export
 * @return void on success, error on failure
 *
 * WARNING: 1PIF export is unencrypted. Use with caution.
 * NOTE: Not fully tested - 1Password import compatibility unverified.
 */
std::expected<void, ExportError>
export_to_1password_1pif(const std::string& filepath,
                         const std::vector<keeptower::AccountRecord>& accounts);

/**
 * @brief Import accounts from KeePass 2.x XML format
 * @param filepath Path to KeePass XML file
 * @return Vector of account records or error
 *
 * Supports KeePass 2.x unencrypted XML export format.
 */
std::expected<std::vector<keeptower::AccountRecord>, ImportError>
import_from_keepass_xml(const std::string& filepath);

/**
 * @brief Import accounts from 1Password 1PIF format
 * @param filepath Path to 1Password 1PIF file
 * @return Vector of account records or error
 *
 * Supports 1Password Interchange Format (1PIF).
 */
std::expected<std::vector<keeptower::AccountRecord>, ImportError>
import_from_1password(const std::string& filepath);

} // namespace ImportExport

#endif // IMPORT_EXPORT_H
