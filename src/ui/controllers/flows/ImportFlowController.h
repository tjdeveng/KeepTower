// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

#include "VaultIOFlowPorts.h"

#include <functional>

namespace UI::Flows {

/** @brief Coordinates the user-facing vault import flow. */
class ImportFlowController {
public:
    /** @brief Callback fired after successful import updates vault content. */
    using UpdateCallback = std::function<void()>;

    /** @brief Dependency ports required to execute import flow steps. */
    struct Ports {
        /** @brief Message dialog/show notification port. */
        MessagePort messages;
        /** @brief Open/save file dialog port. */
        FileDialogPort file_dialogs;
        /** @brief Concrete import operation port. */
        ImportOperationPort import_op;
    };

    /** @brief Construct controller with concrete flow ports. */
    explicit ImportFlowController(Ports ports);

    /** @brief Start import flow and invoke callback when UI refresh is needed. */
    void start_import(const UpdateCallback& on_update);

private:
    Ports m_ports;
};

}  // namespace UI::Flows
