// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "VaultManager.h"
#include "../utils/Log.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/provider.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <random>

#ifdef __linux__
#include <sys/mman.h>  // For mlock/munlock
#include <sys/stat.h>  // For chmod
#include <fcntl.h>     // For open()
#include <unistd.h>    // For fsync(), close()
#elif defined(_WIN32)
#include <windows.h>   // For VirtualLock/VirtualUnlock
#endif

using namespace KeepTower;

// FIPS mode state initialization
std::atomic<bool> VaultManager::s_fips_mode_initialized{false};
std::atomic<bool> VaultManager::s_fips_mode_available{false};
std::atomic<bool> VaultManager::s_fips_mode_enabled{false};

// EVPCipherContext implementation
EVPCipherContext::EVPCipherContext() : ctx_(EVP_CIPHER_CTX_new()) {}

EVPCipherContext::~EVPCipherContext() {
    if (ctx_ != nullptr) {
        EVP_CIPHER_CTX_free(ctx_);
        ctx_ = nullptr;
    }
}

// VaultManager implementation
VaultManager::VaultManager()
    : m_vault_open(false),
      m_modified(false),
      m_use_reed_solomon(false),
      m_rs_redundancy_percent(DEFAULT_RS_REDUNDANCY),
      m_fec_loaded_from_file(false),
      m_backup_enabled(true),
      m_backup_count(DEFAULT_BACKUP_COUNT),
      m_memory_locked(false),
      m_yubikey_required(false),
      m_pbkdf2_iterations(DEFAULT_PBKDF2_ITERATIONS) {
}

VaultManager::~VaultManager() {
    // Ensure sensitive data is securely erased
    secure_clear(m_encryption_key);
    secure_clear(m_salt);
    secure_clear(m_yubikey_challenge);
    (void)close_vault();  // Explicitly ignore return value in destructor
}

bool VaultManager::create_vault(const std::string& path,
                                 const Glib::ustring& password,
                                 bool require_yubikey,
                                 std::string yubikey_serial) {
    if (m_vault_open) {
        if (!close_vault()) {
            std::cerr << "Warning: Failed to close existing vault" << std::endl;
        }
    }

    // Generate new salt
    m_salt = generate_random_bytes(SALT_LENGTH);

    // Derive base encryption key from password
    std::vector<uint8_t> password_key(KEY_LENGTH);
    if (!derive_key(password, m_salt, password_key)) {
        return false;
    }

    // Handle YubiKey integration if requested
    m_yubikey_required = require_yubikey;
    if (require_yubikey) {
#ifdef HAVE_YUBIKEY_SUPPORT
        // Generate random challenge for this vault
        m_yubikey_challenge = generate_random_bytes(YUBIKEY_CHALLENGE_SIZE);

        // Get YubiKey response
        YubiKeyManager yk_manager;
        if (!yk_manager.initialize()) {
            std::cerr << "Failed to initialize YubiKey" << std::endl;
            secure_clear(password_key);
            return false;
        }

        auto response = yk_manager.challenge_response(
            std::span<const unsigned char>(m_yubikey_challenge.data(), m_yubikey_challenge.size()),
            false,  // don't require touch for vault operations
            YUBIKEY_TIMEOUT_MS
        );

        if (!response.success) {
            std::cerr << "YubiKey challenge-response failed: " << response.error_message << std::endl;
            secure_clear(password_key);
            return false;
        }

        // Store YubiKey serial if not provided
        if (yubikey_serial.empty()) {
            auto device_info = yk_manager.get_device_info();
            if (device_info) {
                m_yubikey_serial = device_info->serial_number;
            }
        } else {
            m_yubikey_serial = yubikey_serial;
        }

        // Derive final key: XOR password-derived key with YubiKey response
        // This provides two-factor security: password + physical YubiKey required
        m_encryption_key.resize(KEY_LENGTH);
        const size_t xor_length = std::min(KEY_LENGTH, YUBIKEY_RESPONSE_SIZE);
        for (size_t i = 0; i < xor_length; ++i) {
            m_encryption_key[i] = password_key[i] ^ response.response[i];
        }
        // Copy remaining bytes if YUBIKEY_RESPONSE_SIZE < KEY_LENGTH
        if (YUBIKEY_RESPONSE_SIZE < KEY_LENGTH) {
            std::copy(password_key.begin() + YUBIKEY_RESPONSE_SIZE,
                     password_key.end(),
                     m_encryption_key.begin() + YUBIKEY_RESPONSE_SIZE);
        }

        KeepTower::Log::info("YubiKey-protected vault created with serial: {}", m_yubikey_serial);
#else
        std::cerr << "YubiKey support not compiled in" << std::endl;
        secure_clear(password_key);
        return false;
#endif
    } else {
        // No YubiKey: use password-derived key directly
        m_encryption_key = std::move(password_key);
    }

    // Clear password-derived key (either moved or no longer needed)
    secure_clear(password_key);

    // Lock encryption key and salt in memory (prevents swapping to disk)
    if (lock_memory(m_encryption_key)) {
        m_memory_locked = true;
    }
    lock_memory(m_salt);
    if (m_yubikey_required) {
        lock_memory(m_yubikey_challenge);
    }

    m_current_vault_path = path;
    m_vault_open = true;
    m_modified = true;

    // Initialize empty vault data
    m_vault_data.Clear();

    // Initialize vault metadata
    auto* metadata = m_vault_data.mutable_metadata();
    metadata->set_schema_version(2);  // Version 2: Extended schema
    metadata->set_created_at(std::time(nullptr));
    metadata->set_last_modified(std::time(nullptr));
    metadata->set_last_accessed(std::time(nullptr));
    metadata->set_access_count(0);

#ifdef HAVE_YUBIKEY_SUPPORT
    // Store YubiKey configuration in protobuf if enabled
    if (m_yubikey_required) {
        auto* yk_config = m_vault_data.mutable_yubikey_config();
        yk_config->set_required(true);
        yk_config->set_challenge(m_yubikey_challenge.data(), m_yubikey_challenge.size());
        yk_config->set_configured_at(std::time(nullptr));

        // Add primary key to entries list
        auto* entry = yk_config->add_yubikey_entries();
        entry->set_serial(m_yubikey_serial);
        entry->set_name("Primary");
        entry->set_added_at(std::time(nullptr));
    }
#endif

    // Save empty vault
    return save_vault();
}

bool VaultManager::check_vault_requires_yubikey(const std::string& path, std::string& serial) {
    // Read vault file
    std::vector<uint8_t> file_data;
    if (!read_vault_file(path, file_data)) {
        return false;
    }

    // Check minimum size for flags
    if (file_data.size() < SALT_LENGTH + IV_LENGTH + 1) {
        return false;
    }

    // Read flags byte
    uint8_t flags = file_data[SALT_LENGTH + IV_LENGTH];

    if (!(flags & FLAG_YUBIKEY_REQUIRED)) {
        return false;
    }

    // YubiKey is required, try to read serial number
    // Skip past salt, IV, flags, and RS metadata if present
    size_t offset = SALT_LENGTH + IV_LENGTH + 1;

    // Check if RS is enabled
    if (flags & FLAG_RS_ENABLED) {
        // Skip RS metadata: redundancy (1 byte) + original_size (4 bytes)
        offset += (VAULT_HEADER_SIZE - 1);  // 6 - 1 (flags byte already skipped)
    }

    // Check if we have enough data for YubiKey metadata
    if (offset + 1 > file_data.size()) {
        return true;  // YubiKey required but no serial available
    }

    // Read YubiKey serial
    uint8_t serial_len = file_data[offset++];
    if (offset + serial_len <= file_data.size()) {
        serial.assign(file_data.begin() + offset, file_data.begin() + offset + serial_len);
    }

    return true;
}

// ============================================================================
// Helper functions for open_vault() - Refactored for reduced complexity
// ============================================================================

KeepTower::VaultResult<VaultManager::ParsedVaultData>
VaultManager::parse_vault_format(const std::vector<uint8_t>& file_data) {
    ParsedVaultData result;

    // Validate minimum file size
    if (file_data.size() < SALT_LENGTH + IV_LENGTH) {
        return std::unexpected(VaultError::CorruptedFile);
    }

    // Extract salt
    result.metadata.salt.assign(file_data.begin(), file_data.begin() + SALT_LENGTH);

    // Extract IV
    result.metadata.iv.assign(
        file_data.begin() + SALT_LENGTH,
        file_data.begin() + SALT_LENGTH + IV_LENGTH);

    size_t ciphertext_offset = SALT_LENGTH + IV_LENGTH;

    // Check for flags byte and extended format
    if (file_data.size() > SALT_LENGTH + IV_LENGTH + VAULT_HEADER_SIZE) {
        uint8_t flags = file_data[SALT_LENGTH + IV_LENGTH];

        // Check for YubiKey requirement
        bool yubikey_required = (flags & FLAG_YUBIKEY_REQUIRED);
        result.metadata.requires_yubikey = yubikey_required;

        // Check for Reed-Solomon encoding
        if (flags & FLAG_RS_ENABLED) {
            uint8_t rs_redundancy = file_data[SALT_LENGTH + IV_LENGTH + 1];

            // Validate redundancy is in acceptable range
            if (rs_redundancy >= MIN_RS_REDUNDANCY && rs_redundancy <= MAX_RS_REDUNDANCY) {
                // Extract original size (4 bytes, big-endian)
                uint32_t original_size =
                    (static_cast<uint32_t>(file_data[SALT_LENGTH + IV_LENGTH + 2]) << BIGENDIAN_SHIFT_24) |
                    (static_cast<uint32_t>(file_data[SALT_LENGTH + IV_LENGTH + 3]) << BIGENDIAN_SHIFT_16) |
                    (static_cast<uint32_t>(file_data[SALT_LENGTH + IV_LENGTH + 4]) << BIGENDIAN_SHIFT_8) |
                    static_cast<uint32_t>(file_data[SALT_LENGTH + IV_LENGTH + 5]);

                size_t data_offset = SALT_LENGTH + IV_LENGTH + VAULT_HEADER_SIZE;

                // Account for YubiKey metadata if present
                size_t yk_metadata_size = 0;
                if (yubikey_required && data_offset < file_data.size()) {
                    uint8_t serial_len = file_data[data_offset];
                    yk_metadata_size = 1 + serial_len + YUBIKEY_CHALLENGE_SIZE;
                }

                size_t encoded_size = file_data.size() - data_offset - yk_metadata_size;

                // Validate original size is reasonable
                if (original_size > 0 &&
                    original_size < MAX_VAULT_SIZE &&
                    original_size <= encoded_size) {

                    result.metadata.has_fec = true;
                    result.metadata.fec_redundancy = rs_redundancy;
                    ciphertext_offset += VAULT_HEADER_SIZE;  // Skip flags, redundancy, and original_size

                    // Read YubiKey metadata if required (comes BEFORE RS-encoded data)
                    if (yubikey_required && ciphertext_offset < file_data.size()) {
                        uint8_t serial_len = file_data[ciphertext_offset++];
                        result.metadata.yubikey_serial.assign(
                            file_data.begin() + ciphertext_offset,
                            file_data.begin() + ciphertext_offset + serial_len);
                        ciphertext_offset += serial_len;
                        result.metadata.yubikey_challenge.assign(
                            file_data.begin() + ciphertext_offset,
                            file_data.begin() + ciphertext_offset + YUBIKEY_CHALLENGE_SIZE);
                        ciphertext_offset += YUBIKEY_CHALLENGE_SIZE;
                    }

                    // Extract RS-encoded data
                    std::vector<uint8_t> encoded_data(
                        file_data.begin() + ciphertext_offset,
                        file_data.end());

                    // Decode with Reed-Solomon
                    auto decode_result = decode_with_reed_solomon(encoded_data, original_size, rs_redundancy);
                    if (!decode_result) {
                        return std::unexpected(decode_result.error());
                    }
                    result.ciphertext = std::move(decode_result.value());

                    KeepTower::Log::info("Vault decoded with Reed-Solomon ({}% redundancy, {} -> {} bytes)",
                                        rs_redundancy, encoded_data.size(), result.ciphertext.size());
                } else {
                    // Invalid size ratio - treat as legacy format
                    result.ciphertext.assign(file_data.begin() + (SALT_LENGTH + IV_LENGTH), file_data.end());
                }
            } else {
                // Invalid redundancy - treat as legacy format
                result.ciphertext.assign(file_data.begin() + ciphertext_offset, file_data.end());
            }
        } else {
            // No RS encoding, extract normal ciphertext
            ciphertext_offset += 1;  // Skip flags byte

            // Read YubiKey metadata if required (after flags byte)
            if (yubikey_required && ciphertext_offset < file_data.size()) {
                uint8_t serial_len = file_data[ciphertext_offset++];
                result.metadata.yubikey_serial.assign(
                    file_data.begin() + ciphertext_offset,
                    file_data.begin() + ciphertext_offset + serial_len);
                ciphertext_offset += serial_len;
                result.metadata.yubikey_challenge.assign(
                    file_data.begin() + ciphertext_offset,
                    file_data.begin() + ciphertext_offset + YUBIKEY_CHALLENGE_SIZE);
                ciphertext_offset += YUBIKEY_CHALLENGE_SIZE;
            }

            result.ciphertext.assign(file_data.begin() + ciphertext_offset, file_data.end());
        }
    } else {
        // Legacy format without flags
        result.ciphertext.assign(file_data.begin() + ciphertext_offset, file_data.end());
    }

    return result;
}

KeepTower::VaultResult<std::vector<uint8_t>>
VaultManager::decode_with_reed_solomon(
    const std::vector<uint8_t>& encoded_data,
    uint32_t original_size,
    uint8_t redundancy) {

    // Create ReedSolomon instance if needed
    if (!m_reed_solomon || m_rs_redundancy_percent != redundancy) {
        m_reed_solomon = std::make_unique<ReedSolomon>(redundancy);
    }

    // Decode with Reed-Solomon
    ReedSolomon::EncodedData encoded_struct{
        .data = encoded_data,
        .original_size = original_size,
        .redundancy_percent = redundancy,
        .block_size = 0,  // Not needed for decode
        .num_data_blocks = 0,  // Not needed for decode
        .num_parity_blocks = 0  // Not needed for decode
    };

    auto decode_result = m_reed_solomon->decode(encoded_struct);
    if (!decode_result) {
        KeepTower::Log::error("Reed-Solomon decoding failed: {}",
                             ReedSolomon::error_to_string(decode_result.error()));
        return std::unexpected(VaultError::DecodingFailed);
    }

    return decode_result.value();
}

#ifdef HAVE_YUBIKEY_SUPPORT
KeepTower::VaultResult<>
VaultManager::authenticate_yubikey(
    const VaultFileMetadata& metadata,
    std::vector<uint8_t>& encryption_key) {

    if (metadata.yubikey_challenge.empty() || metadata.yubikey_serial.empty()) {
        return std::unexpected(VaultError::YubiKeyMetadataMissing);
    }

    KeepTower::Log::info("Vault requires YubiKey authentication (serial: {})", metadata.yubikey_serial);

    YubiKeyManager yk_manager;
    if (!yk_manager.initialize() || !yk_manager.is_yubikey_present()) {
        return std::unexpected(VaultError::YubiKeyNotConnected);
    }

    // Get device info to check serial
    auto device_info = yk_manager.get_device_info();
    if (!device_info) {
        return std::unexpected(VaultError::YubiKeyDeviceInfoFailed);
    }

    // Check if this YubiKey's serial is authorized
    const bool key_authorized = is_yubikey_authorized(device_info->serial_number);

    if (!key_authorized) {
        KeepTower::Log::warning("YubiKey serial mismatch: expected {}, found {}",
                               metadata.yubikey_serial, device_info->serial_number);
        // For backward compatibility, allow legacy single-key vaults
        if (metadata.yubikey_serial != device_info->serial_number) {
            KeepTower::Log::error("Unauthorized YubiKey");
            return std::unexpected(VaultError::YubiKeyUnauthorized);
        }
    }

    // Perform challenge-response
    auto response = yk_manager.challenge_response(metadata.yubikey_challenge, 2);
    if (!response.success) {
        KeepTower::Log::error("YubiKey challenge-response failed: {}", response.error_message);
        return std::unexpected(VaultError::YubiKeyChallengeResponseFailed);
    }

    KeepTower::Log::info("YubiKey challenge-response successful");

    // XOR password-derived key with YubiKey response
    for (size_t i = 0; i < KEY_LENGTH && i < YUBIKEY_RESPONSE_SIZE; ++i) {
        encryption_key[i] ^= response.response[i];
    }

    // Store YubiKey data for save operations
    m_yubikey_required = true;
    m_yubikey_serial = metadata.yubikey_serial;
    m_yubikey_challenge = metadata.yubikey_challenge;
    lock_memory(m_yubikey_challenge);

    return {};
}
#endif

KeepTower::VaultResult<keeptower::VaultData>
VaultManager::decrypt_and_parse_vault(
    const std::vector<uint8_t>& ciphertext,
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& iv) {

    // Decrypt data
    std::vector<uint8_t> plaintext;
    if (!decrypt_data(ciphertext, key, iv, plaintext)) {
        return std::unexpected(VaultError::DecryptionFailed);
    }

    // Deserialize protobuf data
    keeptower::VaultData vault_data;
    if (!vault_data.ParseFromArray(plaintext.data(), plaintext.size())) {
        return std::unexpected(VaultError::InvalidProtobuf);
    }

    return vault_data;
}

// ============================================================================
// Refactored open_vault() - Simplified using helper functions
// ============================================================================

bool VaultManager::open_vault(const std::string& path, const Glib::ustring& password) {
    // 1. Close existing vault if needed
    if (m_vault_open && !close_vault()) {
        std::cerr << "Warning: Failed to close existing vault" << std::endl;
    }

    // 2. Read vault file
    std::vector<uint8_t> file_data;
    if (!read_vault_file(path, file_data)) {
        return false;
    }

    // 3. Parse vault format and extract metadata
    auto parsed_result = parse_vault_format(file_data);
    if (!parsed_result) {
        std::cerr << "Failed to parse vault format: "
                  << static_cast<int>(parsed_result.error()) << std::endl;
        return false;
    }

    ParsedVaultData parsed_data = std::move(parsed_result.value());
    VaultFileMetadata& metadata = parsed_data.metadata;
    std::vector<uint8_t>& ciphertext = parsed_data.ciphertext;

    // 4. Derive encryption key from password
    m_encryption_key.resize(KEY_LENGTH);
    m_salt = metadata.salt;
    if (!derive_key(password, m_salt, m_encryption_key)) {
        return false;
    }

    // 5. Authenticate with YubiKey if required
#ifdef HAVE_YUBIKEY_SUPPORT
    if (metadata.requires_yubikey) {
        auto yk_result = authenticate_yubikey(metadata, m_encryption_key);
        if (!yk_result) {
            std::cerr << "YubiKey authentication failed: "
                      << static_cast<int>(yk_result.error()) << std::endl;
            secure_clear(m_encryption_key);
            return false;
        }
    }
#else
    if (metadata.requires_yubikey) {
        std::cerr << "Vault requires YubiKey but YubiKey support is not compiled in" << std::endl;
        return false;
    }
#endif

    // 6. Lock sensitive memory (prevents swapping to disk)
    if (lock_memory(m_encryption_key)) {
        m_memory_locked = true;
    }
    lock_memory(m_salt);

    // 7. Decrypt and parse vault data
    auto vault_result = decrypt_and_parse_vault(ciphertext, m_encryption_key, metadata.iv);
    if (!vault_result) {
        std::cerr << "Failed to decrypt/parse vault (wrong password?): "
                  << static_cast<int>(vault_result.error()) << std::endl;
        return false;
    }

    // 8. Store vault data and metadata
    m_vault_data = std::move(vault_result.value());

    // Migrate old schema if needed
    if (!migrate_vault_schema()) {
        std::cerr << "Failed to migrate vault schema" << std::endl;
        return false;
    }

    // 9. Preserve FEC settings from file
    m_use_reed_solomon = metadata.has_fec;
    // Only update redundancy if FEC was enabled in file
    if (metadata.has_fec && metadata.fec_redundancy > 0) {
        m_rs_redundancy_percent = metadata.fec_redundancy;
    }
    if (metadata.has_fec) {
        KeepTower::Log::info("Preserved FEC settings from file: enabled=true, redundancy={}%",
                            metadata.fec_redundancy);
    } else {
        KeepTower::Log::info("Preserved FEC settings from file: enabled=false");
    }

    // 10. Update vault state
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
    auto* metadata = m_vault_data.mutable_metadata();
    metadata->set_last_modified(std::time(nullptr));

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

    // Prepare flags byte
    uint8_t flags = 0;
    if (m_use_reed_solomon) {
        flags |= FLAG_RS_ENABLED;
    }
    if (m_yubikey_required) {
        flags |= FLAG_YUBIKEY_REQUIRED;
    }
    file_data.push_back(flags);

    // Write Reed-Solomon metadata if enabled
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

        // Write RS metadata: [rs_redundancy][original_size(4 bytes)]
        file_data.push_back(m_rs_redundancy_percent);

        // Store original ciphertext size (4 bytes, big-endian)
        uint32_t original_size = ciphertext.size();
        file_data.push_back((original_size >> 24) & 0xFF);
        file_data.push_back((original_size >> 16) & 0xFF);
        file_data.push_back((original_size >> 8) & 0xFF);
        file_data.push_back(original_size & 0xFF);

        const auto& encoded = encode_result.value();

        // Write YubiKey metadata before encoded data if required
        if (m_yubikey_required) {
            // Write: [serial_len(1 byte)][serial][challenge(64 bytes)]
            uint8_t serial_len = static_cast<uint8_t>(std::min(m_yubikey_serial.length(), size_t{255}));
            file_data.push_back(serial_len);
            file_data.insert(file_data.end(), m_yubikey_serial.begin(),
                           m_yubikey_serial.begin() + serial_len);
            file_data.insert(file_data.end(), m_yubikey_challenge.begin(), m_yubikey_challenge.end());
        }

        file_data.insert(file_data.end(), encoded.data.begin(), encoded.data.end());

        KeepTower::Log::info("Vault saved with Reed-Solomon encoding ({}% redundancy, {} -> {} bytes)",
                            m_rs_redundancy_percent, ciphertext.size(), encoded.data.size());
    } else {
        // Write YubiKey metadata before ciphertext if required
        if (m_yubikey_required) {
            // Write: [serial_len(1 byte)][serial][challenge(64 bytes)]
            uint8_t serial_len = static_cast<uint8_t>(std::min(m_yubikey_serial.length(), size_t{255}));
            file_data.push_back(serial_len);
            file_data.insert(file_data.end(), m_yubikey_serial.begin(),
                           m_yubikey_serial.begin() + serial_len);
            file_data.insert(file_data.end(), m_yubikey_challenge.begin(), m_yubikey_challenge.end());
        }

        // Format: [salt][iv][flags][optional: YubiKey][ciphertext]
        file_data.insert(file_data.end(), ciphertext.begin(), ciphertext.end());
    }

    // Create backup before saving (non-fatal if it fails)
    if (m_backup_enabled) {
        auto backup_result = create_backup(m_current_vault_path);
        if (!backup_result) {
            KeepTower::Log::warning("Failed to create backup: {}", static_cast<int>(backup_result.error()));
        } else {
            // Cleanup old backups after successful creation
            cleanup_old_backups(m_current_vault_path, m_backup_count);
        }
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

// ============================================================================
// Account Reordering (Drag-and-Drop Support)
// ============================================================================

/**
 * @brief Reorder account by moving it from one position to another
 *
 * This method handles drag-and-drop reordering by updating the global_display_order
 * field for all affected accounts. The ordering is normalized to sequential values
 * (0, 1, 2, ...) after the move to prevent gaps.
 *
 * Security considerations:
 * - Validates vault is open before making changes
 * - Performs bounds checking on indices
 * - Automatically saves changes to prevent data loss
 *
 * @param old_index Current position of the account (0-based)
 * @param new_index Target position for the account (0-based)
 * @return true if reordered successfully, false on error
 */
bool VaultManager::reorder_account(size_t old_index, size_t new_index) {
    // Security: Ensure vault is open and decrypted
    if (!is_vault_open()) {
        return false;
    }

    const size_t account_count = get_account_count();

    // Security: Validate indices are within bounds
    if (old_index >= account_count || new_index >= account_count) {
        return false;
    }

    // Optimization: No-op if source and destination are the same
    if (old_index == new_index) {
        return true;
    }

    // Initialize global_display_order for all accounts if not already set
    if (!has_custom_global_ordering()) {
        for (size_t i = 0; i < account_count; i++) {
            m_vault_data.mutable_accounts(i)->set_global_display_order(static_cast<int32_t>(i));
        }
    }

    // Get the account being moved
    auto* account_to_move = m_vault_data.mutable_accounts(old_index);

    if (old_index < new_index) {
        // Moving down: shift accounts up in the range [old_index+1, new_index]
        for (size_t i = old_index + 1; i <= new_index; i++) {
            auto* acc = m_vault_data.mutable_accounts(i);
            acc->set_global_display_order(acc->global_display_order() - 1);
        }
        // Place the moved account at the end of the shifted range
        account_to_move->set_global_display_order(
            m_vault_data.accounts(new_index).global_display_order()
        );
    } else {
        // Moving up: shift accounts down in the range [new_index, old_index-1]
        for (size_t i = new_index; i < old_index; i++) {
            auto* acc = m_vault_data.mutable_accounts(i);
            acc->set_global_display_order(acc->global_display_order() + 1);
        }
        // Place the moved account at the start of the shifted range
        account_to_move->set_global_display_order(
            m_vault_data.accounts(new_index).global_display_order()
        );
    }

    // Normalize display orders to ensure they're sequential (0, 1, 2, ...)
    // This prevents gaps and keeps the logic simple
    std::vector<std::pair<int32_t, size_t>> order_index_pairs;
    order_index_pairs.reserve(account_count);

    for (size_t i = 0; i < account_count; i++) {
        order_index_pairs.emplace_back(
            m_vault_data.accounts(i).global_display_order(),
            i
        );
    }

    std::sort(order_index_pairs.begin(), order_index_pairs.end());

    for (size_t i = 0; i < account_count; i++) {
        size_t account_idx = order_index_pairs[i].second;
        m_vault_data.mutable_accounts(account_idx)->set_global_display_order(static_cast<int32_t>(i));
    }

    // Save changes
    m_modified = true;
    return save_vault();
}

bool VaultManager::reset_global_display_order() {
    if (!is_vault_open()) {
        return false;
    }

    const size_t account_count = get_account_count();
    for (size_t i = 0; i < account_count; i++) {
        m_vault_data.mutable_accounts(i)->set_global_display_order(-1);
    }

    m_modified = true;
    return save_vault();
}

bool VaultManager::has_custom_global_ordering() const {
    if (!is_vault_open() || get_account_count() == 0) {
        return false;
    }

    // Check if any account has global_display_order >= 0 (custom ordering enabled)
    // Using range-based loop for better safety and readability
    const size_t account_count = get_account_count();
    for (size_t i = 0; i < account_count; ++i) {
        if (m_vault_data.accounts(i).global_display_order() >= 0) {
            return true;
        }
    }

    return false;
}

// ============================================================================
// Group Management (Phase 2 - Stub Implementations)
// ============================================================================

// Account Groups Implementation (Phase 3)
// ============================================================================

namespace {
    /**
     * @brief Generate a UUID v4 for group IDs
     * @return UUID string in format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
     *
     * Security: Uses cryptographically secure random number generation
     */
    std::string generate_uuid() {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> dis;

        // UUID v4 format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
        uint64_t part1 = dis(gen);
        uint64_t part2 = dis(gen);

        // Set version bits (4xxx)
        part1 = (part1 & 0xFFFFFFFF0000FFFFULL) | 0x0000000040000000ULL;
        // Set variant bits (yxxx where y = 8, 9, A, or B)
        part2 = (part2 & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

        std::ostringstream oss;
        oss << std::hex << std::setfill('0')
            << std::setw(8) << ((part1 >> 32) & 0xFFFFFFFF) << '-'
            << std::setw(4) << ((part1 >> 16) & 0xFFFF) << '-'
            << std::setw(4) << (part1 & 0xFFFF) << '-'
            << std::setw(4) << ((part2 >> 48) & 0xFFFF) << '-'
            << std::setw(12) << (part2 & 0xFFFFFFFFFFFFULL);

        return oss.str();
    }

    /**
     * @brief Validate group name for security and usability
     * @param name Proposed group name
     * @return true if valid, false if invalid
     *
     * Security checks:
     * - Not empty
     * - Reasonable length (1-100 characters)
     * - No control characters
     * - No path traversal attempts
     */
    bool is_valid_group_name(std::string_view name) {
        if (name.empty() || name.length() > 100) {
            return false;
        }

        // Check for control characters and path traversal
        for (char c : name) {
            if (std::iscntrl(static_cast<unsigned char>(c))) {
                return false;
            }
        }

        // Reject names that could cause issues
        if (name == "." || name == ".." || name.find('/') != std::string::npos ||
            name.find('\\') != std::string::npos) {
            return false;
        }

        return true;
    }

    /**
     * @brief Find a group by ID in the vault data
     * @param vault_data Vault data to search
     * @param group_id Group ID to find
     * @return Pointer to group if found, nullptr otherwise
     */
    keeptower::AccountGroup* find_group_by_id(keeptower::VaultData& vault_data,
                                               std::string_view group_id) {
        for (int i = 0; i < vault_data.groups_size(); ++i) {
            if (vault_data.groups(i).group_id() == group_id) {
                return vault_data.mutable_groups(i);
            }
        }
        return nullptr;
    }

    /**
     * @brief Find a group by ID (const version)
     */
    const keeptower::AccountGroup* find_group_by_id(const keeptower::VaultData& vault_data,
                                                     std::string_view group_id) {
        for (int i = 0; i < vault_data.groups_size(); ++i) {
            if (vault_data.groups(i).group_id() == group_id) {
                return &vault_data.groups(i);
            }
        }
        return nullptr;
    }
}

std::string VaultManager::create_group(std::string_view name) {
    // Security: Ensure vault is open
    if (!is_vault_open()) {
        return "";
    }

    // Security: Validate group name
    if (!is_valid_group_name(name)) {
        return "";
    }

    // Convert to std::string for protobuf API
    std::string name_str{name};

    // Check for duplicate names (usability)
    for (int i = 0; i < m_vault_data.groups_size(); ++i) {
        if (m_vault_data.groups(i).group_name() == name_str) {
            return "";  // Group with this name already exists
        }
    }

    // Generate unique group ID
    std::string group_id = generate_uuid();

    // Create new group
    auto* new_group = m_vault_data.add_groups();
    new_group->set_group_id(group_id);
    new_group->set_group_name(name_str);
    new_group->set_is_system_group(false);
    new_group->set_display_order(m_vault_data.groups_size() - 1);
    new_group->set_is_expanded(true);  // New groups start expanded

    // Mark as modified and save
    m_modified = true;
    if (!save_vault()) {
        // Rollback: remove the group we just added
        m_vault_data.mutable_groups()->RemoveLast();
        return "";
    }

    return group_id;
}

bool VaultManager::delete_group(std::string_view group_id) {
    // Security: Ensure vault is open
    if (!is_vault_open()) {
        return false;
    }

    // Security: Validate group ID format (basic check)
    if (group_id.empty()) {
        return false;
    }

    // Find the group
    int group_index = -1;
    for (int i = 0; i < m_vault_data.groups_size(); ++i) {
        if (m_vault_data.groups(i).group_id() == group_id) {
            // Security: Prevent deletion of system groups
            if (m_vault_data.groups(i).is_system_group()) {
                return false;
            }
            group_index = i;
            break;
        }
    }

    if (group_index == -1) {
        return false;  // Group not found
    }

    // Remove all references to this group from accounts
    for (int i = 0; i < m_vault_data.accounts_size(); ++i) {
        auto* account = m_vault_data.mutable_accounts(i);
        auto* groups = account->mutable_groups();

        // Remove matching group memberships
        for (int j = groups->size() - 1; j >= 0; --j) {
            if (groups->Get(j).group_id() == group_id) {
                groups->erase(groups->begin() + j);
            }
        }
    }

    // Remove the group itself
    m_vault_data.mutable_groups()->erase(
        m_vault_data.mutable_groups()->begin() + group_index
    );

    // Mark as modified and save
    m_modified = true;
    return save_vault();
}

bool VaultManager::add_account_to_group(size_t account_index, std::string_view group_id) {
    // Security: Ensure vault is open
    if (!is_vault_open()) {
        return false;
    }

    // Security: Validate indices
    if (account_index >= get_account_count()) {
        return false;
    }

    // Security: Validate group exists
    const auto* group = find_group_by_id(m_vault_data, group_id);
    if (!group) {
        return false;
    }

    auto* account = m_vault_data.mutable_accounts(account_index);

    // Check if already in group (prevent duplicates)
    for (const auto& membership : account->groups()) {
        if (membership.group_id() == group_id) {
            return true;  // Already in group, success (idempotent)
        }
    }

    // Add group membership - convert string_view to string for protobuf API
    auto* membership = account->add_groups();
    membership->set_group_id(std::string{group_id});
    membership->set_display_order(-1);  // Use automatic ordering initially

    // Mark as modified and save
    m_modified = true;
    return save_vault();
}

bool VaultManager::remove_account_from_group(size_t account_index, std::string_view group_id) {
    // Security: Ensure vault is open
    if (!is_vault_open()) {
        return false;
    }

    // Security: Validate indices
    if (account_index >= get_account_count()) {
        return false;
    }

    auto* account = m_vault_data.mutable_accounts(account_index);
    auto* groups = account->mutable_groups();

    // Find and remove the group membership
    bool found = false;
    for (int i = groups->size() - 1; i >= 0; --i) {
        if (groups->Get(i).group_id() == group_id) {
            groups->erase(groups->begin() + i);
            found = true;
            break;  // Only remove one membership (should be unique anyway)
        }
    }

    if (!found) {
        return true;  // Not in group, success (idempotent)
    }

    // Mark as modified and save
    m_modified = true;
    return save_vault();
}

bool VaultManager::reorder_account_in_group(size_t account_index,
                                            std::string_view group_id,
                                            int new_order) {
    // Security: Ensure vault is open
    if (!is_vault_open()) {
        return false;
    }

    // Security: Validate account index
    if (account_index >= get_account_count()) {
        return false;
    }

    // Validate group exists
    const keeptower::AccountGroup* group = find_group_by_id(m_vault_data, std::string{group_id});
    if (!group) {
        return false;
    }

    // Validate new order is reasonable
    if (new_order < 0) {
        return false;
    }

    auto* account = m_vault_data.mutable_accounts(account_index);

    // Find the membership for this group
    for (int i = 0; i < account->groups_size(); ++i) {
        auto* membership = account->mutable_groups(i);
        if (membership->group_id() == group_id) {
            membership->set_display_order(new_order);
            m_modified = true;
            return save_vault();
        }
    }

    // Account is not in this group
    return false;
}

std::string VaultManager::get_favorites_group_id() {
    // Security: Ensure vault is open
    if (!is_vault_open()) {
        return "";
    }

    // Look for existing Favorites group
    for (int i = 0; i < m_vault_data.groups_size(); ++i) {
        const auto& group = m_vault_data.groups(i);
        if (group.is_system_group() && group.group_name() == "Favorites") {
            return group.group_id();
        }
    }

    // Create Favorites group if it doesn't exist
    std::string group_id = generate_uuid();

    auto* favorites_group = m_vault_data.add_groups();
    favorites_group->set_group_id(group_id);
    favorites_group->set_group_name("Favorites");
    favorites_group->set_is_system_group(true);
    favorites_group->set_display_order(0);  // Always first
    favorites_group->set_is_expanded(true);  // Always expanded
    favorites_group->set_icon("favorite");  // Special icon

    // Save the new group
    m_modified = true;
    if (!save_vault()) {
        // Rollback
        m_vault_data.mutable_groups()->RemoveLast();
        return "";
    }

    return group_id;
}

bool VaultManager::is_account_in_group(size_t account_index, std::string_view group_id) const {
    // Security: Validate indices
    if (!is_vault_open() || account_index >= get_account_count()) {
        return false;
    }

    const auto& account = m_vault_data.accounts(account_index);

    // Check if account has this group membership
    for (const auto& membership : account.groups()) {
        if (membership.group_id() == group_id) {
            return true;
        }
    }

    return false;
}

std::vector<keeptower::AccountGroup> VaultManager::get_all_groups() const {
    std::vector<keeptower::AccountGroup> groups;

    if (!is_vault_open()) {
        return groups;
    }

    // Copy all groups from vault data
    for (const auto& group : m_vault_data.groups()) {
        groups.push_back(group);
    }

    return groups;
}

bool VaultManager::rename_group(std::string_view group_id, std::string_view new_name) {
    // Security: Ensure vault is open
    if (!is_vault_open()) {
        return false;
    }

    // Security: Validate new name
    if (!is_valid_group_name(new_name)) {
        return false;
    }

    // Find the group
    keeptower::AccountGroup* group = find_group_by_id(m_vault_data, std::string{group_id});
    if (!group) {
        return false;
    }

    // Security: Cannot rename system groups
    if (group->is_system_group()) {
        return false;
    }

    // Check for duplicate name (case-sensitive)
    for (const auto& existing_group : m_vault_data.groups()) {
        if (existing_group.group_id() != group_id &&
            existing_group.group_name() == new_name) {
            return false;  // Name already exists
        }
    }

    // Update the group name
    group->set_group_name(std::string{new_name});
    m_modified = true;

    // Save vault
    if (!save_vault()) {
        // Rollback on failure - protobuf doesn't have transaction support
        // so we rely on the fact that save_vault() restores from backup
        return false;
    }

    return true;
}

bool VaultManager::reorder_group(std::string_view group_id, int new_order) {
    // Security: Ensure vault is open
    if (!is_vault_open()) {
        return false;
    }

    // Validate new order is reasonable
    if (new_order < 0) {
        return false;
    }

    // Find the group
    keeptower::AccountGroup* group = find_group_by_id(m_vault_data, std::string{group_id});
    if (!group) {
        return false;
    }

    // System groups always have display_order = 0, cannot be reordered
    if (group->is_system_group()) {
        return false;
    }

    // Update the display order
    group->set_display_order(new_order);
    m_modified = true;

    // Save vault
    return save_vault();
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
        uint32_t iterations = DEFAULT_PBKDF2_ITERATIONS;

        if (size >= static_cast<std::streamsize>(HEADER_SIZE)) {
            uint32_t magic, version;
            file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
            file.read(reinterpret_cast<char*>(&version), sizeof(version));
            file.read(reinterpret_cast<char*>(&iterations), sizeof(iterations));

            if (magic == VAULT_MAGIC) {
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
        #ifdef __linux__
        chmod(path.c_str(), S_IRUSR | S_IWUSR);  // 0600
        #elif defined(_WIN32)
        // Windows permissions handled through ACLs (TODO: implement)
        #endif

        // Sync directory to ensure rename is durable
        #ifdef __linux__
        std::string dir_path = fs::path(path).parent_path().string();
        int dir_fd = open(dir_path.c_str(), O_RDONLY | O_DIRECTORY);
        if (dir_fd >= 0) {
            fsync(dir_fd);
            close(dir_fd);
        }
        #endif

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

    try {
        if (!fs::exists(path_str)) {
            return {};  // No file to backup
        }

        // Generate timestamp: YYYYmmdd_HHMMSS_milliseconds
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        std::ostringstream timestamp;
        timestamp << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S")
                  << "_" << std::setfill('0') << std::setw(3) << ms.count();

        // Create timestamped backup path
        std::string backup_path = path_str + ".backup." + timestamp.str();

        fs::copy_file(path_str, backup_path, fs::copy_options::overwrite_existing);
        KeepTower::Log::info("Created backup: {}", backup_path);

        return {};
    } catch (const fs::filesystem_error& e) {
        KeepTower::Log::warning("Failed to create backup: {}", e.what());
        // Don't fail the operation if backup fails
        return {};
    }
}

KeepTower::VaultResult<> VaultManager::restore_from_backup(std::string_view path) {
    namespace fs = std::filesystem;
    std::string path_str(path);

    // Get all backups and restore from the most recent
    auto backups = list_backups(path);

    try {
        if (backups.empty()) {
            // Try legacy .backup format for backwards compatibility
            std::string legacy_backup = path_str + ".backup";
            if (fs::exists(legacy_backup)) {
                fs::copy_file(legacy_backup, path_str, fs::copy_options::overwrite_existing);
                KeepTower::Log::info("Restored from legacy backup: {}", legacy_backup);
                return {};
            }
            KeepTower::Log::error("No backup files found for: {}", path_str);
            return std::unexpected(VaultError::FileNotFound);
        }

        // Backups are sorted newest first, so restore from [0]
        const std::string& backup_path = backups[0];
        fs::copy_file(backup_path, path_str, fs::copy_options::overwrite_existing);
        KeepTower::Log::info("Restored from backup: {}", backup_path);
        return {};
    } catch (const fs::filesystem_error& e) {
        KeepTower::Log::error("Failed to restore backup: {}", e.what());
        return std::unexpected(VaultError::FileReadFailed);
    }
}

void VaultManager::cleanup_old_backups(std::string_view path, int max_backups) {
    namespace fs = std::filesystem;

    if (max_backups < 1) [[unlikely]] {
        return;
    }

    auto backups = list_backups(path);

    // Delete oldest backups (backups are sorted newest first)
    for (size_t i = static_cast<size_t>(max_backups); i < backups.size(); ++i) {
        try {
            fs::remove(backups[i]);
            KeepTower::Log::info("Deleted old backup: {}", backups[i]);
        } catch (const fs::filesystem_error& e) {
            KeepTower::Log::warning("Failed to delete backup {}: {}", backups[i], e.what());
        }
    }
}

std::vector<std::string> VaultManager::list_backups(std::string_view path) {
    namespace fs = std::filesystem;
    std::vector<std::string> backups;
    std::string path_str(path);
    std::string backup_prefix = path_str + ".backup.";

    try {
        fs::path vault_path(path_str);
        fs::path parent_dir = vault_path.parent_path();

        if (!fs::exists(parent_dir)) {
            return backups;
        }

        // Find all backup files matching pattern
        for (const auto& entry : fs::directory_iterator(parent_dir)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            std::string filename = entry.path().string();
            if (filename.starts_with(backup_prefix)) {
                backups.push_back(filename);
            }
        }

        // Sort by filename (timestamp is in filename), newest first
        std::sort(backups.begin(), backups.end(), std::greater<std::string>());

    } catch (const fs::filesystem_error& e) {
        KeepTower::Log::warning("Failed to list backups: {}", e.what());
    }

    return backups;
}

bool VaultManager::set_rs_redundancy_percent(uint8_t percent) {
    if (percent < 5 || percent > 50) {
        return false;
    }
    m_rs_redundancy_percent = percent;
    m_fec_loaded_from_file = false;  // User is explicitly changing the redundancy
    if (m_reed_solomon) {
        m_reed_solomon = std::make_unique<ReedSolomon>(m_rs_redundancy_percent);
    }
    return true;
}

bool VaultManager::set_backup_count(int count) {
    if (count < 1 || count > 50) [[unlikely]] {
        return false;
    }
    m_backup_count = count;
    return true;
}

bool VaultManager::migrate_vault_schema() {
    // Get current schema version
    auto* metadata = m_vault_data.mutable_metadata();
    int32_t current_version = metadata->schema_version();

    // Check if we have an old vault (version field was used in schema v1)
    // In the old schema, VaultData had version and last_modified as direct fields
    // In new schema, these are in VaultMetadata sub-message

    // If schema_version is not set but we have accounts, this is a v1 vault
    if (current_version == 0 && m_vault_data.accounts_size() > 0) {
        KeepTower::Log::info("Migrating vault from schema v1 to v2");

        // Migrate v1 to v2
        // In v1, accounts had fields: id(1), created_at(2), modified_at(3),
        // account_name(4), user_name(5), password(6), email(7), website(8), notes(9)
        // In v2, these are at: id(1), account_name(2), user_name(3), password(4),
        // email(5), website(6), created_at(16), modified_at(17), notes(19)

        // Good news: protobuf is forward/backward compatible by field number
        // The v1 fields will automatically map to v2 fields with same numbers
        // We just need to set metadata

        // Set metadata for migrated vault
        metadata->set_schema_version(2);
        metadata->set_created_at(std::time(nullptr));  // Unknown, use now
        metadata->set_last_modified(std::time(nullptr));
        metadata->set_last_accessed(std::time(nullptr));
        metadata->set_access_count(1);

        // Mark as modified so it gets saved with new schema
        m_modified = true;

        KeepTower::Log::info("Vault migrated successfully to schema v2");
        return true;
    }

    // If schema version is 0 and no accounts, this is a new empty vault from v2
    if (current_version == 0 && m_vault_data.accounts_size() == 0) {
        // Initialize metadata for empty vault
        metadata->set_schema_version(2);
        metadata->set_created_at(std::time(nullptr));
        metadata->set_last_modified(std::time(nullptr));
        metadata->set_last_accessed(std::time(nullptr));
        metadata->set_access_count(1);
        return true;
    }

    // Already at current version or newer
    if (current_version >= 2) {
        // Update access tracking
        metadata->set_last_accessed(std::time(nullptr));
        metadata->set_access_count(metadata->access_count() + 1);
        m_modified = true;  // Save access tracking
        return true;
    }

    // Unknown version
    KeepTower::Log::warning("Unknown vault schema version: {}", current_version);
    return false;
}

#ifdef HAVE_YUBIKEY_SUPPORT
std::vector<keeptower::YubiKeyEntry> VaultManager::get_yubikey_list() const {
    std::vector<keeptower::YubiKeyEntry> result;

    if (!m_vault_open || !m_yubikey_required) {
        return result;
    }

    if (!m_vault_data.has_yubikey_config()) {
        return result;
    }

    const auto& yk_config = m_vault_data.yubikey_config();
    for (const auto& entry : yk_config.yubikey_entries()) {
        result.push_back(entry);
    }

    return result;
}

bool VaultManager::add_backup_yubikey(const std::string& name) {
    if (!m_vault_open || !m_yubikey_required) {
        std::cerr << "Vault must be open and YubiKey-protected to add backup keys" << std::endl;
        return false;
    }

    // Initialize YubiKey and get device info
    YubiKeyManager yk_manager;
    if (!yk_manager.initialize()) {
        std::cerr << "Failed to initialize YubiKey" << std::endl;
        return false;
    }

    if (!yk_manager.is_yubikey_present()) {
        std::cerr << "No YubiKey connected" << std::endl;
        return false;
    }

    auto device_info = yk_manager.get_device_info();
    if (!device_info) {
        std::cerr << "Failed to get YubiKey device information" << std::endl;
        return false;
    }

    // Check if already registered
    if (is_yubikey_authorized(device_info->serial_number)) {
        std::cerr << "YubiKey with serial " << device_info->serial_number << " is already registered" << std::endl;
        return false;
    }

    // Verify the key works with the current challenge
    auto response = yk_manager.challenge_response(
        std::span<const unsigned char>(m_yubikey_challenge.data(), m_yubikey_challenge.size()),
        false,
        YUBIKEY_TIMEOUT_MS
    );

    if (!response.success) {
        std::cerr << "YubiKey challenge-response failed. Key may not be programmed with same HMAC secret." << std::endl;
        return false;
    }

    // Add to protobuf
    auto* yk_config = m_vault_data.mutable_yubikey_config();
    auto* entry = yk_config->add_yubikey_entries();
    entry->set_serial(device_info->serial_number);
    entry->set_name(name.empty() ? "Backup" : name);
    entry->set_added_at(std::time(nullptr));

    m_modified = true;
    KeepTower::Log::info("Added backup YubiKey with serial: {}", device_info->serial_number);
    return true;
}

bool VaultManager::remove_yubikey(const std::string& serial) {
    if (!m_vault_open || !m_yubikey_required) {
        std::cerr << "Vault must be open and YubiKey-protected" << std::endl;
        return false;
    }

    if (!m_vault_data.has_yubikey_config()) {
        return false;
    }

    auto* yk_config = m_vault_data.mutable_yubikey_config();

    // Cannot remove last key
    if (yk_config->yubikey_entries_size() <= 1) {
        std::cerr << "Cannot remove the last YubiKey" << std::endl;
        return false;
    }

    // Find and remove
    for (int i = 0; i < yk_config->yubikey_entries_size(); ++i) {
        if (yk_config->yubikey_entries(i).serial() == serial) {
            // Remove by swapping with last and deleting
            if (i < yk_config->yubikey_entries_size() - 1) {
                yk_config->mutable_yubikey_entries()->SwapElements(i, yk_config->yubikey_entries_size() - 1);
            }
            yk_config->mutable_yubikey_entries()->RemoveLast();

            m_modified = true;
            KeepTower::Log::info("Removed YubiKey with serial: {}", serial);
            return true;
        }
    }

    std::cerr << "YubiKey with serial " << serial << " not found" << std::endl;
    return false;
}

bool VaultManager::is_yubikey_authorized(const std::string& serial) const {
    if (!m_vault_open || !m_yubikey_required) {
        return false;
    }

    if (!m_vault_data.has_yubikey_config()) {
        // Backward compatibility: check against file header serial
        return serial == m_yubikey_serial;
    }

    const auto& yk_config = m_vault_data.yubikey_config();
    for (const auto& entry : yk_config.yubikey_entries()) {
        if (entry.serial() == serial) {
            return true;
        }
    }

    // Backward compatibility: also check deprecated serial field
    if (!yk_config.serial().empty() && yk_config.serial() == serial) {
        return true;
    }

    return false;
}

bool VaultManager::verify_credentials(const Glib::ustring& password, const std::string& serial) {
    // Thread safety - lock vault data
    std::lock_guard<std::mutex> lock(m_vault_mutex);

    if (!m_vault_open) {
        return false;
    }

    // If vault requires YubiKey, verify both password and YubiKey
    if (m_yubikey_required) {
        if (serial.empty()) {
            return false;  // YubiKey serial required
        }

        // Check if the YubiKey serial is authorized
        if (!is_yubikey_authorized(serial)) {
            return false;
        }

        // Derive the key using password + YubiKey response
        YubiKeyManager yk_manager;
        if (!yk_manager.initialize() || !yk_manager.is_yubikey_present()) {
            return false;
        }

        // Get device info to verify serial matches
        auto device_info = yk_manager.get_device_info();
        if (!device_info || device_info->serial_number != serial) {
            return false;  // Wrong YubiKey connected or failed to get info
        }

        // Ensure challenge is correct size (64 bytes required by YubiKey)
        if (m_yubikey_challenge.size() != YUBIKEY_CHALLENGE_SIZE) {
            KeepTower::Log::error("Invalid YubiKey challenge size: {} (expected {})",
                                 m_yubikey_challenge.size(), YUBIKEY_CHALLENGE_SIZE);
            return false;
        }

        // Perform challenge-response
        auto cr_result = yk_manager.challenge_response(m_yubikey_challenge, true, YUBIKEY_TIMEOUT_MS);
        if (!cr_result.success) {
            KeepTower::Log::error("YubiKey challenge-response failed in verify_credentials: {}",
                                 cr_result.error_message);
            return false;
        }

        // Derive password-based key first
        std::vector<uint8_t> password_key(KEY_LENGTH);
        if (!derive_key(password, m_salt, password_key)) {
            return false;
        }

        // XOR with YubiKey response to get final key
        std::vector<uint8_t> test_key = password_key;
        for (size_t i = 0; i < test_key.size() && i < cr_result.response.size(); i++) {
            test_key[i] ^= cr_result.response[i];
        }

        // Constant-time comparison to prevent timing attacks
        bool match = (test_key.size() == m_encryption_key.size());
        if (match) {
            // Manual constant-time compare
            volatile uint8_t diff = 0;
            for (size_t i = 0; i < test_key.size(); i++) {
                diff |= test_key[i] ^ m_encryption_key[i];
            }
            match = (diff == 0);
        }

        // Securely clear sensitive data
        // Note: These were never locked, so just clear without unlock
        if (!test_key.empty()) {
            OPENSSL_cleanse(test_key.data(), test_key.size());
            test_key.clear();
            test_key.shrink_to_fit();
        }
        if (!password_key.empty()) {
            OPENSSL_cleanse(password_key.data(), password_key.size());
            password_key.clear();
            password_key.shrink_to_fit();
        }
        return match;
    }

    // No YubiKey required - just verify password
    std::vector<uint8_t> test_key(KEY_LENGTH);
    if (!derive_key(password, m_salt, test_key)) {
        return false;
    }

    // Constant-time comparison to prevent timing attacks
    bool match = (test_key.size() == m_encryption_key.size());
    if (match) {
        // Manual constant-time compare
        volatile uint8_t diff = 0;
        for (size_t i = 0; i < test_key.size(); i++) {
            diff |= test_key[i] ^ m_encryption_key[i];
        }
        match = (diff == 0);
    }

    // Securely clear test key (not locked, so don't call unlock)
    if (!test_key.empty()) {
        OPENSSL_cleanse(test_key.data(), test_key.size());
        test_key.clear();
        test_key.shrink_to_fit();
    }
    return match;
}

// FIPS-140-3 mode management implementation

bool VaultManager::init_fips_mode(bool enable) {
    // Check if already initialized
    bool expected = false;
    if (!s_fips_mode_initialized.compare_exchange_strong(expected, true)) {
        KeepTower::Log::warning("FIPS mode already initialized");
        return s_fips_mode_available.load();
    }

    KeepTower::Log::info("Initializing OpenSSL FIPS mode (enable={})", enable);

    // Try to load FIPS provider
    OSSL_PROVIDER* fips_provider = OSSL_PROVIDER_load(nullptr, "fips");
    if (fips_provider == nullptr) {
        KeepTower::Log::warning("FIPS provider not available - using default provider");
        s_fips_mode_available.store(false);
        s_fips_mode_enabled.store(false);

        // Load default provider as fallback
        OSSL_PROVIDER* default_provider = OSSL_PROVIDER_load(nullptr, "default");
        if (default_provider == nullptr) {
            KeepTower::Log::error("Failed to load default OpenSSL provider");
            unsigned long err = ERR_get_error();
            char err_buf[256];
            ERR_error_string_n(err, err_buf, sizeof(err_buf));
            KeepTower::Log::error("OpenSSL error: {}", err_buf);
            return false;
        }

        return true;  // Default provider loaded successfully
    }

    // FIPS provider is available
    s_fips_mode_available.store(true);
    KeepTower::Log::info("FIPS provider loaded successfully");

    // Enable FIPS mode if requested
    if (enable) {
        if (EVP_default_properties_enable_fips(nullptr, 1) != 1) {
            KeepTower::Log::error("Failed to enable FIPS mode");
            unsigned long err = ERR_get_error();
            char err_buf[256];
            ERR_error_string_n(err, err_buf, sizeof(err_buf));
            KeepTower::Log::error("OpenSSL error: {}", err_buf);
            return false;
        }

        s_fips_mode_enabled.store(true);
        KeepTower::Log::info("FIPS mode enabled successfully");
    } else {
        // Load default provider alongside FIPS for flexibility
        OSSL_PROVIDER* default_provider = OSSL_PROVIDER_load(nullptr, "default");
        if (default_provider == nullptr) {
            KeepTower::Log::warning("Failed to load default provider alongside FIPS");
        }

        s_fips_mode_enabled.store(false);
        KeepTower::Log::info("FIPS mode available but not enabled");
    }

    return true;
}

bool VaultManager::is_fips_available() {
    if (!s_fips_mode_initialized.load()) {
        KeepTower::Log::warning("FIPS mode not initialized - call init_fips_mode() first");
        return false;
    }
    return s_fips_mode_available.load();
}

bool VaultManager::is_fips_enabled() {
    if (!s_fips_mode_initialized.load()) {
        KeepTower::Log::warning("FIPS mode not initialized - call init_fips_mode() first");
        return false;
    }
    return s_fips_mode_enabled.load();
}

bool VaultManager::set_fips_mode(bool enable) {
    if (!s_fips_mode_initialized.load()) {
        KeepTower::Log::error("FIPS mode not initialized - call init_fips_mode() first");
        return false;
    }

    if (!s_fips_mode_available.load()) {
        KeepTower::Log::error("Cannot enable FIPS mode - FIPS provider not available");
        return false;
    }

    if (enable == s_fips_mode_enabled.load()) {
        KeepTower::Log::info("FIPS mode already in requested state ({})", enable);
        return true;
    }

    // Change FIPS mode
    if (EVP_default_properties_enable_fips(nullptr, enable ? 1 : 0) != 1) {
        KeepTower::Log::error("Failed to {} FIPS mode", enable ? "enable" : "disable");
        unsigned long err = ERR_get_error();
        char err_buf[256];
        ERR_error_string_n(err, err_buf, sizeof(err_buf));
        KeepTower::Log::error("OpenSSL error: {}", err_buf);
        return false;
    }

    s_fips_mode_enabled.store(enable);
    KeepTower::Log::info("FIPS mode {} successfully", enable ? "enabled" : "disabled");
    return true;
}
#endif
