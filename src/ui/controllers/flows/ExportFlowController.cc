// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "ExportFlowController.h"

#include <format>
#include <utility>

namespace UI::Flows {

ExportFlowController::ExportFlowController(Ports ports)
    : m_ports(std::move(ports)) {
}

void ExportFlowController::start_export(const std::string& current_vault_path, bool vault_open) {
    if (!vault_open) {
        if (m_ports.messages.error) {
            m_ports.messages.error("Please open a vault first before exporting accounts.", "Error");
        }
        return;
    }

    if (!m_ports.warning.confirm_plaintext_export) {
        if (m_ports.messages.error) {
            m_ports.messages.error("Export failed: internal error (missing warning dialog port)",
                                  "Error");
        }
        return;
    }

    m_ports.warning.confirm_plaintext_export([this, current_vault_path](bool proceed) {
        if (!proceed) {
            return;
        }

        const auto continue_flow = [this, current_vault_path]() {
            if (!m_ports.password_prompt.prompt_password) {
                if (m_ports.messages.error) {
                    m_ports.messages.error(
                        "Export failed: internal error (missing password prompt port)", "Error");
                }
                return;
            }

            m_ports.password_prompt.prompt_password([this, current_vault_path](std::string password) {
                auto auth_res = m_ports.auth.authenticate
                                    ? m_ports.auth.authenticate(password)
                                    : std::unexpected("Export failed: internal error (missing auth port)");

                if (m_ports.secret_cleaner.cleanse) {
                    m_ports.secret_cleaner.cleanse(password);
                }

                if (!auth_res.has_value()) {
                    if (m_ports.messages.error) {
                        m_ports.messages.error(auth_res.error(), "Error");
                    }
                    return;
                }

                if (!m_ports.file_dialogs.save_file) {
                    if (m_ports.messages.error) {
                        m_ports.messages.error(
                            "Export failed: internal error (missing save file dialog port)",
                            "Error");
                    }
                    return;
                }

                FileDialogPort::Filters filters = {
                    {"CSV files (*.csv)", "*.csv"},
                    {"KeePass XML (*.xml) - Not fully tested", "*.xml"},
                    {"1Password 1PIF (*.1pif) - Not fully tested", "*.1pif"},
                    {"All files", "*"},
                };

                m_ports.file_dialogs.save_file(
                    "Export Accounts",
                    "passwords_export.csv",
                    filters,
                    [this](const std::string& dest_path) {
                        auto export_res = m_ports.export_op.export_to_path
                                              ? m_ports.export_op.export_to_path(dest_path)
                                              : std::unexpected(
                                                    "Export failed: internal error (missing export port)");

                        if (!export_res.has_value()) {
                            if (m_ports.messages.error) {
                                m_ports.messages.error(std::format("Export failed: {}", export_res.error()),
                                                      "Error");
                            }
                            return;
                        }

                        const auto& s = export_res.value();
                        if (m_ports.messages.info) {
                            m_ports.messages.info(
                                std::format(
                                    "Successfully exported {} account(s) to {} format:\n{}\n\n{}",
                                    s.account_count,
                                    s.format_name,
                                    s.path,
                                    s.warning_text),
                                "Export Successful");
                        }
                    });
            });
        };

        if (m_ports.scheduler.on_idle) {
            m_ports.scheduler.on_idle(continue_flow);
        } else {
            continue_flow();
        }
    });
}

}  // namespace UI::Flows
