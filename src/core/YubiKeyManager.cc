// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "YubiKeyManager.h"
#include "../utils/Log.h"
#include <ykpers-1/ykcore.h>
#include <ykpers-1/ykdef.h>
#include <ykpers-1/ykstatus.h>
#include <yubikey.h>
#include <cstring>
#include <algorithm>

/**
 * @brief Private implementation using PIMPL pattern
 *
 * Separates YubiKey library dependencies from public interface,
 * improving compile times and hiding implementation details.
 */
class YubiKeyManager::Impl {
public:
    YK_KEY* yk{nullptr};          ///< YubiKey device handle
    YK_STATUS* ykst{nullptr};     ///< YubiKey status structure

    Impl() noexcept {
        ykst = ykds_alloc();
    }

    ~Impl() noexcept {
        if (yk) {
            yk_close_key(yk);
            yk = nullptr;
        }
        if (ykst) {
            ykds_free(ykst);
            ykst = nullptr;
        }
    }

    // Non-copyable, non-movable
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;
};

YubiKeyManager::YubiKeyManager() noexcept
    : m_impl(std::make_unique<Impl>()) {
}

YubiKeyManager::~YubiKeyManager() noexcept = default;

bool YubiKeyManager::initialize() noexcept {
    if (m_initialized) {
        return true;
    }

    if (!yk_init()) {
        set_error("Failed to initialize YubiKey library");
        KeepTower::Log::error("YubiKey initialization failed");
        return false;
    }

    m_initialized = true;
    KeepTower::Log::info("YubiKey subsystem initialized");
    return true;
}

bool YubiKeyManager::is_yubikey_present() const noexcept {
    if (!m_initialized) {
        return false;
    }

    // Try to open first YubiKey
    YK_KEY* yk = yk_open_first_key();
    if (yk) {
        yk_close_key(yk);
        return true;
    }

    return false;
}

std::optional<YubiKeyManager::YubiKeyInfo> YubiKeyManager::get_device_info() const noexcept {
    if (!m_initialized) {
        set_error("YubiKey subsystem not initialized");
        return std::nullopt;
    }

    YK_KEY* yk = yk_open_first_key();
    if (!yk) {
        set_error("No YubiKey device found");
        return std::nullopt;
    }

    YubiKeyInfo info{};

    // Get status
    if (!yk_get_status(yk, m_impl->ykst)) {
        set_error("Failed to get YubiKey status");
        yk_close_key(yk);
        return std::nullopt;
    }

    // Get serial number
    unsigned int serial{0};
    if (yk_get_serial(yk, 0, 0, &serial)) {
        info.serial_number = std::to_string(serial);
    }

    // Get firmware version
    info.version_major = ykds_version_major(m_impl->ykst);
    info.version_minor = ykds_version_minor(m_impl->ykst);
    info.version_build = ykds_version_build(m_impl->ykst);

    // Check if slot 2 is configured
    // Note: Slot 2 configuration detection requires querying the key
    // For now, we assume it's available if the key is present
    info.slot2_configured = true;

    yk_close_key(yk);

    KeepTower::Log::info("Detected YubiKey: Serial {}, Version {}.{}.{}, Slot2 configured: {}",
                         info.serial_number, info.version_major, info.version_minor,
                         info.version_build, info.slot2_configured ? "yes" : "no");

    return info;
}

std::vector<YubiKeyManager::YubiKeyInfo> YubiKeyManager::enumerate_devices() const noexcept {
    std::vector<YubiKeyInfo> devices{};

    if (const auto info = get_device_info()) {
        devices.push_back(*info);
    }

    // Note: ykpers library primarily supports single device enumeration
    // For multiple devices, would need to use lower-level libusb calls

    return devices;
}

YubiKeyManager::ChallengeResponse YubiKeyManager::challenge_response(
    std::span<const unsigned char> challenge,
    bool require_touch,
    int timeout_ms
) noexcept {
    ChallengeResponse result{};

    if (!m_initialized) {
        result.error_message = "YubiKey subsystem not initialized";
        set_error(result.error_message);
        return result;
    }

    // Open YubiKey
    YK_KEY* yk = yk_open_first_key();
    if (!yk) {
        result.error_message = "No YubiKey device found";
        set_error(result.error_message);
        return result;
    }

    // Prepare challenge (pad or truncate to 64 bytes)
    std::array<unsigned char, CHALLENGE_SIZE> padded_challenge{};
    const size_t copy_size = std::min(challenge.size(), CHALLENGE_SIZE);
    std::copy_n(challenge.begin(), copy_size, padded_challenge.begin());

    // Note: timeout is handled internally by ykpers library
    // The timeout_ms parameter is kept for API consistency
    (void)timeout_ms;  // Suppress unused warning

    // Perform challenge-response on slot 2
    (void)require_touch;  // Suppress unused warning - always use touch mode

    if (!yk_challenge_response(yk, SLOT2, 1,  // Always require touch for security
                               CHALLENGE_SIZE, padded_challenge.data(),
                               RESPONSE_SIZE, result.response.data())) {
        result.error_message = "Challenge-response failed";
        set_error(result.error_message);
        KeepTower::Log::error("Challenge-response failed");
        yk_close_key(yk);
        return result;
    }

    yk_close_key(yk);

    result.success = true;
    KeepTower::Log::info("Challenge-response completed successfully");

    return result;
}

bool YubiKeyManager::is_device_connected(std::string_view serial_number) const noexcept {
    const auto info = get_device_info();
    return info && info->serial_number == serial_number;
}
