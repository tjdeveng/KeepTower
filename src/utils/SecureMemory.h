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
#include <glibmm/ustring.h>

namespace KeepTower {

/**
 * @brief Custom deleter for EVP_CIPHER_CTX that securely frees context
 */
struct EVPCipherContextDeleter {
    /** @brief Free OpenSSL cipher context
     *  @param ctx Cipher context to free (nullptr-safe) */
    void operator()(EVP_CIPHER_CTX* ctx) const {
        if (ctx) {
            EVP_CIPHER_CTX_free(ctx);
        }
    }
};

/**
 * @brief Secure allocator for std::vector that zeros memory on deallocation
 *
 * Provides automatic zeroization of sensitive data stored in vectors,
 * ensuring cryptographic key material is cleared from memory.
 *
 * @tparam T Type of elements (typically uint8_t for crypto buffers)
 *
 * @section usage Usage Example
 * @code
 * // Use with std::vector for automatic zeroization
 * std::vector<uint8_t, SecureAllocator<uint8_t>> key(32);
 * // ... use key ...
 * // Automatically zeroized on destruction
 *
 * // Or use the convenience alias:
 * SecureVector<uint8_t> sensitive_data(256);
 * // ... use sensitive_data ...
 * // Automatically zeroized on destruction
 * @endcode
 */
template<typename T>
class SecureAllocator : public std::allocator<T> {
public:
    template<typename U>
    struct rebind {
        using other = SecureAllocator<U>;
    };

    SecureAllocator() noexcept = default;

    template<typename U>
    SecureAllocator(const SecureAllocator<U>&) noexcept {}

    /**
     * @brief Deallocate and securely zero memory
     * @param p Pointer to memory to deallocate
     * @param n Number of elements
     */
    void deallocate(T* p, std::size_t n) {
        if (p) {
            // Securely zero memory before deallocation
            OPENSSL_cleanse(p, n * sizeof(T));
            std::allocator<T>::deallocate(p, n);
        }
    }
};

/**
 * @brief Convenience alias for std::vector with secure allocator
 *
 * Use this for any sensitive data that should be automatically
 * zeroized on deallocation (keys, plaintext, passwords, etc.)
 *
 * @tparam T Type of elements (typically uint8_t)
 */
template<typename T>
using SecureVector = std::vector<T, SecureAllocator<T>>;

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
    /** @brief Move constructor - transfers data and clears source
     *  @param other Source buffer to move from */
    SecureBuffer(SecureBuffer&& other) noexcept : data_(std::move(other.data_)) {
        other.secure_clear();
    }

    /** @brief Move assignment - transfers data and clears source
     *  @param other Source buffer to move from
     *  @return Reference to this buffer */
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

/**
 * @brief Securely clear a Glib::ustring containing sensitive data
 *
 * Uses OPENSSL_cleanse() to ensure the password/sensitive string data
 * is overwritten in a way that cannot be optimized away by the compiler.
 * This prevents passwords from remaining in memory after use.
 *
 * @param str String to clear (typically a password)
 *
 * @code
 * Glib::ustring password = entry.get_text();
 * // Use password...
 * secure_clear_ustring(password);  // Securely erase
 * @endcode
 *
 * @note Always use this instead of manual memset or loops, as those can
 *       be optimized away by the compiler.
 */
inline void secure_clear_ustring(Glib::ustring& str) {
    if (!str.empty()) {
        OPENSSL_cleanse(const_cast<char*>(str.data()), str.bytes());
        str.clear();
    }
}

/**
 * @brief RAII wrapper for Glib::ustring with automatic secure destruction
 *
 * Automatically securely clears password/sensitive string data on scope exit
 * using OPENSSL_cleanse. This prevents passwords from remaining in memory
 * and ensures cleanup even in exceptional circumstances.
 *
 * Security features:
 * - Automatic secure clearing on destruction
 * - Move semantics to prevent copying sensitive data
 * - Explicit clear() method for manual cleanup
 * - No default construction (must initialize with data)
 *
 * @code
 * SecureString password{entry.get_text()};
 * // Use password.get()...
 * // Automatically securely cleared on scope exit
 * @endcode
 *
 * @note This is the recommended way to handle passwords from GTK Entry widgets.
 *       It prevents common security issues like:
 *       - Forgetting to clear passwords
 *       - Using std::memset (which can be optimized away)
 *       - Exception-unsafe manual clearing
 */
class SecureString {
public:
    /**
     * @brief Construct from Glib::ustring (takes ownership)
     * @param str String to secure (typically a password from get_text())
     */
    explicit SecureString(Glib::ustring str) : str_(std::move(str)) {}

    /**
     * @brief Destructor securely clears string data
     */
    ~SecureString() {
        secure_clear_ustring(str_);
    }

    // Prevent copying (sensitive data should not be duplicated)
    SecureString(const SecureString&) = delete;
    SecureString& operator=(const SecureString&) = delete;

    /**
     * @brief Move constructor - securely clears source
     */
    SecureString(SecureString&& other) noexcept
        : str_(std::move(other.str_)) {
        secure_clear_ustring(other.str_);
    }

    /**
     * @brief Move assignment - securely clears both source and destination
     */
    SecureString& operator=(SecureString&& other) noexcept {
        if (this != &other) {
            secure_clear_ustring(str_);
            str_ = std::move(other.str_);
            secure_clear_ustring(other.str_);
        }
        return *this;
    }

    /**
     * @brief Get const reference to underlying string
     * @return Const reference to Glib::ustring
     */
    [[nodiscard]] const Glib::ustring& get() const noexcept {
        return str_;
    }

    /**
     * @brief Get mutable reference to underlying string
     * @return Mutable reference to Glib::ustring
     *
     * @warning Use with caution - modifying the string directly bypasses
     *          secure clearing. Only use for operations that maintain security.
     */
    [[nodiscard]] Glib::ustring& get() noexcept {
        return str_;
    }

    /**
     * @brief Manually clear string data (called automatically in destructor)
     */
    void clear() noexcept {
        secure_clear_ustring(str_);
    }

    /**
     * @brief Check if string is empty
     * @return true if string is empty, false otherwise
     */
    [[nodiscard]] bool empty() const noexcept {
        return str_.empty();
    }

    /**
     * @brief Get length in characters (UTF-8 code points)
     * @return Number of characters
     */
    [[nodiscard]] size_t length() const noexcept {
        return str_.length();
    }

    /**
     * @brief Get size in bytes (actual memory used)
     * @return Number of bytes
     */
    [[nodiscard]] size_t bytes() const noexcept {
        return str_.bytes();
    }

private:
    Glib::ustring str_;
};

} // namespace KeepTower

#endif // SECUREMEMORY_H
