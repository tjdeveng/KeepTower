// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include <gtkmm.h>
#include "application/Application.h"

int main(int argc, char* argv[]) {
    auto app = Application::create();
    return app->run(argc, argv);
}
