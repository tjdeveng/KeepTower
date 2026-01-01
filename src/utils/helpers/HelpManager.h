// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file HelpManager.h
 * @brief Help documentation manager with hybrid filesystem/GResources support
 *
 * Provides offline-first help documentation access with automatic fallback:
 * 1. First attempts to load from installed location (e.g., /usr/share/keeptower/help/)
 * 2. Falls back to embedded GResources if filesystem files not found
 * 3. Uses Gtk::show_uri() to open help in default browser
 *
 * This design ensures help is always available:
 * - In development builds
 * - In installed systems
 * - In Flatpak/AppImage containers
 * - When debugging network issues (user's primary use case)
 */

#pragma once

#include <glibmm/refptr.h>
#include <giomm/file.h>
#include <gtkmm/window.h>
#include <string>
#include <optional>
#include <map>

namespace Utils {

/**
 * @brief Help documentation topics
 */
enum class HelpTopic {
    Home,                    ///< Welcome and overview
    GettingStarted,          ///< First vault tutorial
    Installation,            ///< Installation guide
    UserGuide,               ///< Complete feature reference
    FAQ,                     ///< Frequently asked questions
    Security,                ///< Security features and best practices
    SecurityBestPractices    ///< Detailed security best practices
};

/**
 * @brief Manages help documentation access with hybrid storage
 *
 * Implements a hybrid approach for help documentation:
 * - Primary: Filesystem location ($datadir/keeptower/help/)
 * - Fallback: Embedded GResources (/com/tjdeveng/keeptower/help/)
 *
 * Key Features:
 * - Offline-first design (no internet required)
 * - Cross-platform path handling
 * - Automatic fallback to embedded resources
 * - Browser integration via Gtk::show_uri()
 *
 * Usage:
 * @code
 * auto& help = HelpManager::get_instance();
 * help.open_help(HelpTopic::UserGuide, parent_window);
 * @endcode
 */
class HelpManager {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to the HelpManager instance
     */
    static HelpManager& get_instance();

    /**
     * @brief Open help documentation for a specific topic
     * @param topic Help topic to open
     * @param parent Parent window for error dialogs
     * @return true if help was opened successfully, false otherwise
     */
    bool open_help(HelpTopic topic, Gtk::Window& parent);

    /**
     * @brief Check if help documentation is available
     * @param topic Specific help topic to check (or Home for general availability)
     * @return true if help is available (filesystem or GResources), false otherwise
     */
    bool is_help_available(HelpTopic topic = HelpTopic::Home) const;

    /**
     * @brief Get the URI for a help topic
     * @param topic Help topic
     * @return URI string (file:// or resource://) or empty if not found
     */
    std::string get_help_uri(HelpTopic topic) const;

    /**
     * @brief Get filesystem installation directory for help files
     * @return Path to help directory (e.g., /usr/share/keeptower/help)
     */
    static std::string get_help_install_dir();

private:
    // Singleton pattern
    HelpManager();
    ~HelpManager() = default;
    HelpManager(const HelpManager&) = delete;
    HelpManager& operator=(const HelpManager&) = delete;

    /**
     * @brief Find help file, trying filesystem first, then GResources
     * @param filename Help file name (e.g., "00-home.html")
     * @return URI to the help file or empty string if not found
     */
    std::string find_help_file(const std::string& filename) const;

    /**
     * @brief Check if a file exists at the given path
     * @param path Filesystem path to check
     * @return true if file exists and is readable
     */
    [[nodiscard]] bool file_exists(const std::string& path) const noexcept;

    /**
     * @brief Check if a GResource exists
     * @param resource_path GResource path (e.g., "/com/tjdeveng/keeptower/help/00-home.html")
     * @return true if resource exists
     */
    [[nodiscard]] bool gresource_exists(const std::string& resource_path) const noexcept;

    /**
     * @brief Extract GResource to temporary file
     * @param filename Help file name
     * @return URI to extracted file or empty string if failed
     */
    [[nodiscard]] std::string extract_from_gresource(const std::string& filename) const;

    /**
     * @brief Show error dialog (single responsibility helper)
     * @param parent Parent window
     * @param title Dialog title
     * @param message Error message
     */
    void show_error_dialog(Gtk::Window& parent,
                          const std::string& title,
                          const std::string& message) const;

    /**
     * @brief Get filename for a help topic
     * @param topic Help topic
     * @return Filename (e.g., "00-home.html")
     */
    static std::string topic_to_filename(HelpTopic topic);

    /**
     * @brief Get human-readable name for a help topic
     * @param topic Help topic
     * @return Display name (e.g., "User Guide")
     */
    static std::string topic_to_name(HelpTopic topic);

    // Topic to filename mapping
    static const std::map<HelpTopic, std::string> topic_filenames_;
};

} // namespace Utils
