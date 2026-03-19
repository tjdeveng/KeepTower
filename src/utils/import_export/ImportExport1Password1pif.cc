// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "../ImportExport.h"
#include "ImportExportDetail.h"

#include <cctype>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ImportExport {

namespace {

constexpr std::string_view k1PifSeparator = "***5642bee8-a5ff-11dc-8314-0800200c9a66***";

[[nodiscard]] bool is_separator_line(std::string_view line) {
    return line.find(k1PifSeparator) != std::string_view::npos;
}

[[nodiscard]] std::string json_escape(std::string_view input) {
    std::string out;
    out.reserve(input.size() + 16);
    for (unsigned char c : input) {
        switch (c) {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (c < 0x20) {
                    // Encode other control characters as \u00XX
                    static constexpr char kHex[] = "0123456789ABCDEF";
                    out += "\\u00";
                    out += kHex[(c >> 4) & 0x0F];
                    out += kHex[c & 0x0F];
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    return out;
}

[[nodiscard]] int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

void append_utf8(std::string& out, uint32_t codepoint) {
    if (codepoint <= 0x7F) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

// Parses a JSON string starting at s[pos] == '"'.
// Accepts legacy literal newlines inside the string to preserve backward compatibility.
[[nodiscard]] bool parse_json_string(std::string_view s, size_t& pos, std::string& out) {
    if (pos >= s.size() || s[pos] != '"') {
        return false;
    }
    ++pos;
    out.clear();
    out.reserve(64);

    while (pos < s.size()) {
        char c = s[pos++];
        if (c == '"') {
            return true;
        }
        if (c == '\\') {
            if (pos >= s.size()) {
                return false;
            }
            char esc = s[pos++];
            switch (esc) {
                case '"':
                    out.push_back('"');
                    break;
                case '\\':
                    out.push_back('\\');
                    break;
                case '/':
                    out.push_back('/');
                    break;
                case 'b':
                    out.push_back('\b');
                    break;
                case 'f':
                    out.push_back('\f');
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                case 'u': {
                    if (pos + 4 > s.size()) {
                        return false;
                    }
                    int h1 = hex_value(s[pos]);
                    int h2 = hex_value(s[pos + 1]);
                    int h3 = hex_value(s[pos + 2]);
                    int h4 = hex_value(s[pos + 3]);
                    if (h1 < 0 || h2 < 0 || h3 < 0 || h4 < 0) {
                        return false;
                    }
                    uint32_t code = (static_cast<uint32_t>(h1) << 12) |
                                    (static_cast<uint32_t>(h2) << 8) |
                                    (static_cast<uint32_t>(h3) << 4) |
                                    static_cast<uint32_t>(h4);
                    pos += 4;

                    // Handle surrogate pairs
                    if (code >= 0xD800 && code <= 0xDBFF) {
                        if (pos + 6 <= s.size() && s[pos] == '\\' && s[pos + 1] == 'u') {
                            int l1 = hex_value(s[pos + 2]);
                            int l2 = hex_value(s[pos + 3]);
                            int l3 = hex_value(s[pos + 4]);
                            int l4 = hex_value(s[pos + 5]);
                            if (l1 >= 0 && l2 >= 0 && l3 >= 0 && l4 >= 0) {
                                uint32_t low = (static_cast<uint32_t>(l1) << 12) |
                                               (static_cast<uint32_t>(l2) << 8) |
                                               (static_cast<uint32_t>(l3) << 4) |
                                               static_cast<uint32_t>(l4);
                                if (low >= 0xDC00 && low <= 0xDFFF) {
                                    pos += 6;
                                    uint32_t full = 0x10000 + (((code - 0xD800) << 10) | (low - 0xDC00));
                                    append_utf8(out, full);
                                    break;
                                }
                            }
                        }
                        // Invalid surrogate pair: replace with U+FFFD
                        append_utf8(out, 0xFFFD);
                    } else {
                        append_utf8(out, code);
                    }
                    break;
                }
                default:
                    // Unknown escape; keep as-is.
                    out.push_back(esc);
                    break;
            }
            continue;
        }

        // Legacy behavior: accept literal newlines inside string.
        out.push_back(c);
    }

    return false;
}

[[nodiscard]] size_t skip_ws(std::string_view s, size_t pos) {
    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) {
        ++pos;
    }
    return pos;
}

[[nodiscard]] std::optional<std::string> find_key_string_value(std::string_view json, std::string_view key) {
    std::string needle;
    needle.reserve(key.size() + 2);
    needle.push_back('"');
    needle.append(key);
    needle.push_back('"');

    size_t pos = json.find(needle);
    if (pos == std::string_view::npos) {
        return std::nullopt;
    }
    pos += needle.size();
    pos = skip_ws(json, pos);
    if (pos >= json.size() || json[pos] != ':') {
        return std::nullopt;
    }
    ++pos;
    pos = skip_ws(json, pos);
    if (pos >= json.size() || json[pos] != '"') {
        return std::nullopt;
    }
    std::string value;
    if (!parse_json_string(json, pos, value)) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::optional<std::string> find_designated_field_value(
    std::string_view json, std::string_view designation) {
    std::string marker;
    marker.reserve(designation.size() + 16);
    marker.append("\"designation\":\"");
    marker.append(designation);
    marker.push_back('"');

    size_t pos = json.find(marker);
    if (pos == std::string_view::npos) {
        return std::nullopt;
    }

    size_t obj_start = json.rfind('{', pos);
    if (obj_start == std::string_view::npos) {
        return std::nullopt;
    }
    size_t obj_end = json.find('}', pos);
    if (obj_end == std::string_view::npos || obj_end <= obj_start) {
        return std::nullopt;
    }

    std::string_view obj = json.substr(obj_start, obj_end - obj_start + 1);
    return find_key_string_value(obj, "value");
}

[[nodiscard]] bool is_keeptower_generated_uuid(std::string_view uuid) {
    return uuid.starts_with("generated-uuid-");
}

[[nodiscard]] std::string maybe_unescape_legacy_xml_entities(bool enable, std::string_view v) {
    if (!enable) {
        return std::string(v);
    }

    // Only pay the cost (and take the risk of transforming literal entity text)
    // if we actually see entity-like substrings.
    if (v.find("&lt;") == std::string_view::npos && v.find("&gt;") == std::string_view::npos &&
        v.find("&amp;") == std::string_view::npos && v.find("&quot;") == std::string_view::npos &&
        v.find("&apos;") == std::string_view::npos) {
        return std::string(v);
    }

    return detail::unescape_xml(std::string(v));
}

[[nodiscard]] std::vector<std::string> read_1pif_records(std::ifstream& file) {
    std::vector<std::string> records;

    std::string current;
    std::string line;
    while (std::getline(file, line)) {
        if (is_separator_line(line)) {
            if (!current.empty() && current.find_first_not_of(" \t\r\n") != std::string::npos) {
                records.emplace_back(std::move(current));
            }
            current.clear();
            continue;
        }

        // Keep all lines (including empty lines) inside the record.
        if (!current.empty()) {
            current.push_back('\n');
        }
        current.append(line);
    }

    if (!current.empty() && current.find_first_not_of(" \t\r\n") != std::string::npos) {
        records.emplace_back(std::move(current));
    }

    return records;
}

}  // namespace

std::expected<void, ExportError> export_to_1password_1pif(
    const std::string& filepath, const std::vector<keeptower::AccountRecord>& accounts) {
    try {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            return std::unexpected(ExportError::FILE_WRITE_ERROR);
        }

        for (const auto& account : accounts) {
          file << "{\"uuid\":\"generated-uuid-" << std::hash<std::string>{}(account.account_name())
              << "\",";
          file << "\"category\":\"001\",";
          file << "\"title\":\"" << json_escape(account.account_name()) << "\",";
          file << "\"secureContents\":{";

          file << "\"fields\":[";
          file << "{\"value\":\"" << json_escape(account.user_name())
              << "\",\"name\":\"username\",\"type\":\"T\",\"designation\":\"username\"},";
          file << "{\"value\":\"" << json_escape(account.password())
              << "\",\"name\":\"password\",\"type\":\"P\",\"designation\":\"password\"}";
          file << "],";

            if (!account.website().empty()) {
                file << "\"URLs\":[{\"url\":\"" << json_escape(account.website()) << "\"}],";
            }

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
                file << "\"notesPlain\":\"" << json_escape(notes) << "\",";
            }

            file << "\"htmlForm\":null}}";
            file << "\n" << k1PifSeparator << "\n";
        }

        file.flush();
        if (!file.good()) {
            return std::unexpected(ExportError::FILE_WRITE_ERROR);
        }
        file.close();

        detail::set_restrictive_permissions_0600(filepath);
        detail::sync_file_to_disk(filepath);

        return {};
    } catch (const std::exception&) {
        return std::unexpected(ExportError::FILE_WRITE_ERROR);
    }
}

std::expected<std::vector<keeptower::AccountRecord>, ImportError> import_from_1password(
    const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return std::unexpected(ImportError::FILE_NOT_FOUND);
        }

        if (!detail::file_within_size_limit(file, detail::MAX_IMPORT_FILE_SIZE_BYTES)) {
            return std::unexpected(ImportError::INVALID_FORMAT);
        }

        std::vector<keeptower::AccountRecord> accounts;
        accounts.reserve(100);

        auto records = read_1pif_records(file);
        for (const auto& record : records) {
            keeptower::AccountRecord account;

                // Legacy KeepTower 1PIF exports used XML-entity escaping inside JSON strings.
                // Only enable entity unescaping for those records.
                bool legacy_xml_entities = false;
                if (auto uuid = find_key_string_value(record, "uuid")) {
                    legacy_xml_entities = is_keeptower_generated_uuid(*uuid);
                }

            // Title
            if (auto title = find_key_string_value(record, "title")) {
                account.set_account_name(maybe_unescape_legacy_xml_entities(legacy_xml_entities, *title));
            }

            // Username/password (from fields array)
            if (auto user = find_designated_field_value(record, "username")) {
                account.set_user_name(maybe_unescape_legacy_xml_entities(legacy_xml_entities, *user));
            }
            if (auto pass = find_designated_field_value(record, "password")) {
                account.set_password(maybe_unescape_legacy_xml_entities(legacy_xml_entities, *pass));
            }

            // URL (first url in URLs)
            if (auto url = find_key_string_value(record, "url")) {
                account.set_website(maybe_unescape_legacy_xml_entities(legacy_xml_entities, *url));
            }

            // Notes
            if (auto notes_raw = find_key_string_value(record, "notesPlain")) {
                std::string notes = maybe_unescape_legacy_xml_entities(legacy_xml_entities, *notes_raw);
                if (notes.starts_with("Email: ")) {
                    size_t newline = notes.find("\n\n");
                    if (newline != std::string::npos) {
                        account.set_email(notes.substr(7, newline - 7));
                        account.set_notes(notes.substr(newline + 2));
                    } else {
                        account.set_email(notes.substr(7));
                    }
                } else {
                    account.set_notes(notes);
                }
            }

            if (!account.account_name().empty()) {
                accounts.emplace_back(std::move(account));
            }
        }

        if (accounts.empty()) {
            return std::unexpected(ImportError::EMPTY_FILE);
        }

        return accounts;
    } catch (const std::exception&) {
        return std::unexpected(ImportError::PARSE_ERROR);
    }
}

}  // namespace ImportExport
