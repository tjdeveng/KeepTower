// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "VaultManager.h"
#include "../utils/Log.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

#ifdef __linux__
#include <sys/mman.h>  // For mlock/munlock
#elif defined(_WIN32)
#include <windows.h>   // For VirtualLock/VirtualUnlock
#endif

using namespace KeepTower;

// EVPCipherContext implementation
EVPCipherContext::EVPCipherContext() : ctx_(EVP_CIPHER_CTX_new()) {}

EVPCipherContext::~EVPCipherContext() {
    if (ctx_) {
        EVP_CIPHER_CTX_free(ctx_);
        ctx_ = nullptr;
    }
}

// VaultManager implementation
VaultManager::VaultManager()
    : m_vault_open(false),
      m_modified(false),
      m_use_reed_solomon(false),
      m_rs_redundancy_percent(10),
      m_pbkdf2_iterations(DEFAULT_PBKDF2_ITERATIONS) {
}

VaultManager::~VaultManager() {
    // Ensure sensitive data is securely erased
    secure_clear(m_encryption_key);
    secure_clear(m_salt);
    (void)close_vault();  // Explicitly ignore return value in destructor
}

bool VaultManager::create_vault(const std::string& path, const Glib::ustring& password) {
    if (m_vault_open) {
        if (!close_vault()) {
            std::cerr << "Warning: Failed to close existing vault" << std::endl;
        }
    }

    // Generate new salt
    m_salt = generate_random_bytes(SALT_LENGTH);

    // Derive encryption key from password
    m_encryption_key.resize(KEY_LENGTH);
    if (!derive_key(password, m_salt, m_encryption_key)) {
        return false;
    }

    // Lock encryption key and salt in memory (prevents swapping to disk)
    if (lock_memory(m_encryption_key)) {
        m_memory_locked = true;
    }
    lock_memory(m_salt);  // Also lock salt

    m_current_vault_path = path;
    m_vault_open = true;
    m_modified = true;

    // Initialize empty vault data
    m_vault_data.Clear();
    m_vault_data.set_version(1);
    m_vault_data.set_last_modified(std::time(nullptr));

    // Save empty vault
    return save_vault();
}

bool VaultManager::open_vault(const std::string& path, const Glib::ustring& password) {
    if (m_vault_open) {
        if (!close_vault()) {
            std::cerr << "Warning: Failed to close existing vault" << std::endl;
        }
    }

    // Read vault file
    std::vector<uint8_t> file_data;
    if (!read_vault_file(path, file_data)) {
        return false;
    }

    // Parse vault file format:
    // Legacy format: [SALT_LENGTH bytes: salt][IV_LENGTH bytes: iv][remaining: encrypted data]
    // RS format:     [SALT_LENGTH bytes: salt][IV_LENGTH bytes: iv][1 byte: flags][1 byte: rs_redundancy][remaining: RS-encoded encrypted data]
    if (file_data.size() < SALT_LENGTH + IV_LENGTH) {
        std::cerr << "Vault file is corrupted or invalid" << std::endl;
        return false;
    }

    // Extract salt
    m_salt.assign(file_data.begin(), file_data.begin() + SALT_LENGTH);

    // Extract IV
    std::vector<uint8_t> iv(file_data.begin() + SALT_LENGTH,
                           file_data.begin() + SALT_LENGTH + IV_LENGTH);

    // Check for Reed-Solomon encoding
    size_t ciphertext_offset = SALT_LENGTH + IV_LENGTH;
    std::vector<uint8_t> ciphertext;

    // Check if there's room for flags byte and RS metadata
    if (file_data.size() > SALT_LENGTH + IV_LENGTH + 6) {  // flags(1) + redundancy(1) + original_size(4)
        uint8_t flags = file_data[SALT_LENGTH + IV_LENGTH];

        // Check if RS flag is set AND redundancy is valid (5-50%)
        // This helps avoid false positives on legacy vaults
        if (flags & FLAG_RS_ENABLED) {
            uint8_t rs_redundancy = file_data[SALT_LENGTH + IV_LENGTH + 1];

            // Validate redundancy is in acceptable range before treating as RS-encoded
            if (rs_redundancy >= 5 && rs_redundancy <= 50) {
                ciphertext_offset += 2;  // Skip flags and redundancy bytes

                // Extract original size (4 bytes, big-endian)
                uint32_t original_size =
                    (static_cast<uint32_t>(file_data[ciphertext_offset]) << 24) |
                    (static_cast<uint32_t>(file_data[ciphertext_offset + 1]) << 16) |
                    (static_cast<uint32_t>(file_data[ciphertext_offset + 2]) << 8) |
                    static_cast<uint32_t>(file_data[ciphertext_offset + 3]);
                ciphertext_offset += 4;

                // Extract RS-encoded data
                std::vector<uint8_t> encoded_data(file_data.begin() + ciphertext_offset,
                                                 file_data.end());

                // Create ReedSolomon instance if needed
                if (!m_reed_solomon || m_rs_redundancy_percent != rs_redundancy) {
                    m_reed_solomon = std::make_unique<ReedSolomon>(rs_redundancy);
                }

                // Decode with Reed-Solomon
                ReedSolomon::EncodedData encoded_struct{
                    .data = encoded_data,
                    .original_size = original_size,
                    .redundancy_percent = rs_redundancy,
                    .block_size = 0,  // Not needed for decode
                    .num_data_blocks = 0,  // Not needed for decode
                    .num_parity_blocks = 0  // Not needed for decode
                };
                auto decode_result = m_reed_solomon->decode(encoded_struct);
                if (!decode_result) {
                    KeepTower::Log::error("Reed-Solomon decoding failed: {}",
                                         ReedSolomon::error_to_string(decode_result.error()));
                    std::cerr << "Failed to decode Reed-Solomon data (file may be too corrupted)" << std::endl;
                    return false;
                }

                ciphertext = std::move(decode_result.value());
                KeepTower::Log::info("Vault decoded with Reed-Solomon ({}% redundancy, {} -> {} bytes)",
                                    rs_redundancy, encoded_data.size(), ciphertext.size());
            } else {
                // Invalid redundancy - treat as legacy format
                ciphertext.assign(file_data.begin() + ciphertext_offset, file_data.end());
            }
        } else {
            // No RS encoding, extract normal ciphertext
            ciphertext.assign(file_data.begin() + ciphertext_offset, file_data.end());
        }
    } else {
        // Legacy format without flags
        ciphertext.assign(file_data.begin() + ciphertext_offset, file_data.end());
    }    // Derive key from password
    m_encryption_key.resize(KEY_LENGTH);
    if (!derive_key(password, m_salt, m_encryption_key)) {
        return false;
    }

    // Lock encryption key and salt in memory (prevents swapping to disk)
    if (lock_memory(m_encryption_key)) {
        m_memory_locked = true;
    }
    lock_memory(m_salt);  // Also lock salt

    // Decrypt data
    std::vector<uint8_t> plaintext;
    if (!decrypt_data(ciphertext, m_encryption_key, iv, plaintext)) {
        std::cerr << "Failed to decrypt vault (wrong password?)" << std::endl;
        return false;
    }

    // Deserialize protobuf data
    m_vault_data.Clear();
    if (!m_vault_data.ParseFromArray(plaintext.data(), plaintext.size())) {
        std::cerr << "Failed to parse vault data (corrupted file?)" << std::endl;
        return false;
    }

    m_current_vault_path = path;
    m_vault_open = true;
    m_modified = false;

    return true;
}

bool VaultManager::save_vault() {
    if (!m_vault_open) {
        return false;
    }

    // Update timestamp
    m_vault_data.set_last_modified(std::time(nullptr));

    // Serialize protobuf to binary
    std::string serialized_data;
    if (!m_vault_data.SerializeToString(&serialized_data)) {
        std::cerr << "Failed to serialize vault data" << std::endl;
        return false;
    }

    std::vector<uint8_t> plaintext(serialized_data.begin(), serialized_data.end());

    // Encrypt data
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> iv = generate_random_bytes(IV_LENGTH);
    if (!encrypt_data(plaintext, m_encryption_key, ciphertext, iv)) {
        return false;
    }

    // Build vault file
    std::vector<uint8_t> file_data;
    file_data.insert(file_data.end(), m_salt.begin(), m_salt.end());
    file_data.insert(file_data.end(), iv.begin(), iv.end());

    // Apply Reed-Solomon encoding if enabled
    if (m_use_reed_solomon) {
        // Ensure ReedSolomon instance exists
        if (!m_reed_solomon) {
            m_reed_solomon = std::make_unique<ReedSolomon>(m_rs_redundancy_percent);
        }

        // Encode the ciphertext with Reed-Solomon
        auto encode_result = m_reed_solomon->encode(ciphertext);
        if (!encode_result) {
            KeepTower::Log::error("Reed-Solomon encoding failed: {}",
                                 ReedSolomon::error_to_string(encode_result.error()));
            return false;
        }

        // Write format: [salt][iv][flags][rs_redundancy][original_size(4 bytes)][encoded_ciphertext]
        uint8_t flags = FLAG_RS_ENABLED;
        file_data.push_back(flags);
        file_data.push_back(m_rs_redundancy_percent);

        // Store original ciphertext size (4 bytes, big-endian)
        uint32_t original_size = ciphertext.size();
        file_data.push_back((original_size >> 24) & 0xFF);
        file_data.push_back((original_size >> 16) & 0xFF);
        file_data.push_back((original_size >> 8) & 0xFF);
        file_data.push_back(original_size & 0xFF);

        const auto& encoded = encode_result.value();
        file_data.insert(file_data.end(), encoded.data.begin(), encoded.data.end());

        KeepTower::Log::info("Vault saved with Reed-Solomon encoding ({}% redundancy, {} -> {} bytes)",
                            m_rs_redundancy_percent, ciphertext.size(), encoded.data.size());
    } else {
        // Original format: [salt][iv][ciphertext]
        file_data.insert(file_data.end(), ciphertext.begin(), ciphertext.end());
    }

    // Create backup before saving (non-fatal if it fails)
    auto backup_result = create_backup(m_current_vault_path);
    if (!backup_result) {
        KeepTower::Log::warning("Failed to create backup: {}", static_cast<int>(backup_result.error()));
    }

    if (!write_vault_file(m_current_vault_path, file_data)) {
        return false;
    }

    m_modified = false;
    return true;
}


bool VaultManager::close_vault() {
    if (!m_vault_open) {
        return true;
    }

    // Securely clear sensitive data
    secure_clear(m_encryption_key);
    secure_clear(m_salt);
    m_vault_data.Clear();
    m_current_vault_path.clear();

    m_vault_open = false;
    m_modified = false;

    return true;
}

bool VaultManager::add_account(const keeptower::AccountRecord& account) {
    if (!m_vault_open) {
        return false;
    }

    auto* new_account = m_vault_data.add_accounts();
    new_account->CopyFrom(account);
    m_modified = true;
    return true;
}

std::vector<keeptower::AccountRecord> VaultManager::get_all_accounts() const {
    std::vector<keeptower::AccountRecord> accounts;
    for (int i = 0; i < m_vault_data.accounts_size(); i++) {
        accounts.push_back(m_vault_data.accounts(i));
    }
    return accounts;
}

bool VaultManager::update_account(size_t index, const keeptower::AccountRecord& account) {
    if (!m_vault_open || index >= static_cast<size_t>(m_vault_data.accounts_size())) {
        return false;
    }

    m_vault_data.mutable_accounts(index)->CopyFrom(account);
    m_modified = true;
    return true;
}

bool VaultManager::delete_account(size_t index) {
    if (!m_vault_open || index >= static_cast<size_t>(m_vault_data.accounts_size())) {
        return false;
    }

    // Remove account by shifting
    auto* accounts = m_vault_data.mutable_accounts();
    accounts->erase(accounts->begin() + index);
    m_modified = true;
    return true;
}

keeptower::AccountRecord* VaultManager::get_account_mutable(size_t index) {
    if (!m_vault_open || index >= static_cast<size_t>(m_vault_data.accounts_size())) {
        return nullptr;
    }
    return m_vault_data.mutable_accounts(index);
}

const keeptower::AccountRecord* VaultManager::get_account(size_t index) const {
    if (!m_vault_open || index >= static_cast<size_t>(m_vault_data.accounts_size())) {
        return nullptr;
    }
    return &m_vault_data.accounts(index);
}

size_t VaultManager::get_account_count() const {
    return m_vault_data.accounts_size();
}

bool VaultManager::derive_key(const Glib::ustring& password,
                              std::span<const uint8_t> salt,
                              std::vector<uint8_t>& key) {
    // Use PBKDF2 with SHA-256 (NIST recommended)
    int result = PKCS5_PBKDF2_HMAC(
        password.c_str(), password.bytes(),
        salt.data(), salt.size(),
        m_pbkdf2_iterations,
        EVP_sha256(),
        KEY_LENGTH,
        key.data()
    );

    return result == 1;
}

bool VaultManager::encrypt_data(std::span<const uint8_t> plaintext,
                                std::span<const uint8_t> key,
                                std::vector<uint8_t>& ciphertext,
                                std::vector<uint8_t>& iv) {
    EVPCipherContext ctx;
    if (!ctx.is_valid()) {
        return false;
    }

    // Initialize encryption with AES-256-GCM
    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, key.data(), iv.data()) != 1) {
        return false;
    }

    // Allocate output buffer
    ciphertext.resize(plaintext.size() + EVP_CIPHER_block_size(EVP_aes_256_gcm()) + 16);
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

    // Get authentication tag (GCM)
    std::vector<uint8_t> tag(16);
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, 16, tag.data()) != 1) {
        return false;
    }

    // Append tag to ciphertext
    ciphertext.resize(ciphertext_len);
    ciphertext.insert(ciphertext.end(), tag.begin(), tag.end());

    return true;
}

bool VaultManager::decrypt_data(std::span<const uint8_t> ciphertext,
                                std::span<const uint8_t> key,
                                std::span<const uint8_t> iv,
                                std::vector<uint8_t>& plaintext) {
    if (ciphertext.size() < 16) {
        return false;
    }

    // Extract authentication tag (last 16 bytes)
    std::vector<uint8_t> tag(ciphertext.end() - 16, ciphertext.end());
    std::vector<uint8_t> actual_ciphertext(ciphertext.begin(), ciphertext.end() - 16);

    EVPCipherContext ctx;
    if (!ctx.is_valid()) {
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
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, 16, tag.data()) != 1) {
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

bool VaultManager::read_vault_file(const std::string& path, std::vector<uint8_t>& data) {
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open vault file: " << path << std::endl;
            return false;
        }

        file.seekg(0, std::ios::end);
        std::streamsize size = file.tellg();

        if (size < 0) {
            std::cerr << "Failed to determine vault file size" << std::endl;
            return false;
        }

        file.seekg(0, std::ios::beg);

        // Check if file has the new format with magic header
        constexpr size_t HEADER_SIZE = sizeof(uint32_t) * 3;  // magic + version + iterations
        bool has_header = false;
        uint32_t iterations = DEFAULT_PBKDF2_ITERATIONS;

        if (size >= static_cast<std::streamsize>(HEADER_SIZE)) {
            uint32_t magic, version;
            file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
            file.read(reinterpret_cast<char*>(&version), sizeof(version));
            file.read(reinterpret_cast<char*>(&iterations), sizeof(iterations));

            if (magic == VAULT_MAGIC) {
                has_header = true;
                m_pbkdf2_iterations = static_cast<int>(iterations);
                KeepTower::Log::info("Vault format version {}, {} PBKDF2 iterations", version, iterations);

                // Adjust size to exclude header
                size -= HEADER_SIZE;
            } else {
                // Not a new format, rewind to beginning
                KeepTower::Log::info("Legacy vault format detected (no header)");
                file.seekg(0, std::ios::beg);
                m_pbkdf2_iterations = DEFAULT_PBKDF2_ITERATIONS;
            }
        }

        data.resize(static_cast<size_t>(size));
        file.read(reinterpret_cast<char*>(data.data()), size);

        if (!file.good() && !file.eof()) {
            std::cerr << "Error reading vault file" << std::endl;
            return false;
        }

        file.close();
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Exception reading vault file: " << e.what() << std::endl;
        return false;
    }
}

bool VaultManager::write_vault_file(const std::string& path, const std::vector<uint8_t>& data) {
    namespace fs = std::filesystem;
    const std::string temp_path = path + ".tmp";

    try {
        // Write to temporary file
        {
            std::ofstream file(temp_path, std::ios::binary | std::ios::trunc);
            if (!file) {
                std::cerr << "Failed to create temporary vault file" << std::endl;
                return false;
            }

            // Write vault file format header
            uint32_t magic = VAULT_MAGIC;
            uint32_t version = VAULT_VERSION;
            uint32_t iterations = static_cast<uint32_t>(m_pbkdf2_iterations);

            file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
            file.write(reinterpret_cast<const char*>(&version), sizeof(version));
            file.write(reinterpret_cast<const char*>(&iterations), sizeof(iterations));

            // Write encrypted vault data
            file.write(reinterpret_cast<const char*>(data.data()), data.size());
            file.flush();

            if (!file.good()) {
                std::cerr << "Failed to write vault data" << std::endl;
                return false;
            }
        }  // Close file before rename

        // Atomic rename (POSIX guarantees atomicity)
        fs::rename(temp_path, path);

        // Set secure file permissions (owner read/write only)
        fs::permissions(path,
            fs::perms::owner_read | fs::perms::owner_write,
            fs::perm_options::replace);

        return true;

    } catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
        try {
            fs::remove(temp_path);
        } catch (...) {}
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error writing vault: " << e.what() << std::endl;
        try {
            fs::remove(temp_path);
        } catch (...) {}
        return false;
    }
}

std::vector<uint8_t> VaultManager::generate_random_bytes(size_t length) {
    std::vector<uint8_t> bytes(length);
    RAND_bytes(bytes.data(), length);
    return bytes;
}

void VaultManager::secure_clear(std::vector<uint8_t>& data) {
    if (!data.empty()) {
        // Unlock memory before clearing if it was locked
        if (m_memory_locked) {
            unlock_memory(data);
        }
        OPENSSL_cleanse(data.data(), data.size());
        data.clear();
        data.shrink_to_fit();  // Release memory
    }
}

void VaultManager::secure_clear(std::string& data) {
    if (!data.empty()) {
        OPENSSL_cleanse(const_cast<char*>(data.data()), data.size());
        data.clear();
        data.shrink_to_fit();
    }
}

bool VaultManager::lock_memory(std::vector<uint8_t>& data) {
    if (data.empty()) {
        return true;
    }

#ifdef __linux__
    // Lock memory to prevent swapping to disk
    if (mlock(data.data(), data.size()) == 0) {
        KeepTower::Log::debug("Locked {} bytes of sensitive memory", data.size());
        return true;
    } else {
        KeepTower::Log::warning("Failed to lock memory: {} ({})", std::strerror(errno), errno);
        // Not a fatal error - continue without locking
        return false;
    }
#elif _WIN32
    if (VirtualLock(data.data(), data.size())) {
        KeepTower::Log::debug("Locked {} bytes of sensitive memory", data.size());
        return true;
    } else {
        KeepTower::Log::warning("Failed to lock memory: error {}", GetLastError());
        return false;
    }
#else
    KeepTower::Log::debug("Memory locking not supported on this platform");
    return false;
#endif
}

void VaultManager::unlock_memory(std::vector<uint8_t>& data) {
    if (data.empty()) {
        return;
    }

#ifdef __linux__
    if (munlock(data.data(), data.size()) == 0) {
        KeepTower::Log::debug("Unlocked {} bytes of memory", data.size());
    }
#elif _WIN32
    VirtualUnlock(data.data(), data.size());
    KeepTower::Log::debug("Unlocked {} bytes of memory", data.size());
#endif
}

KeepTower::VaultResult<> VaultManager::create_backup(std::string_view path) {
    namespace fs = std::filesystem;
    std::string path_str(path);
    std::string backup_path = path_str + ".backup";

    try {
        if (fs::exists(path_str)) {
            fs::copy_file(path_str, backup_path, fs::copy_options::overwrite_existing);
            KeepTower::Log::info("Created backup: {}", backup_path);
            return {};
        }
        return {};  // No file to backup
    } catch (const fs::filesystem_error& e) {
        KeepTower::Log::warning("Failed to create backup: {}", e.what());
        // Don't fail the operation if backup fails
        return {};
    }
}

KeepTower::VaultResult<> VaultManager::restore_from_backup(std::string_view path) {
    namespace fs = std::filesystem;
    std::string path_str(path);
    std::string backup_path = path_str + ".backup";

    try {
        if (fs::exists(backup_path)) {
            fs::copy_file(backup_path, path_str, fs::copy_options::overwrite_existing);
            KeepTower::Log::info("Restored from backup: {}", backup_path);
            return {};
        } else {
            KeepTower::Log::error("Backup file not found: {}", backup_path);
            return std::unexpected(VaultError::FileNotFound);
        }
    } catch (const fs::filesystem_error& e) {
        KeepTower::Log::error("Failed to restore backup: {}", e.what());
        return std::unexpected(VaultError::FileReadFailed);
    }
}

bool VaultManager::set_rs_redundancy_percent(uint8_t percent) {
    if (percent < 5 || percent > 50) {
        return false;
    }
    m_rs_redundancy_percent = percent;
    if (m_reed_solomon) {
        m_reed_solomon = std::make_unique<ReedSolomon>(m_rs_redundancy_percent);
    }
    return true;
}
