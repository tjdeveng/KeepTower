#include <sigc++/signal.h>
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "Application.h"
#include "../ui/windows/MainWindow.h"
#include "../ui/dialogs/PasswordDialog.h"
#include "../ui/dialogs/PreferencesDialog.h"
#include "../core/VaultManager.h"
#include "../utils/Log.h"
#include "../config.h"
#include <giomm/settings.h>
#include <memory>

Application::Application()
    : Gtk::Application("com.tjdeveng.keeptower", Gio::Application::Flags::DEFAULT_FLAGS) {
}

Glib::RefPtr<Application> Application::create() {
    return Glib::make_refptr_for_instance<Application>(new Application());
}

void Application::on_startup() {
    Gtk::Application::on_startup();

    // Read FIPS preference from GSettings
    bool enable_fips = false;
    try {
        // Check if schema exists before trying to create settings
        auto schema_source = Gio::SettingsSchemaSource::get_default();
        if (!schema_source) {
            KeepTower::Log::warning("GSettings schema source not available - using default FIPS setting (disabled)");
            enable_fips = false;
        } else {
            auto schema = schema_source->lookup("com.tjdeveng.keeptower", false);
            if (!schema) {
                KeepTower::Log::warning("GSettings schema 'com.tjdeveng.keeptower' not found - using default FIPS setting (disabled)");
                KeepTower::Log::info("This is normal for AppImage/portable builds. Install system-wide to enable settings persistence.");
                enable_fips = false;
            } else if (!schema->has_key("fips-mode-enabled")) {
                KeepTower::Log::warning("GSettings key 'fips-mode-enabled' not found in schema - using default (disabled)");
                enable_fips = false;
            } else {
                auto settings = Gio::Settings::create("com.tjdeveng.keeptower");
                enable_fips = settings->get_boolean("fips-mode-enabled");
                KeepTower::Log::info("FIPS mode preference: {}", enable_fips ? "enabled" : "disabled");
            }
        }
    } catch (const Glib::Error& e) {
        KeepTower::Log::warning("Failed to read FIPS preference: {} - defaulting to disabled", e.what());
        enable_fips = false;
    } catch (const std::exception& e) {
        KeepTower::Log::warning("Unexpected error reading settings: {} - defaulting to FIPS disabled", e.what());
        enable_fips = false;
    }

    // Initialize FIPS mode
    if (!VaultManager::init_fips_mode(enable_fips)) {
        KeepTower::Log::error("Failed to initialize FIPS mode");
        // Continue anyway - VaultManager will use default provider
    }

    if (VaultManager::is_fips_available()) {
        KeepTower::Log::info("FIPS-140-3 provider available (enabled={})", VaultManager::is_fips_enabled());
    } else {
        KeepTower::Log::info("FIPS-140-3 provider not available - using default provider");
    }

    // Load custom CSS for theme-aware message colors
    auto css_provider = Gtk::CssProvider::create();
    try {
        css_provider->load_from_resource("/com/tjdeveng/keeptower/styles/message-colors.css");
        Gtk::StyleContext::add_provider_for_display(
            Gdk::Display::get_default(),
            css_provider,
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
        KeepTower::Log::info("Loaded theme-aware CSS");
    } catch (const Glib::Error& e) {
        KeepTower::Log::warning("Failed to load CSS: {}", e.what());
    }

    // Add application actions
    add_action("quit", sigc::mem_fun(*this, &Application::on_action_quit));
    add_action("about", sigc::mem_fun(*this, &Application::on_action_about));

    // Set keyboard accelerators
    set_accel_for_action("app.quit", "<Ctrl>Q");
}

void Application::on_activate() {
    // Show existing window or create a new one
    auto windows = get_windows();
    if (windows.empty()) {
        // For multi-vault support, just create the main window
        // User can open vaults via File menu
        create_window();
    } else {
        windows[0]->present();
    }
}

void Application::create_window() {
    auto window = new MainWindow();
    add_window(*window);

    // GTK will automatically delete the window when closed
    window->set_hide_on_close(false);

    window->present();
}

void Application::on_action_quit() {
    auto windows = get_windows();
    for (auto window : windows) {
        window->close();
    }
}

void Application::on_action_about() {
    auto window = get_active_window();
    if (window == nullptr) return;

    auto dialog = std::make_unique<Gtk::AboutDialog>();
    dialog->set_transient_for(*window);
    dialog->set_modal(true);
    dialog->set_hide_on_close(true);

    dialog->set_program_name(PROJECT_NAME);
    dialog->set_version(VERSION);

    // Build comments with FIPS status
    std::string comments = "Secure password manager with AES-256-GCM encryption and Reed-Solomon error correction";
    if (VaultManager::is_fips_available()) {
        if (VaultManager::is_fips_enabled()) {
            comments += "\n\nFIPS-140-3: Enabled ✓";
        } else {
            comments += "\n\nFIPS-140-3: Available (not enabled)";
        }
    } else {
        comments += "\n\nFIPS-140-3: Not available";
    }
    dialog->set_comments(comments);

    dialog->set_copyright("Copyright © 2025 TJDev");
    dialog->set_license_type(Gtk::License::GPL_3_0);
    dialog->set_website("https://github.com/tjdeveng/KeepTower");
    dialog->set_website_label("GitHub Repository");

    // Set application icon - load from embedded resources
    try {
        // Path includes the exact file path as specified in gresource.xml
        auto resource_path = "/com/tjdeveng/keeptower/../data/icons/hicolor/scalable/apps/com.tjdeveng.keeptower.svg";
        auto pixbuf = Gdk::Pixbuf::create_from_resource(resource_path);
        auto texture = Gdk::Texture::create_for_pixbuf(pixbuf);
        dialog->set_logo(texture);
    } catch (const Glib::Error& ex) {
        g_warning("Failed to load application icon from resources: %s", ex.what());
    }

    std::vector<Glib::ustring> authors = {"TJDev"};
    dialog->set_authors(authors);

    // Transfer ownership to GTK - dialog will be deleted when closed
    dialog.release()->set_visible(true);
}
