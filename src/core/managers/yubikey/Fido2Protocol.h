#pragma once

#include "../YubiKeyManager.h"
#include "../../utils/Log.h"

#ifdef HAVE_YUBIKEY_SUPPORT
#include <fido.h>
#endif

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace FIDO2 {
    // FIDO2 constants
    constexpr size_t SALT_SIZE = 32;        // Salt for hmac-secret (SHA-256 input)
    constexpr size_t SECRET_SIZE = 32;      // Output secret size (SHA-256 output)
    constexpr size_t CRED_ID_MAX = 1024;    // Maximum credential ID size
    constexpr int DEFAULT_TIMEOUT_MS = 30000; // 30 seconds for user interaction

    // Relying Party (RP) information for KeepTower
    constexpr const char* RP_ID = "keeptower.local";
    constexpr const char* RP_NAME = "KeepTower Password Manager";

    inline bool generate_salt(unsigned char* salt) noexcept {
        return RAND_bytes(salt, SALT_SIZE) == 1;
    }

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

namespace KeepTower::YubiKeyInternal {

class Fido2Protocol {
public:
    struct CredentialResult {
        std::optional<std::vector<unsigned char>> credential_id{};
        std::string error{};
    };

    struct HmacSecretResult {
        bool success{false};
        std::array<unsigned char, FIDO2::SECRET_SIZE> response{};
        size_t response_size{0};
        std::string error{};
    };

#ifdef HAVE_YUBIKEY_SUPPORT
    fido_dev_t* dev{nullptr};
    fido_cred_t* cred{nullptr};
    fido_assert_t* assert_{nullptr};
    std::vector<uint8_t> cred_id{};
    std::string device_path{};
    bool has_credential{false};
#endif

    Fido2Protocol() noexcept = default;

    ~Fido2Protocol() noexcept {
        cleanup();
    }

    void cleanup() noexcept {
#ifdef HAVE_YUBIKEY_SUPPORT
        if (assert_) {
            fido_assert_free(&assert_);
            assert_ = nullptr;
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
        device_path.clear();
#endif
    }

#ifdef HAVE_YUBIKEY_SUPPORT
    bool open_device(const std::string& path) noexcept {
        if (dev) {
            return true;
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

    bool get_device_info(YubiKeyManager::YubiKeyInfo& info) noexcept {
        if (!dev) {
            return false;
        }

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
        info.is_fips_capable = has_hmac_secret;
        info.is_fips_mode = has_hmac_secret;

        uint64_t version = fido_cbor_info_fwversion(cbor_info);
        info.version_major = (version >> 32) & 0xFFFF;
        info.version_minor = (version >> 16) & 0xFFFF;
        info.version_build = version & 0xFFFF;

        info.serial_number = device_path;

        if (has_hmac_secret) {
            info.supported_algorithms = {YubiKeyAlgorithm::HMAC_SHA256};
        } else {
            info.supported_algorithms = {};
        }

        if (fido_dev_has_pin(dev)) {
            KeepTower::Log::info("FIDO2: Device has PIN set");
        } else {
            KeepTower::Log::warning("FIDO2: Device does not have PIN set - hmac-secret requires PIN!");
        }

        fido_cbor_info_free(&cbor_info);
        return true;
    }

    CredentialResult create_credential(std::string_view user_id, std::string_view pin) noexcept {
        CredentialResult result{};

        cred = fido_cred_new();
        if (!cred) {
            result.error = "Failed to allocate FIDO2 credential";
            cleanup();
            return result;
        }

        int r = fido_cred_set_type(cred, COSE_ES256);
        if (r != FIDO_OK) {
            result.error = std::format("Failed to set credential type: {}", fido_strerr(r));
            cleanup();
            return result;
        }

        r = fido_cred_set_rp(cred, FIDO2::RP_ID, "KeepTower Vault");
        if (r != FIDO_OK) {
            result.error = std::format("Failed to set RP: {}", fido_strerr(r));
            cleanup();
            return result;
        }

        std::array<unsigned char, 32> user_id_hash{};
        FIDO2::derive_salt_from_data(
            reinterpret_cast<const unsigned char*>(user_id.data()),
            user_id.size(),
            user_id_hash.data()
        );

        r = fido_cred_set_user(
            cred,
            user_id_hash.data(), user_id_hash.size(),
            user_id.data(),
            user_id.data(),
            nullptr
        );
        if (r != FIDO_OK) {
            result.error = std::format("Failed to set user: {}", fido_strerr(r));
            cleanup();
            return result;
        }

        std::array<unsigned char, 32> cdh{};
        FIDO2::generate_salt(cdh.data());

        r = fido_cred_set_clientdata_hash(cred, cdh.data(), cdh.size());
        if (r != FIDO_OK) {
            result.error = std::format("Failed to set client data hash: {}", fido_strerr(r));
            cleanup();
            return result;
        }

        r = fido_cred_set_extensions(cred, FIDO_EXT_HMAC_SECRET);
        if (r != FIDO_OK) {
            result.error = std::format("Failed to enable hmac-secret extension: {}", fido_strerr(r));
            cleanup();
            return result;
        }

        r = fido_cred_set_rk(cred, FIDO_OPT_TRUE);
        if (r != FIDO_OK) {
            result.error = std::format("Failed to set resident key: {}", fido_strerr(r));
            cleanup();
            return result;
        }

        r = fido_cred_set_uv(cred, FIDO_OPT_TRUE);
        if (r != FIDO_OK) {
            result.error = std::format("Failed to set user verification: {}", fido_strerr(r));
            cleanup();
            return result;
        }

        KeepTower::Log::info("FIDO2: Creating credential - please touch your YubiKey");

        std::string pin_str(pin);
        if (pin_str.length() < 4 || pin_str.length() > 63) {
            result.error = std::format("Invalid PIN length: {} (must be 4-63 characters)", pin_str.length());
            KeepTower::Log::error("FIDO2: Invalid PIN length: {}", pin_str.length());
            cleanup();
            return result;
        }

        KeepTower::Log::debug("FIDO2: PIN length: {} characters", pin_str.length());
        KeepTower::Log::debug("FIDO2: User ID: {}", std::string(user_id));

        r = fido_dev_make_cred(dev, cred, pin_str.c_str());
        if (r != FIDO_OK) {
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

            result.error = error_msg;
            KeepTower::Log::error("FIDO2: makeCredential failed: {}", fido_strerr(r));
            cleanup();
            return result;
        }

        const unsigned char* cred_id_ptr = fido_cred_id_ptr(cred);
        size_t cred_id_len = fido_cred_id_len(cred);

        if (!cred_id_ptr || cred_id_len == 0) {
            result.error = "Failed to retrieve credential ID";
            cleanup();
            return result;
        }

        std::vector<unsigned char> credential_id(cred_id_ptr, cred_id_ptr + cred_id_len);
        cred_id.assign(credential_id.begin(), credential_id.end());
        has_credential = true;

        KeepTower::Log::info("FIDO2: Credential created successfully ({} bytes)", cred_id_len);

        result.credential_id = std::move(credential_id);
        cleanup();
        return result;
    }

    HmacSecretResult perform_hmac_secret(
        std::span<const unsigned char> challenge,
        std::string_view pin
    ) noexcept {
        HmacSecretResult result{};

        std::array<unsigned char, FIDO2::SALT_SIZE> salt{};
        if (challenge.size() <= FIDO2::SALT_SIZE) {
            std::copy_n(challenge.begin(), challenge.size(), salt.begin());
        } else {
            unsigned int hash_len = 0;
            if (EVP_Digest(challenge.data(), challenge.size(), salt.data(), &hash_len,
                           EVP_sha256(), nullptr) != 1 || hash_len != FIDO2::SALT_SIZE) {
                result.error = "Failed to derive hmac-secret salt from challenge";
                cleanup();
                return result;
            }
        }

        assert_ = fido_assert_new();
        if (!assert_) {
            result.error = "Failed to allocate FIDO2 assertion";
            cleanup();
            return result;
        }

        int r = fido_assert_set_rp(assert_, FIDO2::RP_ID);
        if (r != FIDO_OK) {
            result.error = std::format("Failed to set RP ID: {}", fido_strerr(r));
            cleanup();
            return result;
        }

        r = fido_assert_allow_cred(assert_, cred_id.data(), cred_id.size());
        if (r != FIDO_OK) {
            result.error = std::format("Failed to set credential ID: {}", fido_strerr(r));
            cleanup();
            return result;
        }

        std::array<unsigned char, 32> cdh{};
        FIDO2::derive_salt_from_data(challenge.data(), challenge.size(), cdh.data());

        r = fido_assert_set_clientdata_hash(assert_, cdh.data(), cdh.size());
        if (r != FIDO_OK) {
            result.error = std::format("Failed to set client data: {}", fido_strerr(r));
            cleanup();
            return result;
        }

        r = fido_assert_set_hmac_salt(assert_, salt.data(), salt.size());
        if (r != FIDO_OK) {
            result.error = std::format("Failed to set hmac-secret salt: {}", fido_strerr(r));
            cleanup();
            return result;
        }

        r = fido_assert_set_extensions(assert_, FIDO_EXT_HMAC_SECRET);
        if (r != FIDO_OK) {
            result.error = std::format("Failed to enable hmac-secret extension: {}", fido_strerr(r));
            cleanup();
            return result;
        }

        r = fido_assert_set_up(assert_, FIDO_OPT_TRUE);
        if (r != FIDO_OK) {
            result.error = std::format("Failed to set user presence: {}", fido_strerr(r));
            cleanup();
            return result;
        }

        std::string pin_str(pin);
        KeepTower::Log::info("FIDO2: Performing assertion - please touch your YubiKey");

        r = fido_dev_get_assert(dev, assert_, pin_str.c_str());
        if (r != FIDO_OK) {
            result.error = std::format(
                "YubiKey assertion failed: {} ({}). Please touch your YubiKey and ensure PIN is correct.",
                fido_strerr(r), r
            );
            KeepTower::Log::error("FIDO2: Assertion failed: {}", fido_strerr(r));
            cleanup();
            return result;
        }

        const unsigned char* hmac_secret = fido_assert_hmac_secret_ptr(assert_, 0);
        size_t hmac_secret_len = fido_assert_hmac_secret_len(assert_, 0);

        if (!hmac_secret || hmac_secret_len == 0) {
            result.error = "YubiKey did not return hmac-secret";
            cleanup();
            return result;
        }

        result.response_size = std::min(hmac_secret_len, result.response.size());
        std::copy_n(hmac_secret, result.response_size, result.response.begin());
        result.success = true;

        KeepTower::Log::info("FIDO2: Challenge-response successful ({} bytes)", result.response_size);

        cleanup();
        return result;
    }
#endif

    Fido2Protocol(const Fido2Protocol&) = delete;
    Fido2Protocol& operator=(const Fido2Protocol&) = delete;
    Fido2Protocol(Fido2Protocol&&) = delete;
    Fido2Protocol& operator=(Fido2Protocol&&) = delete;
};

} // namespace KeepTower::YubiKeyInternal
