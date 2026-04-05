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

/**
 * @class KekDerivationSettingsAdapter
 * @brief Maps persisted Gio settings to KEK derivation choices.
 *
 * Keeps settings/schema interpretation in core while preserving
 * `KekDerivationService` as a pure cryptographic library surface.
 */
class KekDerivationSettingsAdapter {
public:
    /**
     * @brief Resolve KEK derivation algorithm from settings.
     * @param settings Optional Gio settings handle.
     * @return Effective algorithm, applying policy-safe fallbacks.
     */
    [[nodiscard]] static KekDerivationService::Algorithm get_algorithm(
        const Glib::RefPtr<Gio::Settings>& settings) noexcept;

    /**
     * @brief Resolve KEK derivation parameters from settings.
     * @param settings Optional Gio settings handle.
     * @return Effective parameter set with defaults/fallbacks.
     */
    [[nodiscard]] static KekDerivationService::AlgorithmParameters get_parameters(
        const Glib::RefPtr<Gio::Settings>& settings) noexcept;
};

} // namespace KeepTower