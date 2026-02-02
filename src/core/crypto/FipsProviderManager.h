// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

namespace KeepTower {

/**
 * Small helper that encapsulates OpenSSL 3 provider/FIPS initialization and toggling.
 *
 * This is intentionally process-global state (OpenSSL providers and default properties
 * are process-wide). VaultManager remains the public API for FIPS state tracking.
 */
class FipsProviderManager {
public:
    FipsProviderManager() = delete;

    /**
     * Initialize OpenSSL provider system and attempt to load the FIPS provider.
     *
     * @param enable If true, attempt to enable FIPS mode immediately.
     * @param out_available Set true if the FIPS provider was loaded.
     * @param out_enabled Set true if FIPS mode is enabled after init.
     * @return true if OpenSSL is usable (default provider loaded at minimum).
     */
    [[nodiscard]] static bool init(bool enable, bool& out_available, bool& out_enabled);

    /**
     * Toggle OpenSSL default properties to enable/disable FIPS.
     *
     * Note: this requires the FIPS provider to be available when enabling.
     */
    [[nodiscard]] static bool set_fips_default_properties(bool enable);
};

}  // namespace KeepTower
