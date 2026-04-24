// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "HelpManager.h"
#include <config.h>
#include <gtkmm.h>
#include <gtk/gtk.h>
#include <giomm/resource.h>
#include <gio/gio.h>
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>
#include <filesystem>
#include <string_view>
#include <array>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#ifdef ERROR
#undef ERROR
#endif
#endif

namespace Utils {

namespace {
    // Anonymous namespace for internal helpers - single responsibility principle
    constexpr std::string_view GITHUB_WIKI_URL = "https://github.com/tjdeveng/KeepTower/wiki";
    constexpr std::string_view GRESOURCE_PREFIX = "/com/tjdeveng/keeptower/help/";
    constexpr std::string_view TEMP_FILE_PREFIX = "keeptower-help-";

    // C++23: Use std::filesystem for safer path operations
    namespace fs = std::filesystem;

    [[nodiscard]] std::string path_to_file_uri(const fs::path& path) {
        try {
            return Glib::filename_to_uri(fs::absolute(path).string());
        } catch (const Glib::Error&) {
            return "";
        } catch (const fs::filesystem_error&) {
            return "";
        }
    }

    [[nodiscard]] fs::path get_executable_dir() {
#ifdef _WIN32
        std::array<wchar_t, 32768> exe_path{};
        const DWORD length = GetModuleFileNameW(nullptr, exe_path.data(), static_cast<DWORD>(exe_path.size()));
        if (length == 0 || length >= exe_path.size()) {
            return fs::current_path();
        }
        return fs::path(exe_path.data()).parent_path();
#else
        return fs::current_path();
#endif
    }

#ifdef _WIN32
    [[nodiscard]] std::wstring utf8_to_wstring(const std::string& utf8) {
        if (utf8.empty()) {
            return L"";
        }

        const int wide_len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
        if (wide_len <= 0) {
            return L"";
        }

        std::wstring wide(static_cast<size_t>(wide_len), L'\0');
        const int result = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wide.data(), wide_len);
        if (result <= 0) {
            return L"";
        }

        if (!wide.empty() && wide.back() == L'\0') {
            wide.pop_back();
        }
        return wide;
    }

    [[nodiscard]] bool launch_uri_with_shell_execute(const std::string& uri) {
        std::string target = uri;

        if (uri.rfind("file://", 0) == 0) {
            GError* file_error = nullptr;
            char* filename = g_filename_from_uri(uri.c_str(), nullptr, &file_error);
            if (filename != nullptr) {
                target.assign(filename);
                g_free(filename);
            }
            if (file_error != nullptr) {
                g_error_free(file_error);
            }
        }

        const std::wstring target_w = utf8_to_wstring(target);
        if (target_w.empty()) {
            return false;
        }

        const HINSTANCE result = ShellExecuteW(nullptr, L"open", target_w.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return reinterpret_cast<INT_PTR>(result) > 32;
    }
#endif
} // anonymous namespace

// Topic to filename mapping
const std::map<HelpTopic, std::string> HelpManager::topic_filenames_ = {
    {HelpTopic::Home, "00-home.html"},
    {HelpTopic::GettingStarted, "01-getting-started.html"},
    {HelpTopic::Installation, "02-installation.html"},
    {HelpTopic::UserGuide, "03-user-guide.html"},
    {HelpTopic::FAQ, "04-faq.html"},
    {HelpTopic::Security, "05-security.html"},
    {HelpTopic::SecurityBestPractices, "SECURITY_BEST_PRACTICES.html"}
};

HelpManager& HelpManager::get_instance() {
    static HelpManager instance;
    return instance;
}

HelpManager::HelpManager() = default;

bool HelpManager::open_help(HelpTopic topic, Gtk::Window& parent) {
    const std::string uri = get_help_uri(topic);

    if (uri.empty()) {
        const std::string message = "Help documentation could not be found. "
                                   "Please ensure KeepTower is properly installed.\n\n"
                                   "You can also view the documentation online at:\n" +
                                   std::string(GITHUB_WIKI_URL);
        show_error_dialog(parent, "Help documentation not available", message);
        return false;
    }

    // Security: Validate URI scheme before opening
    if (!uri.starts_with("file://")) {
        show_error_dialog(parent,
            "Invalid help URI",
            "Help documentation URI has invalid scheme. Only file:// URIs are supported.");
        return false;
    }

    try {
#ifdef _WIN32
        GError* launch_error = nullptr;
        const gboolean launched = g_app_info_launch_default_for_uri(uri.c_str(), nullptr, &launch_error);
        if (launch_error != nullptr) {
            g_error_free(launch_error);
        }
        if (launched) {
            return true;
        }

        // Fallback for Windows environments where GLib launcher is unreliable.
        if (launch_uri_with_shell_execute(uri)) {
            return true;
        }
#else
        // Use GTK4 C API for URI launching
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
        gtk_show_uri(GTK_WINDOW(parent.gobj()), uri.c_str(), GDK_CURRENT_TIME);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
        return true;
#endif

        const std::string message = "Could not open help in browser using system URI launcher.\n\n"
                                   "Help file location: " + uri +
                                   "\n\nPlease open this file manually in your web browser.";
        show_error_dialog(parent, "Failed to open help documentation", message);
        return false;
    } catch (const Glib::Error& ex) {
        const std::string message = std::string("Could not open help in browser: ") + ex.what() +
                                   "\n\nHelp file location: " + uri +
                                   "\n\nPlease open this file manually in your web browser.";
        show_error_dialog(parent, "Failed to open help documentation", message);
        return false;
    }
}

bool HelpManager::is_help_available(HelpTopic topic) const {
    return !get_help_uri(topic).empty();
}

std::string HelpManager::get_help_uri(HelpTopic topic) const {
    std::string filename = topic_to_filename(topic);
    return find_help_file(filename);
}

std::string HelpManager::get_help_install_dir() {
#ifdef _WIN32
    const fs::path exe_dir = get_executable_dir();
    return (exe_dir / "share" / "keeptower" / "help").string();
#else
    return std::string(KEEPTOWER_DATADIR) + "/keeptower/help";
#endif
}

std::string HelpManager::find_help_file(const std::string& filename) const {
    // Security: Validate filename to prevent path traversal
    if (filename.find("..") != std::string::npos ||
        filename.find('/') != std::string::npos ||
        filename.find('\\') != std::string::npos) {
        return "";
    }

    // Strategy 1: Check installed location (production)
    const fs::path installed_path = fs::path(get_help_install_dir()) / filename;
    if (file_exists(installed_path.string())) {
        return path_to_file_uri(installed_path);
    }

    // Strategy 2: Check development paths (relative to executable)
    const fs::path exe_dir = get_executable_dir();
    const std::array dev_paths = {
        exe_dir / "share" / "keeptower" / "help" / filename,
        exe_dir / "resources" / "help" / filename,
        exe_dir / ".." / "resources" / "help" / filename,
        exe_dir / ".." / ".." / "resources" / "help" / filename,
        exe_dir / ".." / ".." / ".." / "resources" / "help" / filename,
    };

    for (const auto& path : dev_paths) {
        if (file_exists(path.string())) {
            return path_to_file_uri(path);
        }
    }

    // Strategy 3: Extract from embedded GResources
    return extract_from_gresource(filename);
}

bool HelpManager::file_exists(const std::string& path) const noexcept {
    try {
        // C++23: Use std::filesystem for safer file operations
        return fs::exists(path) && fs::is_regular_file(path);
    } catch (const fs::filesystem_error&) {
        return false;
    }
}

bool HelpManager::gresource_exists(const std::string& resource_path) const noexcept {
    try {
        auto resource = Gio::Resource::lookup_data_global(resource_path);
        return static_cast<bool>(resource);
    } catch (const Glib::Error&) {
        return false;
    }
}

std::string HelpManager::extract_from_gresource(const std::string& filename) const {
    const std::string resource_path = std::string(GRESOURCE_PREFIX) + filename;

    if (!gresource_exists(resource_path)) {
        return "";
    }

    try {
        auto resource = Gio::Resource::lookup_data_global(resource_path);
        gsize size = 0;
        const char* data = static_cast<const char*>(resource->get_data(size));

        // Security: Validate data size to prevent DoS
        constexpr gsize MAX_HELP_FILE_SIZE = 10 * 1024 * 1024; // 10 MB
        if (size == 0 || size > MAX_HELP_FILE_SIZE) {
            return "";
        }

        // Create secure temp file path
        const fs::path temp_dir = fs::temp_directory_path();
        const fs::path temp_file = temp_dir / (std::string(TEMP_FILE_PREFIX) + filename);

        // Write resource to temp file using Glib (better error handling)
        Glib::file_set_contents(temp_file.string(), std::string(data, size));

        return path_to_file_uri(temp_file);
    } catch (const Glib::Error& ex) {
        // Don't use std::cerr - maintain separation of concerns
        // Caller should handle errors through return value
        return "";
    } catch (const fs::filesystem_error&) {
        return "";
    }
}

void HelpManager::show_error_dialog(Gtk::Window& parent,
                                     const std::string& title,
                                     const std::string& message) const {
    Gtk::MessageDialog dialog(
        parent,
        title,
        false,
        Gtk::MessageType::ERROR,
        Gtk::ButtonsType::OK,
        true
    );

    dialog.set_secondary_text(message);
    dialog.set_hide_on_close(true);
    dialog.show();
}

std::string HelpManager::topic_to_filename(HelpTopic topic) {
    auto it = topic_filenames_.find(topic);
    if (it != topic_filenames_.end()) {
        return it->second;
    }
    return "00-home.html";
}

} // namespace Utils
