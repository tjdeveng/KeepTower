// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "Application.h"
#include "../ui/windows/MainWindow.h"
#include "../ui/dialogs/PasswordDialog.h"
#include "../ui/dialogs/PreferencesDialog.h"
#include "../config.h"

Application::Application()
    : Gtk::Application("com.tjdeveng.keeptower", Gio::Application::Flags::DEFAULT_FLAGS) {
}

Glib::RefPtr<Application> Application::create() {
    return Glib::make_refptr_for_instance<Application>(new Application());
}

void Application::on_startup() {
    Gtk::Application::on_startup();

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

    window->signal_hide().connect(
        sigc::bind(sigc::mem_fun(*this, &Application::on_hide_window), window)
    );

    window->present();
}

void Application::on_hide_window(Gtk::Window* window) {
    delete window;
}

void Application::on_action_quit() {
    auto windows = get_windows();
    for (auto window : windows) {
        window->close();
    }
}

void Application::on_action_about() {
    auto window = get_active_window();
    if (!window) return;

    auto dialog = new Gtk::AboutDialog();
    dialog->set_transient_for(*window);
    dialog->set_modal(true);
    dialog->set_hide_on_close(true);

    dialog->set_program_name(PROJECT_NAME);
    dialog->set_version(VERSION);
    dialog->set_comments("Secure password manager with AES-256-GCM encryption and Reed-Solomon error correction");
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

    dialog->signal_close_request().connect([dialog]() {
        delete dialog;
        return true;
    }, false);

    dialog->present();
}
