// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

#include <atomic>

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
     * Initialize process-global OpenSSL provider/FIPS state once.
        * @param enable True to request FIPS mode during initialization.
        * @return True when OpenSSL providers were initialized successfully.
     */
    [[nodiscard]] static bool init_fips_mode(bool enable = false);

    /**
     * Query whether FIPS provider is available after initialization.
        * @return True when the FIPS provider is available for use.
     */
    [[nodiscard]] static bool is_fips_available();

    /**
     * Query whether FIPS mode is currently enabled.
        * @return True when FIPS mode is currently enabled.
     */
    [[nodiscard]] static bool is_fips_enabled();

    /**
     * Toggle FIPS mode at runtime.
        * @param enable True to enable FIPS mode, false to disable it.
        * @return True when the requested mode was applied successfully.
     */
    [[nodiscard]] static bool set_fips_mode(bool enable);

private:
    // Process-global state (OpenSSL providers/default properties are process-wide).
    static std::atomic<bool> s_fips_mode_initialized;
    static std::atomic<bool> s_fips_mode_available;
    static std::atomic<bool> s_fips_mode_enabled;

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
