// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "FipsProviderManager.h"

#include "../../utils/Log.h"

#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/provider.h>

#include <atomic>
#include <cstdlib>
#include <mutex>

namespace KeepTower {

std::atomic<bool> FipsProviderManager::s_fips_mode_initialized{false};
std::atomic<bool> FipsProviderManager::s_fips_mode_available{false};
std::atomic<bool> FipsProviderManager::s_fips_mode_enabled{false};

namespace {
std::once_flag g_openssl_cleanup_registered;
OSSL_PROVIDER* g_fips_provider{nullptr};
OSSL_PROVIDER* g_default_provider{nullptr};

void log_openssl_error(const char* context) {
    unsigned long err = ERR_get_error();
    if (err == 0) {
        return;
    }

    char err_buf[256];
    ERR_error_string_n(err, err_buf, sizeof(err_buf));
    KeepTower::Log::error("{}: OpenSSL error: {}", context, err_buf);
}

void openssl_cleanup_at_exit() {
    // Best-effort cleanup for sanitizer runs and orderly shutdown.
    // Unload providers first, then clean up global OpenSSL state.
    if (g_default_provider != nullptr) {
        OSSL_PROVIDER_unload(g_default_provider);
        g_default_provider = nullptr;
    }
    if (g_fips_provider != nullptr) {
        OSSL_PROVIDER_unload(g_fips_provider);
        g_fips_provider = nullptr;
    }

    OPENSSL_cleanup();
}

void register_openssl_cleanup_once() {
    std::call_once(g_openssl_cleanup_registered, []() {
        (void)std::atexit(openssl_cleanup_at_exit);
    });
}

}  // namespace

bool FipsProviderManager::init_fips_mode(bool enable) {
    bool expected = false;
    if (!s_fips_mode_initialized.compare_exchange_strong(expected, true)) {
        KeepTower::Log::warning("FIPS mode already initialized");
        return true;
    }

    KeepTower::Log::info("Initializing OpenSSL FIPS mode (enable={})", enable);

    bool available = false;
    bool enabled = false;
    const bool ok = init(enable, available, enabled);

    s_fips_mode_available.store(available);
    s_fips_mode_enabled.store(enabled);
    return ok;
}

bool FipsProviderManager::is_fips_available() {
    if (!s_fips_mode_initialized.load()) {
        KeepTower::Log::warning("FIPS mode not initialized - call init_fips_mode() first");
        return false;
    }
    return s_fips_mode_available.load();
}

bool FipsProviderManager::is_fips_enabled() {
    if (!s_fips_mode_initialized.load()) {
        KeepTower::Log::warning("FIPS mode not initialized - call init_fips_mode() first");
        return false;
    }
    return s_fips_mode_enabled.load();
}

bool FipsProviderManager::set_fips_mode(bool enable) {
    if (!s_fips_mode_initialized.load()) {
        KeepTower::Log::error("FIPS mode not initialized - call init_fips_mode() first");
        return false;
    }

    if (enable == s_fips_mode_enabled.load()) {
        KeepTower::Log::info("FIPS mode already in requested state ({})", enable);
        return true;
    }

    if (enable && !s_fips_mode_available.load()) {
        KeepTower::Log::error("Cannot enable FIPS mode - FIPS provider not available");
        return false;
    }

    if (!enable && !s_fips_mode_available.load()) {
        s_fips_mode_enabled.store(false);
        KeepTower::Log::info("FIPS provider not available; FIPS mode disabled");
        return true;
    }

    if (!set_fips_default_properties(enable)) {
        return false;
    }

    s_fips_mode_enabled.store(enable);
    KeepTower::Log::info("FIPS mode {} successfully", enable ? "enabled" : "disabled");
    return true;
}

bool FipsProviderManager::init(bool enable, bool& out_available, bool& out_enabled) {
    out_available = false;
    out_enabled = false;

    // Ensure we clean up provider/global allocations at process exit.
    register_openssl_cleanup_once();

    // Force OpenSSL to load configuration file (if OPENSSL_CONF is set).
    OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CONFIG, nullptr);

    // Check if FIPS provider is already available (e.g., loaded via OPENSSL_CONF).
    // Use OSSL_PROVIDER_try_load which respects configuration files.
    OSSL_PROVIDER* fips_provider = OSSL_PROVIDER_try_load(nullptr, "fips", 1);

    if (fips_provider == nullptr) {
        KeepTower::Log::warning("FIPS provider not available - using default provider");

        // Ensure default provider is available.
        OSSL_PROVIDER* default_provider = OSSL_PROVIDER_try_load(nullptr, "default", 1);
        if (default_provider == nullptr) {
            KeepTower::Log::error("Failed to load default OpenSSL provider");
            log_openssl_error("Failed to load default OpenSSL provider");
            return false;
        }

        g_default_provider = default_provider;
        return true;
    }

    g_fips_provider = fips_provider;

    out_available = true;
    KeepTower::Log::info("FIPS provider loaded successfully");

    if (enable) {
        if (!set_fips_default_properties(true)) {
            KeepTower::Log::error("Failed to enable FIPS mode");
            return false;
        }

        out_enabled = true;
        KeepTower::Log::info("FIPS mode enabled successfully");
        return true;
    }

    // Load default provider alongside FIPS for flexibility.
    OSSL_PROVIDER* default_provider = OSSL_PROVIDER_try_load(nullptr, "default", 1);
    if (default_provider == nullptr) {
        KeepTower::Log::warning("Failed to load default provider alongside FIPS");
    } else {
        g_default_provider = default_provider;
    }

    out_enabled = false;
    KeepTower::Log::info("FIPS mode available but not enabled");
    return true;
}

bool FipsProviderManager::set_fips_default_properties(bool enable) {
    if (EVP_default_properties_enable_fips(nullptr, enable ? 1 : 0) != 1) {
        KeepTower::Log::error("Failed to {} FIPS mode", enable ? "enable" : "disable");
        log_openssl_error("EVP_default_properties_enable_fips");
        return false;
    }

    return true;
}

}  // namespace KeepTower
