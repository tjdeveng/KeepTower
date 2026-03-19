// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

#include "VaultIOFlowPorts.h"

#include <functional>
#include <string>

namespace UI::Flows {

class MigrationFlowController {
public:
    using UpdateCallback = std::function<void()>;

    struct Ports {
        MessagePort messages;
        MigrationDialogPort dialog;
        MigrationOperationPort op;
    };

    explicit MigrationFlowController(Ports ports);

    void start_migration(const std::string& current_vault_path,
                         bool vault_open,
                         const UpdateCallback& on_success);

private:
    Ports m_ports;
};

}  // namespace UI::Flows
