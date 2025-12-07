// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#ifndef VAULTMANAGER_H
#define VAULTMANAGER_H

#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <memory>
#include <expected>
#include <glibmm.h>
#include "record.pb.h"
#include "VaultError.h"

// Forward declarations for OpenSSL types
typedef struct evp_cipher_ctx_st EVP_CIPHER_CTX;

// RAII wrapper for OpenSSL cipher context
class EVPCipherContext {
public:
    EVPCipherContext();
    ~EVPCipherContext();

    // Delete copy and move operations
    EVPCipherContext(const EVPCipherContext&) = delete;
    EVPCipherContext& operator=(const EVPCipherContext&) = delete;
    EVPCipherContext(EVPCipherContext&&) = delete;
    EVPCipherContext& operator=(EVPCipherContext&&) = delete;

    EVP_CIPHER_CTX* get() noexcept { return ctx_; }
    [[nodiscard]] bool is_valid() const noexcept { return ctx_ != nullptr; }

private:
    EVP_CIPHER_CTX* ctx_;
};

class VaultManager {
public:
    VaultManager();
    ~VaultManager();

    // Vault operations
    [[nodiscard]] bool create_vault(const std::string& path, const Glib::ustring& password);
    [[nodiscard]] bool open_vault(const std::string& path, const Glib::ustring& password);
    [[nodiscard]] bool save_vault();
    [[nodiscard]] bool close_vault();

    // Account operations
    [[nodiscard]] bool add_account(const keeptower::AccountRecord& account);
    [[nodiscard]] std::vector<keeptower::AccountRecord> get_all_accounts() const;
    [[nodiscard]] bool update_account(size_t index, const keeptower::AccountRecord& account);
    [[nodiscard]] bool delete_account(size_t index);
    const keeptower::AccountRecord* get_account(size_t index) const;
    keeptower::AccountRecord* get_account_mutable(size_t index);
    [[nodiscard]] size_t get_account_count() const;

    // State queries
    bool is_vault_open() const { return m_vault_open; }
    std::string get_current_vault_path() const { return m_current_vault_path; }
    bool is_modified() const { return m_modified; }

private:
    // Cryptographic operations
    bool derive_key(const Glib::ustring& password,
                    std::span<const uint8_t> salt,
                    std::vector<uint8_t>& key);

    bool encrypt_data(std::span<const uint8_t> plaintext,
                      std::span<const uint8_t> key,
                      std::vector<uint8_t>& ciphertext,
                      std::vector<uint8_t>& iv);

    bool decrypt_data(std::span<const uint8_t> ciphertext,
                      std::span<const uint8_t> key,
                      std::span<const uint8_t> iv,
                      std::vector<uint8_t>& plaintext);

    // File I/O
    bool read_vault_file(const std::string& path, std::vector<uint8_t>& data);
    bool write_vault_file(const std::string& path, const std::vector<uint8_t>& data);

    // Generate random bytes for salt and IV
    std::vector<uint8_t> generate_random_bytes(size_t length);

    // Secure memory clearing and locking
    void secure_clear(std::vector<uint8_t>& data);
    void secure_clear(std::string& data);
    bool lock_memory(std::vector<uint8_t>& data);
    void unlock_memory(std::vector<uint8_t>& data);

    // Backup management
    KeepTower::VaultResult<> create_backup(std::string_view path);
    KeepTower::VaultResult<> restore_from_backup(std::string_view path);

    // State
    bool m_vault_open;
    bool m_modified;
    std::string m_current_vault_path;
    std::vector<uint8_t> m_encryption_key;
    std::vector<uint8_t> m_salt;
    bool m_memory_locked;  // Track if sensitive memory is locked

    // In-memory vault data (protobuf)
    keeptower::VaultData m_vault_data;

    // Vault file format constants
    static constexpr uint32_t VAULT_MAGIC = 0x4B505457;  // "KPTW" (KeepTower)
    static constexpr uint32_t VAULT_VERSION = 1;

    // Constants for encryption (AES-256-GCM with PBKDF2)
    static constexpr size_t SALT_LENGTH = 32;
    static constexpr size_t KEY_LENGTH = 32;  // 256 bits
    static constexpr size_t IV_LENGTH = 12;   // GCM recommended
    static constexpr int DEFAULT_PBKDF2_ITERATIONS = 100000;  // NIST recommendation

    // Current vault PBKDF2 iterations (configurable per vault)
    int m_pbkdf2_iterations;
};

#endif // VAULTMANAGER_H
