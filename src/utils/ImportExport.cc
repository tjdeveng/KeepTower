// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "ImportExport.h"
#include <fstream>
#include <sstream>
#include <ctime>
#include <sys/stat.h>  // For chmod
#include <unistd.h>    // For fsync
#include <fcntl.h>     // For open

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

// Helper: Escape CSV field (handle commas, quotes, newlines)
static std::string escape_csv_field(const std::string& field) {
    if (field.find(',') == std::string::npos &&
        field.find('"') == std::string::npos &&
        field.find('\n') == std::string::npos &&
        field.find('\r') == std::string::npos) {
        return field;
    }

    std::string escaped;
    escaped.reserve(field.size() + 10);  // Pre-allocate (field size + quotes + some escapes)
    escaped += '"';

    for (char c : field) {
        if (c == '"') {
            escaped.append("\"\"");  // Double quotes to escape
        } else {
            escaped += c;
        }
    }

    escaped += '"';
    return escaped;
}

// Helper: Unescape CSV field
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

// Helper: Parse CSV line considering quoted fields
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

std::expected<std::vector<keeptower::AccountRecord>, ImportError>
import_from_csv(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return std::unexpected(ImportError::FILE_NOT_FOUND);
        }

        std::vector<keeptower::AccountRecord> accounts;
        accounts.reserve(100);  // Pre-allocate for typical case
        std::string line;
        bool first_line = true;

    while (std::getline(file, line)) {
        // Skip header line
        if (first_line) {
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
        if (fields.size() < 2) {
            continue;  // Skip invalid lines
        }

        keeptower::AccountRecord record;

        // Map fields: Account Name, Username, Password, Email, Website, Notes
        if (!fields[0].empty()) record.set_account_name(fields[0]);
        if (fields.size() > 1 && !fields[1].empty()) record.set_user_name(fields[1]);
        if (fields.size() > 2 && !fields[2].empty()) record.set_password(fields[2]);
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
    } catch (const std::exception& e) {
        // Handle any exceptions (bad_alloc, parse errors, etc.)
        return std::unexpected(ImportError::PARSE_ERROR);
    }
}

std::expected<void, ExportError>
export_to_csv(const std::string& filepath,
              const std::vector<keeptower::AccountRecord>& accounts) {
    try {
        // Open file with C++ stream
        std::ofstream file(filepath);
        if (!file.is_open()) {
            return std::unexpected(ExportError::FILE_WRITE_ERROR);
        }

        // Write header
        file << "Account Name,Username,Password,Email,Website,Notes\n";

        // Write each account
        for (const auto& account : accounts) {
            file << escape_csv_field(account.account_name()) << ","
                 << escape_csv_field(account.user_name()) << ","
                 << escape_csv_field(account.password()) << ","
                 << escape_csv_field(account.email()) << ","
                 << escape_csv_field(account.website()) << ","
                 << escape_csv_field(account.notes()) << "\n";
        }

        // Flush to ensure all data is written
        file.flush();

        if (!file.good()) {
            return std::unexpected(ExportError::FILE_WRITE_ERROR);
        }

        // Close file (RAII ensures cleanup)
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
    } catch (const std::exception& e) {
        // Handle any exceptions (bad_alloc, etc.)
        return std::unexpected(ExportError::FILE_WRITE_ERROR);
    }
}

// Helper: Escape XML special characters
static std::string escape_xml(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size() + 20);

    for (char c : text) {
        switch (c) {
            case '<':  escaped.append("&lt;"); break;
            case '>':  escaped.append("&gt;"); break;
            case '&':  escaped.append("&amp;"); break;
            case '"':  escaped.append("&quot;"); break;
            case '\'': escaped.append("&apos;"); break;
            default:   escaped += c; break;
        }
    }

    return escaped;
}

// Helper: Get current timestamp in ISO 8601 format
static std::string get_iso_timestamp() {
    std::time_t now = std::time(nullptr);
    char buffer[25];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));
    return std::string(buffer);
}

std::expected<void, ExportError>
export_to_keepass_xml(const std::string& filepath,
                      const std::vector<keeptower::AccountRecord>& accounts) {
    try {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            return std::unexpected(ExportError::FILE_WRITE_ERROR);
        }

        std::string timestamp = get_iso_timestamp();

        // Write KeePass 2.x XML header
        file << R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)" << "\n";
        file << R"(<KeePassFile>)" << "\n";
        file << R"(  <Meta>)" << "\n";
        file << R"(    <Generator>KeepTower Password Manager</Generator>)" << "\n";
        file << R"(    <DatabaseName>KeepTower Export</DatabaseName>)" << "\n";
        file << "    <DatabaseDescription>Exported on " << timestamp << "</DatabaseDescription>\n";
        file << R"(  </Meta>)" << "\n";
        file << R"(  <Root>)" << "\n";
        file << R"(    <Group>)" << "\n";
        file << R"(      <Name>Root</Name>)" << "\n";
        file << R"(      <IconID>48</IconID>)" << "\n";

        // Write each account as an entry
        for (const auto& account : accounts) {
            file << R"(      <Entry>)" << "\n";

            // Title
            file << R"(        <String>)" << "\n";
            file << R"(          <Key>Title</Key>)" << "\n";
            file << "          <Value>" << escape_xml(account.account_name()) << "</Value>\n";
            file << R"(        </String>)" << "\n";

            // UserName
            file << R"(        <String>)" << "\n";
            file << R"(          <Key>UserName</Key>)" << "\n";
            file << "          <Value>" << escape_xml(account.user_name()) << "</Value>\n";
            file << R"(        </String>)" << "\n";

            // Password
            file << R"(        <String>)" << "\n";
            file << R"(          <Key>Password</Key>)" << "\n";
            file << "          <Value>" << escape_xml(account.password()) << "</Value>\n";
            file << R"(        </String>)" << "\n";

            // URL
            if (!account.website().empty()) {
                file << R"(        <String>)" << "\n";
                file << R"(          <Key>URL</Key>)" << "\n";
                file << "          <Value>" << escape_xml(account.website()) << "</Value>\n";
                file << R"(        </String>)" << "\n";
            }

            // Notes (combining email and notes)
            std::string notes;
            if (!account.email().empty()) {
                notes = "Email: " + account.email();
                if (!account.notes().empty()) {
                    notes += "\n\n" + account.notes();
                }
            } else {
                notes = account.notes();
            }

            if (!notes.empty()) {
                file << R"(        <String>)" << "\n";
                file << R"(          <Key>Notes</Key>)" << "\n";
                file << "          <Value>" << escape_xml(notes) << "</Value>\n";
                file << R"(        </String>)" << "\n";
            }

            // Timestamps
            file << "        <Times>\n";
            file << "          <LastModificationTime>" << timestamp << "</LastModificationTime>\n";
            file << "          <CreationTime>" << timestamp << "</CreationTime>\n";
            file << "          <LastAccessTime>" << timestamp << "</LastAccessTime>\n";
            file << "          <ExpiryTime>2999-12-31T23:59:59Z</ExpiryTime>\n";
            file << "          <Expires>False</Expires>\n";
            file << "        </Times>\n";

            file << R"(      </Entry>)" << "\n";
        }

        file << R"(    </Group>)" << "\n";
        file << R"(  </Root>)" << "\n";
        file << R"(</KeePassFile>)" << "\n";

        file.flush();
        if (!file.good()) {
            return std::unexpected(ExportError::FILE_WRITE_ERROR);
        }
        file.close();

        // Set restrictive permissions (0600)
        chmod(filepath.c_str(), S_IRUSR | S_IWUSR);

        // Sync to disk
        int fd = open(filepath.c_str(), O_RDONLY);
        if (fd >= 0) {
            fsync(fd);
            close(fd);
        }

        return {};
    } catch (const std::exception& e) {
        return std::unexpected(ExportError::FILE_WRITE_ERROR);
    }
}

std::expected<void, ExportError>
export_to_1password_1pif(const std::string& filepath,
                         const std::vector<keeptower::AccountRecord>& accounts) {
    try {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            return std::unexpected(ExportError::FILE_WRITE_ERROR);
        }

        // 1PIF is a JSON-based format with one entry per line
        for (const auto& account : accounts) {
            file << R"({"uuid":")" << "generated-uuid-" << std::hash<std::string>{}(account.account_name()) << "\",";
            file << R"("category":"001",)";  // 001 = Login category
            file << R"("title":")" << escape_xml(account.account_name()) << "\",";
            file << R"("secureContents":{)";

            // Fields
            file << R"("fields":[)";

            // Username
            file << R"({"value":")" << escape_xml(account.user_name()) << R"(","name":"username","type":"T","designation":"username"},)";

            // Password
            file << R"({"value":")" << escape_xml(account.password()) << R"(","name":"password","type":"P","designation":"password"})";

            file << R"(],)";

            // URLs
            if (!account.website().empty()) {
                file << R"("URLs":[{"url":")" << escape_xml(account.website()) << R"("}],)";
            }

            // Notes (including email)
            std::string notes;
            if (!account.email().empty()) {
                notes = "Email: " + account.email();
                if (!account.notes().empty()) {
                    notes += "\n\n" + account.notes();
                }
            } else {
                notes = account.notes();
            }

            if (!notes.empty()) {
                file << R"("notesPlain":")" << escape_xml(notes) << R"(",)";
            }

            file << R"("htmlForm":null}})";
            file << "***5642bee8-a5ff-11dc-8314-0800200c9a66***\n";
        }

        file.flush();
        if (!file.good()) {
            return std::unexpected(ExportError::FILE_WRITE_ERROR);
        }
        file.close();

        // Set restrictive permissions (0600)
        chmod(filepath.c_str(), S_IRUSR | S_IWUSR);

        // Sync to disk
        int fd = open(filepath.c_str(), O_RDONLY);
        if (fd >= 0) {
            fsync(fd);
            close(fd);
        }

        return {};
    } catch (const std::exception& e) {
        return std::unexpected(ExportError::FILE_WRITE_ERROR);
    }
}

// Helper: Extract text between XML tags (simple parser for our exported format)
static std::string extract_xml_value(std::string_view xml, std::string_view tag) {
    std::string open_tag = std::format("<{}>", tag);
    std::string close_tag = std::format("</{}>", tag);

    size_t start = xml.find(open_tag);
    if (start == std::string::npos) {
        return "";
    }
    start += open_tag.length();

    size_t end = xml.find(close_tag, start);
    if (end == std::string::npos) {
        return "";
    }

    return std::string(xml.substr(start, end - start));
}

// Helper: Unescape XML entities
static std::string unescape_xml(const std::string& text) {
    std::string result = text;

    size_t pos = 0;
    while ((pos = result.find("&lt;", pos)) != std::string::npos) {
        result.replace(pos, 4, "<");
        pos += 1;
    }

    pos = 0;
    while ((pos = result.find("&gt;", pos)) != std::string::npos) {
        result.replace(pos, 4, ">");
        pos += 1;
    }

    pos = 0;
    while ((pos = result.find("&amp;", pos)) != std::string::npos) {
        result.replace(pos, 5, "&");
        pos += 1;
    }

    pos = 0;
    while ((pos = result.find("&quot;", pos)) != std::string::npos) {
        result.replace(pos, 6, "\"");
        pos += 1;
    }

    pos = 0;
    while ((pos = result.find("&apos;", pos)) != std::string::npos) {
        result.replace(pos, 6, "'");
        pos += 1;
    }

    return result;
}

std::expected<std::vector<keeptower::AccountRecord>, ImportError>
import_from_keepass_xml(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return std::unexpected(ImportError::FILE_NOT_FOUND);
        }

        // Check file size for security (prevent DoS with huge files)
        file.seekg(0, std::ios::end);
        auto file_size = file.tellg();
        if (file_size < 0 || file_size > 100 * 1024 * 1024) {  // 100MB limit
            return std::unexpected(ImportError::INVALID_FORMAT);
        }
        file.seekg(0, std::ios::beg);

        // Read entire file
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        std::vector<keeptower::AccountRecord> accounts;
        // Estimate: average entry ~500 bytes
        accounts.reserve(std::min<size_t>(content.size() / 500 + 1, 10000));

        // Simple XML parsing - look for <Entry> blocks
        size_t pos = 0;
        while ((pos = content.find("<Entry>", pos)) != std::string::npos) {
            size_t end = content.find("</Entry>", pos);
            if (end == std::string::npos) {
                break;
            }

            std::string_view entry{content.data() + pos, end - pos + 8};

            // Extract fields from String elements
            keeptower::AccountRecord account;

            // Parse each String element
            size_t string_pos = 0;
            while ((string_pos = entry.find("<String>", string_pos)) != std::string::npos) {
                size_t string_end = entry.find("</String>", string_pos);
                if (string_end == std::string::npos) break;

                std::string_view string_block = entry.substr(string_pos, string_end - string_pos + 9);
                std::string key = unescape_xml(extract_xml_value(string_block, "Key"));
                std::string value = unescape_xml(extract_xml_value(string_block, "Value"));

                if (key == "Title") {
                    account.set_account_name(value);
                } else if (key == "UserName") {
                    account.set_user_name(value);
                } else if (key == "Password") {
                    account.set_password(value);
                } else if (key == "URL") {
                    account.set_website(value);
                } else if (key == "Notes") {
                    // Check if notes contain email
                    if (value.starts_with("Email: ")) {
                        size_t newline = value.find("\n\n");
                        if (newline != std::string::npos) {
                            account.set_email(value.substr(7, newline - 7));
                            account.set_notes(value.substr(newline + 2));
                        } else {
                            account.set_email(value.substr(7));
                        }
                    } else {
                        account.set_notes(value);
                    }
                }

                string_pos = string_end + 9;
            }

            accounts.emplace_back(std::move(account));
            pos = end + 8;
        }

        if (accounts.empty()) {
            return std::unexpected(ImportError::EMPTY_FILE);
        }

        return accounts;
    } catch (const std::exception& e) {
        return std::unexpected(ImportError::PARSE_ERROR);
    }
}

std::expected<std::vector<keeptower::AccountRecord>, ImportError>
import_from_1password(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return std::unexpected(ImportError::FILE_NOT_FOUND);
        }

        // Check file size for security (prevent DoS with huge files)
        file.seekg(0, std::ios::end);
        auto file_size = file.tellg();
        if (file_size < 0 || file_size > 100 * 1024 * 1024) {  // 100MB limit
            return std::unexpected(ImportError::INVALID_FORMAT);
        }
        file.seekg(0, std::ios::beg);

        std::vector<keeptower::AccountRecord> accounts;
        accounts.reserve(100);

        // Read line by line (1PIF has one JSON object per line)
        std::string line;
        while (std::getline(file, line)) {
            // Skip separator lines
            if (line.find("***") != std::string::npos) {
                continue;
            }

            if (line.empty()) {
                continue;
            }

            // Simple JSON parsing for our known format
            keeptower::AccountRecord account;

            // Extract title
            size_t title_pos = line.find("\"title\":\"");
            if (title_pos != std::string::npos) {
                size_t start = title_pos + 9;
                size_t end = line.find("\"", start);
                if (end != std::string::npos) {
                    account.set_account_name(unescape_xml(line.substr(start, end - start)));
                }
            }

            // Extract fields array
            size_t fields_pos = line.find("\"fields\":[");
            if (fields_pos != std::string::npos) {
                size_t fields_end = line.find("]", fields_pos);
                if (fields_end != std::string::npos) {
                    std::string fields_section = line.substr(fields_pos, fields_end - fields_pos);

                    // Find username field
                    size_t user_pos = fields_section.find("\"designation\":\"username\"");
                    if (user_pos != std::string::npos) {
                        size_t value_start = fields_section.rfind("\"value\":\"", user_pos);
                        if (value_start != std::string::npos) {
                            value_start += 9;
                            size_t value_end = fields_section.find("\"", value_start);
                            if (value_end != std::string::npos) {
                                account.set_user_name(unescape_xml(fields_section.substr(value_start, value_end - value_start)));
                            }
                        }
                    }

                    // Find password field
                    size_t pass_pos = fields_section.find("\"designation\":\"password\"");
                    if (pass_pos != std::string::npos) {
                        size_t value_start = fields_section.rfind("\"value\":\"", pass_pos);
                        if (value_start != std::string::npos) {
                            value_start += 9;
                            size_t value_end = fields_section.find("\"", value_start);
                            if (value_end != std::string::npos) {
                                account.set_password(unescape_xml(fields_section.substr(value_start, value_end - value_start)));
                            }
                        }
                    }
                }
            }

            // Extract URL
            size_t url_pos = line.find("\"URLs\":[{\"url\":\"");
            if (url_pos != std::string::npos) {
                size_t start = url_pos + 16;
                size_t end = line.find("\"", start);
                if (end != std::string::npos) {
                    account.set_website(unescape_xml(line.substr(start, end - start)));
                }
            }

            // Extract notes
            size_t notes_pos = line.find("\"notesPlain\":\"");
            if (notes_pos != std::string::npos) {
                size_t start = notes_pos + 14;
                size_t end = line.find("\"", start);
                if (end != std::string::npos) {
                    std::string notes = unescape_xml(line.substr(start, end - start));

                    // Check if notes contain email
                    if (notes.starts_with("Email: ")) {
                        size_t newline = notes.find("\\n\\n");
                        if (newline != std::string::npos) {
                            account.set_email(notes.substr(7, newline - 7));
                            account.set_notes(notes.substr(newline + 4));
                        } else {
                            account.set_email(notes.substr(7));
                        }
                    } else {
                        account.set_notes(notes);
                    }
                }
            }

            // Only add if we got at least a title
            if (!account.account_name().empty()) {
                accounts.emplace_back(std::move(account));
            }
        }

        if (accounts.empty()) {
            return std::unexpected(ImportError::EMPTY_FILE);
        }

        return accounts;
    } catch (const std::exception& e) {
        return std::unexpected(ImportError::PARSE_ERROR);
    }
}

} // namespace ImportExport
