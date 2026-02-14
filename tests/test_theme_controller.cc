// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file test_theme_controller.cc
 * @brief Unit tests for ThemeController (headless via injected apply callback)
 */

#include <gtest/gtest.h>

#include <glibmm/init.h>
#include <giomm/init.h>
#include <giomm/settings.h>

#include <cstdlib>
#include <filesystem>

#include "../src/ui/controllers/ThemeController.h"

class ThemeControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        Glib::init();
        Gio::init();

        const char* schema_dir = std::getenv("GSETTINGS_SCHEMA_DIR");
        if (!schema_dir) {
            GTEST_SKIP() << "GSETTINGS_SCHEMA_DIR not set";
        }

        namespace fs = std::filesystem;
        const fs::path schema_path{schema_dir};
        const fs::path schema_xml = schema_path / "com.tjdeveng.keeptower.gschema.xml";
        if (fs::exists(schema_xml)) {
            const std::string cmd = "glib-compile-schemas " + schema_path.string();
            const int rc = std::system(cmd.c_str());
            if (rc != 0) {
                GTEST_SKIP() << "Failed to compile GSettings schemas with: " << cmd;
            }
        }

        try {
            m_settings = Gio::Settings::create("com.tjdeveng.keeptower");
        } catch (const Glib::Error& e) {
            GTEST_SKIP() << "Could not create settings: " << e.what();
        }

        // Ensure a known baseline for each test.
        m_settings->reset("color-scheme");
    }

    Glib::RefPtr<Gio::Settings> m_settings;
};

TEST_F(ThemeControllerTest, ApplyNow_DarkSetsPreferDarkTrue) {
    bool applied_value = false;
    int apply_calls = 0;

    m_settings->set_string("color-scheme", "dark");

    ThemeController controller(
        m_settings,
        [&](bool prefer_dark) {
            applied_value = prefer_dark;
            apply_calls++;
        }
    );

    controller.apply_now();

    EXPECT_EQ(apply_calls, 1);
    EXPECT_TRUE(applied_value);
}

TEST_F(ThemeControllerTest, ApplyNow_LightSetsPreferDarkFalse) {
    bool applied_value = true;
    int apply_calls = 0;

    m_settings->set_string("color-scheme", "light");

    ThemeController controller(
        m_settings,
        [&](bool prefer_dark) {
            applied_value = prefer_dark;
            apply_calls++;
        }
    );

    controller.apply_now();

    EXPECT_EQ(apply_calls, 1);
    EXPECT_FALSE(applied_value);
}

TEST_F(ThemeControllerTest, Start_CallsApplyAndDoesNotRequireGtkSettings) {
    int apply_calls = 0;

    m_settings->set_string("color-scheme", "dark");

    ThemeController controller(
        m_settings,
        [&](bool) {
            apply_calls++;
        }
    );

    controller.start();

    EXPECT_GE(apply_calls, 1);

    // Idempotent start/stop should be safe.
    controller.start();
    controller.stop();
    controller.stop();
}
