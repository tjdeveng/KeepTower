// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "MigrationFlowController.h"

#include <format>
#include <utility>

namespace UI::Flows {

MigrationFlowController::MigrationFlowController(Ports ports)
    : m_ports(std::move(ports)) {
}

void MigrationFlowController::start_migration(const std::string& current_vault_path,
                                             bool vault_open,
                                             const UpdateCallback& on_success) {
    if (!vault_open) {
        if (m_ports.messages.error) {
            m_ports.messages.error("No vault is currently open.\nPlease open a vault first.", "Error");
        }
        return;
    }

    if (m_ports.op.is_already_v2 && m_ports.op.is_already_v2()) {
        if (m_ports.messages.error) {
            m_ports.messages.error(
                "This vault is already in V2 multi-user format.\nNo migration needed.",
                "Error");
        }
        return;
    }

    if (!m_ports.dialog.prompt_migration) {
        if (m_ports.messages.error) {
            m_ports.messages.error("Migration failed: internal error (missing migration dialog port)",
                                  "Error");
        }
        return;
    }

    m_ports.dialog.prompt_migration(
        current_vault_path,
        [this, on_success, current_vault_path](std::optional<MigrationDialogPort::Params> params) {
            if (!params.has_value()) {
                return;
            }

            if (!m_ports.op.migrate_v1_to_v2) {
                if (m_ports.messages.error) {
                    m_ports.messages.error(
                        "Migration failed: internal error (missing migration operation port)",
                        "Error");
                }
                return;
            }

            auto res = m_ports.op.migrate_v1_to_v2(params.value(), current_vault_path);
            if (!res.has_value()) {
                if (m_ports.messages.error) {
                    m_ports.messages.error(res.error(), "Error");
                }
                return;
            }

            if (on_success) {
                on_success();
            }

            if (m_ports.messages.info) {
                m_ports.messages.info(
                    std::format(
                        "Your vault has been successfully upgraded to V2 multi-user format.\n\n"
                        "• Administrator account: {}\n"
                        "• Backup created: {}.v1.backup\n"
                        "• You can now add additional users via Tools → Manage Users",
                        params->admin_username,
                        current_vault_path),
                    "Migration Successful");
            }
        });
}

}  // namespace UI::Flows
