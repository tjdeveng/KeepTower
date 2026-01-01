// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "KeyWrapping.h"
#include "../utils/Log.h"
#include "../utils/SecureMemory.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/kdf.h>
#include <openssl/err.h>
#include <cstring>
#include <algorithm>

namespace KeepTower {

// ============================================================================
// AES-256-KW Key Wrapping (RFC 3394, NIST SP 800-38F)
// ============================================================================

std::expected<KeyWrapping::WrappedKey, KeyWrapping::Error>
KeyWrapping::wrap_key(const std::array<uint8_t, KEK_SIZE>& kek,
                      const std::array<uint8_t, DEK_SIZE>& dek) {

    // Create cipher context for AES-256-WRAP with RAII
    EVPCipherContextPtr ctx(EVP_CIPHER_CTX_new());
    if (!ctx) {
        Log::error("KeyWrapping: Failed to create cipher context");
        return std::unexpected(Error::OPENSSL_ERROR);
    }

    // Initialize wrapping operation with AES-256-WRAP
    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_wrap(), nullptr, kek.data(), nullptr) != 1) {
        Log::error("KeyWrapping: Failed to initialize wrap operation");
        return std::unexpected(Error::WRAP_FAILED);
    }

    // Disable padding (AES-KW has specific block handling)
    EVP_CIPHER_CTX_set_flags(ctx.get(), EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);

    // Wrap the DEK
    WrappedKey result;
    int outlen = 0;
    if (EVP_EncryptUpdate(ctx.get(), result.wrapped_key.data(), &outlen, dek.data(), DEK_SIZE) != 1) {
        Log::error("KeyWrapping: EVP_EncryptUpdate failed");
        return std::unexpected(Error::WRAP_FAILED);
    }

    int final_len = 0;
    if (EVP_EncryptFinal_ex(ctx.get(), result.wrapped_key.data() + outlen, &final_len) != 1) {
        Log::error("KeyWrapping: EVP_EncryptFinal_ex failed");
        return std::unexpected(Error::WRAP_FAILED);
    }

    int total_len = outlen + final_len;
    if (total_len != WRAPPED_KEY_SIZE) {
        Log::error("KeyWrapping: Unexpected wrapped key size: {} (expected {})",
                   total_len, WRAPPED_KEY_SIZE);
        return std::unexpected(Error::WRAP_FAILED);
    }

    // Context automatically freed by RAII wrapper
    return result;
}

std::expected<KeyWrapping::UnwrappedKey, KeyWrapping::Error>
KeyWrapping::unwrap_key(const std::array<uint8_t, KEK_SIZE>& kek,
                        const std::array<uint8_t, WRAPPED_KEY_SIZE>& wrapped_dek) {

    // Create cipher context for AES-256-WRAP with RAII
    EVPCipherContextPtr ctx(EVP_CIPHER_CTX_new());
    if (!ctx) {
        Log::error("KeyWrapping: Failed to create cipher context");
        return std::unexpected(Error::OPENSSL_ERROR);
    }

    // Initialize unwrapping operation with AES-256-WRAP
    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_wrap(), nullptr, kek.data(), nullptr) != 1) {
        Log::error("KeyWrapping: Failed to initialize unwrap operation");
        return std::unexpected(Error::UNWRAP_FAILED);
    }

    // Enable unwrap mode
    EVP_CIPHER_CTX_set_flags(ctx.get(), EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);

    // Unwrap the DEK
    UnwrappedKey result;
    int outlen = 0;
    if (EVP_DecryptUpdate(ctx.get(), result.dek.data(), &outlen, wrapped_dek.data(), WRAPPED_KEY_SIZE) != 1) {
        // This is expected to fail if password is wrong (KEK incorrect)
        Log::debug("KeyWrapping: EVP_DecryptUpdate failed (likely wrong password)");
        return std::unexpected(Error::UNWRAP_FAILED);
    }

    int final_len = 0;
    if (EVP_DecryptFinal_ex(ctx.get(), result.dek.data() + outlen, &final_len) != 1) {
        // This is expected to fail if password is wrong or data is corrupted
        Log::debug("KeyWrapping: EVP_DecryptFinal_ex failed (likely wrong password or corrupted data)");
        return std::unexpected(Error::UNWRAP_FAILED);
    }

    int total_len = outlen + final_len;
    if (total_len != DEK_SIZE) {
        Log::error("KeyWrapping: Unexpected unwrapped key size: {} (expected {})",
                   total_len, DEK_SIZE);
        return std::unexpected(Error::UNWRAP_FAILED);
    }

    // Context automatically freed by RAII wrapper
    return result;
}

// ============================================================================
// PBKDF2-HMAC-SHA256 Key Derivation
// ============================================================================

std::expected<std::array<uint8_t, KeyWrapping::KEK_SIZE>, KeyWrapping::Error>
KeyWrapping::derive_kek_from_password(const Glib::ustring& password,
                                      const std::array<uint8_t, SALT_SIZE>& salt,
                                      uint32_t iterations) {

    std::array<uint8_t, KEK_SIZE> kek;

    // Debug: Log first 16 bytes of password for comparison
    std::string hex_preview;
    size_t preview_len = std::min<size_t>(16, password.bytes());
    for (size_t i = 0; i < preview_len; ++i) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%02x", static_cast<unsigned char>(password.data()[i]));
        hex_preview += buf;
    }
    Log::info("KeyWrapping: PBKDF2 input - {} bytes, hex preview: {}",
              password.bytes(), hex_preview);

    // Use PKCS5_PBKDF2_HMAC (supports SHA-256)
    int result = PKCS5_PBKDF2_HMAC(
        password.data(),           // Password
        password.bytes(),          // Password length
        salt.data(),               // Salt
        SALT_SIZE,                 // Salt length
        iterations,                // Iteration count
        EVP_sha256(),              // Hash function (SHA-256)
        KEK_SIZE,                  // Output key length
        kek.data()                 // Output buffer
    );

    if (result != 1) {
        Log::error("KeyWrapping: PBKDF2 derivation failed");
        return std::unexpected(Error::PBKDF2_FAILED);
    }

    return kek;
}

// ============================================================================
// YubiKey Integration
// ============================================================================

std::array<uint8_t, KeyWrapping::KEK_SIZE>
KeyWrapping::combine_with_yubikey(const std::array<uint8_t, KEK_SIZE>& kek,
                                   const std::array<uint8_t, YUBIKEY_RESPONSE_SIZE>& yubikey_response) {

    std::array<uint8_t, KEK_SIZE> combined_kek = kek;

    // XOR the first 20 bytes of KEK with YubiKey response
    // Remaining 12 bytes of KEK stay unchanged
    for (size_t i = 0; i < YUBIKEY_RESPONSE_SIZE; ++i) {
        combined_kek[i] ^= yubikey_response[i];
    }

    // Note: yubikey_response is const reference, caller responsible for cleanup
    // combined_kek will be moved/copied to caller and secured there
    return combined_kek;
}

std::array<uint8_t, KeyWrapping::KEK_SIZE>
KeyWrapping::combine_with_yubikey_v2(const std::array<uint8_t, KEK_SIZE>& kek,
                                      const std::vector<uint8_t>& yubikey_response) {

    std::array<uint8_t, KEK_SIZE> combined_kek = kek;
    std::array<uint8_t, KEK_SIZE> normalized_response{};

    if (yubikey_response.empty()) {
        Log::error("KeyWrapping: Empty YubiKey response");
        return kek;  // Return unchanged KEK on error
    }

    // Normalize response to KEK_SIZE (32 bytes)
    if (yubikey_response.size() <= KEK_SIZE) {
        // Response ≤ 32 bytes: zero-pad to 32 bytes
        // SHA-1 (20 bytes) → pad with 12 zeros
        // SHA-256 (32 bytes) → use as-is
        std::copy(yubikey_response.begin(), yubikey_response.end(), normalized_response.begin());
        Log::debug("KeyWrapping: YubiKey response {} bytes, zero-padded to 32",
                   yubikey_response.size());
    } else {
        // Response > 32 bytes: hash with SHA-256 to 32 bytes
        // SHA-512 (64 bytes) → hash to 32 bytes
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hash_len = 0;

        EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
        if (!mdctx) {
            Log::error("KeyWrapping: Failed to create hash context");
            return kek;
        }

        if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1 ||
            EVP_DigestUpdate(mdctx, yubikey_response.data(), yubikey_response.size()) != 1 ||
            EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
            Log::error("KeyWrapping: Failed to hash YubiKey response");
            EVP_MD_CTX_free(mdctx);
            return kek;
        }

        EVP_MD_CTX_free(mdctx);
        std::copy_n(hash, KEK_SIZE, normalized_response.begin());
        Log::debug("KeyWrapping: YubiKey response {} bytes, hashed to 32 with SHA-256",
                   yubikey_response.size());
    }

    // XOR KEK with normalized response
    for (size_t i = 0; i < KEK_SIZE; ++i) {
        combined_kek[i] ^= normalized_response[i];
    }

    // Secure cleanup - use OPENSSL_cleanse for cryptographic material
    // FIPS-140-3 Section 7.9: Zeroization of SSPs (Security-Sensitive Parameters)
    OPENSSL_cleanse(normalized_response.data(), normalized_response.size());

    return combined_kek;
}

// ============================================================================
// Random Generation
// ============================================================================

std::expected<std::array<uint8_t, KeyWrapping::DEK_SIZE>, KeyWrapping::Error>
KeyWrapping::generate_random_dek() {

    std::array<uint8_t, DEK_SIZE> dek;

    // Use OpenSSL's RAND_bytes (FIPS DRBG when FIPS mode enabled)
    if (RAND_bytes(dek.data(), DEK_SIZE) != 1) {
        Log::error("KeyWrapping: Failed to generate random DEK");
        return std::unexpected(Error::OPENSSL_ERROR);
    }

    return dek;
}

std::expected<std::array<uint8_t, KeyWrapping::SALT_SIZE>, KeyWrapping::Error>
KeyWrapping::generate_random_salt() {

    std::array<uint8_t, SALT_SIZE> salt;

    // Use OpenSSL's RAND_bytes (FIPS DRBG when FIPS mode enabled)
    if (RAND_bytes(salt.data(), SALT_SIZE) != 1) {
        Log::error("KeyWrapping: Failed to generate random salt");
        return std::unexpected(Error::OPENSSL_ERROR);
    }

    return salt;
}

// ============================================================================
// Error Handling
// ============================================================================

std::string KeyWrapping::error_to_string(Error error) {
    switch (error) {
        case Error::INVALID_KEK_SIZE:
            return "Invalid KEK size (must be 32 bytes)";
        case Error::INVALID_DEK_SIZE:
            return "Invalid DEK size (must be 32 bytes)";
        case Error::INVALID_WRAPPED_SIZE:
            return "Invalid wrapped key size (must be 40 bytes)";
        case Error::WRAP_FAILED:
            return "Key wrapping failed";
        case Error::UNWRAP_FAILED:
            return "Key unwrapping failed (wrong password or corrupted data)";
        case Error::PBKDF2_FAILED:
            return "Password-based key derivation failed";
        case Error::OPENSSL_ERROR:
            return "OpenSSL error";
        default:
            return "Unknown error";
    }
}

} // namespace KeepTower
