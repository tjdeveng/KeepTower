// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file SecureMemory.h
 * @brief Secure memory handling utilities
 *
 * Provides RAII wrappers and utilities for securely handling sensitive
 * data like encryption keys, ensuring proper cleanup even in exceptional
 * circumstances.
 */

#ifndef SECUREMEMORY_H
#define SECUREMEMORY_H

#include <array>
#include <memory>
#include <openssl/evp.h>
#include <openssl/crypto.h>

namespace KeepTower {

/**
 * @brief Custom deleter for EVP_CIPHER_CTX that securely frees context
 */
struct EVPCipherContextDeleter {
    void operator()(EVP_CIPHER_CTX* ctx) const {
        if (ctx) {
            EVP_CIPHER_CTX_free(ctx);
        }
    }
};

/**
 * @brief RAII wrapper for EVP_CIPHER_CTX
 *
 * Automatically frees OpenSSL cipher context on scope exit,
 * providing exception-safe resource management.
 *
 * @code
 * EVPCipherContextPtr ctx(EVP_CIPHER_CTX_new());
 * if (!ctx) {
 *     return error;
 * }
 * EVP_EncryptInit_ex(ctx.get(), ...);
 * // Automatically freed on scope exit
 * @endcode
 */
using EVPCipherContextPtr = std::unique_ptr<EVP_CIPHER_CTX, EVPCipherContextDeleter>;

/**
 * @brief RAII wrapper for sensitive data that securely clears on destruction
 *
 * Uses OPENSSL_cleanse() to ensure sensitive data is overwritten before
 * deallocation, preventing memory dumps and swap file leakage.
 *
 * @tparam T Type of data (must be std::array<uint8_t, N>)
 *
 * @code
 * SecureBuffer<std::array<uint8_t, 32>> kek(derive_kek(...));
 * // Use kek.get() to access data
 * // Automatically securely cleared on scope exit
 * @endcode
 */
template<typename T>
class SecureBuffer {
public:
    /**
     * @brief Construct from existing data (copies and takes ownership)
     */
    explicit SecureBuffer(const T& data) : data_(data) {}

    /**
     * @brief Construct with default-initialized data
     */
    SecureBuffer() = default;

    /**
     * @brief Destructor securely clears data
     */
    ~SecureBuffer() {
        secure_clear();
    }

    // Prevent copying (sensitive data should not be duplicated)
    SecureBuffer(const SecureBuffer&) = delete;
    SecureBuffer& operator=(const SecureBuffer&) = delete;

    // Allow moving
    SecureBuffer(SecureBuffer&& other) noexcept : data_(std::move(other.data_)) {
        other.secure_clear();
    }

    SecureBuffer& operator=(SecureBuffer&& other) noexcept {
        if (this != &other) {
            secure_clear();
            data_ = std::move(other.data_);
            other.secure_clear();
        }
        return *this;
    }

    /**
     * @brief Get const reference to data
     */
    [[nodiscard]] const T& get() const { return data_; }

    /**
     * @brief Get mutable reference to data
     */
    [[nodiscard]] T& get() { return data_; }

    /**
     * @brief Manually clear data (called automatically in destructor)
     */
    void secure_clear() {
        if constexpr (requires { data_.data(); data_.size(); }) {
            OPENSSL_cleanse(data_.data(), data_.size());
        }
    }

private:
    T data_;
};

/**
 * @brief Securely clear a std::array
 *
 * Uses OPENSSL_cleanse() to overwrite memory with zeros in a way
 * that won't be optimized away by the compiler.
 *
 * @tparam N Size of array
 * @param arr Array to clear
 *
 * @code
 * std::array<uint8_t, 32> kek = derive_kek(...);
 * // Use kek...
 * secure_clear(kek);  // Securely erase
 * @endcode
 */
template<size_t N>
inline void secure_clear(std::array<uint8_t, N>& arr) {
    OPENSSL_cleanse(arr.data(), arr.size());
}

} // namespace KeepTower

#endif // SECUREMEMORY_H
