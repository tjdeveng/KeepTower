// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng
//
// FIDO2 Implementation: libfido2-based YubiKey Manager
// Uses FIDO2 hmac-secret extension for FIPS-140-3 compliant HMAC-SHA256

#include "YubiKeyManager.h"
#include "../../utils/Log.h"

#ifdef HAVE_YUBIKEY_SUPPORT
#include <fido.h>
#include <fido/credman.h>
#endif

#include <openssl/crypto.h>  // For OPENSSL_cleanse
#include <openssl/evp.h>     // For SHA-256
#include <openssl/rand.h>    // For CSPRNG
#include <cstring>
#include <algorithm>
#include <vector>
#include <format>
#include <cstdlib>
#include <mutex>
#include <atomic>
#include <chrono>

// Global mutex for libfido2 initialization and device enumeration
static std::mutex g_fido2_mutex;
static std::atomic<bool> g_fido2_initialized{false};

// Device cache to avoid repeated enumeration (thread-safety issue in libfido2)
static std::string g_cached_yubikey_path;
static std::chrono::steady_clock::time_point g_cache_time;
static constexpr std::chrono::seconds CACHE_DURATION{5};  // Cache for 5 seconds

/**
 * @brief FIDO2 hmac-secret constants and helpers
 *
 * FIDO2 hmac-secret extension provides HMAC-SHA256 challenge-response
 * Reference: https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#sctn-hmac-secret-extension
 */
namespace FIDO2 {
    // FIDO2 constants
    constexpr size_t SALT_SIZE = 32;        // Salt for hmac-secret (SHA-256 input)
    constexpr size_t SECRET_SIZE = 32;      // Output secret size (SHA-256 output)
    constexpr size_t CRED_ID_MAX = 1024;    // Maximum credential ID size
    constexpr int DEFAULT_TIMEOUT_MS = 30000; // 30 seconds for user interaction

    // Relying Party (RP) information for KeepTower
    constexpr const char* RP_ID = "keeptower.local";
    constexpr const char* RP_NAME = "KeepTower Password Manager";

    /**
     * @brief Generate cryptographically secure salt for hmac-secret
     * @param salt Output buffer (32 bytes)
     * @return true if generation succeeded
     */
    inline bool generate_salt(unsigned char* salt) noexcept {
        return RAND_bytes(salt, SALT_SIZE) == 1;
    }

    /**
     * @brief Derive challenge from user data using SHA-256
     * @param user_data Input data (e.g., vault path, user ID)
     * @param user_data_len Length of input data
     * @param salt Output salt (32 bytes)
     * @return true if derivation succeeded
     */
    inline bool derive_salt_from_data(
        const unsigned char* user_data,
        size_t user_data_len,
        unsigned char* salt
    ) noexcept {
        unsigned int hash_len = 0;
        return EVP_Digest(user_data, user_data_len, salt, &hash_len,
                         EVP_sha256(), nullptr) == 1 && hash_len == SALT_SIZE;
    }
}

/**
 * @brief Private implementation using PIMPL pattern with libfido2
 *
 * Manages FIDO2 device handles and credentials for YubiKey communication
 * Thread-safe: Each instance has its own mutex protecting its state
 */
class YubiKeyManager::Impl {
public:
#ifdef HAVE_YUBIKEY_SUPPORT
    fido_dev_t* dev{nullptr};               ///< FIDO2 device handle
    fido_cred_t* cred{nullptr};             ///< Credential handle (for creation)
    fido_assert_t* assert{nullptr};         ///< Assertion handle (for hmac-secret)
    std::vector<uint8_t> cred_id{};         ///< Current credential ID
    std::string device_path{};              ///< Device path (e.g., /dev/hidraw0)
    bool has_credential{false};             ///< Whether credential is enrolled

    Impl() noexcept = default;

    ~Impl() noexcept {
        cleanup();
    }

    /**
     * @brief Clean up all FIDO2 resources
     * @note Caller must hold g_fido2_mutex
     * @note Preserves credential ID for reuse
     */
    void cleanup() noexcept {
        if (assert) {
            fido_assert_free(&assert);
            assert = nullptr;
        }
        if (cred) {
            fido_cred_free(&cred);
            cred = nullptr;
        }
        if (dev) {
            fido_dev_close(dev);
            fido_dev_free(&dev);
            dev = nullptr;
        }
        // Note: Don't clear cred_id or has_credential - they persist across operations
        device_path.clear();
    }

    /**
     * @brief Find first YubiKey FIDO2 device (with caching)
     * @return Device path or empty string if not found
     * @note Caller must hold g_fido2_mutex
     */
    std::string find_yubikey() noexcept {
        // Check cache first (reduces concurrent enumeration issues)
        auto now = std::chrono::steady_clock::now();
        if (now - g_cache_time < CACHE_DURATION && !g_cached_yubikey_path.empty()) {
            return g_cached_yubikey_path;
        }

        fido_dev_info_t* devlist = fido_dev_info_new(16);
        if (!devlist) {
            KeepTower::Log::error("FIDO2: Failed to allocate device info list");
            g_cached_yubikey_path.clear();
            g_cache_time = now;
            return {};
        }

        size_t ndevs = 0;
        if (fido_dev_info_manifest(devlist, 16, &ndevs) != FIDO_OK) {
            KeepTower::Log::warning("FIDO2: No FIDO2 devices found");
            fido_dev_info_free(&devlist, 16);
            g_cached_yubikey_path.clear();
            g_cache_time = now;
            return {};
        }

        std::string result;
        for (size_t i = 0; i < ndevs; ++i) {
            const fido_dev_info_t* di = fido_dev_info_ptr(devlist, i);
            if (!di) continue;

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
                KeepTower::Log::info("FIDO2: Found YubiKey at {}: {} {}",
                                   path,
                                   manufacturer ? manufacturer : "Unknown",
                                   product ? product : "Unknown");
                break;
            }
        }

        fido_dev_info_free(&devlist, 16);

        // Update cache
        g_cached_yubikey_path = result;
        g_cache_time = now;

        return result;
    }

    /**
     * @brief Open FIDO2 device
     * @param path Device path
     * @return true if opened successfully
     * @note Caller must hold g_fido2_mutex
     */
    bool open_device(const std::string& path) noexcept {
        if (dev) {
            return true;  // Already open
        }

        dev = fido_dev_new();
        if (!dev) {
            KeepTower::Log::error("FIDO2: Failed to allocate device");
            return false;
        }

        int r = fido_dev_open(dev, path.c_str());
        if (r != FIDO_OK) {
            KeepTower::Log::error("FIDO2: Failed to open {}: {} ({})",
                                path, fido_strerr(r), r);
            fido_dev_free(&dev);
            dev = nullptr;
            return false;
        }

        device_path = path;
        KeepTower::Log::info("FIDO2: Opened device {}", path);
        return true;
    }

    /**
     * @brief Get device information
     * @param info Output YubiKeyInfo structure
     * @return true if information retrieved successfully
     * @note Caller must hold g_fido2_mutex
     */
    bool get_device_info(YubiKeyManager::YubiKeyInfo& info) noexcept {
        if (!dev) {
            return false;
        }

        // Get FIDO2 information
        fido_cbor_info_t* cbor_info = fido_cbor_info_new();
        if (!cbor_info) {
            return false;
        }

        int r = fido_dev_get_cbor_info(dev, cbor_info);
        if (r != FIDO_OK) {
            KeepTower::Log::warning("FIDO2: Failed to get CBOR info: {}", fido_strerr(r));
            fido_cbor_info_free(&cbor_info);
            return false;
        }

        // Check for hmac-secret extension support
        const char* const* extensions = fido_cbor_info_extensions_ptr(cbor_info);
        size_t n_extensions = fido_cbor_info_extensions_len(cbor_info);
        bool has_hmac_secret = false;

        for (size_t i = 0; i < n_extensions; ++i) {
            if (extensions[i] && std::string(extensions[i]) == "hmac-secret") {
                has_hmac_secret = true;
                break;
            }
        }

        info.slot2_configured = has_hmac_secret;
        // Only FIPS-capable/compliant if device supports hmac-secret extension
        // FIDO2 hmac-secret uses SHA-256 only (FIPS-140-3 approved)
        info.is_fips_capable = has_hmac_secret;
        info.is_fips_mode = has_hmac_secret;

        // Get firmware version (best effort)
        uint64_t version = fido_cbor_info_fwversion(cbor_info);
        info.version_major = (version >> 32) & 0xFFFF;
        info.version_minor = (version >> 16) & 0xFFFF;
        info.version_build = version & 0xFFFF;

        // YubiKey serial (not available via FIDO2, use device path as identifier)
        info.serial_number = device_path;

        // FIDO2 hmac-secret only supports SHA-256 (FIPS-approved)
        if (has_hmac_secret) {
            info.supported_algorithms = {YubiKeyAlgorithm::HMAC_SHA256};
        } else {
            info.supported_algorithms = {};  // No supported algorithms
        }

        // Check if PIN is set
        if (fido_dev_has_pin(dev)) {
            KeepTower::Log::info("FIDO2: Device has PIN set");
        } else {
            KeepTower::Log::warning("FIDO2: Device does not have PIN set - hmac-secret requires PIN!");
        }

        fido_cbor_info_free(&cbor_info);
        return true;
    }

#else
    // No YubiKey support compiled in
    Impl() noexcept = default;
    ~Impl() noexcept = default;
    void cleanup() noexcept {}
#endif

    // Non-copyable, non-movable
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;
};

YubiKeyManager::YubiKeyManager() noexcept
    : m_impl(std::make_unique<Impl>()) {
    // Connect dispatcher to callback executor
    m_dispatcher.connect([this]() {
        std::function<void()> callback;
        {
            std::lock_guard<std::mutex> lock(m_callback_mutex);
            callback = std::move(m_pending_callback);
        }
        if (callback) {
            callback();
        }
    });
}

YubiKeyManager::~YubiKeyManager() noexcept {
    // Cancel any pending operations
    cancel_async();

    // Wait for worker thread to finish
    if (m_worker_thread && m_worker_thread->joinable()) {
        m_worker_thread->join();
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
    if (!g_fido2_initialized.exchange(true)) {
        std::lock_guard<std::mutex> lock(g_fido2_mutex);
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
    std::lock_guard<std::mutex> lock(g_fido2_mutex);
    return !m_impl->find_yubikey().empty();
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
    std::lock_guard<std::mutex> lock(g_fido2_mutex);

    std::string path = m_impl->find_yubikey();
    if (path.empty()) {
        set_error("No YubiKey device found");
        return std::nullopt;
    }

    if (!m_impl->open_device(path)) {
        set_error("Failed to open YubiKey device");
        return std::nullopt;
    }

    YubiKeyInfo info{};
    if (!m_impl->get_device_info(info)) {
        set_error("Failed to retrieve device information");
        m_impl->cleanup();
        return std::nullopt;
    }

    // Note: is_fips_mode reflects device capability (FIDO2 hmac-secret = SHA-256 only = FIPS)
    // Software enforcement via m_fips_mode is separate and checked during operations

    KeepTower::Log::info("Detected YubiKey via FIDO2: Version {}.{}.{}, FIPS: {}, hmac-secret: {}",
                         info.version_major, info.version_minor, info.version_build,
                         info.is_fips_mode ? "YES" : "no",
                         info.slot2_configured ? "YES" : "NO");

    m_impl->cleanup();
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

    // Validate challenge size (use as salt for hmac-secret)
    if (challenge.empty() || challenge.size() > FIDO2::SALT_SIZE) {
        result.error_message = std::format(
            "Invalid challenge size: {} (must be 1-32 bytes for hmac-secret)",
            challenge.size()
        );
        set_error(result.error_message);
        return result;
    }

    // Find and open YubiKey
    std::string path = m_impl->find_yubikey();
    if (path.empty()) {
        result.error_message = "No YubiKey FIDO2 device found";
        set_error(result.error_message);
        return result;
    }

    if (!m_impl->open_device(path)) {
        result.error_message = std::format("Failed to open YubiKey at {}", path);
        set_error(result.error_message);
        return result;
    }

    // Check if we have a credential, if not create one
    if (!m_impl->has_credential) {
        KeepTower::Log::error("FIDO2: No credential enrolled. Use create_credential() first.");
        result.error_message = "No FIDO2 credential enrolled. Please create a new vault with YubiKey.";
        set_error(result.error_message);
        m_impl->cleanup();
        return result;
    }

    // Prepare salt (challenge padded to 32 bytes)
    std::array<unsigned char, FIDO2::SALT_SIZE> salt{};
    std::copy_n(challenge.begin(), challenge.size(), salt.begin());

    // Create assertion for hmac-secret
    m_impl->assert = fido_assert_new();
    if (!m_impl->assert) {
        result.error_message = "Failed to allocate FIDO2 assertion";
        set_error(result.error_message);
        m_impl->cleanup();
        return result;
    }

    // Set RP ID
    int r = fido_assert_set_rp(m_impl->assert, FIDO2::RP_ID);
    if (r != FIDO_OK) {
        result.error_message = std::format("Failed to set RP ID: {}", fido_strerr(r));
        set_error(result.error_message);
        m_impl->cleanup();
        return result;
    }

    // Set credential ID to authenticate with
    r = fido_assert_allow_cred(m_impl->assert, m_impl->cred_id.data(), m_impl->cred_id.size());
    if (r != FIDO_OK) {
        result.error_message = std::format("Failed to set credential ID: {}", fido_strerr(r));
        set_error(result.error_message);
        m_impl->cleanup();
        return result;
    }

    // Set client data hash (SHA-256 of challenge)
    std::array<unsigned char, 32> cdh{};
    FIDO2::derive_salt_from_data(challenge.data(), challenge.size(), cdh.data());

    r = fido_assert_set_clientdata_hash(m_impl->assert, cdh.data(), cdh.size());
    if (r != FIDO_OK) {
        result.error_message = std::format("Failed to set client data: {}", fido_strerr(r));
        set_error(result.error_message);
        m_impl->cleanup();
        return result;
    }

    // Set hmac-secret extension with salt
    r = fido_assert_set_hmac_salt(m_impl->assert, salt.data(), salt.size());
    if (r != FIDO_OK) {
        result.error_message = std::format("Failed to set hmac-secret salt: {}", fido_strerr(r));
        set_error(result.error_message);
        m_impl->cleanup();
        return result;
    }

    // Enable hmac-secret extension for assertion
    r = fido_assert_set_extensions(m_impl->assert, FIDO_EXT_HMAC_SECRET);
    if (r != FIDO_OK) {
        result.error_message = std::format("Failed to enable hmac-secret extension: {}", fido_strerr(r));
        set_error(result.error_message);
        m_impl->cleanup();
        return result;
    }

    // Allow resident key discovery
    r = fido_assert_set_up(m_impl->assert, FIDO_OPT_TRUE);
    if (r != FIDO_OK) {
        result.error_message = std::format("Failed to set user presence: {}", fido_strerr(r));
        set_error(result.error_message);
        m_impl->cleanup();
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
        m_impl->cleanup();
        return result;
    }

    // Perform assertion (will prompt for touch)
    KeepTower::Log::info("FIDO2: Performing assertion - please touch your YubiKey");

    r = fido_dev_get_assert(m_impl->dev, m_impl->assert, pin_str.c_str());
    if (r != FIDO_OK) {
        result.error_message = std::format(
            "YubiKey assertion failed: {} ({}). Please touch your YubiKey and ensure PIN is correct.",
            fido_strerr(r), r
        );
        set_error(result.error_message);
        KeepTower::Log::error("FIDO2: Assertion failed: {}", fido_strerr(r));
        m_impl->cleanup();
        return result;
    }

    // Get hmac-secret output
    const unsigned char* hmac_secret = fido_assert_hmac_secret_ptr(m_impl->assert, 0);
    size_t hmac_secret_len = fido_assert_hmac_secret_len(m_impl->assert, 0);

    if (!hmac_secret || hmac_secret_len == 0) {
        result.error_message = "YubiKey did not return hmac-secret";
        set_error(result.error_message);
        m_impl->cleanup();
        return result;
    }

    // Copy response
    result.response_size = std::min(hmac_secret_len, result.response.size());
    std::copy_n(hmac_secret, result.response_size, result.response.begin());
    result.success = true;

    KeepTower::Log::info("FIDO2: Challenge-response successful ({} bytes)", result.response_size);

    m_impl->cleanup();
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

    // For FIDO2, we use device path as identifier since serial not readily available
    std::string path = m_impl->find_yubikey();
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

    // Find and open YubiKey
    std::string path = m_impl->find_yubikey();
    if (path.empty()) {
        set_error("No YubiKey FIDO2 device found");
        return std::nullopt;
    }

    if (!m_impl->open_device(path)) {
        set_error(std::format("Failed to open YubiKey at {}", path));
        return std::nullopt;
    }

    // Create credential
    m_impl->cred = fido_cred_new();
    if (!m_impl->cred) {
        set_error("Failed to allocate FIDO2 credential");
        m_impl->cleanup();
        return std::nullopt;
    }

    // Set credential type to ES256 (required for hmac-secret)
    int r = fido_cred_set_type(m_impl->cred, COSE_ES256);
    if (r != FIDO_OK) {
        set_error(std::format("Failed to set credential type: {}", fido_strerr(r)));
        m_impl->cleanup();
        return std::nullopt;
    }

    // Set RP (Relying Party)
    r = fido_cred_set_rp(m_impl->cred, FIDO2::RP_ID, "KeepTower Vault");
    if (r != FIDO_OK) {
        set_error(std::format("Failed to set RP: {}", fido_strerr(r)));
        m_impl->cleanup();
        return std::nullopt;
    }

    // Set user information
    std::array<unsigned char, 32> user_id_hash{};
    FIDO2::derive_salt_from_data(
        reinterpret_cast<const unsigned char*>(user_id.data()),
        user_id.size(),
        user_id_hash.data()
    );

    r = fido_cred_set_user(
        m_impl->cred,
        user_id_hash.data(), user_id_hash.size(),
        user_id.data(),
        user_id.data(),
        nullptr  // No icon
    );
    if (r != FIDO_OK) {
        set_error(std::format("Failed to set user: {}", fido_strerr(r)));
        m_impl->cleanup();
        return std::nullopt;
    }

    // Generate client data hash
    std::array<unsigned char, 32> cdh{};
    FIDO2::generate_salt(cdh.data());

    r = fido_cred_set_clientdata_hash(m_impl->cred, cdh.data(), cdh.size());
    if (r != FIDO_OK) {
        set_error(std::format("Failed to set client data hash: {}", fido_strerr(r)));
        m_impl->cleanup();
        return std::nullopt;
    }

    // Enable hmac-secret extension
    r = fido_cred_set_extensions(m_impl->cred, FIDO_EXT_HMAC_SECRET);
    if (r != FIDO_OK) {
        set_error(std::format("Failed to enable hmac-secret extension: {}", fido_strerr(r)));
        m_impl->cleanup();
        return std::nullopt;
    }

    // Set resident key (required for discoverable credentials)
    r = fido_cred_set_rk(m_impl->cred, FIDO_OPT_TRUE);
    if (r != FIDO_OK) {
        set_error(std::format("Failed to set resident key: {}", fido_strerr(r)));
        m_impl->cleanup();
        return std::nullopt;
    }

    // Set user verification (PIN required)
    r = fido_cred_set_uv(m_impl->cred, FIDO_OPT_TRUE);
    if (r != FIDO_OK) {
        set_error(std::format("Failed to set user verification: {}", fido_strerr(r)));
        m_impl->cleanup();
        return std::nullopt;
    }

    // Make credential (requires touch + PIN)
    KeepTower::Log::info("FIDO2: Creating credential - please touch your YubiKey");

    // Convert string_view to null-terminated string for FIDO2 API
    std::string pin_str(pin);

    // Validate PIN length (FIDO2 spec: 4-63 characters)
    if (pin_str.length() < 4 || pin_str.length() > 63) {
        set_error(std::format("Invalid PIN length: {} (must be 4-63 characters)", pin_str.length()));
        KeepTower::Log::error("FIDO2: Invalid PIN length: {}", pin_str.length());
        m_impl->cleanup();
        return std::nullopt;
    }

    KeepTower::Log::debug("FIDO2: PIN length: {} characters", pin_str.length());
    KeepTower::Log::debug("FIDO2: User ID: {}", std::string(user_id));

    r = fido_dev_make_cred(m_impl->dev, m_impl->cred, pin_str.c_str());
    if (r != FIDO_OK) {
        // Provide specific error messages for common failures
        std::string error_msg;
        if (r == FIDO_ERR_PIN_INVALID) {
            error_msg = "Incorrect YubiKey PIN. Please check your PIN and try again.";
        } else if (r == FIDO_ERR_PIN_AUTH_BLOCKED) {
            error_msg = "YubiKey PIN blocked due to too many incorrect attempts. Remove and reinsert YubiKey.";
        } else if (r == FIDO_ERR_PIN_REQUIRED) {
            error_msg = "YubiKey PIN is required but not provided.";
        } else if (r == FIDO_ERR_OPERATION_DENIED) {
            error_msg = "Operation denied - please touch your YubiKey when prompted.";
        } else {
            error_msg = std::format(
                "Failed to create credential: {} ({}). Please touch your YubiKey and ensure PIN is correct.",
                fido_strerr(r), r
            );
        }

        set_error(error_msg);
        KeepTower::Log::error("FIDO2: makeCredential failed: {}", fido_strerr(r));
        m_impl->cleanup();
        return std::nullopt;
    }

    // Extract credential ID
    const unsigned char* cred_id_ptr = fido_cred_id_ptr(m_impl->cred);
    size_t cred_id_len = fido_cred_id_len(m_impl->cred);

    if (!cred_id_ptr || cred_id_len == 0) {
        set_error("Failed to retrieve credential ID");
        m_impl->cleanup();
        return std::nullopt;
    }

    // Store credential ID
    std::vector<unsigned char> credential_id(cred_id_ptr, cred_id_ptr + cred_id_len);
    m_impl->cred_id = credential_id;
    m_impl->has_credential = true;

    KeepTower::Log::info("FIDO2: Credential created successfully ({} bytes)", cred_id_len);

    m_impl->cleanup();
    return credential_id;
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

    m_impl->cred_id.assign(credential_id.begin(), credential_id.end());
    m_impl->has_credential = true;

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
    if (is_busy()) {
        KeepTower::Log::warning("YubiKeyManager: Async operation already in progress");
        callback(std::nullopt, "Operation already in progress");
        return;
    }

    // Wait for previous thread to finish
    if (m_worker_thread && m_worker_thread->joinable()) {
        m_worker_thread->join();
    }

    m_is_busy.store(true, std::memory_order_release);
    m_cancel_requested.store(false, std::memory_order_release);

    KeepTower::Log::info("YubiKeyManager: Starting async credential creation for user '{}'", user_name);

    // Copy parameters for thread (span/string_view not safe across threads)
    std::string user_name_copy{user_name};
    std::optional<std::string> pin_copy = pin ? std::optional<std::string>(*pin) : std::nullopt;

    // Launch background thread
    m_worker_thread = std::make_unique<std::thread>(
        &YubiKeyManager::thread_create_credential,
        this,
        std::move(user_name_copy),
        std::move(pin_copy),
        require_touch,
        std::move(callback));
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

    // Wait for previous thread to finish
    if (m_worker_thread && m_worker_thread->joinable()) {
        m_worker_thread->join();
    }

    m_is_busy.store(true, std::memory_order_release);
    m_cancel_requested.store(false, std::memory_order_release);

    KeepTower::Log::info("YubiKeyManager: Starting async challenge-response");

    // Copy parameters for thread
    std::vector<unsigned char> challenge_copy(challenge.begin(), challenge.end());
    std::optional<std::string> pin_copy = pin ? std::optional<std::string>(*pin) : std::nullopt;

    // Launch background thread
    m_worker_thread = std::make_unique<std::thread>(
        &YubiKeyManager::thread_challenge_response,
        this,
        std::move(challenge_copy),
        algorithm,
        require_touch,
        timeout_ms,
        std::move(pin_copy),
        std::move(callback));
}

bool YubiKeyManager::is_busy() const noexcept {
    return m_is_busy.load(std::memory_order_acquire);
}

void YubiKeyManager::cancel_async() noexcept {
    if (is_busy()) {
        KeepTower::Log::warning("YubiKeyManager: Cancellation requested");
        m_cancel_requested.store(true, std::memory_order_release);
    }
}

void YubiKeyManager::thread_create_credential(
    std::string user_name,
    std::optional<std::string> pin,
    bool require_touch,
    CreateCredentialCallback callback
) noexcept {
    KeepTower::Log::info("YubiKeyManager: Worker thread started for credential creation");

    // Check for cancellation before starting
    if (m_cancel_requested.load(std::memory_order_acquire)) {
        KeepTower::Log::info("YubiKeyManager: Operation cancelled before starting");
        m_is_busy.store(false, std::memory_order_release);
        return;
    }

    // Execute blocking operation
    std::string_view pin_view = pin ? std::string_view(*pin) : std::string_view("");
    auto credential_id = create_credential(user_name, pin_view);

    // Check for cancellation after operation
    if (m_cancel_requested.load(std::memory_order_acquire)) {
        KeepTower::Log::info("YubiKeyManager: Operation cancelled after completion");
        m_is_busy.store(false, std::memory_order_release);
        return;
    }

    KeepTower::Log::info("YubiKeyManager: Credential creation completed with {}",
                         credential_id ? "success" : "error");

    // Capture credential_id by value for the callback
    std::optional<std::vector<unsigned char>> cred_copy = credential_id;
    std::string error_msg = std::string(m_last_error);

    // Schedule callback on UI thread
    {
        std::lock_guard<std::mutex> lock(m_callback_mutex);
        m_pending_callback = [callback = std::move(callback),
                              cred_copy = std::move(cred_copy),
                              error_msg = std::move(error_msg)]() mutable {
            callback(cred_copy, error_msg);
        };
    }

    m_is_busy.store(false, std::memory_order_release);
    m_dispatcher.emit();  // Trigger UI thread callback
}

void YubiKeyManager::thread_challenge_response(
    std::vector<unsigned char> challenge,
    YubiKeyAlgorithm algorithm,
    bool require_touch,
    int timeout_ms,
    std::optional<std::string> pin,
    ChallengeResponseCallback callback
) noexcept {
    KeepTower::Log::info("YubiKeyManager: Worker thread started for challenge-response");

    // Check for cancellation before starting
    if (m_cancel_requested.load(std::memory_order_acquire)) {
        KeepTower::Log::info("YubiKeyManager: Operation cancelled before starting");
        m_is_busy.store(false, std::memory_order_release);
        return;
    }

    // Execute blocking operation
    auto response = challenge_response(
        challenge,
        algorithm,
        require_touch,
        timeout_ms,
        pin);

    // Check for cancellation after operation
    if (m_cancel_requested.load(std::memory_order_acquire)) {
        KeepTower::Log::info("YubiKeyManager: Operation cancelled after completion");
        m_is_busy.store(false, std::memory_order_release);
        return;
    }

    KeepTower::Log::info("YubiKeyManager: Challenge-response completed with {}",
                         response.success ? "success" : "error");

    // Schedule callback on UI thread
    {
        std::lock_guard<std::mutex> lock(m_callback_mutex);
        m_pending_callback = [callback = std::move(callback), response = std::move(response)]() mutable {
            callback(response);
        };
    }

    m_is_busy.store(false, std::memory_order_release);
    m_dispatcher.emit();  // Trigger UI thread callback
}
