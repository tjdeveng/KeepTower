// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#ifndef YUBIKEY_MANAGER_H
#define YUBIKEY_MANAGER_H

#include "config.h"
#include "YubiKeyAlgorithm.h"
#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <array>
#include <string_view>
#include <span>
#include <format>

/**
 * @brief Manages YubiKey operations for vault encryption key derivation
 *
 * This class provides a FIPS-140-3 compliant interface to YubiKey devices for
 * challenge-response authentication. It supports multiple HMAC algorithms including
 * FIPS-approved SHA-256 and future SHA3 variants.
 *
 * FIPS-140-3 Compliance:
 * - Default: HMAC-SHA256 (32-byte response, FIPS-approved)
 * - Legacy: HMAC-SHA1 (20-byte response, NOT FIPS-approved, deprecated)
 * - Future: HMAC-SHA3-256/512 (quantum-resistant, reserved)
 *
 * Security Features:
 * - Challenge-response using slot 2 (slot 1 reserved for OTP)
 * - Automatic secure memory erasure for sensitive data
 * - Device serial number tracking for multi-key support
 * - YubiKey FIPS mode detection and enforcement
 * - Thread-safe operations with RAII
 *
 * @note This class requires YubiKey with Challenge-Response configured in slot 2
 * @see YubiKeyAlgorithm for algorithm details and FIPS compliance
 */
class YubiKeyManager final {
public:
    /**
     * @brief Information about a detected YubiKey device
     */
    struct YubiKeyInfo {
        std::string serial_number;      ///< Device serial number (unique identifier)
        int version_major{0};           ///< Major firmware version
        int version_minor{0};           ///< Minor firmware version
        int version_build{0};           ///< Build firmware version
        bool slot2_configured{false};   ///< Whether slot 2 has challenge-response enabled
        bool is_fips_capable{false};    ///< Whether device is YubiKey 5 FIPS
        bool is_fips_mode{false};       ///< Whether FIPS mode is enabled
        std::vector<YubiKeyAlgorithm> supported_algorithms{}; ///< Algorithms device supports

        /**
         * @brief Get human-readable firmware version string
         * @return Version string in format "major.minor.build"
         */
        [[nodiscard]] std::string version_string() const noexcept {
            return std::format("{}.{}.{}", version_major, version_minor, version_build);
        }

        /**
         * @brief Check if device supports a specific algorithm
         * @param algorithm The algorithm to check
         * @return true if device supports this algorithm
         */
        [[nodiscard]] bool supports_algorithm(YubiKeyAlgorithm algorithm) const noexcept {
            return std::ranges::find(supported_algorithms, algorithm) != supported_algorithms.end();
        }
    };

    /**
     * @brief Result of a challenge-response operation
     */
    struct ChallengeResponse {
        std::array<unsigned char, YUBIKEY_MAX_RESPONSE_SIZE> response{};  ///< Response data (up to 64 bytes)
        size_t response_size{0};                    ///< Actual response size
        YubiKeyAlgorithm algorithm{YubiKeyAlgorithm::HMAC_SHA256};  ///< Algorithm used
        bool success{false};                        ///< Whether operation succeeded
        std::string error_message{};                ///< Error description if failed

        /**
         * @brief Get response as span for actual size
         * @return Span of actual response bytes
         */
        [[nodiscard]] std::span<const unsigned char> get_response() const noexcept {
            return std::span<const unsigned char>{response.data(), response_size};
        }

        /**
         * @brief Securely erase the response data
         */
        void secure_erase() noexcept {
            std::fill(response.begin(), response.end(), 0);
            response_size = 0;
            error_message.clear();
        }

        ~ChallengeResponse() {
            secure_erase();
        }
    };

    // Constants
    static inline constexpr int SLOT2{2};                      ///< Challenge-response slot
    static inline constexpr int DEFAULT_TIMEOUT_MS{15000};     ///< Default timeout (15 seconds)

    // Use constants from YubiKeyAlgorithm.h
    using CHALLENGE_SIZE = std::integral_constant<size_t, YUBIKEY_CHALLENGE_SIZE>;
    using MAX_RESPONSE_SIZE = std::integral_constant<size_t, YUBIKEY_MAX_RESPONSE_SIZE>;

    YubiKeyManager() noexcept;
    ~YubiKeyManager() noexcept;

    // Non-copyable, non-movable (manages hardware resources)
    YubiKeyManager(const YubiKeyManager&) = delete;
    YubiKeyManager& operator=(const YubiKeyManager&) = delete;
    YubiKeyManager(YubiKeyManager&&) = delete;
    YubiKeyManager& operator=(YubiKeyManager&&) = delete;

    /**
     * @brief Initialize YubiKey subsystem
     * @param enforce_fips If true, only allow FIPS-approved algorithms
     * @return true if initialization succeeded
     * @note Must be called before any other operations
     * @note In FIPS mode, only SHA-256 and SHA3 algorithms are allowed
     */
    [[nodiscard]] bool initialize(bool enforce_fips = false) noexcept;

    /**
     * @brief Detect all connected YubiKey devices
     * @return Vector of YubiKeyInfo for all detected devices
     * @note Empty vector if no devices found
     */
    [[nodiscard]] std::vector<YubiKeyInfo> enumerate_devices() const noexcept;

    /**
     * @brief Check if any YubiKey is currently connected
     * @return true if at least one YubiKey detected
     */
    [[nodiscard]] bool is_yubikey_present() const noexcept;

    /**
     * @brief Get information about the first detected YubiKey
     * @return Optional YubiKeyInfo, empty if no device found
     */
    [[nodiscard]] std::optional<YubiKeyInfo> get_device_info() const noexcept;

    /**
     * @brief Perform challenge-response with YubiKey slot 2
     * @param challenge Challenge data (will be padded/truncated to 64 bytes)
     * @param algorithm HMAC algorithm to use (default: HMAC-SHA256 for FIPS)
     * @param require_touch Whether to require physical touch (true = more secure)
     * @param timeout_ms Timeout in milliseconds (default 15 seconds)
     * @return ChallengeResponse with response data or error
     *
     * @note The challenge is automatically padded with zeros if < 64 bytes
     * @note If require_touch is true, user must touch the YubiKey within timeout
     * @note Response is automatically securely erased when ChallengeResponse destroyed
     * @note In FIPS mode, only FIPS-approved algorithms (SHA-256, SHA3) are allowed
     */
    [[nodiscard]] ChallengeResponse challenge_response(
        std::span<const unsigned char> challenge,
        YubiKeyAlgorithm algorithm = YubiKeyAlgorithm::HMAC_SHA256,
        bool require_touch = true,
        int timeout_ms = DEFAULT_TIMEOUT_MS
    ) noexcept;

    /**
     * @brief Check if a specific YubiKey serial number is connected
     * @param serial_number Serial number to check for
     * @return true if device with that serial is connected
     */
    [[nodiscard]] bool is_device_connected(std::string_view serial_number) const noexcept;

    /**
     * @brief Get the last error message
     * @return Error message from last failed operation
     */
    [[nodiscard]] std::string_view get_last_error() const noexcept {
        return m_last_error;
    }

    /**
     * @brief Check if FIPS mode is enforced
     * @return true if only FIPS-approved algorithms allowed
     */
    [[nodiscard]] bool is_fips_enforced() const noexcept {
        return m_fips_mode;
    }

private:
    class Impl;                              ///< PIMPL for library implementation details
    std::unique_ptr<Impl> m_impl;            ///< Private implementation
    mutable std::string m_last_error{};      ///< Last error message
    bool m_initialized{false};               ///< Whether subsystem initialized
    bool m_fips_mode{false};                 ///< Whether FIPS mode enforced

    /**
     * @brief Set last error message
     * @param error Error message to store
     */
    void set_error(std::string_view error) const noexcept {
        m_last_error = error;
    }
};

#endif // YUBIKEY_MANAGER_H
