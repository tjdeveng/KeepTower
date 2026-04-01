// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace UI::Flows {

struct MessagePort {
    std::function<void(const std::string& message, const std::string& title)> info;
    std::function<void(const std::string& message, const std::string& title)> warning;
    std::function<void(const std::string& message, const std::string& title)> error;
};

struct SchedulerPort {
    std::function<void(std::function<void()> fn)> on_idle;
};

struct FileDialogPort {
    using Filters = std::vector<std::pair<std::string, std::string>>;

    std::function<void(const std::string& title,
                       const Filters& filters,
                       std::function<void(const std::string& path)>)>
        open_file;

    std::function<void(const std::string& title,
                       const std::string& suggested_name,
                       const Filters& filters,
                       std::function<void(const std::string& path)>)>
        save_file;
};

struct PasswordPromptPort {
    std::function<std::optional<std::string>()> current_username_for_title;

    std::function<void(std::function<void(std::string password_utf8)>)> prompt_password;
};

struct SecretCleanerPort {
    std::function<void(std::string& secret)> cleanse;
};

struct ExportWarningPort {
    std::function<void(std::function<void(bool proceed)>)> confirm_plaintext_export;
};

struct ExportAuthPort {
    std::function<std::expected<void, std::string>(const std::string& password_utf8)> authenticate;
};

struct ExportOperationPort {
    struct Success {
        std::string path;
        std::string format_name;
        std::string warning_text;
        size_t account_count = 0;
    };

    std::function<std::expected<Success, std::string>(const std::string& dest_path)> export_to_path;
};

struct ImportOperationPort {
    struct Summary {
        std::string format_name;
        int imported_count = 0;
        int failed_count = 0;
        std::vector<std::string> failed_accounts;
    };

    std::function<std::expected<Summary, std::string>(const std::string& source_path)> import_into_vault;
};

}  // namespace UI::Flows
