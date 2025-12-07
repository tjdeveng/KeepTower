// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "Application.h"
#include "../ui/windows/MainWindow.h"
#include "../ui/dialogs/PasswordDialog.h"
#include "../config.h"

Application::Application()
    : Gtk::Application("com.example.keeptower", Gio::Application::Flags::DEFAULT_FLAGS) {
}

Glib::RefPtr<Application> Application::create() {
    return Glib::make_refptr_for_instance<Application>(new Application());
}

void Application::on_startup() {
    Gtk::Application::on_startup();

    // Add application actions
    add_action("quit", sigc::mem_fun(*this, &Application::on_action_quit));
    add_action("about", sigc::mem_fun(*this, &Application::on_action_about));
    add_action("preferences", sigc::mem_fun(*this, &Application::on_action_preferences));

    // Set keyboard accelerators
    set_accel_for_action("app.quit", "<Ctrl>Q");
    set_accel_for_action("app.preferences", "<Ctrl>comma");
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
    auto dialog = Gtk::AboutDialog();
    dialog.set_transient_for(*get_active_window());
    dialog.set_modal(true);

    dialog.set_program_name(PROJECT_NAME);
    dialog.set_version(VERSION);
    dialog.set_comments("A GNOME GTK4 Application");
    dialog.set_license_type(Gtk::License::GPL_3_0);
    dialog.set_website("https://github.com/yourusername/keeptower");
    dialog.set_website_label("GitHub Repository");

    std::vector<Glib::ustring> authors = {"Your Name"};
    dialog.set_authors(authors);

    dialog.present();
}

void Application::on_action_preferences() {
    // TODO: Implement preferences dialog
    g_print("Preferences dialog not yet implemented\n");
}
