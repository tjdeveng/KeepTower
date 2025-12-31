// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024 Travis E. Hansen

#include "VaultCrypto.h"
#include "../../utils/SecureMemory.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>

namespace KeepTower {

bool VaultCrypto::derive_key(
    const Glib::ustring& password,
    std::span<const uint8_t> salt,
    std::vector<uint8_t>& key,
    int iterations) {

    // Ensure key is properly sized
    if (key.size() != KEY_LENGTH) {
        key.resize(KEY_LENGTH);
    }

    // Use PBKDF2 with SHA-256 (NIST recommended)
    int result = PKCS5_PBKDF2_HMAC(
        password.c_str(), password.bytes(),
        salt.data(), salt.size(),
        iterations,
        EVP_sha256(),
        KEY_LENGTH,
        key.data()
    );

    return result == 1;
}

bool VaultCrypto::encrypt_data(
    std::span<const uint8_t> plaintext,
    std::span<const uint8_t> key,
    std::vector<uint8_t>& ciphertext,
    std::span<const uint8_t> iv) {

    // Validate inputs
    if (key.size() != KEY_LENGTH) {
        return false;
    }
    if (iv.size() != IV_LENGTH) {
        return false;
    }

    KeepTower::EVPCipherContextPtr ctx(EVP_CIPHER_CTX_new());
    if (!ctx) {
        return false;
    }

    // Initialize encryption with AES-256-GCM
    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, key.data(), iv.data()) != 1) {
        return false;
    }

    // Allocate output buffer
    ciphertext.resize(plaintext.size() + EVP_CIPHER_block_size(EVP_aes_256_gcm()) + TAG_LENGTH);
    int len = 0;
    int ciphertext_len = 0;

    // Encrypt
    if (EVP_EncryptUpdate(ctx.get(), ciphertext.data(), &len, plaintext.data(), plaintext.size()) != 1) {
        return false;
    }
    ciphertext_len = len;

    // Finalize
    if (EVP_EncryptFinal_ex(ctx.get(), ciphertext.data() + len, &len) != 1) {
        return false;
    }
    ciphertext_len += len;

    // Get authentication tag (GCM) - use SecureVector for auto-zeroization
    KeepTower::SecureVector<uint8_t> tag(TAG_LENGTH);
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, TAG_LENGTH, tag.data()) != 1) {
        return false;
    }

    // Append tag to ciphertext
    ciphertext.resize(ciphertext_len);
    ciphertext.insert(ciphertext.end(), tag.begin(), tag.end());

    return true;
}

bool VaultCrypto::decrypt_data(
    std::span<const uint8_t> ciphertext,
    std::span<const uint8_t> key,
    std::span<const uint8_t> iv,
    std::vector<uint8_t>& plaintext) {

    // Validate inputs
    if (key.size() != KEY_LENGTH) {
        return false;
    }
    if (iv.size() != IV_LENGTH) {
        return false;
    }
    if (ciphertext.size() < TAG_LENGTH) {
        return false;
    }

    // Extract authentication tag (last TAG_LENGTH bytes)
    // Use SecureVector for tag (contains key-derived authentication data)
    KeepTower::SecureVector<uint8_t> tag(ciphertext.end() - TAG_LENGTH, ciphertext.end());
    KeepTower::SecureVector<uint8_t> actual_ciphertext(ciphertext.begin(), ciphertext.end() - TAG_LENGTH);

    KeepTower::EVPCipherContextPtr ctx(EVP_CIPHER_CTX_new());
    if (!ctx) {
        return false;
    }

    // Initialize decryption with AES-256-GCM
    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, key.data(), iv.data()) != 1) {
        return false;
    }

    // Allocate output buffer
    plaintext.resize(actual_ciphertext.size() + EVP_CIPHER_block_size(EVP_aes_256_gcm()));
    int len = 0;
    int plaintext_len = 0;

    // Decrypt
    if (EVP_DecryptUpdate(ctx.get(), plaintext.data(), &len, actual_ciphertext.data(), actual_ciphertext.size()) != 1) {
        return false;
    }
    plaintext_len = len;

    // Set authentication tag
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, TAG_LENGTH, tag.data()) != 1) {
        return false;
    }

    // Finalize (verifies authentication tag)
    if (EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + len, &len) != 1) {
        return false;
    }
    plaintext_len += len;

    plaintext.resize(plaintext_len);

    return true;
}

std::vector<uint8_t> VaultCrypto::generate_random_bytes(size_t length) {
    std::vector<uint8_t> bytes(length);
    // FIPS-140-3 requirement: Check CSPRNG return value
    // RAND_bytes returns 1 on success, 0 or -1 on failure
    if (RAND_bytes(bytes.data(), static_cast<int>(length)) != 1) {
        // CRITICAL: PRNG failure is a security event
        // Clear buffer and throw - do not return predictable data
        OPENSSL_cleanse(bytes.data(), bytes.size());
        throw std::runtime_error("CSPRNG failure: RAND_bytes() failed");
    }
    return bytes;
}

}  // namespace KeepTower
