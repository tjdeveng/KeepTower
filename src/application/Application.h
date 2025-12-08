// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file Application.h
 * @brief Main GTK application class for KeepTower
 */

#ifndef APPLICATION_H
#define APPLICATION_H

#include <gtkmm.h>
#include <memory>

class MainWindow;

/**
 * @brief Main application class for KeepTower Password Manager
 *
 * Manages the GTK application lifecycle, window creation, and application actions.
 * Follows the GTK/GNOME application design patterns with proper action handling.
 *
 * @section actions Application Actions
 * - `quit` - Exit the application
 * - `about` - Show about dialog
 * - `preferences` - Show preferences dialog (TODO)
 */
class Application : public Gtk::Application {
public:
    /**
     * @brief Factory method to create Application instance
     * @return RefPtr to new Application instance
     */
    static Glib::RefPtr<Application> create();

protected:
    Application();

    /**
     * @brief Called when application is activated
     *
     * Creates and shows the main window.
     */
    void on_activate() override;

    /**
     * @brief Called during application startup
     *
     * Registers application actions and sets up the application menu.
     */
    void on_startup() override;

private:
    void create_window();
    void on_hide_window(Gtk::Window* window);
    void on_action_quit();
    void on_action_about();
    bool show_password_dialog();

    Glib::ustring m_password;
};

#endif // APPLICATION_H
