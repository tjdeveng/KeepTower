// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#ifndef IMPORT_EXPORT_DETAIL_H
#define IMPORT_EXPORT_DETAIL_H

#include "../ImportExport.h"

#include <fstream>
#include <cstddef>
#include <string>
#include <string_view>

namespace ImportExport::detail {

constexpr std::size_t MAX_IMPORT_FILE_SIZE_BYTES = 100U * 1024U * 1024U;  // 100MB

// For import-side DoS protection.
[[nodiscard]] bool file_within_size_limit(std::ifstream& file, std::size_t max_bytes);

// Export-side best practices (non-fatal if chmod/fsync fail).
void set_restrictive_permissions_0600(const std::string& filepath);
void sync_file_to_disk(const std::string& filepath);

// XML helpers shared by KeePass XML + 1PIF (uses XML-style escaping today).
[[nodiscard]] std::string escape_xml(const std::string& text);
[[nodiscard]] std::string unescape_xml(const std::string& text);

// Extract value between <Tag>...</Tag>.
[[nodiscard]] std::string extract_xml_value(std::string_view xml, std::string_view tag);

// ISO8601 timestamp (UTC).
[[nodiscard]] std::string get_iso_timestamp();

}  // namespace ImportExport::detail

#endif  // IMPORT_EXPORT_DETAIL_H
