// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "YubiKeyManager.h"
#include "../utils/Log.h"
#include <ykpers-1/ykcore.h>
#include <ykpers-1/ykdef.h>
#include <ykpers-1/ykstatus.h>
#include <yubikey.h>
#include <openssl/crypto.h>  // For OPENSSL_cleanse
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

bool YubiKeyManager::initialize(bool enforce_fips) noexcept {
    if (m_initialized) {
        return true;
    }

    if (!yk_init()) {
        set_error("Failed to initialize YubiKey library");
        KeepTower::Log::error("YubiKey initialization failed");
        return false;
    }

    m_fips_mode = enforce_fips;
    m_initialized = true;

    if (m_fips_mode) {
        KeepTower::Log::info("YubiKey subsystem initialized in FIPS-140-3 mode (SHA-256+ required)");
        KeepTower::Log::warning("TODO: ykpers library does not support SHA-256. Migration to yubikey-manager or PCSC required.");
    } else {
        KeepTower::Log::info("YubiKey subsystem initialized (all algorithms)");
    }

    // NOTE: ykpers library only supports HMAC-SHA1 challenge-response
    // SHA-256 and other algorithms require newer yubikey-manager (ykman) or direct PCSC
    KeepTower::Log::warning("Current ykpers library only supports HMAC-SHA1. SHA-256+ requires different library.");

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

    // Detect YubiKey 5 FIPS Edition
    // YubiKey 5 FIPS has serial numbers in specific range and supports FIPS mode
    // FIPS capability: firmware 5.4.0+, special FIPS-validated hardware
    info.is_fips_capable = (info.version_major >= 5 && info.version_minor >= 4);

    // Detect if FIPS mode is enabled (YubiKey 5 FIPS devices)
    // In FIPS mode, only approved algorithms are allowed
    // Note: ykpers doesn't expose FIPS mode status directly
    // We infer from device type and configuration
    info.is_fips_mode = info.is_fips_capable && (info.version_build >= 3);

    // Determine supported algorithms based on firmware version
    // YubiKey 5 Series (firmware 5.0+): Hardware supports HMAC-SHA256
    // TODO: Software support pending - need to migrate from ykpers to yubikey-manager/PCSC
    info.supported_algorithms.clear();

    if (info.version_major >= 5) {
        // YubiKey 5 hardware supports SHA-256 (FIPS-approved)
        // Software implementation pending library migration
        info.supported_algorithms.push_back(YubiKeyAlgorithm::HMAC_SHA256);
        
        // Legacy SHA-1 support (not FIPS-approved, but works with ykpers)
        if (!m_fips_mode) {
            info.supported_algorithms.push_back(YubiKeyAlgorithm::HMAC_SHA1);
        }
    } else {
        // Older YubiKeys only support SHA-1
        info.supported_algorithms.push_back(YubiKeyAlgorithm::HMAC_SHA1);
    }

    // Check if slot 2 is configured
    // Note: Slot 2 configuration detection requires querying the key
    // For now, we assume it's available if the key is present
    info.slot2_configured = true;

    yk_close_key(yk);

    KeepTower::Log::info("Detected YubiKey: Serial {}, Version {}.{}.{}, FIPS: {}, Slot2: {}, Algorithms: {}",
                         info.serial_number, info.version_major, info.version_minor,
                         info.version_build,
                         info.is_fips_mode ? "YES" : (info.is_fips_capable ? "capable" : "no"),
                         info.slot2_configured ? "yes" : "no",
                         info.supported_algorithms.size());

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
    YubiKeyAlgorithm algorithm,
    bool require_touch,
    int timeout_ms
) noexcept {
    ChallengeResponse result{};
    result.algorithm = algorithm;

    if (!m_initialized) {
        result.error_message = "YubiKey subsystem not initialized";
        set_error(result.error_message);
        return result;
    }

    // FIPS mode enforcement
    if (m_fips_mode && !yubikey_algorithm_is_fips_approved(algorithm)) {
        result.error_message = std::format(
            "Algorithm {} is not FIPS-140-3 approved. Only SHA-256 and SHA3 variants allowed in FIPS mode.",
            yubikey_algorithm_name(algorithm)
        );
        set_error(result.error_message);
        KeepTower::Log::error("FIPS mode violation: Attempted to use {}", yubikey_algorithm_name(algorithm));
        return result;
    }

    // Get expected response size for this algorithm
    const size_t expected_response_size = yubikey_algorithm_response_size(algorithm);
    if (expected_response_size == 0) {
        result.error_message = std::format("Unknown algorithm: {}",
                                          static_cast<int>(algorithm));
        set_error(result.error_message);
        return result;
    }

    // Release and re-init to ensure clean state (workaround for ykpers state issues)
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

    // Verify device supports this algorithm
    const auto device_info = get_device_info();
    if (device_info && !device_info->supports_algorithm(algorithm)) {
        result.error_message = std::format(
            "YubiKey firmware {} does not support {}. Supported: SHA-256{}, SHA-1{}",
            device_info->version_string(),
            yubikey_algorithm_name(algorithm),
            device_info->supports_algorithm(YubiKeyAlgorithm::HMAC_SHA256) ? " ✓" : "",
            device_info->supports_algorithm(YubiKeyAlgorithm::HMAC_SHA1) ? " ✓" : ""
        );
        set_error(result.error_message);
        KeepTower::Log::error("Device does not support {}", yubikey_algorithm_name(algorithm));
        yk_close_key(yk);
        return result;
    }

    // Prepare challenge (pad or truncate to 64 bytes)
    std::array<unsigned char, YUBIKEY_CHALLENGE_SIZE> padded_challenge{};
    if (challenge.empty()) {
        result.error_message = "Challenge cannot be empty";
        set_error(result.error_message);
        KeepTower::Log::error("Empty challenge provided");
        yk_close_key(yk);
        return result;
    }
    const size_t copy_size = std::min(challenge.size(), YUBIKEY_CHALLENGE_SIZE);
    std::copy_n(challenge.begin(), copy_size, padded_challenge.begin());

    KeepTower::Log::info("Sending challenge to YubiKey slot 2 using {}...",
                         yubikey_algorithm_name(algorithm));
    (void)require_touch;  // Touch requirement is in slot configuration
    (void)timeout_ms;     // Timeout handled by ykpers

    // Prepare response buffer - must be 64 bytes per ykpers API requirements
    std::array<unsigned char, YUBIKEY_CHALLENGE_SIZE> response_buffer{};

    // Determine slot command based on algorithm
    int slot_command;
    switch (algorithm) {
        case YubiKeyAlgorithm::HMAC_SHA1:
            slot_command = SLOT_CHAL_HMAC2;  // 0x38 - Slot 2 HMAC-SHA1
            break;
        case YubiKeyAlgorithm::HMAC_SHA256:
            // TODO: ykpers library does NOT support SHA-256 challenge-response
            // Need to migrate to: yubikey-manager (ykman), libfido2, or direct PCSC
            result.error_message = "SHA-256 YubiKey support not yet implemented. "
                                  "ykpers library limitation - requires migration to yubikey-manager or PCSC. "
                                  "Temporary workaround: Use non-YubiKey vault or wait for library update.";
            set_error(result.error_message);
            KeepTower::Log::error("SHA-256 YubiKey not yet implemented - library migration required");
            yk_close_key(yk);
            return result;
        case YubiKeyAlgorithm::HMAC_SHA3_256:
        case YubiKeyAlgorithm::HMAC_SHA3_512:
        case YubiKeyAlgorithm::HMAC_SHA512:
            result.error_message = std::format("{} not yet supported by YubiKey firmware",
                                              yubikey_algorithm_name(algorithm));
            set_error(result.error_message);
            KeepTower::Log::error("Algorithm not yet supported: {}", yubikey_algorithm_name(algorithm));
            yk_close_key(yk);
            return result;
        default:
            result.error_message = "Unknown algorithm";
            set_error(result.error_message);
            yk_close_key(yk);
            return result;
    }

    // Perform challenge-response
    const int ret = yk_challenge_response(
        yk,
        slot_command,
        1,                        // may_block=1 (blocking)
        YUBIKEY_CHALLENGE_SIZE,   // 64 bytes
        padded_challenge.data(),
        YUBIKEY_CHALLENGE_SIZE,   // Response buffer must be 64 bytes
        response_buffer.data()
    );

    if (ret != 1) {
        result.error_message = std::format(
            "yk_challenge_response failed (returned {}). "
            "Verify slot 2 is configured for {} challenge-response.",
            ret, yubikey_algorithm_name(algorithm)
        );
        set_error(result.error_message);
        KeepTower::Log::error("yk_challenge_response failed: returned {}", ret);
        yk_close_key(yk);
        return result;
    }

    // Copy the actual response (size depends on algorithm)
    result.response_size = expected_response_size;
    std::copy_n(response_buffer.begin(), expected_response_size, result.response.begin());

    // Secure cleanup of sensitive buffers
    // FIPS-140-3 Section 7.9: Zeroization of SSPs (Security-Sensitive Parameters)
    // Use OPENSSL_cleanse to prevent compiler optimization from removing the cleanup
    OPENSSL_cleanse(padded_challenge.data(), padded_challenge.size());
    OPENSSL_cleanse(response_buffer.data(), response_buffer.size());

    KeepTower::Log::info("Challenge-response succeeded, got {}-byte {} response",
                         expected_response_size, yubikey_algorithm_name(algorithm));
    yk_close_key(yk);

    result.success = true;
    KeepTower::Log::info("Challenge-response completed successfully");

    return result;
}

bool YubiKeyManager::is_device_connected(std::string_view serial_number) const noexcept {
    const auto info = get_device_info();
    return info && info->serial_number == serial_number;
}
