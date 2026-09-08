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
#include <fstream>
#include <vector>

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

    [[nodiscard]] std::vector<std::string> get_log_candidates() {
        namespace fs = std::filesystem;
        std::vector<std::string> candidates;

        try {
            candidates.push_back((fs::temp_directory_path() / "keeptower-help-debug.log").string());
        } catch (...) {
            // Ignore and continue to fallbacks.
        }

        if (const char* glib_tmp = g_get_tmp_dir(); glib_tmp != nullptr && *glib_tmp != '\0') {
            try {
                candidates.push_back((fs::path(glib_tmp) / "keeptower-help-debug.log").string());
            } catch (...) {
                // Ignore malformed paths.
            }
        }

        try {
            candidates.push_back((fs::current_path() / "keeptower-help-debug.log").string());
        } catch (...) {
            // Ignore and keep relative fallback below.
        }

        candidates.push_back("keeptower-help-debug.log");
        return candidates;
    }

    [[nodiscard]] std::string get_log_file_path() {
        for (const auto& candidate : get_log_candidates()) {
            try {
                std::ofstream log(candidate, std::ios::app);
                if (log.is_open()) {
                    return candidate;
                }
            } catch (...) {
                // Try next candidate.
            }
        }
        return "";
    }

    void log_diagnostic(const std::string& message) {
#ifdef _WIN32
        const std::string debug_line = std::string("[HelpManager] ") + message + "\n";
        OutputDebugStringA(debug_line.c_str());
#endif

        const std::string log_file = get_log_file_path();
        if (log_file.empty()) {
            return;
        }

        try {
            std::ofstream log(log_file, std::ios::app);
            if (log.is_open()) {
                log << "[HelpManager] " << message << std::endl;
                log.flush();
            }
        } catch (...) {
            // Silently fail - logging should never break main functionality
        }
    }

    // C++23: Use std::filesystem for safer path operations
    namespace fs = std::filesystem;

    [[nodiscard]] std::string path_to_file_uri(const fs::path& path) {
        try {
            const std::string uri = Glib::filename_to_uri(fs::absolute(path).string());
            log_diagnostic("Converted path to URI: " + path.string() + " -> " + uri);
            return uri;
        } catch (const Glib::Error& ex) {
            log_diagnostic("Failed to convert path to URI: " + path.string() + " - " + std::string(ex.what()));
            return "";
        } catch (const fs::filesystem_error& ex) {
            log_diagnostic("Filesystem error converting path to URI: " + std::string(ex.what()));
            return "";
        }
    }

    [[nodiscard]] fs::path get_executable_dir() {
#ifdef _WIN32
        std::array<wchar_t, 32768> exe_path{};
        const DWORD length = GetModuleFileNameW(nullptr, exe_path.data(), static_cast<DWORD>(exe_path.size()));
        if (length == 0 || length >= exe_path.size()) {
            log_diagnostic("GetModuleFileNameW failed, returning current_path");
            return fs::current_path();
        }

        fs::path result = fs::path(exe_path.data()).parent_path();
        log_diagnostic("Executable dir: " + result.string());
        return result;
#else
        const char* appdir = std::getenv("APPDIR");
        if (appdir && *appdir != '\0') {
            const fs::path appdir_path = fs::path(appdir);
            log_diagnostic("Using APPDIR as executable root: " + appdir_path.string());
            return appdir_path;
        }

        std::error_code ec;
        const fs::path self = fs::read_symlink("/proc/self/exe", ec);
        if (!ec && !self.empty()) {
            const fs::path result = self.parent_path();
            log_diagnostic("Executable dir (proc/self/exe): " + result.string());
            return result;
        }

        const fs::path result = fs::current_path();
        log_diagnostic("Executable dir (fallback current_path): " + result.string());
        return result;
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
                log_diagnostic("g_filename_from_uri error: " + std::string(file_error->message));
                g_error_free(file_error);
            }
        }

        log_diagnostic("ShellExecute target: " + target);

        const std::wstring target_w = utf8_to_wstring(target);
        if (target_w.empty()) {
            log_diagnostic("Failed to convert target to wide string");
            return false;
        }

        const HINSTANCE result = ShellExecuteW(nullptr, L"open", target_w.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        const bool success = reinterpret_cast<INT_PTR>(result) > 32;

        log_diagnostic("ShellExecute result: " + std::to_string(reinterpret_cast<INT_PTR>(result)) +
                      " (success: " + std::string(success ? "true" : "false") + ")");

        return success;
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
    log_diagnostic("--- open_help called ---");

    const std::string uri = get_help_uri(topic);

    if (uri.empty()) {
        log_diagnostic("Help URI is empty - help file not found");

        const std::string log_file = get_log_file_path();
        const std::string message = "Help documentation could not be found. "
                                   "Please ensure KeepTower is properly installed.\n\n"
                                   "You can also view the documentation online at:\n" +
                                   std::string(GITHUB_WIKI_URL) +
                                   "\n\nDebug log: " + log_file;
        show_error_dialog(parent, "Help documentation not available", message);
        return false;
    }

    log_diagnostic("Help URI found: " + uri);

    // Security: Validate URI scheme before opening
    if (!uri.starts_with("file://")) {
        log_diagnostic("Invalid URI scheme: " + uri);
        show_error_dialog(parent,
            "Invalid help URI",
            "Help documentation URI has invalid scheme. Only file:// URIs are supported.");
        return false;
    }

    try {
#ifdef _WIN32
        // On Windows, GLib's URI launcher can rely on optional D-Bus helpers
        // that may be absent in portable bundles. Prefer native shell launch.
        log_diagnostic("Attempting to launch via ShellExecute");
        if (launch_uri_with_shell_execute(uri)) {
            log_diagnostic("Successfully launched via ShellExecute");
            return true;
        }

        // Secondary fallback through GIO launcher.
        log_diagnostic("ShellExecute failed, trying GIO launcher");
        GError* launch_error = nullptr;
        const gboolean launched = g_app_info_launch_default_for_uri(uri.c_str(), nullptr, &launch_error);
        if (launch_error != nullptr) {
            log_diagnostic("GIO launcher error: " + std::string(launch_error->message));
            g_error_free(launch_error);
        }
        if (launched) {
            log_diagnostic("Successfully launched via GIO");
            return true;
        }
#else
        GError* launch_error = nullptr;
        if (g_app_info_launch_default_for_uri(uri.c_str(), nullptr, &launch_error)) {
            log_diagnostic("Successfully launched via GIO default handler");
            return true;
        }

        if (launch_error != nullptr) {
            log_diagnostic("GIO default launch failed: " + std::string(launch_error->message));
            g_error_free(launch_error);
            launch_error = nullptr;
        }

        if (uri.rfind("file://", 0) == 0) {
            char* filename = nullptr;
            filename = g_filename_from_uri(uri.c_str(), nullptr, &launch_error);
            if (filename == nullptr && launch_error != nullptr) {
                log_diagnostic("g_filename_from_uri failed: " + std::string(launch_error->message));
                g_error_free(launch_error);
                launch_error = nullptr;
            }

            if (filename != nullptr) {
                const char* quoted = g_shell_quote(filename);
                std::string command = "xdg-open " + std::string(quoted);
                g_free((void*)quoted);
                g_free(filename);

                log_diagnostic("Attempting xdg-open fallback: " + command);
                if (g_spawn_command_line_async(command.c_str(), &launch_error)) {
                    log_diagnostic("Successfully launched via xdg-open fallback");
                    return true;
                }

                if (launch_error != nullptr) {
                    log_diagnostic("xdg-open fallback failed: " + std::string(launch_error->message));
                    g_error_free(launch_error);
                }
            }
        }

        // Final fallback to the GTK URI launcher for local file URIs.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
        gtk_show_uri(GTK_WINDOW(parent.gobj()), uri.c_str(), GDK_CURRENT_TIME);
        log_diagnostic("gtk_show_uri called");
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
        return true;
#endif

        log_diagnostic("All launch methods failed");

        const std::string log_file = get_log_file_path();
        const std::string message = "Could not open help in browser using system URI launcher.\n\n"
                                   "Help file location: " + uri +
                                   "\n\nPlease open this file manually in your web browser."
                                   "\n\nDebug log: " + log_file;
        show_error_dialog(parent, "Failed to open help documentation", message);
        return false;
    } catch (const Glib::Error& ex) {
        log_diagnostic("Exception during launch: " + std::string(ex.what()));

        const std::string log_file = get_log_file_path();
        const std::string message = std::string("Could not open help in browser: ") + ex.what() +
                                   "\n\nHelp file location: " + uri +
                                   "\n\nPlease open this file manually in your web browser."
                                   "\n\nDebug log: " + log_file;
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
    const std::array candidates = {
        exe_dir / "share" / "keeptower" / "help",
        exe_dir / ".." / "share" / "keeptower" / "help",
        exe_dir / ".." / ".." / "share" / "keeptower" / "help",
    };

    for (const auto& candidate : candidates) {
        try {
            if (fs::exists(candidate) && fs::is_directory(candidate)) {
                const std::string result = candidate.lexically_normal().string();
                log_diagnostic("Help install dir (Windows, detected): " + result);
                return result;
            }
        } catch (const fs::filesystem_error&) {
            // Continue trying fallback locations.
        }
    }

    const std::string fallback = (exe_dir / "share" / "keeptower" / "help").lexically_normal().string();
    log_diagnostic("Help install dir (Windows, fallback): " + fallback);
    return fallback;
#else
    const char* appdir = std::getenv("APPDIR");
    if (appdir && *appdir != '\0') {
        const std::array appimage_candidates = {
            fs::path(appdir) / "usr" / "share" / "keeptower" / "help",
            fs::path(appdir) / "usr" / "local" / "share" / "keeptower" / "help",
            fs::path(appdir) / "share" / "keeptower" / "help",
        };

        for (const auto& candidate : appimage_candidates) {
            try {
                if (fs::exists(candidate) && fs::is_directory(candidate)) {
                    const std::string result = candidate.lexically_normal().string();
                    log_diagnostic("Help install dir (Unix, AppImage): " + result);
                    return result;
                }
            } catch (const fs::filesystem_error&) {
                // Continue checking other AppImage candidates.
            }
        }
    }

    const std::string result = std::string(KEEPTOWER_DATADIR) + "/keeptower/help";
    log_diagnostic("Help install dir (Unix): " + result);
    return result;
#endif
}

std::string HelpManager::find_help_file(const std::string& filename) const {
    // Security: Validate filename to prevent path traversal
    if (filename.find("..") != std::string::npos ||
        filename.find('/') != std::string::npos ||
        filename.find('\\') != std::string::npos) {
        log_diagnostic("Invalid filename (contains path traversal): " + filename);
        return "";
    }

    log_diagnostic("Finding help file: " + filename);

    // Strategy 1: Check installed location (production)
    const fs::path installed_path = fs::path(get_help_install_dir()) / filename;
    log_diagnostic("Strategy 1 - Checking installed: " + installed_path.string());
    if (file_exists(installed_path.string())) {
        log_diagnostic("Found at installed location: " + installed_path.string());
        return path_to_file_uri(installed_path);
    }
    log_diagnostic("Not found at installed location");

    // Strategy 2: Check development paths (relative to executable)
    const fs::path exe_dir = get_executable_dir();
    const std::array dev_paths = {
        exe_dir / "share" / "keeptower" / "help" / filename,
        exe_dir / "usr" / "share" / "keeptower" / "help" / filename,
        exe_dir / "resources" / "help" / filename,
        exe_dir / ".." / "resources" / "help" / filename,
        exe_dir / ".." / ".." / "resources" / "help" / filename,
        exe_dir / ".." / ".." / ".." / "resources" / "help" / filename,
    };

    int dev_path_idx = 0;
    for (const auto& path : dev_paths) {
        log_diagnostic("Strategy 2 - Checking dev path " + std::to_string(dev_path_idx) + ": " + path.string());
        if (file_exists(path.string())) {
            log_diagnostic("Found at dev path: " + path.string());
            return path_to_file_uri(path);
        }
        dev_path_idx++;
    }
    log_diagnostic("Not found in any dev paths");

    // Strategy 3: Extract from embedded GResources
    log_diagnostic("Strategy 3 - Attempting GResource extraction");
    return extract_from_gresource(filename);
}

bool HelpManager::file_exists(const std::string& path) const noexcept {
    try {
        // C++23: Use std::filesystem for safer file operations
        const bool exists = fs::exists(path) && fs::is_regular_file(path);
        if (!exists) {
            log_diagnostic("File does not exist: " + path);
        }
        return exists;
    } catch (const fs::filesystem_error& ex) {
        log_diagnostic("Filesystem error checking file " + path + ": " + ex.what());
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

    log_diagnostic("Checking GResource: " + resource_path);

    if (!gresource_exists(resource_path)) {
        log_diagnostic("GResource not found: " + resource_path);
        return "";
    }

    try {
        auto resource = Gio::Resource::lookup_data_global(resource_path);
        gsize size = 0;
        const char* data = static_cast<const char*>(resource->get_data(size));

        // Security: Validate data size to prevent DoS
        constexpr gsize MAX_HELP_FILE_SIZE = 10 * 1024 * 1024; // 10 MB
        if (size == 0 || size > MAX_HELP_FILE_SIZE) {
            log_diagnostic("GResource data size invalid: " + std::to_string(size));
            return "";
        }

        // Create secure temp file path
        const fs::path temp_dir = fs::temp_directory_path();
        const fs::path temp_file = temp_dir / (std::string(TEMP_FILE_PREFIX) + filename);

        log_diagnostic("Extracting GResource to: " + temp_file.string());

        // Write resource to temp file using Glib (better error handling)
        Glib::file_set_contents(temp_file.string(), std::string(data, size));

        log_diagnostic("Successfully extracted GResource to: " + temp_file.string());

        return path_to_file_uri(temp_file);
    } catch (const Glib::Error& ex) {
        log_diagnostic("GResource extraction failed: " + std::string(ex.what()));
        return "";
    } catch (const fs::filesystem_error& ex) {
        log_diagnostic("Filesystem error during GResource extraction: " + std::string(ex.what()));
        return "";
    }
}

void HelpManager::show_error_dialog(Gtk::Window& parent,
                                     const std::string& title,
                                     const std::string& message) const {
    auto* dialog = Gtk::make_managed<Gtk::MessageDialog>(
        parent,
        title,
        false,
        Gtk::MessageType::ERROR,
        Gtk::ButtonsType::OK,
        true
    );

    dialog->set_secondary_text(message);
    dialog->set_hide_on_close(true);
    dialog->signal_response().connect([dialog](int) {
        dialog->hide();
    });
    dialog->present();
}

std::string HelpManager::topic_to_filename(HelpTopic topic) {
    auto it = topic_filenames_.find(topic);
    if (it != topic_filenames_.end()) {
        return it->second;
    }
    return "00-home.html";
}

} // namespace Utils
