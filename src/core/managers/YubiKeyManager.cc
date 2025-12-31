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
    if (yk_get_serial(yk, 0, 0, &serial) && serial != 0) {
        info.serial_number = std::to_string(serial);
    } else {
        KeepTower::Log::warning("Failed to retrieve YubiKey serial number or serial is zero");
        set_error("Failed to retrieve device serial number");
        yk_close_key(yk);
        return std::nullopt;
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

    // Release and re-init to ensure clean state (workaround for ykpers state issues)
    // This addresses known ykpers library issues where repeated operations can fail
    // if the library state isn't reset. See: https://github.com/Yubico/yubikey-personalization/issues
    yk_release();
    if (!yk_init()) {
        result.error_message = "Failed to re-initialize YubiKey library";
        set_error(result.error_message);
        KeepTower::Log::error("YubiKey re-initialization failed");
        return result;
    }

    // Open YubiKey
    YK_KEY* yk = yk_open_first_key();
    if (!yk) {
        result.error_message = "No YubiKey device found";
        set_error(result.error_message);
        KeepTower::Log::error("No YubiKey device found");
        return result;
    }

    // Prepare challenge (pad or truncate to 64 bytes)
    std::array<unsigned char, CHALLENGE_SIZE> padded_challenge{};
    if (challenge.empty()) {
        result.error_message = "Challenge cannot be empty";
        set_error(result.error_message);
        KeepTower::Log::error("Empty challenge provided");
        yk_close_key(yk);
        return result;
    }
    const size_t copy_size = std::min(challenge.size(), CHALLENGE_SIZE);
    std::copy_n(challenge.begin(), copy_size, padded_challenge.begin());

    KeepTower::Log::info("Sending challenge to YubiKey slot 2...");
    (void)require_touch;  // Touch requirement is in slot configuration
    (void)timeout_ms;     // Timeout handled by ykpers

    // Prepare response buffer - must be 64 bytes per ykpers API requirements
    // Even though HMAC-SHA1 is only 20 bytes, the API requires 64-byte buffer
    std::array<unsigned char, CHALLENGE_SIZE> response_buffer{};

    // Use yk_challenge_response as ykchalresp does
    // Parameters: yk, slot_command, may_block, challenge_len, challenge, response_buf_len, response_buf
    const int ret = yk_challenge_response(
        yk,
        SLOT_CHAL_HMAC2,         // 0x38 - Slot 2 HMAC-SHA1
        1,                        // may_block=1 (blocking, as ykchalresp default)
        CHALLENGE_SIZE,           // 64 bytes
        padded_challenge.data(),
        CHALLENGE_SIZE,           // Response buffer must be 64 bytes!
        response_buffer.data()
    );

    if (ret != 1) {
        result.error_message = std::format(
            "yk_challenge_response failed (returned {}). "
            "Verify slot 2 is configured for HMAC-SHA1 challenge-response.",
            ret
        );
        set_error(result.error_message);
        KeepTower::Log::error("yk_challenge_response failed: returned {}", ret);
        yk_close_key(yk);
        return result;
    }

    // Copy the actual 20-byte HMAC-SHA1 response from the 64-byte buffer
    std::copy_n(response_buffer.begin(), RESPONSE_SIZE, result.response.begin());

    // Secure cleanup of sensitive buffers
    std::fill(padded_challenge.begin(), padded_challenge.end(), 0);
    std::fill(response_buffer.begin(), response_buffer.end(), 0);

    KeepTower::Log::info("Challenge-response succeeded, got {} byte HMAC-SHA1 response", RESPONSE_SIZE);
    yk_close_key(yk);

    result.success = true;
    KeepTower::Log::info("Challenge-response completed successfully");

    return result;
}

bool YubiKeyManager::is_device_connected(std::string_view serial_number) const noexcept {
    const auto info = get_device_info();
    return info && info->serial_number == serial_number;
}
