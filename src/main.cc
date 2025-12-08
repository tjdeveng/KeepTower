// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include <gtkmm.h>
#include <giomm/resource.h>
#include <gio/gio.h>
#include "application/Application.h"

// Declare the resource getter function (C++ name mangled)
GResource* keeptower_get_resource(void);

int main(int argc, char* argv[]) {
    // Register embedded resources
    auto resource = Glib::wrap(keeptower_get_resource());
    resource->register_global();

    auto app = Application::create();
    return app->run(argc, argv);
}