// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include <gtkmm.h>
#include <giomm/resource.h>
#include <gio/gio.h>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <string>
#include "application/Application.h"

// Declare the resource getter function (C++ name mangled)
GResource* keeptower_get_resource(void);

namespace {
void configure_appimage_schema_env() {
    const char* appdir = std::getenv("APPDIR");
    if (!appdir || *appdir == '\0') {
        return;
    }

    const std::filesystem::path appdir_path(appdir);
    const std::array<std::filesystem::path, 3> candidates = {
        appdir_path / "usr" / "share" / "glib-2.0" / "schemas",
        appdir_path / "share" / "glib-2.0" / "schemas",
        appdir_path / "usr" / "local" / "share" / "glib-2.0" / "schemas",
    };

    for (const auto& candidate : candidates) {
        const std::filesystem::path compiled = candidate / "gschemas.compiled";
        std::error_code ec;
        if (std::filesystem::exists(compiled, ec)) {
            const std::string schema_dir = candidate.string();
            ::setenv("GSETTINGS_SCHEMA_DIR", schema_dir.c_str(), 1);

            const std::filesystem::path share_dir = candidate.parent_path().parent_path();
            const std::string share_str = share_dir.string();
            const char* existing = std::getenv("XDG_DATA_DIRS");
            std::string xdg = share_str;
            if (existing && *existing != '\0') {
                xdg += ":";
                xdg += existing;
            } else {
                xdg += ":/usr/local/share:/usr/share";
            }
            ::setenv("XDG_DATA_DIRS", xdg.c_str(), 1);
            return;
        }
    }
}
} // namespace

int main(int argc, char* argv[]) {
    configure_appimage_schema_env();

    // Register embedded resources
    auto resource = Glib::wrap(keeptower_get_resource());
    resource->register_global();

    auto app = Application::create();
    return app->run(argc, argv);
}