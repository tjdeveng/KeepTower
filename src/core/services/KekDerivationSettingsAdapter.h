// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file KekDerivationSettingsAdapter.h
 * @brief Gio settings adapter for KEK derivation preferences
 */

#pragma once

#include "lib/crypto/KekDerivationService.h"
#include <giomm/settings.h>

namespace KeepTower {

class KekDerivationSettingsAdapter {
public:
    [[nodiscard]] static KekDerivationService::Algorithm get_algorithm(
        const Glib::RefPtr<Gio::Settings>& settings) noexcept;

    [[nodiscard]] static KekDerivationService::AlgorithmParameters get_parameters(
        const Glib::RefPtr<Gio::Settings>& settings) noexcept;
};

} // namespace KeepTower