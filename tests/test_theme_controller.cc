// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file test_theme_controller.cc
 * @brief Unit tests for ThemeController (headless via injected apply callback)
 */

#include <gtest/gtest.h>

#include <glibmm/init.h>
#include <glibmm/main.h>
#include <giomm/init.h>
#include <giomm/settings.h>
#include <giomm/settingsschemasource.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>

#include <optional>

#include "../src/ui/controllers/ThemeController.h"

namespace {
constexpr const char* kMockDesktopSchemaId = "com.tjdeveng.keeptower.mockdesktop";

class ScopedEnvVar {
public:
    ScopedEnvVar(const char* name, const char* value)
        : m_name(name) {
        const char* existing = std::getenv(name);
        if (existing) {
            m_previous = existing;
        }
        setenv(name, value, 1);
    }

    ~ScopedEnvVar() {
        if (m_previous.has_value()) {
            setenv(m_name, m_previous->c_str(), 1);
        } else {
            unsetenv(m_name);
        }
    }

private:
    const char* m_name;
    std::optional<std::string> m_previous;
};
}

class ThemeControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        setenv("GSETTINGS_BACKEND", "memory", 1);

        const char* original_schema_dir = std::getenv("GSETTINGS_SCHEMA_DIR");
        if (!original_schema_dir) {
            GTEST_SKIP() << "GSETTINGS_SCHEMA_DIR not set";
        }

        namespace fs = std::filesystem;
        m_original_schema_dir = original_schema_dir;
        m_schema_dir = fs::temp_directory_path() / "keeptower_theme_controller_schemas";
        fs::remove_all(m_schema_dir);
        fs::create_directories(m_schema_dir);

        const fs::path source_schema_path{original_schema_dir};
        const fs::path app_schema_xml = source_schema_path / "com.tjdeveng.keeptower.gschema.xml";
        if (!fs::exists(app_schema_xml)) {
            GTEST_SKIP() << "Could not find app schema xml at " << app_schema_xml;
        }

        fs::copy_file(
            app_schema_xml,
            m_schema_dir / "com.tjdeveng.keeptower.gschema.xml",
            fs::copy_options::overwrite_existing);

        std::ofstream mock_schema_file(m_schema_dir / "com.tjdeveng.keeptower.mockdesktop.gschema.xml");
        mock_schema_file
            << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            << "<schemalist>\n"
            << "  <schema id=\"" << kMockDesktopSchemaId << "\" path=\"/com/tjdeveng/keeptower/mockdesktop/\">\n"
            << "    <key name=\"color-scheme\" type=\"s\">\n"
            << "      <default>'default'</default>\n"
            << "    </key>\n"
            << "  </schema>\n"
            << "</schemalist>\n";
        mock_schema_file.close();

        setenv("GSETTINGS_SCHEMA_DIR", m_schema_dir.c_str(), 1);

        const std::string cmd = "glib-compile-schemas " + m_schema_dir.string();
        const int rc = std::system(cmd.c_str());
        if (rc != 0) {
            GTEST_SKIP() << "Failed to compile GSettings schemas with: " << cmd;
        }

        Glib::init();
        Gio::init();

        try {
            m_settings = Gio::Settings::create("com.tjdeveng.keeptower");
        } catch (const Glib::Error& e) {
            GTEST_SKIP() << "Could not create settings: " << e.what();
        }

        // Ensure a known baseline for each test.
        m_settings->reset("color-scheme");
    }

    void TearDown() override {
        namespace fs = std::filesystem;

        if (!m_original_schema_dir.empty()) {
            setenv("GSETTINGS_SCHEMA_DIR", m_original_schema_dir.c_str(), 1);
        } else {
            unsetenv("GSETTINGS_SCHEMA_DIR");
        }

        if (!m_schema_dir.empty()) {
            fs::remove_all(m_schema_dir);
        }
    }

    Glib::RefPtr<Gio::Settings> create_desktop_settings() {
        try {
            return Gio::Settings::create(kMockDesktopSchemaId);
        } catch (const Glib::Error& e) {
            ADD_FAILURE() << "Could not create desktop settings: " << e.what();
            return {};
        }
    }

    Glib::RefPtr<Gio::Settings> m_settings;
    std::filesystem::path m_schema_dir;
    std::string m_original_schema_dir;
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

TEST_F(ThemeControllerTest, ApplyNow_DefaultFollowsDesktopPreferDark) {
    auto desktop_settings = create_desktop_settings();
    desktop_settings->set_string("color-scheme", "prefer-dark");
    m_settings->set_string("color-scheme", "default");

    bool applied_value = false;
    int apply_calls = 0;

    ThemeController controller(
        m_settings,
        [&](bool prefer_dark) {
            applied_value = prefer_dark;
            apply_calls++;
        },
        desktop_settings
    );

    controller.apply_now();

    EXPECT_EQ(apply_calls, 1);
    EXPECT_TRUE(applied_value);
}

TEST_F(ThemeControllerTest, ApplyNow_DefaultFollowsDesktopLightPreference) {
    auto desktop_settings = create_desktop_settings();
    desktop_settings->set_string("color-scheme", "default");
    m_settings->set_string("color-scheme", "default");

    bool applied_value = true;
    int apply_calls = 0;

    ThemeController controller(
        m_settings,
        [&](bool prefer_dark) {
            applied_value = prefer_dark;
            apply_calls++;
        },
        desktop_settings
    );

    controller.apply_now();

    EXPECT_EQ(apply_calls, 1);
    EXPECT_FALSE(applied_value);
}

TEST_F(ThemeControllerTest, Start_ReactsToDesktopThemeChangesWhenFollowingDefault) {
    auto desktop_settings = create_desktop_settings();
    desktop_settings->set_string("color-scheme", "default");
    m_settings->set_string("color-scheme", "default");

    bool applied_value = false;
    int apply_calls = 0;

    ThemeController controller(
        m_settings,
        [&](bool prefer_dark) {
            applied_value = prefer_dark;
            apply_calls++;
        },
        desktop_settings
    );

    controller.start();
    ASSERT_GE(apply_calls, 1);
    EXPECT_FALSE(applied_value);

    desktop_settings->set_string("color-scheme", "prefer-dark");
    while (Glib::MainContext::get_default()->iteration(false)) {
    }

    EXPECT_GE(apply_calls, 2);
    EXPECT_TRUE(applied_value);
}

TEST_F(ThemeControllerTest, ApplyNow_DefaultFallsBackToLightWhenGtkThemeMissing) {
    unsetenv("GTK_THEME");
    m_settings->set_string("color-scheme", "default");

    bool applied_value = true;
    int apply_calls = 0;

    ThemeController controller(
        m_settings,
        [&](bool prefer_dark) {
            applied_value = prefer_dark;
            apply_calls++;
        },
        {}
    );

    controller.apply_now();

    EXPECT_EQ(apply_calls, 1);
    EXPECT_FALSE(applied_value);
}

TEST_F(ThemeControllerTest, Start_ReactsToAppThemeChanges) {
    m_settings->set_string("color-scheme", "dark");

    bool applied_value = false;
    int apply_calls = 0;

    ThemeController controller(
        m_settings,
        [&](bool prefer_dark) {
            applied_value = prefer_dark;
            apply_calls++;
        }
    );

    controller.start();
    ASSERT_GE(apply_calls, 1);
    EXPECT_TRUE(applied_value);

    m_settings->set_string("color-scheme", "light");
    while (Glib::MainContext::get_default()->iteration(false)) {
    }

    EXPECT_GE(apply_calls, 2);
    EXPECT_FALSE(applied_value);
}

TEST_F(ThemeControllerTest, ExplicitAppThemeDisconnectsDesktopMonitoring) {
    auto desktop_settings = create_desktop_settings();
    desktop_settings->set_string("color-scheme", "default");
    m_settings->set_string("color-scheme", "default");

    bool applied_value = false;
    int apply_calls = 0;

    ThemeController controller(
        m_settings,
        [&](bool prefer_dark) {
            applied_value = prefer_dark;
            apply_calls++;
        },
        desktop_settings
    );

    controller.start();
    ASSERT_GE(apply_calls, 1);
    EXPECT_FALSE(applied_value);

    m_settings->set_string("color-scheme", "dark");
    while (Glib::MainContext::get_default()->iteration(false)) {
    }

    const int calls_after_explicit_dark = apply_calls;
    EXPECT_TRUE(applied_value);

    desktop_settings->set_string("color-scheme", "prefer-dark");
    while (Glib::MainContext::get_default()->iteration(false)) {
    }

    EXPECT_EQ(apply_calls, calls_after_explicit_dark);
    EXPECT_TRUE(applied_value);
}

TEST_F(ThemeControllerTest, ExplicitSettingsConstructorWithoutApplyTargetIsSafeNoOp) {
    ThemeController controller(
        m_settings,
        Glib::RefPtr<Gtk::Settings>{},
        {}
    );

    m_settings->set_string("color-scheme", "dark");
    controller.apply_now();
    controller.stop();

    SUCCEED();
}

TEST_F(ThemeControllerTest, DefaultConstructorCanBeCreated) {
    ThemeController controller;
    controller.stop();

    SUCCEED();
}
