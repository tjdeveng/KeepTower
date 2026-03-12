// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

#include "config.h"

#include <chrono>
#include <cstddef>
#include <string>

#include "../../../utils/Log.h"

#ifdef HAVE_YUBIKEY_SUPPORT
#include <fido.h>
#endif

namespace KeepTower::YubiKeyInternal {

// NOTE: Header-only to avoid updating Meson source lists across many existing
// test binaries. Intended for internal use by YubiKeyManager only.

inline std::string& cached_yubikey_path() noexcept {
    static std::string cached;
    return cached;
}

inline std::chrono::steady_clock::time_point& cache_time() noexcept {
    static std::chrono::steady_clock::time_point time_point{};
    return time_point;
}

inline constexpr std::chrono::seconds cache_duration() noexcept {
    return std::chrono::seconds{5};
}

/**
 * @brief Find first YubiKey FIDO2 device (with caching)
 * @return Device path or empty string if not found
 */
[[nodiscard]] inline std::string find_yubikey_path() noexcept {
#ifndef HAVE_YUBIKEY_SUPPORT
    return {};
#else
    // Check cache first (reduces concurrent enumeration issues)
    const auto now = std::chrono::steady_clock::now();
    if (now - cache_time() < cache_duration() && !cached_yubikey_path().empty()) {
        return cached_yubikey_path();
    }

    fido_dev_info_t* devlist = fido_dev_info_new(16);
    if (!devlist) {
        KeepTower::Log::error("FIDO2: Failed to allocate device info list");
        cached_yubikey_path().clear();
        cache_time() = now;
        return {};
    }

    size_t ndevs = 0;
    if (fido_dev_info_manifest(devlist, 16, &ndevs) != FIDO_OK) {
        KeepTower::Log::warning("FIDO2: No FIDO2 devices found");
        fido_dev_info_free(&devlist, 16);
        cached_yubikey_path().clear();
        cache_time() = now;
        return {};
    }

    std::string result;
    for (size_t i = 0; i < ndevs; ++i) {
        const fido_dev_info_t* di = fido_dev_info_ptr(devlist, i);
        if (!di) {
            continue;
        }

        const char* path = fido_dev_info_path(di);
        const char* manufacturer = fido_dev_info_manufacturer_string(di);
        const char* product = fido_dev_info_product_string(di);

        // Look for Yubico devices
        bool is_yubikey = false;
        if (manufacturer && std::string(manufacturer).find("Yubico") != std::string::npos) {
            is_yubikey = true;
        }
        if (product && std::string(product).find("YubiKey") != std::string::npos) {
            is_yubikey = true;
        }

        if (is_yubikey && path) {
            result = path;
            KeepTower::Log::info(
                "FIDO2: Found YubiKey at {}: {} {}",
                path,
                manufacturer ? manufacturer : "Unknown",
                product ? product : "Unknown"
            );
            break;
        }
    }

    fido_dev_info_free(&devlist, 16);

    // Update cache
    cached_yubikey_path() = result;
    cache_time() = now;

    return result;
#endif
}

} // namespace KeepTower::YubiKeyInternal
