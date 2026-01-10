// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "VaultCryptoService.h"
#include "../crypto/VaultCrypto.h"
#include "../KeyWrapping.h"
#include "../../utils/Log.h"
#include "../../utils/SecureMemory.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <cstring>

#ifdef __linux__
#include <sys/mman.h>
#include <cerrno>
#elif _WIN32
#include <windows.h>
#endif

namespace KeepTower {

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Lock memory to prevent swapping (best-effort)
 * @param data Pointer to memory to lock
 * @param size Size of memory region in bytes
 * @return true if memory was locked, false otherwise
 */
static bool lock_memory(void* data, size_t size) {
    if (!data || size == 0) {
        return true;
    }

#ifdef __linux__
    if (mlock(data, size) == 0) {
        Log::debug("VaultCryptoService: Locked {} bytes in memory", size);
        return true;
    } else {
        Log::debug("VaultCryptoService: Failed to lock memory: {} ({})",
                   std::strerror(errno), errno);
        return false;
    }
#elif _WIN32
    if (VirtualLock(data, size)) {
        Log::debug("VaultCryptoService: Locked {} bytes in memory", size);
        return true;
    } else {
        Log::debug("VaultCryptoService: Failed to lock memory: error {}", GetLastError());
        return false;
    }
#else
    Log::debug("VaultCryptoService: Memory locking not supported on this platform");
    return false;
#endif
}

// ============================================================================
// DEK Generation
// ============================================================================

VaultResult<VaultCryptoService::DEKResult> VaultCryptoService::generate_dek() {
    auto dek_result = KeyWrapping::generate_random_dek();
    if (!dek_result) {
        Log::error("VaultCryptoService: Failed to generate DEK");
        return std::unexpected(VaultError::CryptoError);
    }

    DEKResult result;
    result.dek = dek_result.value();

    // Attempt to lock DEK in memory (best effort)
    result.memory_locked = lock_memory(result.dek.data(), result.dek.size());
    if (result.memory_locked) {
        Log::debug("VaultCryptoService: DEK locked in memory");
    } else {
        Log::debug("VaultCryptoService: DEK memory lock failed (continuing anyway)");
    }

    return result;
}

// ============================================================================
// KEK Derivation
// ============================================================================

VaultResult<VaultCryptoService::KEKResult> VaultCryptoService::derive_kek_from_password(
    const Glib::ustring& password,
    uint32_t iterations) {

    // Generate random salt
    auto salt_result = KeyWrapping::generate_random_salt();
    if (!salt_result) {
        Log::error("VaultCryptoService: Failed to generate salt");
        return std::unexpected(VaultError::CryptoError);
    }

    // Derive KEK from password
    auto kek_result = KeyWrapping::derive_kek_from_password(
        password,
        salt_result.value(),
        iterations);
    if (!kek_result) {
        Log::error("VaultCryptoService: Failed to derive KEK from password");
        return std::unexpected(VaultError::CryptoError);
    }

    KEKResult result;
    result.kek = kek_result.value();
    result.salt = salt_result.value();

    Log::debug("VaultCryptoService: KEK derived (password length: {} bytes, iterations: {})",
               password.bytes(), iterations);

    return result;
}

VaultResult<std::array<uint8_t, 32>> VaultCryptoService::derive_kek_with_salt(
    const Glib::ustring& password,
    const std::array<uint8_t, 32>& salt,
    uint32_t iterations) {

    auto kek_result = KeyWrapping::derive_kek_from_password(
        password,
        salt,
        iterations);
    if (!kek_result) {
        Log::error("VaultCryptoService: Failed to derive KEK with provided salt");
        return std::unexpected(VaultError::CryptoError);
    }

    return kek_result.value();
}

// ============================================================================
// Key Wrapping
// ============================================================================

VaultResult<std::vector<uint8_t>> VaultCryptoService::wrap_dek(
    const std::array<uint8_t, 32>& kek,
    const std::array<uint8_t, 32>& dek) {

    auto wrapped_result = KeyWrapping::wrap_key(kek, dek);
    if (!wrapped_result) {
        Log::error("VaultCryptoService: Failed to wrap DEK");
        return std::unexpected(VaultError::CryptoError);
    }

    const auto& wrapped_array = wrapped_result.value().wrapped_key;
    std::vector<uint8_t> wrapped_vec(wrapped_array.begin(), wrapped_array.end());

    Log::debug("VaultCryptoService: DEK wrapped ({} bytes)", wrapped_vec.size());

    return wrapped_vec;
}

VaultResult<std::array<uint8_t, 32>> VaultCryptoService::unwrap_dek(
    const std::array<uint8_t, 32>& kek,
    const std::vector<uint8_t>& wrapped_dek) {

    // KeyWrapping::unwrap_key expects std::array<uint8_t, 40>
    if (wrapped_dek.size() != 40) {
        Log::error("VaultCryptoService: Invalid wrapped DEK size (expected 40, got {})",
                   wrapped_dek.size());
        return std::unexpected(VaultError::CryptoError);
    }

    // Convert vector to array
    std::array<uint8_t, 40> wrapped_array;
    std::copy(wrapped_dek.begin(), wrapped_dek.end(), wrapped_array.begin());

    auto unwrapped_result = KeyWrapping::unwrap_key(kek, wrapped_array);
    if (!unwrapped_result) {
        Log::error("VaultCryptoService: Failed to unwrap DEK");
        return std::unexpected(VaultError::CryptoError);
    }

    Log::debug("VaultCryptoService: DEK unwrapped successfully");

    return unwrapped_result.value().dek;
}

// ============================================================================
// Vault Data Encryption/Decryption
// ============================================================================

VaultResult<VaultCryptoService::EncryptionResult> VaultCryptoService::encrypt_vault_data(
    const std::vector<uint8_t>& plaintext,
    const std::array<uint8_t, 32>& dek) {

    // Generate random IV
    std::vector<uint8_t> iv = VaultCrypto::generate_random_bytes(VaultCrypto::IV_LENGTH);

    // Encrypt with AES-256-GCM
    std::vector<uint8_t> ciphertext;
    if (!VaultCrypto::encrypt_data(plaintext, dek, ciphertext, iv)) {
        Log::error("VaultCryptoService: Failed to encrypt vault data");
        return std::unexpected(VaultError::CryptoError);
    }

    EncryptionResult result;
    result.ciphertext = std::move(ciphertext);
    result.iv = std::move(iv);

    Log::debug("VaultCryptoService: Vault data encrypted ({} -> {} bytes)",
               plaintext.size(), result.ciphertext.size());

    return result;
}

VaultResult<std::vector<uint8_t>> VaultCryptoService::decrypt_vault_data(
    const std::vector<uint8_t>& ciphertext,
    const std::array<uint8_t, 32>& dek,
    const std::vector<uint8_t>& iv) {

    std::vector<uint8_t> plaintext;
    // VaultCrypto::decrypt_data takes (ciphertext, key, iv, plaintext_output)
    if (!VaultCrypto::decrypt_data(ciphertext, dek, iv, plaintext)) {
        Log::error("VaultCryptoService: Failed to decrypt vault data");
        return std::unexpected(VaultError::CryptoError);
    }

    Log::debug("VaultCryptoService: Vault data decrypted ({} -> {} bytes)",
               ciphertext.size(), plaintext.size());

    return plaintext;
}

// ============================================================================
// PIN Encryption/Decryption
// ============================================================================

VaultResult<VaultCryptoService::PINEncryptionResult> VaultCryptoService::encrypt_pin(
    const std::string& pin,
    const std::array<uint8_t, 32>& kek) {

    // Generate random IV
    std::vector<uint8_t> iv = VaultCrypto::generate_random_bytes(VaultCrypto::IV_LENGTH);

    // Convert PIN to bytes
    std::vector<uint8_t> pin_bytes(pin.begin(), pin.end());

    // Encrypt with AES-256-GCM
    std::vector<uint8_t> ciphertext;
    if (!VaultCrypto::encrypt_data(pin_bytes, kek, ciphertext, iv)) {
        Log::error("VaultCryptoService: Failed to encrypt PIN");
        secure_clear(pin_bytes);
        return std::unexpected(VaultError::CryptoError);
    }
    secure_clear(pin_bytes);

    // Create storage format: [IV(12) || ciphertext+tag]
    PINEncryptionResult result;
    result.encrypted_pin.reserve(iv.size() + ciphertext.size());
    result.encrypted_pin.insert(result.encrypted_pin.end(), iv.begin(), iv.end());
    result.encrypted_pin.insert(result.encrypted_pin.end(), ciphertext.begin(), ciphertext.end());

    Log::debug("VaultCryptoService: PIN encrypted ({} bytes)", result.encrypted_pin.size());

    return result;
}

VaultResult<std::string> VaultCryptoService::decrypt_pin(
    const std::vector<uint8_t>& encrypted_pin,
    const std::array<uint8_t, 32>& kek) {

    // Validate size (at least IV + minimal ciphertext)
    if (encrypted_pin.size() < VaultCrypto::IV_LENGTH + 16) {
        Log::error("VaultCryptoService: Encrypted PIN too small");
        return std::unexpected(VaultError::CryptoError);
    }

    // Extract IV (first 12 bytes)
    std::vector<uint8_t> iv(encrypted_pin.begin(),
                            encrypted_pin.begin() + VaultCrypto::IV_LENGTH);

    // Extract ciphertext (remaining bytes)
    std::vector<uint8_t> ciphertext(encrypted_pin.begin() + VaultCrypto::IV_LENGTH,
                                     encrypted_pin.end());

    // Decrypt
    std::vector<uint8_t> plaintext;
    if (!VaultCrypto::decrypt_data(ciphertext, kek, iv, plaintext)) {
        Log::error("VaultCryptoService: Failed to decrypt PIN");
        return std::unexpected(VaultError::CryptoError);
    }

    // Convert to string
    std::string pin(plaintext.begin(), plaintext.end());
    secure_clear(plaintext);

    Log::debug("VaultCryptoService: PIN decrypted successfully");

    return pin;
}

// ============================================================================
// YubiKey KEK Combination
// ============================================================================

VaultResult<std::array<uint8_t, 32>> VaultCryptoService::combine_kek_with_yubikey(
    const std::array<uint8_t, 32>& password_kek,
    const std::vector<uint8_t>& yubikey_response) {

    if (yubikey_response.empty()) {
        Log::error("VaultCryptoService: Empty YubiKey response");
        return std::unexpected(VaultError::CryptoError);
    }

    // Use KeyWrapping's combine function (handles response extension if needed)
    std::array<uint8_t, 32> combined_kek = KeyWrapping::combine_with_yubikey_v2(
        password_kek,
        yubikey_response);

    Log::debug("VaultCryptoService: KEK combined with YubiKey response ({} bytes)",
               yubikey_response.size());

    return combined_kek;
}

// ============================================================================
// Utility Methods
// ============================================================================

void VaultCryptoService::secure_clear(void* data, size_t size) {
    if (data && size > 0) {
        OPENSSL_cleanse(data, size);
    }
}

void VaultCryptoService::secure_clear(std::vector<uint8_t>& data) {
    if (!data.empty()) {
        OPENSSL_cleanse(data.data(), data.size());
        data.clear();
    }
}

} // namespace KeepTower
