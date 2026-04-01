// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace UI::Flows {

/** @brief UI message sink for info/warning/error dialogs. */
struct MessagePort {
    /** @brief Show informational message with title. */
    std::function<void(const std::string& message, const std::string& title)> info;
    /** @brief Show warning message with title. */
    std::function<void(const std::string& message, const std::string& title)> warning;
    /** @brief Show error message with title. */
    std::function<void(const std::string& message, const std::string& title)> error;
};

/** @brief Scheduler abstraction used to marshal callbacks to the UI thread. */
struct SchedulerPort {
    /** @brief Queue a callback on the main loop/idle queue. */
    std::function<void(std::function<void()> fn)> on_idle;
};

/** @brief File chooser abstraction for open/save dialogs. */
struct FileDialogPort {
    /** @brief (Display name, pattern) filter list for file dialogs. */
    using Filters = std::vector<std::pair<std::string, std::string>>;

    /** @brief Open-file dialog callback. */
    std::function<void(const std::string& title,
                       const Filters& filters,
                       std::function<void(const std::string& path)>)>
        open_file;

    /** @brief Save-file dialog callback. */
    std::function<void(const std::string& title,
                       const std::string& suggested_name,
                       const Filters& filters,
                       std::function<void(const std::string& path)>)>
        save_file;
};

/** @brief Password prompt interface used by export/import flows. */
struct PasswordPromptPort {
    /** @brief Optional current username for prompt title/context. */
    std::function<std::optional<std::string>()> current_username_for_title;

    /** @brief Prompt for password and return UTF-8 text in callback. */
    std::function<void(std::function<void(std::string password_utf8)>)> prompt_password;
};

/** @brief Secure in-memory secret cleanup hook. */
struct SecretCleanerPort {
    /** @brief Overwrite and clear a secret string. */
    std::function<void(std::string& secret)> cleanse;
};

/** @brief Export warning confirmation interface. */
struct ExportWarningPort {
    /** @brief Ask user to confirm plaintext export risk. */
    std::function<void(std::function<void(bool proceed)>)> confirm_plaintext_export;
};

/** @brief Authentication hook used before export. */
struct ExportAuthPort {
    /** @brief Authenticate user with password, or return error text. */
    std::function<std::expected<void, std::string>(const std::string& password_utf8)> authenticate;
};

/** @brief Export operation entry point and success payload. */
struct ExportOperationPort {
    /** @brief Successful export details for user-facing status messaging. */
    struct Success {
        /** @brief Destination path written to disk. */
        std::string path;
        /** @brief Human-readable format name (for example, CSV). */
        std::string format_name;
        /** @brief Optional warning text to surface post-export. */
        std::string warning_text;
        /** @brief Number of accounts exported. */
        size_t account_count = 0;
    };

    /** @brief Perform export to destination path. */
    std::function<std::expected<Success, std::string>(const std::string& dest_path)> export_to_path;
};

/** @brief Import operation entry point and summary payload. */
struct ImportOperationPort {
    /** @brief Import summary displayed to user after completion. */
    struct Summary {
        /** @brief Human-readable format name (for example, 1PIF). */
        std::string format_name;
        /** @brief Number of accounts imported successfully. */
        int imported_count = 0;
        /** @brief Number of records that failed to import. */
        int failed_count = 0;
        /** @brief Names/identifiers of failed account records. */
        std::vector<std::string> failed_accounts;
    };

    /** @brief Perform import from source path into the currently open vault. */
    std::function<std::expected<Summary, std::string>(const std::string& source_path)> import_into_vault;
};

}  // namespace UI::Flows
