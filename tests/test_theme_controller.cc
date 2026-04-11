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
#include <mutex>
#include <vector>

#include <optional>

#include "../src/ui/controllers/ThemeController.h"

namespace {
constexpr const char* kMockDesktopSchemaId = "com.tjdeveng.keeptower.mockdesktop";

std::once_flag g_theme_test_setup_once;
bool g_theme_test_setup_ready = false;
std::string g_theme_test_setup_error;
std::filesystem::path g_theme_test_schema_dir;

std::optional<std::filesystem::path> find_app_schema_xml() {
    namespace fs = std::filesystem;

    std::vector<fs::path> candidates;
    if (const char* schema_dir = std::getenv("GSETTINGS_SCHEMA_DIR")) {
        candidates.emplace_back(schema_dir);
    }

    const fs::path cwd = fs::current_path();
    candidates.push_back(cwd / "data");
    candidates.push_back(cwd.parent_path() / "data");

    for (const auto& candidate_dir : candidates) {
        const fs::path candidate = candidate_dir / "com.tjdeveng.keeptower.gschema.xml";
        if (fs::exists(candidate)) {
            return candidate;
        }
    }

    return std::nullopt;
}

void ensure_theme_test_environment() {
    std::call_once(g_theme_test_setup_once, []() {
        namespace fs = std::filesystem;

        auto app_schema_xml = find_app_schema_xml();
        if (!app_schema_xml) {
            g_theme_test_setup_error = "Could not locate com.tjdeveng.keeptower.gschema.xml";
            return;
        }

        std::string template_path =
            (fs::temp_directory_path() / "keeptower_theme_controller_schemas_XXXXXX").string();
        std::vector<char> mutable_template(template_path.begin(), template_path.end());
        mutable_template.push_back('\0');

        char* created_dir = mkdtemp(mutable_template.data());
        if (!created_dir) {
            g_theme_test_setup_error = "Failed to create unique schema directory";
            return;
        }

        g_theme_test_schema_dir = created_dir;

        std::error_code copy_error;
        fs::copy_file(
            *app_schema_xml,
            g_theme_test_schema_dir / "com.tjdeveng.keeptower.gschema.xml",
            fs::copy_options::overwrite_existing,
            copy_error);
        if (copy_error) {
            g_theme_test_setup_error = "Failed to copy app schema xml: " + copy_error.message();
            return;
        }

        std::ofstream mock_schema_file(g_theme_test_schema_dir / "com.tjdeveng.keeptower.mockdesktop.gschema.xml");
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

        setenv("GSETTINGS_BACKEND", "memory", 1);
        setenv("GSETTINGS_SCHEMA_DIR", g_theme_test_schema_dir.c_str(), 1);

        const std::string cmd = "glib-compile-schemas " + g_theme_test_schema_dir.string();
        const int rc = std::system(cmd.c_str());
        if (rc != 0) {
            g_theme_test_setup_error = "Failed to compile GSettings schemas with: " + cmd;
            return;
        }

        Glib::init();
        Gio::init();
        g_theme_test_setup_ready = true;
    });
}
}

class ThemeControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        ensure_theme_test_environment();
        if (!g_theme_test_setup_ready) {
            GTEST_SKIP() << g_theme_test_setup_error;
        }

        try {
            m_settings = Gio::Settings::create("com.tjdeveng.keeptower");
        } catch (const Glib::Error& e) {
            GTEST_SKIP() << "Could not create settings: " << e.what();
        }

        // Ensure a known baseline for each test.
        m_settings->reset("color-scheme");
    }

    void TearDown() override {
        m_settings.reset();
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
