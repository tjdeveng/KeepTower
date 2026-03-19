// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "../ImportExport.h"

#include <ctime>
#include <fstream>
#include <string>
#include <vector>

#include <sys/stat.h>  // For chmod
#include <unistd.h>    // For fsync
#include <fcntl.h>     // For open

namespace ImportExport {

/** @brief Escape CSV field for RFC 4180 compliance
 *  @param field Field text to escape
 *  @return Escaped field with quotes if needed
 *  @note Adds quotes if field contains comma, quote, or newline */
static std::string escape_csv_field(const std::string& field) {
    if (field.find(',') == std::string::npos && field.find('"') == std::string::npos &&
        field.find('\n') == std::string::npos && field.find('\r') == std::string::npos) {
        return field;
    }

    std::string escaped;
    escaped.reserve(field.size() + 10);
    escaped += '"';

    for (char c : field) {
        if (c == '"') {
            escaped.append("\"\"");
        } else {
            escaped += c;
        }
    }

    escaped += '"';
    return escaped;
}

/** @brief Unescape CSV field (remove quotes and unescape double quotes)
 *  @param field Escaped CSV field
 *  @return Unescaped field text */
static std::string unescape_csv_field(const std::string& field) {
    if (field.empty()) {
        return field;
    }

    std::string unescaped;
    if (field.front() == '"' && field.back() == '"') {
        // Remove surrounding quotes
        std::string content = field.substr(1, field.length() - 2);
        // Replace double quotes with single quotes
        for (size_t i = 0; i < content.length(); ++i) {
            if (content[i] == '"' && i + 1 < content.length() && content[i + 1] == '"') {
                unescaped += '"';
                ++i;  // Skip next quote
            } else {
                unescaped += content[i];
            }
        }
        return unescaped;
    }

    return field;
}

/** @brief Parse CSV line into fields (respects quoted fields)
 *  @param line CSV line to parse
 *  @return Vector of unescaped field values */
static std::vector<std::string> parse_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::string current_field;
    bool in_quotes = false;

    for (size_t i = 0; i < line.length(); ++i) {
        char c = line[i];

        if (c == '"') {
            if (in_quotes && i + 1 < line.length() && line[i + 1] == '"') {
                current_field += '"';
                ++i;  // Skip next quote
            } else {
                in_quotes = !in_quotes;
                current_field += c;
            }
        } else if (c == ',' && !in_quotes) {
            fields.push_back(unescape_csv_field(current_field));
            current_field.clear();
        } else {
            current_field += c;
        }
    }

    // Add last field
    fields.push_back(unescape_csv_field(current_field));
    return fields;
}

static void strip_utf8_bom(std::string& s) {
    if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEF &&
        static_cast<unsigned char>(s[1]) == 0xBB && static_cast<unsigned char>(s[2]) == 0xBF) {
        s.erase(0, 3);
    }
}

std::expected<std::vector<keeptower::AccountRecord>, ImportError> import_from_csv(
    const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return std::unexpected(ImportError::FILE_NOT_FOUND);
        }

        std::vector<keeptower::AccountRecord> accounts;
        accounts.reserve(100);

        std::string line;
        bool first_line = true;

        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            // Skip header line
            if (first_line) {
                strip_utf8_bom(line);
                first_line = false;
                // Verify it's a header (contains "Account" or "Password")
                if (line.find("Account") == std::string::npos &&
                    line.find("Password") == std::string::npos) {
                    // No header, process as data
                    first_line = false;
                } else {
                    continue;  // Skip header
                }
            }

            // Skip empty lines
            if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos) {
                continue;
            }

            auto fields = parse_csv_line(line);

            // Require at least account name and password
            if (fields.size() < 3) {
                continue;  // Skip invalid lines
            }

            if (fields[0].empty() || fields[2].empty()) {
                continue;
            }

            keeptower::AccountRecord record;

            // Map fields: Account Name, Username, Password, Email, Website, Notes
            record.set_account_name(fields[0]);
            if (!fields[1].empty()) record.set_user_name(fields[1]);
            record.set_password(fields[2]);
            if (fields.size() > 3 && !fields[3].empty()) record.set_email(fields[3]);
            if (fields.size() > 4 && !fields[4].empty()) record.set_website(fields[4]);
            if (fields.size() > 5 && !fields[5].empty()) record.set_notes(fields[5]);

            // Set timestamps
            std::time_t now = std::time(nullptr);
            record.set_created_at(now);
            record.set_modified_at(now);
            record.set_password_changed_at(now);

            accounts.push_back(std::move(record));
        }

        if (accounts.empty()) {
            return std::unexpected(ImportError::EMPTY_FILE);
        }

        return accounts;
    } catch (const std::exception&) {
        return std::unexpected(ImportError::PARSE_ERROR);
    }
}

std::expected<void, ExportError> export_to_csv(
    const std::string& filepath, const std::vector<keeptower::AccountRecord>& accounts) {
    try {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            return std::unexpected(ExportError::FILE_WRITE_ERROR);
        }

        file << "Account Name,Username,Password,Email,Website,Notes\n";

        for (const auto& account : accounts) {
            file << escape_csv_field(account.account_name()) << ","
                 << escape_csv_field(account.user_name()) << ","
                 << escape_csv_field(account.password()) << ","
                 << escape_csv_field(account.email()) << ","
                 << escape_csv_field(account.website()) << ","
                 << escape_csv_field(account.notes()) << "\n";
        }

        file.flush();
        if (!file.good()) {
            return std::unexpected(ExportError::FILE_WRITE_ERROR);
        }

        file.close();

        // Set restrictive permissions (owner read/write only: 0600)
        if (chmod(filepath.c_str(), S_IRUSR | S_IWUSR) != 0) {
            // Permission change failed, but file was written
            // This is a warning condition, not a failure
        }

        // Sync to disk for critical data
        int fd = open(filepath.c_str(), O_RDONLY);
        if (fd >= 0) {
            fsync(fd);
            close(fd);
        }

        return {};
    } catch (const std::exception&) {
        return std::unexpected(ExportError::FILE_WRITE_ERROR);
    }
}

}  // namespace ImportExport
