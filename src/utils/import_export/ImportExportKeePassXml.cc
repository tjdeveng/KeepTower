// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "../ImportExport.h"
#include "ImportExportDetail.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace ImportExport {

std::expected<void, ExportError> export_to_keepass_xml(
    const std::string& filepath, const std::vector<keeptower::AccountRecord>& accounts) {
    try {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            return std::unexpected(ExportError::FILE_WRITE_ERROR);
        }

        std::string timestamp = detail::get_iso_timestamp();

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

        for (const auto& account : accounts) {
            file << R"(      <Entry>)" << "\n";

            file << R"(        <String>)" << "\n";
            file << R"(          <Key>Title</Key>)" << "\n";
            file << "          <Value>" << detail::escape_xml(account.account_name())
                 << "</Value>\n";
            file << R"(        </String>)" << "\n";

            file << R"(        <String>)" << "\n";
            file << R"(          <Key>UserName</Key>)" << "\n";
            file << "          <Value>" << detail::escape_xml(account.user_name()) << "</Value>\n";
            file << R"(        </String>)" << "\n";

            file << R"(        <String>)" << "\n";
            file << R"(          <Key>Password</Key>)" << "\n";
            file << "          <Value>" << detail::escape_xml(account.password()) << "</Value>\n";
            file << R"(        </String>)" << "\n";

            if (!account.website().empty()) {
                file << R"(        <String>)" << "\n";
                file << R"(          <Key>URL</Key>)" << "\n";
                file << "          <Value>" << detail::escape_xml(account.website()) << "</Value>\n";
                file << R"(        </String>)" << "\n";
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
                file << R"(        <String>)" << "\n";
                file << R"(          <Key>Notes</Key>)" << "\n";
                file << "          <Value>" << detail::escape_xml(notes) << "</Value>\n";
                file << R"(        </String>)" << "\n";
            }

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

        detail::set_restrictive_permissions_0600(filepath);
        detail::sync_file_to_disk(filepath);

        return {};
    } catch (const std::exception&) {
        return std::unexpected(ExportError::FILE_WRITE_ERROR);
    }
}

std::expected<std::vector<keeptower::AccountRecord>, ImportError> import_from_keepass_xml(
    const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return std::unexpected(ImportError::FILE_NOT_FOUND);
        }

        if (!detail::file_within_size_limit(file, detail::MAX_IMPORT_FILE_SIZE_BYTES)) {
            return std::unexpected(ImportError::INVALID_FORMAT);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        std::vector<keeptower::AccountRecord> accounts;
        accounts.reserve(std::min<size_t>(content.size() / 500 + 1, 10000));

        size_t pos = 0;
        while ((pos = content.find("<Entry>", pos)) != std::string::npos) {
            size_t end = content.find("</Entry>", pos);
            if (end == std::string::npos) {
                break;
            }

            std::string_view entry{content.data() + pos, end - pos + 8};

            keeptower::AccountRecord account;

            size_t string_pos = 0;
            while ((string_pos = entry.find("<String>", string_pos)) != std::string::npos) {
                size_t string_end = entry.find("</String>", string_pos);
                if (string_end == std::string::npos) {
                    break;
                }

                std::string_view string_block = entry.substr(string_pos, string_end - string_pos + 9);
                std::string key = detail::unescape_xml(detail::extract_xml_value(string_block, "Key"));
                std::string value = detail::unescape_xml(detail::extract_xml_value(string_block, "Value"));

                if (key == "Title") {
                    account.set_account_name(value);
                } else if (key == "UserName") {
                    account.set_user_name(value);
                } else if (key == "Password") {
                    account.set_password(value);
                } else if (key == "URL") {
                    account.set_website(value);
                } else if (key == "Notes") {
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
    } catch (const std::exception&) {
        return std::unexpected(ImportError::PARSE_ERROR);
    }
}

}  // namespace ImportExport
