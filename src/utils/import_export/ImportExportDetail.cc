// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "ImportExportDetail.h"

#include <ctime>
#include <fstream>
#include <sstream>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

// Feature detection for std::format (C++20)
#if __has_include(<format>)
    #include <format>
    #if defined(__cpp_lib_format)
        #define HAS_STD_FORMAT 1
    #endif
#endif

// Compile-time warning if std::format is not available
#ifndef HAS_STD_FORMAT
    #warning "std::format not available, using fallback string concatenation. Update to gcc 14+ or newer libstdc++ when available."
#endif

namespace ImportExport::detail {

bool file_within_size_limit(std::ifstream& file, std::size_t max_bytes) {
    file.clear();
    file.seekg(0, std::ios::end);
    auto file_size = file.tellg();
    if (file_size < 0) {
        return false;
    }
    if (static_cast<std::size_t>(file_size) > max_bytes) {
        return false;
    }
    file.seekg(0, std::ios::beg);
    return true;
}

void set_restrictive_permissions_0600(const std::string& filepath) {
    chmod(filepath.c_str(), S_IRUSR | S_IWUSR);
}

void sync_file_to_disk(const std::string& filepath) {
    int fd = open(filepath.c_str(), O_RDONLY);
    if (fd >= 0) {
        fsync(fd);
        close(fd);
    }
}

std::string escape_xml(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size() + 20);

    for (char c : text) {
        switch (c) {
            case '<':
                escaped.append("&lt;");
                break;
            case '>':
                escaped.append("&gt;");
                break;
            case '&':
                escaped.append("&amp;");
                break;
            case '"':
                escaped.append("&quot;");
                break;
            case '\'':
                escaped.append("&apos;");
                break;
            default:
                escaped += c;
                break;
        }
    }

    return escaped;
}

std::string unescape_xml(const std::string& text) {
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

std::string extract_xml_value(std::string_view xml, std::string_view tag) {
#ifdef HAS_STD_FORMAT
    std::string open_tag = std::format("<{}>", tag);
    std::string close_tag = std::format("</{}>", tag);
#else
    std::string open_tag = "<" + std::string(tag) + ">";
    std::string close_tag = "</" + std::string(tag) + ">";
#endif

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

std::string get_iso_timestamp() {
    std::time_t now = std::time(nullptr);
    char buffer[25];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));
    return std::string(buffer);
}

}  // namespace ImportExport::detail
