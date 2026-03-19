// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

#include "VaultIOFlowPorts.h"

#include <functional>

namespace UI::Flows {

class ImportFlowController {
public:
    using UpdateCallback = std::function<void()>;

    struct Ports {
        MessagePort messages;
        FileDialogPort file_dialogs;
        ImportOperationPort import_op;
    };

    explicit ImportFlowController(Ports ports);

    void start_import(const UpdateCallback& on_update);

private:
    Ports m_ports;
};

}  // namespace UI::Flows
