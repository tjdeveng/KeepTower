// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "HelpManager.h"
#include <config.h>
#include <gtkmm.h>
#include <gtk/gtk.h>
#include <giomm/resource.h>
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>
#include <filesystem>
#include <string_view>
#include <array>

namespace Utils {

namespace {
    // Anonymous namespace for internal helpers - single responsibility principle
    constexpr std::string_view GITHUB_WIKI_URL = "https://github.com/tjdeveng/KeepTower/wiki";
    constexpr std::string_view GRESOURCE_PREFIX = "/com/tjdeveng/keeptower/help/";
    constexpr std::string_view TEMP_FILE_PREFIX = "keeptower-help-";

    // C++23: Use std::filesystem for safer path operations
    namespace fs = std::filesystem;
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
        // Use GTK4 C API for URI launching
        gtk_show_uri(GTK_WINDOW(parent.gobj()), uri.c_str(), GDK_CURRENT_TIME);
        return true;
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
    return std::string(DATADIR) + "/keeptower/help";
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
        return "file://" + installed_path.string();
    }

    // Strategy 2: Check development paths (relative to executable)
    const fs::path exe_dir = fs::current_path();
    const std::array dev_paths = {
        exe_dir / "resources" / "help" / filename,
        exe_dir / ".." / "resources" / "help" / filename,
        exe_dir / ".." / ".." / "resources" / "help" / filename,
        exe_dir / ".." / ".." / ".." / "resources" / "help" / filename,
    };

    for (const auto& path : dev_paths) {
        if (file_exists(path.string())) {
            return "file://" + fs::canonical(path).string();
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

        return "file://" + temp_file.string();
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

std::string HelpManager::topic_to_name(HelpTopic topic) {
    switch (topic) {
        case HelpTopic::Home:
            return "Home";
        case HelpTopic::GettingStarted:
            return "Getting Started";
        case HelpTopic::Installation:
            return "Installation";
        case HelpTopic::UserGuide:
            return "User Guide";
        case HelpTopic::FAQ:
            return "FAQ";
        case HelpTopic::Security:
            return "Security";
        case HelpTopic::SecurityBestPractices:
            return "Security Best Practices";
        default:
            return "Help";
    }
}

} // namespace Utils
