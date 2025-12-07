// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#ifndef APPLICATION_H
#define APPLICATION_H

#include <gtkmm.h>
#include <memory>

class MainWindow;

class Application : public Gtk::Application {
public:
    static Glib::RefPtr<Application> create();

protected:
    Application();

    void on_activate() override;
    void on_startup() override;

private:
    void create_window();
    void on_hide_window(Gtk::Window* window);
    void on_action_quit();
    void on_action_about();
    void on_action_preferences();
    bool show_password_dialog();

    Glib::ustring m_password;
};

#endif // APPLICATION_H
