// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include <gtest/gtest.h>

#include "ui/controllers/flows/ExportFlowController.h"
#include "ui/controllers/flows/ImportFlowController.h"
#include "ui/controllers/flows/MigrationFlowController.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

struct CallLog {
    std::vector<std::string> calls;

    void push(std::string s) { calls.push_back(std::move(s)); }
};

}  // namespace

TEST(ExportFlowController, VaultClosedShowsErrorOnly) {
    CallLog log;

    UI::Flows::ExportFlowController controller(UI::Flows::ExportFlowController::Ports{
        UI::Flows::MessagePort{
            [&](const std::string&, const std::string&) { log.push("info"); },
            [&](const std::string&, const std::string&) { log.push("warning_msg"); },
            [&](const std::string& msg, const std::string&) {
                log.push(std::string("error:") + msg);
            },
        },
        UI::Flows::SchedulerPort{[&](std::function<void()> fn) {
            log.push("idle");
            fn();
        }},
        UI::Flows::ExportWarningPort{[&](std::function<void(bool)>) { log.push("warning"); }},
        UI::Flows::PasswordPromptPort{
            []() -> std::optional<std::string> { return std::nullopt; },
            [&](std::function<void(std::string)>) { log.push("password_prompt"); },
        },
        UI::Flows::SecretCleanerPort{[&](std::string&) { log.push("cleanse"); }},
        UI::Flows::ExportAuthPort{[&](const std::string&) {
            log.push("auth");
            return std::expected<void, std::string>{};
        }},
        UI::Flows::FileDialogPort{{}, {},},
        UI::Flows::ExportOperationPort{[&](const std::string&) {
            log.push("export");
            return std::expected<UI::Flows::ExportOperationPort::Success, std::string>{};
        }},
    });

    controller.start_export("/vault.vault", false);

    ASSERT_EQ(log.calls.size(), 1u);
    EXPECT_TRUE(log.calls[0].starts_with("error:"));
}

TEST(ExportFlowController, HappyPathRunsWarningIdlePasswordAuthCleanseSaveExportInfo) {
    CallLog log;
    std::string observed_password;

    UI::Flows::ExportFlowController controller(UI::Flows::ExportFlowController::Ports{
        UI::Flows::MessagePort{
            [&](const std::string&, const std::string&) { log.push("info"); },
            [&](const std::string&, const std::string&) { log.push("warning_msg"); },
            [&](const std::string&, const std::string&) { log.push("error"); },
        },
        UI::Flows::SchedulerPort{[&](std::function<void()> fn) {
            log.push("idle");
            fn();
        }},
        UI::Flows::ExportWarningPort{[&](std::function<void(bool)> cb) {
            log.push("warning");
            cb(true);
        }},
        UI::Flows::PasswordPromptPort{
            []() -> std::optional<std::string> { return std::nullopt; },
            [&](std::function<void(std::string)> cb) {
                log.push("password_prompt");
                cb("pw");
            },
        },
        UI::Flows::SecretCleanerPort{[&](std::string& secret) {
            log.push("cleanse");
            observed_password = secret;
            secret.clear();
        }},
        UI::Flows::ExportAuthPort{[&](const std::string& password) {
            log.push("auth");
            EXPECT_EQ(password, "pw");
            return std::expected<void, std::string>{};
        }},
        UI::Flows::FileDialogPort{
            {},
            [&](const std::string&, const std::string&, const UI::Flows::FileDialogPort::Filters&, std::function<void(const std::string&)> cb) {
                log.push("save_file");
                cb("/tmp/out.csv");
            },
        },
        UI::Flows::ExportOperationPort{[&](const std::string& dest_path) {
            log.push("export");
            EXPECT_EQ(dest_path, "/tmp/out.csv");
            return std::expected<UI::Flows::ExportOperationPort::Success, std::string>{
                UI::Flows::ExportOperationPort::Success{
                    .path = dest_path,
                    .format_name = "CSV",
                    .warning_text = "warn",
                    .account_count = 3,
                }};
        }},
    });

    controller.start_export("/vault.vault", true);

    EXPECT_EQ(observed_password, "pw");

    std::vector<std::string> expected = {
        "warning",
        "idle",
        "password_prompt",
        "auth",
        "cleanse",
        "save_file",
        "export",
        "info",
    };
    EXPECT_EQ(log.calls, expected);
}

TEST(ExportFlowController, AuthFailureStillCleansesAndStopsBeforeSave) {
    CallLog log;

    UI::Flows::ExportFlowController controller(UI::Flows::ExportFlowController::Ports{
        UI::Flows::MessagePort{
            [&](const std::string&, const std::string&) { log.push("info"); },
            [&](const std::string&, const std::string&) { log.push("warning_msg"); },
            [&](const std::string& msg, const std::string&) {
                log.push(std::string("error:") + msg);
            },
        },
        UI::Flows::SchedulerPort{[&](std::function<void()> fn) {
            log.push("idle");
            fn();
        }},
        UI::Flows::ExportWarningPort{[&](std::function<void(bool)> cb) {
            log.push("warning");
            cb(true);
        }},
        UI::Flows::PasswordPromptPort{
            []() -> std::optional<std::string> { return std::nullopt; },
            [&](std::function<void(std::string)> cb) {
                log.push("password_prompt");
                cb("pw");
            },
        },
        UI::Flows::SecretCleanerPort{[&](std::string&) { log.push("cleanse"); }},
        UI::Flows::ExportAuthPort{[&](const std::string&) -> std::expected<void, std::string> {
            log.push("auth");
            return std::unexpected("Authentication failed. Export cancelled.");
        }},
        UI::Flows::FileDialogPort{
            {},
            [&](const std::string&, const std::string&, const UI::Flows::FileDialogPort::Filters&, std::function<void(const std::string&)>) {
                log.push("save_file");
            },
        },
        UI::Flows::ExportOperationPort{[&](const std::string&) {
            log.push("export");
            return std::expected<UI::Flows::ExportOperationPort::Success, std::string>{};
        }},
    });

    controller.start_export("/vault.vault", true);

    std::vector<std::string> expected_prefix = {
        "warning",
        "idle",
        "password_prompt",
        "auth",
        "cleanse",
    };

    ASSERT_GE(log.calls.size(), expected_prefix.size());
    EXPECT_TRUE(std::equal(expected_prefix.begin(), expected_prefix.end(), log.calls.begin()));

    // Must show error, and must not proceed to save/export
    bool saw_error = false;
    for (const auto& c : log.calls) {
        if (c.starts_with("error:")) {
            saw_error = true;
        }
        EXPECT_NE(c, "save_file");
        EXPECT_NE(c, "export");
    }
    EXPECT_TRUE(saw_error);
}

TEST(ImportFlowController, SuccessShowsInfoAndCallsUpdate) {
    CallLog log;
    bool updated = false;

    UI::Flows::ImportFlowController controller(UI::Flows::ImportFlowController::Ports{
        UI::Flows::MessagePort{
            [&](const std::string&, const std::string&) { log.push("info"); },
            [&](const std::string&, const std::string&) { log.push("warning_msg"); },
            [&](const std::string&, const std::string&) { log.push("error"); },
        },
        UI::Flows::FileDialogPort{
            [&](const std::string&, const UI::Flows::FileDialogPort::Filters&, std::function<void(const std::string&)> cb) {
                log.push("open_file");
                cb("/tmp/in.csv");
            },
            {},
        },
        UI::Flows::ImportOperationPort{[&](const std::string&) {
            log.push("import");
            return std::expected<UI::Flows::ImportOperationPort::Summary, std::string>{
                UI::Flows::ImportOperationPort::Summary{
                    .format_name = "CSV",
                    .imported_count = 2,
                    .failed_count = 0,
                    .failed_accounts = {},
                }};
        }},
    });

    controller.start_import([&]() {
        log.push("update");
        updated = true;
    });

    EXPECT_TRUE(updated);
    EXPECT_EQ(log.calls, (std::vector<std::string>{"open_file", "import", "update", "info"}));
}

TEST(MigrationFlowController, SuccessCallsMigrateThenSuccessCallbackThenInfo) {
    CallLog log;
    bool success = false;

    UI::Flows::MigrationFlowController controller(UI::Flows::MigrationFlowController::Ports{
        UI::Flows::MessagePort{
            [&](const std::string&, const std::string&) { log.push("info"); },
            [&](const std::string&, const std::string&) { log.push("warning_msg"); },
            [&](const std::string&, const std::string&) { log.push("error"); },
        },
        UI::Flows::MigrationDialogPort{
            [&](const std::string&, std::function<void(std::optional<UI::Flows::MigrationDialogPort::Params>)> cb) {
                log.push("prompt_migration");
                cb(UI::Flows::MigrationDialogPort::Params{.admin_username = "admin", .admin_password = "pw"});
            },
        },
        UI::Flows::MigrationOperationPort{
            [&]() {
                log.push("is_already_v2");
                return false;
            },
            [&](const UI::Flows::MigrationDialogPort::Params& params, const std::string&) {
                log.push(std::string("migrate:") + params.admin_username);
                return std::expected<void, std::string>{};
            },
        },
    });

    controller.start_migration("/vault.vault", true, [&]() {
        log.push("on_success");
        success = true;
    });

    EXPECT_TRUE(success);
    EXPECT_EQ(log.calls,
              (std::vector<std::string>{"is_already_v2", "prompt_migration", "migrate:admin", "on_success", "info"}));
}
