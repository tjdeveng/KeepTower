// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng
//
// FIDO2 Implementation: libfido2-based YubiKey Manager
// Uses FIDO2 hmac-secret extension for FIPS-140-3 compliant HMAC-SHA256

#include "YubiKeyManager.h"
#include "../../utils/Log.h"

#include "yubikey/Fido2Global.h"
#include "yubikey/Fido2Discovery.h"
#include "yubikey/Fido2Protocol.h"
#include "yubikey/AsyncRunner.h"

#ifdef HAVE_YUBIKEY_SUPPORT
#include <fido.h>
#endif

#include <cstring>
#include <algorithm>
#include <vector>
#include <format>
#include <cstdlib>
#include <mutex>
#include <chrono>

/**
 * @brief Private implementation using PIMPL pattern
 *
 * Delegates libfido2 protocol work to KeepTower::YubiKeyInternal::Fido2Protocol.
 */
class YubiKeyManager::Impl final {
public:
    KeepTower::YubiKeyInternal::Fido2Protocol protocol{};  ///< FIDO2 protocol helper (credential + hmac-secret operations)
    KeepTower::YubiKeyInternal::AsyncRunner async{};        ///< Background runner for blocking device operations

    Impl() noexcept = default;

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;
};

YubiKeyManager::YubiKeyManager() noexcept
    : m_impl(std::make_unique<Impl>()) {
}

YubiKeyManager::~YubiKeyManager() noexcept {
    // Ensure any background async work has finished before destruction
    if (m_impl) {
        m_impl->async.shutdown();
    }
}


bool YubiKeyManager::initialize(bool enforce_fips) noexcept {
#ifndef HAVE_YUBIKEY_SUPPORT
    set_error("YubiKey support not compiled in - install libfido2-devel and recompile");
    KeepTower::Log::warning("YubiKey support not available (recompile with libfido2)");
    return false;
#else
    if (m_initialized) {
        return true;
    }

    // Initialize libfido2 once globally (thread-safe)
    if (!KeepTower::YubiKeyInternal::fido2_initialized_flag().exchange(true)) {
        std::lock_guard<std::mutex> lock(KeepTower::YubiKeyInternal::fido2_mutex());
        fido_init(0);
        KeepTower::Log::info("libfido2 initialized globally");
    }

    m_fips_mode = enforce_fips;
    m_initialized = true;

    if (m_fips_mode) {
        KeepTower::Log::info("YubiKey subsystem initialized in FIPS-140-3 mode (HMAC-SHA256 via FIDO2)");
    } else {
        KeepTower::Log::info("YubiKey subsystem initialized (libfido2 FIDO2/WebAuthn)");
    }

    return true;
#endif
}

bool YubiKeyManager::is_yubikey_present() const noexcept {
#ifndef HAVE_YUBIKEY_SUPPORT
    return false;
#else
    if (!m_initialized) {
        return false;
    }

    // Allow disabling YubiKey detection via environment variable (useful for tests)
    if (std::getenv("DISABLE_YUBIKEY_DETECT")) {
        return false;
    }

    // Serialize all device enumeration globally due to libfido2 thread-safety issues
    std::lock_guard<std::mutex> lock(KeepTower::YubiKeyInternal::fido2_mutex());
    return !KeepTower::YubiKeyInternal::find_yubikey_path().empty();
#endif
}

std::optional<YubiKeyManager::YubiKeyInfo> YubiKeyManager::get_device_info() const noexcept {
#ifndef HAVE_YUBIKEY_SUPPORT
    set_error("YubiKey support not compiled in");
    return std::nullopt;
#else
    if (!m_initialized) {
        set_error("YubiKey subsystem not initialized");
        return std::nullopt;
    }

    // Serialize all device operations globally due to libfido2 thread-safety issues
    std::lock_guard<std::mutex> lock(KeepTower::YubiKeyInternal::fido2_mutex());

    std::string path = KeepTower::YubiKeyInternal::find_yubikey_path();
    if (path.empty()) {
        set_error("No YubiKey device found");
        return std::nullopt;
    }

    if (!m_impl->protocol.open_device(path)) {
        set_error("Failed to open YubiKey device");
        return std::nullopt;
    }

    YubiKeyInfo info{};
    if (!m_impl->protocol.get_device_info(info)) {
        set_error("Failed to retrieve device information");
        m_impl->protocol.cleanup();
        return std::nullopt;
    }

    // Note: is_fips_mode reflects device capability (FIDO2 hmac-secret = SHA-256 only = FIPS)
    // Software enforcement via m_fips_mode is separate and checked during operations

    KeepTower::Log::info("Detected YubiKey via FIDO2: Version {}.{}.{}, FIPS: {}, hmac-secret: {}",
                         info.version_major, info.version_minor, info.version_build,
                         info.is_fips_mode ? "YES" : "no",
                         info.slot2_configured ? "YES" : "NO");

    m_impl->protocol.cleanup();
    return info;
#endif
}
std::vector<YubiKeyManager::YubiKeyInfo> YubiKeyManager::enumerate_devices() const noexcept {
    std::vector<YubiKeyInfo> devices{};

    if (const auto info = get_device_info()) {
        devices.push_back(*info);
    }

    return devices;
}

YubiKeyManager::ChallengeResponse YubiKeyManager::challenge_response(
    std::span<const unsigned char> challenge,
    YubiKeyAlgorithm algorithm,
    bool require_touch,
    int timeout_ms,
    std::optional<std::string_view> pin
) noexcept {
    ChallengeResponse result{};
    result.algorithm = algorithm;

    // FIDO2 always requires touch (cannot be disabled)
    (void)require_touch;
    (void)timeout_ms;  // Timeout handled by libfido2 internally

#ifndef HAVE_YUBIKEY_SUPPORT
    result.error_message = "YubiKey support not compiled in";
    set_error(result.error_message);
    return result;
#else

    if (!m_initialized) {
        result.error_message = "YubiKey subsystem not initialized";
        set_error(result.error_message);
        return result;
    }

    // FIDO2 hmac-secret only supports SHA-256
    if (algorithm != YubiKeyAlgorithm::HMAC_SHA256) {
        result.error_message = std::format(
            "Algorithm {} not supported. FIDO2 hmac-secret only supports HMAC-SHA256.",
            yubikey_algorithm_name(algorithm)
        );
        set_error(result.error_message);
        return result;
    }

    // FIPS mode enforcement
    if (m_fips_mode && !yubikey_algorithm_is_fips_approved(algorithm)) {
        result.error_message = std::format(
            "Algorithm {} is not FIPS-140-3 approved.",
            yubikey_algorithm_name(algorithm)
        );
        set_error(result.error_message);
        return result;
    }

    // Validate challenge size (FIDO2 hmac-secret salt is 32 bytes; callers may provide up to 64 bytes)
    // For challenges > 32 bytes we deterministically hash down to a 32-byte salt in the protocol helper.
    if (challenge.empty() || challenge.size() > YUBIKEY_CHALLENGE_SIZE) {
        result.error_message = std::format(
            "Invalid challenge size: {} (must be 1-{} bytes)",
            challenge.size(),
            YUBIKEY_CHALLENGE_SIZE
        );
        set_error(result.error_message);
        return result;
    }

    // Serialize all libfido2 operations globally due to libfido2 thread-safety issues
    std::lock_guard<std::mutex> lock(KeepTower::YubiKeyInternal::fido2_mutex());

    // Find and open YubiKey
    std::string path = KeepTower::YubiKeyInternal::find_yubikey_path();
    if (path.empty()) {
        result.error_message = "No YubiKey FIDO2 device found";
        set_error(result.error_message);
        return result;
    }

    if (!m_impl->protocol.open_device(path)) {
        result.error_message = std::format("Failed to open YubiKey at {}", path);
        set_error(result.error_message);
        return result;
    }

    // Check if we have a credential, if not create one
    if (!m_impl->protocol.has_credential) {
        KeepTower::Log::error("FIDO2: No credential enrolled. Use create_credential() first.");
        result.error_message = "No FIDO2 credential enrolled. Please create a new vault with YubiKey.";
        set_error(result.error_message);
        m_impl->protocol.cleanup();
        return result;
    }

    // Get PIN (from parameter or environment variable)
    std::string pin_str;
    if (pin.has_value()) {
        pin_str = std::string(pin.value());
    } else {
        const char* env_pin = std::getenv("YUBIKEY_PIN");
        if (env_pin) {
            pin_str = env_pin;
        }
    }

    if (pin_str.empty()) {
        result.error_message = "YubiKey PIN required";
        set_error(result.error_message);
        KeepTower::Log::error("FIDO2: PIN required but not provided");
        m_impl->protocol.cleanup();
        return result;
    }

    auto proto_result = m_impl->protocol.perform_hmac_secret(challenge, pin_str);
    if (!proto_result.success) {
        result.error_message = std::move(proto_result.error);
        set_error(result.error_message);
        return result;
    }

    result.response_size = std::min(proto_result.response_size, result.response.size());
    std::copy_n(proto_result.response.begin(), result.response_size, result.response.begin());
    result.success = true;
    return result;
#endif
}
bool YubiKeyManager::is_device_connected(std::string_view serial_number) const noexcept {
#ifndef HAVE_YUBIKEY_SUPPORT
    (void)serial_number;
    return false;
#else
    if (!m_initialized) {
        return false;
    }

    // Serialize enumeration globally due to libfido2 thread-safety issues
    std::lock_guard<std::mutex> lock(KeepTower::YubiKeyInternal::fido2_mutex());

    // For FIDO2, we use device path as identifier since serial not readily available
    std::string path = KeepTower::YubiKeyInternal::find_yubikey_path();
    if (path.empty()) {
        return false;
    }

    // Check if the path matches the serial_number (device path)
    return path == serial_number || path.find(std::string(serial_number)) != std::string::npos;
#endif
}

std::optional<std::vector<unsigned char>> YubiKeyManager::create_credential(
    std::string_view user_id,
    std::string_view pin
) noexcept {
#ifndef HAVE_YUBIKEY_SUPPORT
    (void)user_id;
    (void)pin;
    set_error("YubiKey support not compiled in");
    return std::nullopt;
#else
    if (!m_initialized) {
        set_error("YubiKey subsystem not initialized");
        return std::nullopt;
    }

    if (pin.empty()) {
        set_error("PIN required for credential creation");
        return std::nullopt;
    }

    // Serialize all libfido2 operations globally due to libfido2 thread-safety issues
    std::lock_guard<std::mutex> lock(KeepTower::YubiKeyInternal::fido2_mutex());

    // Find and open YubiKey
    std::string path = KeepTower::YubiKeyInternal::find_yubikey_path();
    if (path.empty()) {
        set_error("No YubiKey FIDO2 device found");
        return std::nullopt;
    }

    if (!m_impl->protocol.open_device(path)) {
        set_error(std::format("Failed to open YubiKey at {}", path));
        return std::nullopt;
    }

    auto proto_result = m_impl->protocol.create_credential(user_id, pin);
    if (!proto_result.credential_id.has_value()) {
        set_error(proto_result.error);
        return std::nullopt;
    }

    return std::move(proto_result.credential_id);
#endif
}

bool YubiKeyManager::set_credential(std::span<const unsigned char> credential_id) noexcept {
#ifndef HAVE_YUBIKEY_SUPPORT
    (void)credential_id;
    return false;
#else
    if (credential_id.empty()) {
        set_error("Empty credential ID");
        return false;
    }

    m_impl->protocol.cred_id.assign(credential_id.begin(), credential_id.end());
    m_impl->protocol.has_credential = true;

    KeepTower::Log::info("FIDO2: Credential ID set ({} bytes)", credential_id.size());
    return true;
#endif
}

// ============================================================================
// Async Operations (Thread-Safe)
// ============================================================================

void YubiKeyManager::create_credential_async(
    std::string_view rp_id,
    std::string_view user_name,
    std::span<const unsigned char> user_id,
    std::optional<std::string_view> pin,
    bool require_touch,
    CreateCredentialCallback callback
) noexcept {
    (void)rp_id;
    (void)user_id;

    if (is_busy()) {
        KeepTower::Log::warning("YubiKeyManager: Async operation already in progress");
        callback(std::nullopt, "Operation already in progress");
        return;
    }

    KeepTower::Log::info("YubiKeyManager: Starting async credential creation for user '{}'", user_name);

    // Copy parameters for thread (span/string_view not safe across threads)
    std::string user_name_copy{user_name};
    std::optional<std::string> pin_copy = pin ? std::optional<std::string>(*pin) : std::nullopt;

    struct Result {
        std::optional<std::vector<unsigned char>> credential_id;
        std::string error_msg;
    };

    const bool started = m_impl->async.start(
        [this,
         user_name_copy = std::move(user_name_copy),
         pin_copy = std::move(pin_copy),
         require_touch]() mutable -> Result {
            (void)require_touch;
            KeepTower::Log::info("YubiKeyManager: Worker thread started for credential creation");

            std::string_view pin_view = pin_copy ? std::string_view(*pin_copy) : std::string_view("");
            auto credential_id = create_credential(user_name_copy, pin_view);

            KeepTower::Log::info("YubiKeyManager: Credential creation completed with {}",
                                 credential_id ? "success" : "error");

            Result result;
            result.credential_id = credential_id;
            result.error_msg = std::string(m_last_error);
            return result;
        },
        [callback = std::move(callback)](Result result) mutable {
            callback(result.credential_id, result.error_msg);
        });

    if (!started) {
        KeepTower::Log::warning("YubiKeyManager: Async operation already in progress");
        callback(std::nullopt, "Operation already in progress");
        return;
    }
}

void YubiKeyManager::challenge_response_async(
    std::span<const unsigned char> challenge,
    YubiKeyAlgorithm algorithm,
    bool require_touch,
    int timeout_ms,
    std::optional<std::string_view> pin,
    ChallengeResponseCallback callback
) noexcept {
    if (is_busy()) {
        KeepTower::Log::warning("YubiKeyManager: Async operation already in progress");
        ChallengeResponse error_response;
        error_response.success = false;
        error_response.error_message = "Operation already in progress";
        callback(error_response);
        return;
    }

    KeepTower::Log::info("YubiKeyManager: Starting async challenge-response");

    // Copy parameters for thread
    std::vector<unsigned char> challenge_copy(challenge.begin(), challenge.end());
    std::optional<std::string> pin_copy = pin ? std::optional<std::string>(*pin) : std::nullopt;

    const bool started = m_impl->async.start(
        [this,
         challenge_copy = std::move(challenge_copy),
         algorithm,
         require_touch,
         timeout_ms,
         pin_copy = std::move(pin_copy)]() mutable -> ChallengeResponse {
            KeepTower::Log::info("YubiKeyManager: Worker thread started for challenge-response");

            auto response = challenge_response(
                challenge_copy,
                algorithm,
                require_touch,
                timeout_ms,
                pin_copy);

            KeepTower::Log::info("YubiKeyManager: Challenge-response completed with {}",
                                 response.success ? "success" : "error");
            return response;
        },
        [callback = std::move(callback)](ChallengeResponse response) mutable {
            callback(response);
        });

    if (!started) {
        KeepTower::Log::warning("YubiKeyManager: Async operation already in progress");
        ChallengeResponse error_response;
        error_response.success = false;
        error_response.error_message = "Operation already in progress";
        callback(error_response);
        return;
    }
}

bool YubiKeyManager::is_busy() const noexcept {
    return m_impl && m_impl->async.is_busy();
}

void YubiKeyManager::cancel_async() noexcept {
    if (m_impl) {
        m_impl->async.cancel();
    }
}
