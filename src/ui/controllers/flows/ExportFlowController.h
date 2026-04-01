// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

#include "VaultIOFlowPorts.h"

#include <string>

namespace UI::Flows {

/** @brief Coordinates the user-facing vault export flow. */
class ExportFlowController {
public:
    /** @brief Dependency ports required to execute export flow steps. */
    struct Ports {
        /** @brief Message dialog/show notification port. */
        MessagePort messages;
        /** @brief UI-thread scheduling port. */
        SchedulerPort scheduler;
        /** @brief Plaintext export warning confirmation port. */
        ExportWarningPort warning;
        /** @brief Password prompt port for re-authentication. */
        PasswordPromptPort password_prompt;
        /** @brief Secret cleanup port for sensitive values. */
        SecretCleanerPort secret_cleaner;
        /** @brief Authentication port used before export. */
        ExportAuthPort auth;
        /** @brief Open/save file dialog port. */
        FileDialogPort file_dialogs;
        /** @brief Concrete export operation port. */
        ExportOperationPort export_op;
    };

    /** @brief Construct controller with concrete flow ports. */
    explicit ExportFlowController(Ports ports);

    /** @brief Start export flow from current vault context. */
    void start_export(const std::string& current_vault_path, bool vault_open);

private:
    Ports m_ports;
};

}  // namespace UI::Flows
