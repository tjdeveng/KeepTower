// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "ImportFlowController.h"

#include <format>
#include <utility>

namespace UI::Flows {

ImportFlowController::ImportFlowController(Ports ports)
    : m_ports(std::move(ports)) {
}

void ImportFlowController::start_import(const UpdateCallback& on_update) {
    if (!m_ports.file_dialogs.open_file) {
        if (m_ports.messages.error) {
            m_ports.messages.error("Import failed: internal error (missing open file dialog port)",
                                  "Error");
        }
        return;
    }

    FileDialogPort::Filters filters = {
        {"CSV files (*.csv)", "*.csv"},
        {"KeePass XML (*.xml)", "*.xml"},
        {"1Password 1PIF (*.1pif)", "*.1pif"},
        {"All files", "*"},
    };

    m_ports.file_dialogs.open_file(
        "Import Accounts",
        filters,
        [this, on_update](const std::string& source_path) {
            auto result = m_ports.import_op.import_into_vault
                              ? m_ports.import_op.import_into_vault(source_path)
                              : std::unexpected("internal error (missing import port)");

            if (!result.has_value()) {
                if (m_ports.messages.error) {
                    m_ports.messages.error(std::format("Import failed: {}", result.error()), "Error");
                }
                return;
            }

            const auto& s = result.value();

            if (on_update) {
                on_update();
            }

            std::string message;
            if (s.failed_count == 0) {
                message = std::format("Successfully imported {} account(s) from {} format.",
                                      s.imported_count,
                                      s.format_name);
                if (m_ports.messages.info) {
                    m_ports.messages.info(message, "Import Successful");
                }
                return;
            }

            if (s.imported_count > 0) {
                message = std::format(
                    "Imported {} account(s) successfully.\n{} account(s) failed to import.",
                    s.imported_count,
                    s.failed_count);

                if (!s.failed_accounts.empty()) {
                    message += "\n\nFailed accounts:\n";
                    for (const auto& name : s.failed_accounts) {
                        message += "• " + name + "\n";
                    }

                    if (s.failed_count > static_cast<int>(s.failed_accounts.size())) {
                        message += std::format("... and {} more",
                                               s.failed_count - static_cast<int>(s.failed_accounts.size()));
                    }
                }

                if (m_ports.messages.warning) {
                    m_ports.messages.warning(message, "Import Completed with Issues");
                }
                return;
            }

            if (m_ports.messages.error) {
                m_ports.messages.error("Failed to import all accounts.", "Import Failed");
            }
        });
}

}  // namespace UI::Flows
