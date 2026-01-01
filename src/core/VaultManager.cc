// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "VaultManager.h"
#include "KeyWrapping.h"  // For V2 password verification
#include "VaultFormatV2.h"  // For V2 vault parsing
#include "../utils/Log.h"
#include "../utils/SecureMemory.h"  // For secure_clear template
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/provider.h>
#include <openssl/crypto.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <random>

#ifdef __linux__
#include <sys/mman.h>  // For mlock/munlock
#include <sys/resource.h>  // For setrlimit/RLIMIT_MEMLOCK
#include <sys/stat.h>  // For chmod
#include <fcntl.h>     // For open()
#include <unistd.h>    // For fsync(), close()
#elif defined(_WIN32)
#include <windows.h>   // For VirtualLock/VirtualUnlock
#endif

using namespace KeepTower;

// ============================================================================
// FIPS-140-3 Mode State Initialization
// ============================================================================

/**
 * @brief Static initialization of FIPS mode state variables
 *
 * All three atomic booleans initialize to false, representing the
 * uninitialized state. Thread-safe initialization occurs on first
 * access (guaranteed by C++11 static initialization rules).
 *
 * @section fips_state_machine FIPS State Machine
 *
 * Valid state transitions:
 * 1. All false (startup) → initialized=true, available=false (default provider loaded)
 * 2. All false (startup) → initialized=true, available=true, enabled=false (FIPS loaded but not enabled)
 * 3. All false (startup) → initialized=true, available=true, enabled=true (FIPS loaded and enabled)
 * 4. available=true, enabled=false → enabled=true (runtime FIPS activation)
 * 5. available=true, enabled=true → enabled=false (runtime FIPS deactivation)
 *
 * Invalid transitions (prevented by code):
 * - initialized false → true more than once (compare_exchange prevents)
 * - available false → enabled true (set_fips_mode() checks availability)
 * - Any state → initialized false (never reset once initialized)
 *
 * @note These are zero-initialized by default for std::atomic<bool>
 */
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
      m_is_v2_vault(false),
      m_v2_dek{},  // Zero-initialize 32-byte DEK for security
      m_use_reed_solomon(false),
      m_rs_redundancy_percent(DEFAULT_RS_REDUNDANCY),
      m_fec_loaded_from_file(false),
      m_backup_enabled(true),
      m_backup_count(DEFAULT_BACKUP_COUNT),
      m_backup_path(""),  // Empty = same directory as vault
      m_memory_locked(false),
      m_yubikey_required(false),
      m_pbkdf2_iterations(DEFAULT_PBKDF2_ITERATIONS) {

#ifdef __linux__
    // Check if we need to increase RLIMIT_MEMLOCK for sensitive memory locking
    // V2 vaults with multiple users need ~50 KB worst case
    // We request 10 MB for safety margin, but only warn if below 5 MB (100x actual need)
    struct rlimit current_limit{};
    if (getrlimit(RLIMIT_MEMLOCK, &current_limit) == 0) {
        constexpr rlim_t MIN_REQUIRED = 5UL * 1024 * 1024;  // 5 MB (100x actual ~50 KB need)
        constexpr rlim_t DESIRED = 10UL * 1024 * 1024;      // 10 MB (optimal safety margin)

        if (current_limit.rlim_cur >= MIN_REQUIRED) {
            // Current limit is sufficient - no action needed
            KeepTower::Log::debug("VaultManager: RLIMIT_MEMLOCK sufficient ({} KB available, {} KB minimum)",
                                 current_limit.rlim_cur / 1024, MIN_REQUIRED / 1024);
        } else {
            // Current limit is low - try to increase, warn if we can't
            struct rlimit new_limit{};
            new_limit.rlim_cur = DESIRED;
            new_limit.rlim_max = DESIRED;

            if (setrlimit(RLIMIT_MEMLOCK, &new_limit) == 0) {
                KeepTower::Log::debug("VaultManager: Increased RLIMIT_MEMLOCK to {} KB", DESIRED / 1024);
            } else {
                // Warning: current limit is insufficient and we can't increase it
                KeepTower::Log::warning("VaultManager: Low RLIMIT_MEMLOCK ({} KB < {} KB recommended)",
                                       current_limit.rlim_cur / 1024, MIN_REQUIRED / 1024);
                KeepTower::Log::warning("VaultManager: Memory locking may fail for large vaults. Run with CAP_IPC_LOCK or increase ulimit -l to {} KB",
                                       MIN_REQUIRED / 1024);
            }
        }
    } else {
        KeepTower::Log::warning("VaultManager: Failed to query RLIMIT_MEMLOCK: {} ({})",
                               std::strerror(errno), errno);
    }
#endif
}

VaultManager::~VaultManager() noexcept {
    // Ensure sensitive data is securely erased
    try {
        secure_clear(m_encryption_key);
        secure_clear(m_salt);
        secure_clear(m_yubikey_challenge);
        OPENSSL_cleanse(m_v2_dek.data(), m_v2_dek.size());  // V2 vault DEK (std::array)
        (void)close_vault();  // Explicitly ignore return value in destructor
    } catch (const std::exception& e) {
        // Log but don't propagate - destructors must not throw
        KeepTower::Log::error("VaultManager destructor error: {}", e.what());
    } catch (...) {
        // Silently handle unknown exceptions in destructor
    }
}

bool VaultManager::create_vault(const std::string& path,
                                 const Glib::ustring& password,
                                 bool require_yubikey,
                                 std::string yubikey_serial) {
    if (m_vault_open) {
        if (!close_vault()) {
            std::cerr << "Warning: Failed to close existing vault\n";
        }
    }

    // Generate new salt
    m_salt = KeepTower::VaultCrypto::generate_random_bytes(SALT_LENGTH);

    // Derive base encryption key from password
    std::vector<uint8_t> password_key(KEY_LENGTH);
    if (!KeepTower::VaultCrypto::derive_key(password, m_salt, password_key, m_pbkdf2_iterations)) {
        return false;
    }

    // Handle YubiKey integration if requested
    m_yubikey_required = require_yubikey;
    if (require_yubikey) {
#ifdef HAVE_YUBIKEY_SUPPORT
        // Generate random challenge for this vault
        m_yubikey_challenge = KeepTower::VaultCrypto::generate_random_bytes(YUBIKEY_CHALLENGE_SIZE);

        // Get YubiKey response
        YubiKeyManager yk_manager;
        if (!yk_manager.initialize()) {
            std::cerr << "Failed to initialize YubiKey" << '\n';
            secure_clear(password_key);
            return false;
        }

        auto response = yk_manager.challenge_response(
            std::span<const unsigned char>(m_yubikey_challenge.data(), m_yubikey_challenge.size()),
            YubiKeyAlgorithm::HMAC_SHA1,  // V1 vaults use SHA-1
            false,  // don't require touch for vault operations
            YUBIKEY_TIMEOUT_MS
        );

        if (!response.success) {
            std::cerr << "YubiKey challenge-response failed: " << response.error_message << '\n';
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
        std::cerr << "YubiKey support not compiled in" << '\n';
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

    // Initialize managers
    m_account_manager = std::make_unique<KeepTower::AccountManager>(m_vault_data, m_modified);
    m_group_manager = std::make_unique<KeepTower::GroupManager>(m_vault_data, m_modified);

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
    if (!KeepTower::VaultIO::read_file(path, file_data, false, m_pbkdf2_iterations)) {
        return false;
    }

    // Check if V2 vault (multi-user format)
    if (file_data.size() >= 16) {  // Need at least magic + version + pbkdf2_iters + header_size
        uint32_t magic = (static_cast<uint32_t>(file_data[0]) << 0) |
                        (static_cast<uint32_t>(file_data[1]) << 8) |
                        (static_cast<uint32_t>(file_data[2]) << 16) |
                        (static_cast<uint32_t>(file_data[3]) << 24);
        uint32_t version = (static_cast<uint32_t>(file_data[4]) << 0) |
                          (static_cast<uint32_t>(file_data[5]) << 8) |
                          (static_cast<uint32_t>(file_data[6]) << 16) |
                          (static_cast<uint32_t>(file_data[7]) << 24);

        if (magic == 0x4B505457 && version == 2) {  // "KPTW" V2
            // This is a V2 vault - check if any user has YubiKey enrolled
            // We need to parse the header to check
            auto parse_result = VaultFormatV2::read_header(file_data);
            if (parse_result) {
                const auto& [header, _] = *parse_result;
                const auto& vault_header = header.vault_header;
                // Check if any active key slot has YubiKey enrolled
                for (const auto& slot : vault_header.key_slots) {
                    if (slot.active && slot.yubikey_enrolled) {
                        serial = slot.yubikey_serial;
                        return true;
                    }
                }
                // Also check if security policy requires YubiKey
                if (vault_header.security_policy.require_yubikey) {
                    return true;
                }
            }
            return false;  // V2 vault but no YubiKey required
        }
    }

    // V1 vault handling (original code)
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
        serial.assign(file_data.begin() + static_cast<std::ptrdiff_t>(offset),
                      file_data.begin() + static_cast<std::ptrdiff_t>(offset + serial_len));
    }

    return true;
}

// ============================================================================
// Helper functions for open_vault() - Refactored for reduced complexity
// ============================================================================

#ifdef HAVE_YUBIKEY_SUPPORT
KeepTower::VaultResult<>
VaultManager::authenticate_yubikey(
    const KeepTower::VaultFileMetadata& metadata,
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
    auto response = yk_manager.challenge_response(
        metadata.yubikey_challenge,
        YubiKeyAlgorithm::HMAC_SHA1,  // V1 vaults use SHA-1
        false,
        YUBIKEY_TIMEOUT_MS
    );
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
    if (!KeepTower::VaultCrypto::decrypt_data(ciphertext, key, iv, plaintext)) {
        return std::unexpected(VaultError::DecryptionFailed);
    }

    // Deserialize protobuf data
    auto vault_result = KeepTower::VaultSerialization::deserialize(plaintext);
    if (!vault_result) {
        return std::unexpected(vault_result.error());
    }

    return std::move(vault_result.value());
}

// ============================================================================
// Refactored open_vault() - Simplified using helper functions
// ============================================================================

bool VaultManager::open_vault(const std::string& path, const Glib::ustring& password) {
    // 1. Close existing vault if needed
    if (m_vault_open && !close_vault()) {
        std::cerr << "Warning: Failed to close existing vault" << '\n';
    }

    // 2. Read vault file
    std::vector<uint8_t> file_data;
    if (!KeepTower::VaultIO::read_file(path, file_data, false, m_pbkdf2_iterations)) {
        return false;
    }

    // 3. Parse vault format and extract metadata
    auto parsed_result = KeepTower::VaultFormat::parse(file_data);
    if (!parsed_result) {
        std::cerr << "Failed to parse vault format: "
                  << static_cast<int>(parsed_result.error()) << '\n';
        return false;
    }

    KeepTower::ParsedVaultData parsed_data = std::move(parsed_result.value());
    KeepTower::VaultFileMetadata& metadata = parsed_data.metadata;
    std::vector<uint8_t>& ciphertext = parsed_data.ciphertext;

    // 4. Derive encryption key from password
    m_encryption_key.resize(KEY_LENGTH);
    m_salt = metadata.salt;
    if (!KeepTower::VaultCrypto::derive_key(password, m_salt, m_encryption_key, m_pbkdf2_iterations)) {
        return false;
    }

    // 5. Authenticate with YubiKey if required
#ifdef HAVE_YUBIKEY_SUPPORT
    if (metadata.requires_yubikey) {
        auto yk_result = authenticate_yubikey(metadata, m_encryption_key);
        if (!yk_result) {
            std::cerr << "YubiKey authentication failed: "
                      << static_cast<int>(yk_result.error()) << '\n';
            secure_clear(m_encryption_key);
            return false;
        }
    }
#else
    if (metadata.requires_yubikey) {
        std::cerr << "Vault requires YubiKey but YubiKey support is not compiled in" << '\n';
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
                  << static_cast<int>(vault_result.error()) << '\n';
        return false;
    }

    // 8. Store vault data and metadata
    m_vault_data = std::move(vault_result.value());

    // Migrate old schema if needed
    if (!migrate_vault_schema()) {
        std::cerr << "Failed to migrate vault schema" << '\n';
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

    // Load backup settings from vault data if available (0 means not set)
    if (m_vault_data.backup_count() > 0) {
        m_backup_enabled = m_vault_data.backup_enabled();
        m_backup_count = m_vault_data.backup_count();
        KeepTower::Log::info("Loaded backup settings from vault: enabled={}, count={}",
                            m_backup_enabled, m_backup_count);
    }

    // 10. Update vault state
    m_current_vault_path = path;
    m_vault_open = true;
    m_modified = false;

    // Initialize managers after vault data is loaded
    m_account_manager = std::make_unique<KeepTower::AccountManager>(m_vault_data, m_modified);
    m_group_manager = std::make_unique<KeepTower::GroupManager>(m_vault_data, m_modified);

    return true;
}

bool VaultManager::save_vault(bool explicit_save) {
    if (!m_vault_open) {
        return false;
    }

    // V2 vault saving
    if (m_is_v2_vault) {
        if (!m_v2_header || !m_current_session) {
            KeepTower::Log::error("VaultManager: Invalid V2 vault state");
            return false;
        }

        // Update timestamp
        auto* metadata = m_vault_data.mutable_metadata();
        metadata->set_last_modified(std::time(nullptr));

        // Serialize protobuf to binary
        auto serialized_result = KeepTower::VaultSerialization::serialize(m_vault_data);
        if (!serialized_result) {
            KeepTower::Log::error("VaultManager: Failed to serialize vault data");
            return false;
        }
        std::vector<uint8_t> plaintext = std::move(serialized_result.value());
        std::vector<uint8_t> ciphertext;
        std::vector<uint8_t> data_iv = KeepTower::VaultCrypto::generate_random_bytes(KeepTower::VaultCrypto::IV_LENGTH);

        if (!KeepTower::VaultCrypto::encrypt_data(plaintext, m_v2_dek, ciphertext, data_iv)) {
            KeepTower::Log::error("VaultManager: Failed to encrypt vault data");
            secure_clear(plaintext);
            return false;
        }
        secure_clear(plaintext);

        // Build V2 file format
        if (!m_v2_header.has_value()) {
            KeepTower::Log::error("VaultManager: V2 header not initialized");
            return false;
        }
        KeepTower::VaultFormatV2::V2FileHeader file_header;
        file_header.pbkdf2_iterations = m_v2_header->security_policy.pbkdf2_iterations;
        file_header.vault_header = *m_v2_header;
        std::copy(data_iv.begin(),
                  std::min(data_iv.begin() + static_cast<std::ptrdiff_t>(32), data_iv.end()),
                  file_header.data_salt.begin());
        std::copy(data_iv.begin(),
                  std::min(data_iv.begin() + static_cast<std::ptrdiff_t>(12), data_iv.end()),
                  file_header.data_iv.begin());

        // Write header with FEC (use configured redundancy if RS enabled, 0 if disabled)
        uint8_t fec_redundancy = m_use_reed_solomon ? m_rs_redundancy_percent : 0;
        auto write_result = KeepTower::VaultFormatV2::write_header(file_header, fec_redundancy);
        if (!write_result) {
            KeepTower::Log::error("VaultManager: Failed to write V2 header");
            return false;
        }

        // Combine header + encrypted data
        std::vector<uint8_t> file_data = write_result.value();
        file_data.insert(file_data.end(), ciphertext.begin(), ciphertext.end());

        // Create backup before saving (only on explicit save, non-fatal if it fails)
        if (explicit_save && m_backup_enabled) {
            auto backup_result = KeepTower::VaultIO::create_backup(m_current_vault_path, m_backup_path);
            if (!backup_result) {
                KeepTower::Log::warning("VaultManager: Failed to create backup: {}", static_cast<int>(backup_result.error()));
            } else {
                // Cleanup old backups after successful creation
                KeepTower::VaultIO::cleanup_old_backups(m_current_vault_path, m_backup_count, m_backup_path);
            }
        }

        // Write to file
        if (!KeepTower::VaultIO::write_file(m_current_vault_path, file_data, true, 0)) {
            KeepTower::Log::error("VaultManager: Failed to write vault file");
            return false;
        }

        m_modified = false;
        KeepTower::Log::info("VaultManager: V2 vault saved successfully");
        return true;
    }

    // V1 vault saving (existing logic)
    // Update timestamp
    auto* metadata = m_vault_data.mutable_metadata();
    metadata->set_last_modified(std::time(nullptr));

    // Serialize protobuf to binary
    auto serialized_result = KeepTower::VaultSerialization::serialize(m_vault_data);
    if (!serialized_result) {
        std::cerr << "Failed to serialize vault data\n";
        return false;
    }
    std::vector<uint8_t> plaintext = std::move(serialized_result.value());

    // Encrypt data
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> iv = KeepTower::VaultCrypto::generate_random_bytes(IV_LENGTH);
    if (!KeepTower::VaultCrypto::encrypt_data(plaintext, m_encryption_key, ciphertext, iv)) {
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
                           m_yubikey_serial.begin() + static_cast<std::ptrdiff_t>(serial_len));
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
                           m_yubikey_serial.begin() + static_cast<std::ptrdiff_t>(serial_len));
            file_data.insert(file_data.end(), m_yubikey_challenge.begin(), m_yubikey_challenge.end());
        }

        // Format: [salt][iv][flags][optional: YubiKey][ciphertext]
        file_data.insert(file_data.end(), ciphertext.begin(), ciphertext.end());
    }

    // Create backup before saving (only on explicit save, non-fatal if it fails)
    if (explicit_save && m_backup_enabled) {
        auto backup_result = KeepTower::VaultIO::create_backup(m_current_vault_path, m_backup_path);
        if (!backup_result) {
            KeepTower::Log::warning("Failed to create backup: {}", static_cast<int>(backup_result.error()));
        } else {
            // Cleanup old backups after successful creation
            KeepTower::VaultIO::cleanup_old_backups(m_current_vault_path, m_backup_count, m_backup_path);
        }
    }

    if (!KeepTower::VaultIO::write_file(m_current_vault_path, file_data, false, m_pbkdf2_iterations)) {
        return false;
    }

    m_modified = false;
    return true;
}


bool VaultManager::close_vault() {
    if (!m_vault_open) {
        return true;
    }

    // FIPS-140-3 Compliance: Unlock and zeroize all cryptographic key material (Section 7.9)
    if (m_is_v2_vault && m_v2_header.has_value()) {
        // Unlock and clear V2 Data Encryption Key (DEK)
        unlock_memory(m_v2_dek.data(), m_v2_dek.size());
        OPENSSL_cleanse(m_v2_dek.data(), m_v2_dek.size());
        KeepTower::Log::debug("VaultManager: Unlocked and cleared V2 DEK");

        // Unlock and clear policy-level YubiKey challenge (shared by all users)
        if (m_v2_header->security_policy.require_yubikey) {
            auto& policy_challenge = m_v2_header->security_policy.yubikey_challenge;
            unlock_memory(policy_challenge.data(), policy_challenge.size());
            OPENSSL_cleanse(policy_challenge.data(), policy_challenge.size());
            KeepTower::Log::debug("VaultManager: Unlocked and cleared V2 policy YubiKey challenge");
        }

        // Unlock and clear per-user YubiKey challenges
        for (auto& slot : m_v2_header->key_slots) {
            if (slot.yubikey_enrolled) {
                unlock_memory(slot.yubikey_challenge.data(), slot.yubikey_challenge.size());
                OPENSSL_cleanse(slot.yubikey_challenge.data(), slot.yubikey_challenge.size());
            }
        }
        KeepTower::Log::debug("VaultManager: Unlocked and cleared all per-user YubiKey challenges");
    }

    // Securely clear sensitive data
    secure_clear(m_encryption_key);
    secure_clear(m_salt);
    m_vault_data.Clear();
    m_current_vault_path.clear();

    // Clear managers
    m_account_manager.reset();
    m_group_manager.reset();

    m_vault_open = false;
    m_modified = false;

    return true;
}

bool VaultManager::add_account(const keeptower::AccountRecord& account) {
    if (!m_vault_open) {
        return false;
    }

    return m_account_manager->add_account(account);
}

std::vector<keeptower::AccountRecord> VaultManager::get_all_accounts() const {
    if (!m_account_manager) {
        return {};
    }
    return m_account_manager->get_all_accounts();
}

bool VaultManager::update_account(size_t index, const keeptower::AccountRecord& account) {
    if (!m_vault_open) {
        return false;
    }

    return m_account_manager->update_account(index, account);
}

bool VaultManager::delete_account(size_t index) {
    if (!m_vault_open) {
        return false;
    }

    return m_account_manager->delete_account(index);
}

keeptower::AccountRecord* VaultManager::get_account_mutable(size_t index) {
    if (!m_vault_open) {
        return nullptr;
    }
    return m_account_manager->get_account_mutable(index);
}

const keeptower::AccountRecord* VaultManager::get_account(size_t index) const {
    if (!m_vault_open) {
        return nullptr;
    }
    return m_account_manager->get_account(index);
}

size_t VaultManager::get_account_count() const {
    if (!m_account_manager) {
        return 0;
    }
    return m_account_manager->get_account_count();
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

    // Delegate to AccountManager and save changes
    if (m_account_manager->reorder_account(old_index, new_index)) {
        return save_vault();
    }
    return false;
}

bool VaultManager::reset_global_display_order() {
    if (!is_vault_open()) {
        return false;
    }

    const size_t account_count = get_account_count();
    for (size_t i = 0; i < account_count; i++) {
        m_vault_data.mutable_accounts(static_cast<int>(i))->set_global_display_order(-1);
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
// Group Management
// ============================================================================

std::string VaultManager::create_group(std::string_view name) {
    if (!is_vault_open()) {
        return "";
    }

    std::string group_id = m_group_manager->create_group(name);
    if (!group_id.empty()) {
        // Save vault after creating group
        if (!save_vault()) {
            // Note: GroupManager has already modified vault_data, but save failed
            // In production, consider rollback mechanism
            return "";
        }
    }
    return group_id;
}

bool VaultManager::delete_group(std::string_view group_id) {
    if (!is_vault_open()) {
        return false;
    }

    if (m_group_manager->delete_group(group_id)) {
        return save_vault();
    }
    return false;
}

bool VaultManager::add_account_to_group(size_t account_index, std::string_view group_id) {
    if (!is_vault_open()) {
        return false;
    }

    if (m_group_manager->add_account_to_group(account_index, group_id)) {
        return save_vault();
    }
    return false;
}

bool VaultManager::remove_account_from_group(size_t account_index, std::string_view group_id) {
    if (!is_vault_open()) {
        return false;
    }

    if (m_group_manager->remove_account_from_group(account_index, group_id)) {
        return save_vault();
    }
    return false;
}

bool VaultManager::reorder_account_in_group(size_t account_index,
                                            std::string_view group_id,
                                            int new_order) {
    if (!is_vault_open()) {
        return false;
    }

    if (m_group_manager->reorder_account_in_group(account_index, group_id, new_order)) {
        return save_vault();
    }
    return false;
}

std::string VaultManager::get_favorites_group_id() {
    if (!is_vault_open()) {
        return "";
    }

    std::string group_id = m_group_manager->get_favorites_group_id();
    if (!group_id.empty() && m_modified) {
        // Save if favorites group was created (ignore result - favorites ID still valid)
        (void)save_vault();
    }
    return group_id;
}

bool VaultManager::is_account_in_group(size_t account_index, std::string_view group_id) const {
    if (!is_vault_open()) {
        return false;
    }
    return m_group_manager->is_account_in_group(account_index, group_id);
}

std::vector<keeptower::AccountGroup> VaultManager::get_all_groups() const {
    if (!m_group_manager) {
        return {};
    }
    return m_group_manager->get_all_groups();
}

bool VaultManager::rename_group(std::string_view group_id, std::string_view new_name) {
    if (!is_vault_open()) {
        return false;
    }

    if (m_group_manager->rename_group(group_id, new_name)) {
        return save_vault();
    }
    return false;
}

bool VaultManager::reorder_group(std::string_view group_id, int new_order) {
    if (!is_vault_open()) {
        return false;
    }

    if (m_group_manager->reorder_group(group_id, new_order)) {
        return save_vault();
    }
    return false;
}

// File I/O methods have been moved to VaultIO class

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

bool VaultManager::lock_memory(void* data, size_t size) {
    if (!data || size == 0) {
        return true;
    }

#ifdef __linux__
    if (mlock(data, size) == 0) {
        KeepTower::Log::debug("Locked {} bytes of sensitive memory (raw pointer)", size);
        return true;
    } else {
        KeepTower::Log::warning("Failed to lock memory: {} ({})", std::strerror(errno), errno);
        return false;
    }
#elif _WIN32
    if (VirtualLock(data, size)) {
        KeepTower::Log::debug("Locked {} bytes of sensitive memory (raw pointer)", size);
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

void VaultManager::unlock_memory(void* data, size_t size) {
    if (!data || size == 0) {
        return;
    }

#ifdef __linux__
    if (munlock(data, size) == 0) {
        KeepTower::Log::debug("Unlocked {} bytes of memory (raw pointer)", size);
    }
#elif _WIN32
    VirtualUnlock(data, size);
    KeepTower::Log::debug("Unlocked {} bytes of memory (raw pointer)", size);
#endif
}

// Backup management methods have been moved to VaultIO class

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

void VaultManager::set_backup_enabled(bool enable) {
    m_backup_enabled = enable;
    if (m_vault_open) {
        m_vault_data.set_backup_enabled(enable);
    }
}

bool VaultManager::set_backup_count(int count) {
    if (count < 1 || count > 50) [[unlikely]] {
        return false;
    }
    m_backup_count = count;
    if (m_vault_open) {
        m_vault_data.set_backup_count(count);
    }
    return true;
}

void VaultManager::set_clipboard_timeout(int timeout_seconds) {
    if (!m_vault_open) {
        return;
    }
    auto* metadata = m_vault_data.mutable_metadata();
    metadata->set_clipboard_timeout_seconds(timeout_seconds);
}

int VaultManager::get_clipboard_timeout() const {
    if (!m_vault_open || !m_vault_data.has_metadata()) {
        return 0;
    }
    return m_vault_data.metadata().clipboard_timeout_seconds();
}

void VaultManager::set_auto_lock_enabled(bool enabled) {
    if (!m_vault_open) {
        return;
    }
    auto* metadata = m_vault_data.mutable_metadata();
    metadata->set_auto_lock_enabled(enabled);
    m_modified = true;
}

bool VaultManager::get_auto_lock_enabled() const {
    if (!m_vault_open || !m_vault_data.has_metadata()) {
        return false;
    }
    return m_vault_data.metadata().auto_lock_enabled();
}

void VaultManager::set_auto_lock_timeout(int timeout_seconds) {
    if (!m_vault_open) {
        return;
    }
    auto* metadata = m_vault_data.mutable_metadata();
    metadata->set_auto_lock_timeout_seconds(timeout_seconds);
    m_modified = true;
}

int VaultManager::get_auto_lock_timeout() const {
    if (!m_vault_open || !m_vault_data.has_metadata()) {
        return 0;
    }
    return m_vault_data.metadata().auto_lock_timeout_seconds();
}

void VaultManager::set_undo_redo_enabled(bool enabled) {
    if (!m_vault_open) {
        return;
    }
    auto* metadata = m_vault_data.mutable_metadata();
    metadata->set_undo_redo_enabled(enabled);
    m_modified = true;
}

bool VaultManager::get_undo_redo_enabled() const {
    if (!m_vault_open || !m_vault_data.has_metadata()) {
        return false;
    }
    return m_vault_data.metadata().undo_redo_enabled();
}

void VaultManager::set_undo_history_limit(int limit) {
    if (!m_vault_open) {
        return;
    }
    auto* metadata = m_vault_data.mutable_metadata();
    metadata->set_undo_history_limit(limit);
    m_modified = true;
}

int VaultManager::get_undo_history_limit() const {
    if (!m_vault_open || !m_vault_data.has_metadata()) {
        return 0;
    }
    return m_vault_data.metadata().undo_history_limit();
}

void VaultManager::set_account_password_history_enabled(bool enabled) {
    if (!m_vault_open) {
        return;
    }
    auto* metadata = m_vault_data.mutable_metadata();
    metadata->set_account_password_history_enabled(enabled);
    m_modified = true;
}

bool VaultManager::get_account_password_history_enabled() const {
    if (!m_vault_open || !m_vault_data.has_metadata()) {
        return false;
    }
    return m_vault_data.metadata().account_password_history_enabled();
}

void VaultManager::set_account_password_history_limit(int limit) {
    if (!m_vault_open) {
        return;
    }
    auto* metadata = m_vault_data.mutable_metadata();
    metadata->set_account_password_history_limit(limit);
    m_modified = true;
}

int VaultManager::get_account_password_history_limit() const {
    if (!m_vault_open || !m_vault_data.has_metadata()) {
        return 0;
    }
    return m_vault_data.metadata().account_password_history_limit();
}

bool VaultManager::migrate_vault_schema() {
    return KeepTower::VaultSerialization::migrate_schema(m_vault_data, m_modified);
}

#ifdef HAVE_YUBIKEY_SUPPORT
std::vector<keeptower::YubiKeyEntry> VaultManager::get_yubikey_list() const {
    std::vector<keeptower::YubiKeyEntry> result;

    Log::info("VaultManager", "get_yubikey_list() called");

    if (!m_vault_open) {
        Log::info("VaultManager", "Vault not open, returning empty list");
        return result;
    }

    // V2 vault: Return per-user YubiKey entries
    if (m_is_v2_vault && m_v2_header.has_value()) {
        Log::info("VaultManager", std::format("V2 vault detected, {} key slots", m_v2_header->key_slots.size()));
        for (size_t i = 0; i < m_v2_header->key_slots.size(); ++i) {
            const auto& slot = m_v2_header->key_slots[i];
            Log::info("VaultManager", std::format("Slot {}: active={}, enrolled={}, username={}",
                i, slot.active, slot.yubikey_enrolled, slot.username));

            if (slot.active && slot.yubikey_enrolled) {
                // Safety check: ensure username and serial are valid
                if (slot.username.empty() || slot.yubikey_serial.empty()) {
                    KeepTower::Log::warning("VaultManager: Skipping invalid YubiKey entry (empty username or serial)");
                    continue;
                }

                keeptower::YubiKeyEntry entry;
                try {
                    entry.set_name(std::format("{}'s YubiKey", slot.username));
                    entry.set_serial(slot.yubikey_serial);
                    // Store timestamp directly (int64)
                    entry.set_added_at(slot.yubikey_enrolled_at);
                    Log::info("VaultManager", std::format("Added YubiKey entry: name={}, serial={}",
                        entry.name(), entry.serial()));
                    result.push_back(entry);
                } catch (const std::exception& e) {
                    KeepTower::Log::error("VaultManager: Error creating YubiKey entry: {}", e.what());
                    continue;
                }
            }
        }
        Log::info("VaultManager", std::format("Returning {} YubiKey entries", result.size()));
        return result;
    }

    // V1 vault: Use old YubiKey config method
    if (!m_yubikey_required) {
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
        std::cerr << "Vault must be open and YubiKey-protected to add backup keys" << '\n';
        return false;
    }

    // Initialize YubiKey and get device info
    YubiKeyManager yk_manager;
    if (!yk_manager.initialize()) {
        std::cerr << "Failed to initialize YubiKey" << '\n';
        return false;
    }

    if (!yk_manager.is_yubikey_present()) {
        std::cerr << "No YubiKey connected" << '\n';
        return false;
    }

    auto device_info = yk_manager.get_device_info();
    if (!device_info) {
        std::cerr << "Failed to get YubiKey device information" << '\n';
        return false;
    }

    // Check if already registered
    if (is_yubikey_authorized(device_info->serial_number)) {
        std::cerr << "YubiKey with serial " << device_info->serial_number << " is already registered" << '\n';
        return false;
    }

    // Verify the key works with the current challenge
    auto response = yk_manager.challenge_response(
        std::span<const unsigned char>(m_yubikey_challenge.data(), m_yubikey_challenge.size()),
        YubiKeyAlgorithm::HMAC_SHA1,  // V1 vaults use SHA-1
        false,
        YUBIKEY_TIMEOUT_MS
    );

    if (!response.success) {
        std::cerr << "YubiKey challenge-response failed. Key may not be programmed with same HMAC secret.\n";
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
        std::cerr << "Vault must be open and YubiKey-protected" << '\n';
        return false;
    }

    if (!m_vault_data.has_yubikey_config()) {
        return false;
    }

    auto* yk_config = m_vault_data.mutable_yubikey_config();

    // Cannot remove last key
    if (yk_config->yubikey_entries_size() <= 1) {
        std::cerr << "Cannot remove the last YubiKey" << '\n';
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

    std::cerr << "YubiKey with serial " << serial << " not found\n";
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

    // V2 vault authentication - verify against current user's key slot
    if (m_is_v2_vault) {
        if (!m_current_session) {
            return false;  // No active session
        }

        if (!m_v2_header.has_value()) {
            KeepTower::Log::error("VaultManager: V2 header not initialized");
            return false;
        }

        // Find current user's key slot
        KeySlot* user_slot = nullptr;
        for (auto& slot : m_v2_header->key_slots) {
            if (slot.active && slot.username == m_current_session->username) {
                user_slot = &slot;
                break;
            }
        }

        if (!user_slot) {
            return false;  // User not found
        }

        // Check if user's slot requires YubiKey
        bool slot_requires_yubikey = !user_slot->yubikey_serial.empty();

        if (slot_requires_yubikey) {
            // YubiKey authentication required for this slot
            if (serial.empty()) {
                KeepTower::Log::error("VaultManager: YubiKey serial required but not provided");
                return false;  // YubiKey serial required
            }

            // Verify YubiKey serial matches slot
            if (user_slot->yubikey_serial != serial) {
                KeepTower::Log::error("VaultManager: YubiKey serial mismatch");
                return false;
            }

#ifdef HAVE_YUBIKEY_SUPPORT
            // Perform YubiKey challenge-response
            YubiKeyManager yk_manager;
            if (!yk_manager.initialize() || !yk_manager.is_yubikey_present()) {
                KeepTower::Log::error("VaultManager: YubiKey not present");
                return false;
            }

            // Get device info to verify serial matches
            auto device_info = yk_manager.get_device_info();
            if (!device_info) {
                KeepTower::Log::error("VaultManager: Failed to get YubiKey device info");
                return false;
            }

            KeepTower::Log::debug("VaultManager: verify_credentials - YubiKey serial from device: '{}', expected from slot: '{}'",
                                 device_info->serial_number, user_slot->yubikey_serial);

            if (device_info->serial_number != serial) {
                KeepTower::Log::error("VaultManager: Serial mismatch - provided '{}' != device '{}'",
                                     serial, device_info->serial_number);
                return false;  // Provided serial doesn't match connected device
            }

            if (device_info->serial_number != user_slot->yubikey_serial) {
                KeepTower::Log::error("VaultManager: Wrong YubiKey - device '{}' != enrolled '{}'",
                                     device_info->serial_number, user_slot->yubikey_serial);
                return false;  // Wrong YubiKey connected (not the one enrolled)
            }

            // Validate challenge is not empty (size check removed - YubiKey library handles various sizes)
            if (user_slot->yubikey_challenge.empty()) {
                KeepTower::Log::error("VaultManager: YubiKey challenge is empty");
                return false;
            }

            // Perform challenge-response
            KeepTower::Log::debug("VaultManager: Starting YubiKey challenge-response (timeout: {}ms)", YUBIKEY_TIMEOUT_MS);
            auto cr_result = yk_manager.challenge_response(
                user_slot->yubikey_challenge,
                YubiKeyAlgorithm::HMAC_SHA1,  // Legacy vaults use SHA-1
                true,
                YUBIKEY_TIMEOUT_MS
            );
            if (!cr_result.success) {
                KeepTower::Log::error("YubiKey challenge-response failed in verify_credentials: {}",
                                     cr_result.error_message);
                return false;
            }

            // Derive password-based KEK first
            auto kek_result = KeyWrapping::derive_kek_from_password(
                password,
                user_slot->salt,
                m_v2_header->security_policy.pbkdf2_iterations);

            if (!kek_result) {
                KeepTower::Log::error("VaultManager: Failed to derive KEK from password");
                return false;
            }

            // XOR KEK with YubiKey response to get final KEK
            std::array<uint8_t, 32> final_kek = kek_result.value();
            for (size_t i = 0; i < final_kek.size() && i < cr_result.response.size(); i++) {
                final_kek[i] ^= cr_result.response[i];
            }

            // Try to unwrap DEK - if successful, credentials are correct
            auto unwrap_result = KeyWrapping::unwrap_key(final_kek, user_slot->wrapped_dek);

            // Securely clear sensitive data
            OPENSSL_cleanse(final_kek.data(), final_kek.size());

            return unwrap_result.has_value();
#else
            KeepTower::Log::error("VaultManager: YubiKey support not compiled");
            return false;
#endif
        } else {
            // No YubiKey required - just verify password
            // Derive KEK from password
            auto kek_result = KeyWrapping::derive_kek_from_password(
                password,
                user_slot->salt,
                m_v2_header->security_policy.pbkdf2_iterations);

            if (!kek_result) {
                return false;
            }

            // Try to unwrap DEK - if successful, password is correct
            auto unwrap_result = KeyWrapping::unwrap_key(kek_result.value(), user_slot->wrapped_dek);

            return unwrap_result.has_value();
        }
    }

    // V1 vault authentication below
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
        auto cr_result = yk_manager.challenge_response(
            m_yubikey_challenge,
            YubiKeyAlgorithm::HMAC_SHA1,  // V1 vaults use SHA-1
            true,
            YUBIKEY_TIMEOUT_MS
        );
        if (!cr_result.success) {
            KeepTower::Log::error("YubiKey challenge-response failed in verify_credentials: {}",
                                 cr_result.error_message);
            return false;
        }

        // Derive password-based key first
        std::vector<uint8_t> password_key(KEY_LENGTH);
        if (!KeepTower::VaultCrypto::derive_key(password, m_salt, password_key, m_pbkdf2_iterations)) {
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
    if (!KeepTower::VaultCrypto::derive_key(password, m_salt, test_key, m_pbkdf2_iterations)) {
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

// ============================================================================
// FIPS-140-3 Mode Management Implementation
// ============================================================================

/**
 * @brief Initialize OpenSSL FIPS provider for FIPS-140-3 compliant cryptography
 *
 * This method initializes the OpenSSL 3.5+ provider system and optionally loads
 * the FIPS cryptographic module. It uses atomic compare-and-exchange to ensure
 * single initialization even when called from multiple threads simultaneously.
 *
 * @section fips_init_process Initialization Process
 *
 * **When enable = true:**
 * 1. Attempt to load FIPS provider via OSSL_PROVIDER_load()
 * 2. If successful: Mark available=true, enabled=true
 * 3. If failed: Load default provider, mark available=false, enabled=false
 * 4. Log detailed status for troubleshooting
 *
 * **When enable = false:**
 * 1. Load default OpenSSL provider
 * 2. Mark available=false, enabled=false
 * 3. Skip FIPS provider loading entirely
 *
 * @section fips_init_concurrency Thread Safety
 *
 * Uses atomic compare-and-exchange pattern:
 * @code
 * bool expected = false;
 * if (!s_fips_mode_initialized.compare_exchange_strong(expected, true)) {
 *     return true;  // Already initialized by another thread
 * }
 * // ... perform initialization (only one thread gets here)
 * @endcode
 *
 * This guarantees:
 * - Only one thread performs initialization
 * - All threads see consistent initialization state
 * - No deadlocks or race conditions
 * - Subsequent calls return immediately with cached result
 *
 * @section fips_init_requirements FIPS Provider Requirements
 *
 * For FIPS provider to load successfully:
 * - OpenSSL 3.5.0+ installed with FIPS module
 * - fipsmodule.cnf exists and is readable
 * - Valid openssl.cnf configuration OR OPENSSL_CONF env variable
 * - FIPS module self-tests pass (automatic on load)
 * - Sufficient permissions to load shared libraries
 *
 * @section fips_init_failure_handling Failure Handling
 *
 * **FIPS provider load failure is non-fatal:**
 * - Application continues with default provider
 * - Logs warning about FIPS unavailability
 * - Users can still encrypt/decrypt vaults
 * - UI shows FIPS as unavailable
 *
 * This graceful degradation ensures usability even when FIPS
 * configuration is incorrect or unavailable.
 *
 * @param enable If true, attempts to load FIPS provider; if false, uses default provider
 * @return true if initialization successful, false only on catastrophic OpenSSL failure
 *
 * @post s_fips_mode_initialized == true
 * @post s_fips_mode_available reflects FIPS provider load status
 * @post s_fips_mode_enabled == true only if FIPS provider loaded successfully
 *
 * @note **Single Initialization:** Subsequent calls return true without re-initializing
 * @note **Process-Wide Effect:** Affects all OpenSSL operations in the process
 *
 * @see VaultManager::is_fips_available() to check if FIPS provider loaded
 * @see VaultManager::is_fips_enabled() to check current FIPS status
 * @see VaultManager::set_fips_mode() to toggle FIPS mode after initialization
 *
 * @par Implementation Details:
 * - Uses OSSL_PROVIDER_load() from OpenSSL 3.5 provider API
 * - Relies on OpenSSL configuration files for FIPS module location
 * - Does not explicitly configure provider properties (uses OpenSSL defaults)
 * - Logs all state transitions with KeepTower::Log for debugging
 *
 * @warning Do not call after any cryptographic operations have been performed.
 *          Provider initialization must happen before first crypto operation.
 */
bool VaultManager::init_fips_mode(bool enable) {
    // Thread-safe single initialization using atomic compare-exchange
    // Only the first thread to call this will perform initialization
    bool expected = false;
    if (!s_fips_mode_initialized.compare_exchange_strong(expected, true)) {
        KeepTower::Log::warning("FIPS mode already initialized");
        // Return true = initialization already succeeded (we have a working provider)
        // Don't return s_fips_mode_available because that indicates FIPS provider
        // availability, not whether initialization succeeded overall
        return true;
    }

    KeepTower::Log::info("Initializing OpenSSL FIPS mode (enable={})", enable);

    // Force OpenSSL to load configuration file (if OPENSSL_CONF is set)
    // This ensures providers specified in the config are loaded before we check
    OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CONFIG, nullptr);

    // Check if FIPS provider is already available (e.g., loaded via OPENSSL_CONF)
    // Use OSSL_PROVIDER_try_load which respects configuration files
    OSSL_PROVIDER* fips_provider = OSSL_PROVIDER_try_load(nullptr, "fips", 1);

    if (fips_provider == nullptr) {
        KeepTower::Log::warning("FIPS provider not available - using default provider");
        s_fips_mode_available.store(false);
        s_fips_mode_enabled.store(false);

        // Ensure default provider is available
        // try_load returns existing provider if already loaded, or loads it
        OSSL_PROVIDER* default_provider = OSSL_PROVIDER_try_load(nullptr, "default", 1);
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
        // try_load will return existing provider if already loaded
        OSSL_PROVIDER* default_provider = OSSL_PROVIDER_try_load(nullptr, "default", 1);
        if (default_provider == nullptr) {
            KeepTower::Log::warning("Failed to load default provider alongside FIPS");
        }

        s_fips_mode_enabled.store(false);
        KeepTower::Log::info("FIPS mode available but not enabled");
    }

    return true;
}

/**
 * @brief Query whether OpenSSL FIPS provider is available
 *
 * Checks if the FIPS cryptographic provider was successfully loaded during
 * initialization. This is a cached check (no provider probe occurs).
 *
 * @section fips_avail_behavior Availability Status
 *
 * **Returns true when:**
 * - init_fips_mode() was called with enable=true
 * - FIPS provider loaded successfully
 * - FIPS module self-tests passed
 *
 * **Returns false when:**
 * - init_fips_mode() not called yet (initialization check)
 * - init_fips_mode() was called with enable=false
 * - FIPS provider load failed (missing module, config error, etc.)
 *
 * @section fips_avail_ui_integration UI Integration Pattern
 *
 * Use this method to control UI elements:
 * @code
 * // In PreferencesDialog::setup_security_page()
 * if (VaultManager::is_fips_available()) {
 *     m_fips_mode_check.set_sensitive(true);  // Enable checkbox
 *     m_fips_status_label.set_text("✓ FIPS module available");
 * } else {
 *     m_fips_mode_check.set_sensitive(false);  // Disable checkbox
 *     m_fips_status_label.set_text("⚠️ FIPS module not available");
 * }
 * @endcode
 *
 * @return true if FIPS provider is available, false otherwise
 * @retval true FIPS provider loaded and ready to use
 * @retval false FIPS provider not available (or not initialized)
 *
 * @pre init_fips_mode() should have been called at application startup
 *
 * @note Logs warning if called before initialization
 * @note Does not perform runtime provider availability check (cached result only)
 *
 * @see init_fips_mode() to initialize FIPS provider
 * @see is_fips_enabled() to check if FIPS mode is currently active
 */
bool VaultManager::is_fips_available() {
    if (!s_fips_mode_initialized.load()) {
        KeepTower::Log::warning("FIPS mode not initialized - call init_fips_mode() first");
        return false;
    }
    return s_fips_mode_available.load();
}

/**
 * @brief Query whether FIPS-140-3 mode is currently active
 *
 * Checks if FIPS mode is enabled and all cryptographic operations are using
 * the FIPS-validated provider. This is a real-time status check.
 *
 * @section fips_enabled_states Enabled States
 *
 * **Returns true when:**
 * - FIPS provider is available (is_fips_available() == true)
 * - FIPS mode was enabled via init_fips_mode(true) or set_fips_mode(true)
 * - All new crypto operations use FIPS provider
 *
 * **Returns false when:**
 * - init_fips_mode() not called yet
 * - FIPS provider not available
 * - FIPS mode explicitly disabled
 * - Using default OpenSSL provider
 *
 * @section fips_enabled_compliance Compliance Verification
 *
 * For FIPS-140-3 compliance, applications must verify this returns true:
 * @code
 * if (requires_fips_compliance()) {
 *     if (!VaultManager::is_fips_enabled()) {
 *         Log::error("FIPS mode required but not enabled");
 *         show_compliance_error();
 *         return false;
 *     }
 *     Log::info("FIPS-140-3 compliance active");
 * }
 * @endcode
 *
 * @section fips_enabled_display Status Display Pattern
 *
 * Display FIPS status to users:
 * @code
 * std::string fips_status;
 * if (VaultManager::is_fips_enabled()) {
 *     fips_status = "FIPS-140-3: Enabled ✓";
 * } else if (VaultManager::is_fips_available()) {
 *     fips_status = "FIPS-140-3: Available (not enabled)";
 * } else {
 *     fips_status = "FIPS-140-3: Not available";
 * }
 * @endcode
 *
 * @return true if FIPS mode is currently active, false otherwise
 * @retval true All cryptographic operations use FIPS provider (compliant)
 * @retval false Using default provider or not initialized
 *
 * @pre init_fips_mode() should have been called at application startup
 *
 * @note Logs warning if called before initialization
 * @note This reflects current operational state, which may change via set_fips_mode()
 *
 * @see init_fips_mode() to set initial FIPS mode
 * @see is_fips_available() to check if FIPS provider is installed
 * @see set_fips_mode() to toggle FIPS mode at runtime
 */
bool VaultManager::is_fips_enabled() {
    if (!s_fips_mode_initialized.load()) {
        KeepTower::Log::warning("FIPS mode not initialized - call init_fips_mode() first");
        return false;
    }
    return s_fips_mode_enabled.load();
}

/**
 * @brief Enable or disable FIPS-140-3 mode at runtime
 *
 * Dynamically switches between FIPS and default cryptographic providers using
 * OpenSSL's EVP_default_properties_enable_fips() API. This allows toggling
 * FIPS mode without process restart (though restart is recommended).
 *
 * @section fips_set_process Provider Switching Process
 *
 * **To Enable FIPS (enable = true):**
 * 1. Verify FIPS provider is available
 * 2. Call EVP_default_properties_enable_fips(NULL, 1)
 * 3. Update s_fips_mode_enabled to true
 * 4. All new crypto operations use FIPS provider
 *
 * **To Disable FIPS (enable = false):**
 * 1. Call EVP_default_properties_enable_fips(NULL, 0)
 * 2. Update s_fips_mode_enabled to false
 * 3. All new crypto operations use default provider
 *
 * @section fips_set_limitations Runtime Switching Limitations
 *
 * **Potential Issues:**
 * - Existing EVP_CIPHER_CTX contexts may continue using old provider
 * - Some OpenSSL configurations make FIPS mode irreversible
 * - Thread-local contexts may not switch immediately
 * - FIPS self-tests only run once during provider load
 *
 * **Best Practice:**
 * Always display a restart warning when toggling FIPS mode:
 * @code
 * if (VaultManager::set_fips_mode(new_state)) {
 *     show_info_dialog(
 *         "FIPS mode changed. Please restart the application\n"
 *         "for the change to take full effect."
 *     );
 * }
 * @endcode
 *
 * @section fips_set_validation Input Validation
 *
 * **Method fails (returns false) when:**
 * - init_fips_mode() not called (s_fips_mode_initialized == false)
 * - Trying to enable FIPS when provider not available
 * - OpenSSL provider switching API fails
 *
 * **Method succeeds (returns true) when:**
 * - Already in requested state (idempotent, no-op)
 * - Successfully switched to requested provider
 *
 * @section fips_set_error_handling Error Handling
 *
 * Detailed error logging using OpenSSL error queue:
 * @code
 * unsigned long err = ERR_get_error();
 * char err_buf[256];
 * ERR_error_string_n(err, err_buf, sizeof(err_buf));
 * Log::error("OpenSSL error: {}", err_buf);
 * @endcode
 *
 * Common error causes:
 * - FIPS provider not available (user trying to enable without FIPS module)
 * - OpenSSL internal state corruption (rare)
 * - Invalid provider configuration
 *
 * @param enable If true, enable FIPS mode; if false, use default provider
 *
 * @return true if mode change successful or already in requested state, false on error
 * @retval true FIPS mode successfully changed to requested state
 * @retval true Already in requested state (no action taken, success)
 * @retval false Not initialized (must call init_fips_mode() first)
 * @retval false FIPS provider unavailable (when trying to enable)
 * @retval false OpenSSL provider switch API failed
 *
 * @pre init_fips_mode() must have been called
 * @pre FIPS provider must be available (for enable = true)
 *
 * @post is_fips_enabled() will return the new state (if successful)
 * @post New cryptographic operations use the selected provider
 *
 * @note Idempotent: Calling with current state is a no-op (returns true)
 * @note Application restart recommended for consistent behavior
 *
 * @see init_fips_mode() to perform initial initialization
 * @see is_fips_available() to check if FIPS provider is loaded
 * @see is_fips_enabled() to query current FIPS state
 *
 * @warning Some cryptographic contexts may not immediately reflect the change.
 *          Recommend application restart for guaranteed consistency.
 */
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

// Backup path management
void VaultManager::set_backup_path(const std::string& path) {
    m_backup_path = path;
}

// Restore from most recent backup
KeepTower::VaultResult<> VaultManager::restore_from_most_recent_backup(const std::string& vault_path) {
    namespace fs = std::filesystem;

    if (m_vault_open) {
        KeepTower::Log::error("VaultManager: Cannot restore while vault is open");
        return std::unexpected(KeepTower::VaultError::VaultAlreadyOpen);
    }

    // Get list of backups (respects m_backup_path)
    auto backups = KeepTower::VaultIO::list_backups(vault_path, m_backup_path);

    if (backups.empty()) {
        KeepTower::Log::error("VaultManager: No backups found for vault");
        return std::unexpected(KeepTower::VaultError::FileNotFound);
    }

    try {
        // Backups are sorted newest first
        const std::string& most_recent_backup = backups[0];
        fs::copy_file(most_recent_backup, vault_path, fs::copy_options::overwrite_existing);
        KeepTower::Log::info("VaultManager: Restored vault from backup: {}", most_recent_backup);
        return {};
    } catch (const fs::filesystem_error& e) {
        KeepTower::Log::error("VaultManager: Failed to restore from backup: {}", e.what());
        return std::unexpected(KeepTower::VaultError::FileReadFailed);
    }
}

std::string VaultManager::get_current_username() const {
    if (m_is_v2_vault && m_current_session) {
        return m_current_session->username;
    }
    return "";
}

bool VaultManager::current_user_requires_yubikey() const {
    // V1 vaults: use global flag
    if (!m_is_v2_vault) {
        return m_yubikey_required;
    }

    // V2 vaults: check current user's key slot
    if (!m_current_session || !m_v2_header.has_value()) {
        return false;
    }

    // Find current user's key slot
    for (const auto& slot : m_v2_header->key_slots) {
        if (slot.active && slot.username == m_current_session->username) {
            return slot.yubikey_enrolled;
        }
    }

    return false;
}
