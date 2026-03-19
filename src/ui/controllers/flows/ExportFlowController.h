// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

#include "VaultIOFlowPorts.h"

#include <string>

namespace UI::Flows {

class ExportFlowController {
public:
    struct Ports {
        MessagePort messages;
        SchedulerPort scheduler;
        ExportWarningPort warning;
        PasswordPromptPort password_prompt;
        SecretCleanerPort secret_cleaner;
        ExportAuthPort auth;
        FileDialogPort file_dialogs;
        ExportOperationPort export_op;
    };

    explicit ExportFlowController(Ports ports);

    void start_export(const std::string& current_vault_path, bool vault_open);

private:
    Ports m_ports;
};

}  // namespace UI::Flows
