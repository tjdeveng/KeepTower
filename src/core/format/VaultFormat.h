// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 TJDev
/**
 * @file VaultFormat.h
 * @brief Vault file format parsing and encoding utilities
 *
 * This module provides utilities for parsing and encoding vault file formats,
 * including support for Reed-Solomon FEC, YubiKey metadata, and format versioning.
 */

#ifndef KEEPTOWER_VAULT_FORMAT_H
#define KEEPTOWER_VAULT_FORMAT_H

#include "../VaultError.h"
#include "../ReedSolomon.h"
#include <vector>
#include <cstdint>
#include <string>
#include <memory>

namespace KeepTower {

/**
 * @struct VaultFileMetadata
 * @brief Metadata extracted from vault file format
 *
 * Contains information about encryption parameters, FEC settings,
 * and YubiKey requirements extracted from the vault file header.
 */
struct VaultFileMetadata {
    std::vector<uint8_t> salt;              ///< PBKDF2 salt (16 bytes)
    std::vector<uint8_t> iv;                ///< AES-GCM IV (12 bytes)
    bool has_fec = false;                   ///< Whether Reed-Solomon FEC is enabled
    uint8_t fec_redundancy = 0;             ///< FEC redundancy percentage (0-100)
    bool requires_yubikey = false;          ///< Whether YubiKey authentication is required
    std::string yubikey_serial;             ///< YubiKey serial number (if required)
    std::vector<uint8_t> yubikey_challenge; ///< YubiKey challenge data (64 bytes)
};

/**
 * @struct ParsedVaultData
 * @brief Result of parsing a vault file
 *
 * Contains the extracted ciphertext and associated metadata
 * from a vault file after format parsing and FEC decoding.
 */
struct ParsedVaultData {
    std::vector<uint8_t> ciphertext; ///< Decrypted vault ciphertext
    VaultFileMetadata metadata;      ///< Extracted file metadata
};

/**
 * @class VaultFormat
 * @brief Static utility class for vault file format operations
 *
 * This class provides methods for:
 * - Parsing V1 vault file format with FEC support
 * - Extracting metadata from vault files
 * - Decoding Reed-Solomon FEC when present
 * - Handling YubiKey metadata
 *
 * ## Vault V1 File Format (Single-User)
 *
 * ### Basic Format (no FEC, no YubiKey):
 * ```
 * [salt(32)][iv(12)][ciphertext]
 * ```
 *
 * ### With Flags Byte:
 * ```
 * [salt(32)][iv(12)][flags(1)][ciphertext]
 * ```
 *
 * ### With YubiKey (no FEC):
 * ```
 * [salt(32)][iv(12)][flags(1)][serial_len(1)][serial][challenge(64)][ciphertext]
 * ```
 *
 * ### With FEC (Reed-Solomon):
 * ```
 * [salt(32)][iv(12)][flags(1)][redundancy(1)][original_size(4)]
 * [optional: YubiKey metadata][encoded_ciphertext]
 * ```
 *
 * ### V1 FEC Behavior:
 * - **No header FEC protection** - header is plaintext
 * - **Optional data FEC** - User-configurable via preferences
 * - FEC only applied to ciphertext when enabled
 *
 * ## Vault V2 File Format (Multi-User, LUKS-Style)
 *
 * V2 vaults use a different format with LUKS-style key slots and are handled
 * by VaultFormatV2 class. Key V2 characteristics:
 *
 * ### V2 Header FEC Protection:
 * - **Always enabled** for header protection (LUKS-style header includes key slots)
 * - **Minimum 20% redundancy** - can recover from ~10% header corruption
 * - **Adaptive redundancy** - uses max(20%, user_data_preference)
 *   - If user sets 30% for data → header gets 30%
 *   - If user sets 50% for data → header gets 50%
 *   - Critical authentication data always protected at ≥20%
 * - Separate FEC for header vs data allows guaranteed minimum protection
 *
 * ### V2 Format Structure:
 * ```
 * [magic(4)][version(4)][pbkdf2_iters(4)][header_size(4)]
 * [FEC_encoded_header: key_slots, security_policy, metadata]
 * [data_content with optional FEC]
 * ```
 *
 * ## Flags Byte (V1 only - bit fields):
 * - Bit 0 (0x01): Reed-Solomon FEC enabled
 * - Bit 1 (0x02): YubiKey required
 * - Bits 2-7: Reserved (must be 0)
 *
 * ## Thread Safety
 * All methods are thread-safe as they operate on provided data
 * without maintaining shared state.
 *
 * ## Example Usage
 * @code
 * // Parse vault file
 * std::vector<uint8_t> file_data = read_file(...);
 * auto result = VaultFormat::parse(file_data);
 * if (result) {
 *     ParsedVaultData& parsed = result.value();
 *     // Use parsed.ciphertext and parsed.metadata
 * }
 * @endcode
 */
class VaultFormat {
public:
    /**
     * @brief Parse V1 vault file format
     *
     * Parses a V1 vault file, extracting metadata and ciphertext.
     * Automatically handles:
     * - Legacy format without flags byte
     * - Modern format with flags byte
     * - Reed-Solomon FEC decoding when enabled
     * - YubiKey metadata extraction
     *
     * @param file_data Raw vault file data to parse
     * @return VaultResult containing ParsedVaultData with ciphertext and metadata,
     *         or VaultError on failure:
     *         - VaultError::CorruptedFile: File too small or invalid format
     *         - VaultError::DecodingFailed: Reed-Solomon decoding failed
     *
     * @note The ciphertext in the result is still encrypted and must be
     *       decrypted separately using VaultCrypto::decrypt_data()
     *
     * @see VaultCrypto::decrypt_data
     * @see VaultSerialization::deserialize
     */
    [[nodiscard]] static VaultResult<ParsedVaultData> parse(
        const std::vector<uint8_t>& file_data);

    /**
     * @brief Decode Reed-Solomon encoded data
     *
     * Decodes data that was encoded with Reed-Solomon forward error correction.
     * This is an internal helper used by parse() but can be called independently
     * for testing or special use cases.
     *
     * @param encoded_data Reed-Solomon encoded data
     * @param original_size Original size before encoding (bytes)
     * @param redundancy Redundancy percentage used during encoding (0-100)
     * @param reed_solomon_instance Shared ReedSolomon instance (created if null)
     * @param current_redundancy Current redundancy of the instance (updated if recreated)
     * @return VaultResult containing decoded data, or VaultError::DecodingFailed
     *
     * @note If reed_solomon_instance is null or current_redundancy doesn't match,
     *       a new ReedSolomon instance will be created
     *
     * @see ReedSolomon
     */
    [[nodiscard]] static VaultResult<std::vector<uint8_t>> decode_with_reed_solomon(
        const std::vector<uint8_t>& encoded_data,
        uint32_t original_size,
        uint8_t redundancy,
        std::unique_ptr<ReedSolomon>& reed_solomon_instance,
        uint8_t& current_redundancy);

private:
    /// Minimum file size: salt (16) + IV (12) = 28 bytes
    static constexpr size_t MIN_FILE_SIZE = 44;  // 32-byte salt + 12-byte IV

    /// Salt length in bytes (PBKDF2)
    static constexpr size_t SALT_LENGTH = 32;

    /// IV length in bytes (AES-GCM)
    static constexpr size_t IV_LENGTH = 12;

    /// Vault header size: flags(1) + redundancy(1) + original_size(4) = 6 bytes
    static constexpr size_t VAULT_HEADER_SIZE = 6;

    /// YubiKey challenge size in bytes
    static constexpr size_t YUBIKEY_CHALLENGE_SIZE = 64;

    /// Flag bit: Reed-Solomon encoding enabled
    static constexpr uint8_t FLAG_RS_ENABLED = 0x01;

    /// Flag bit: YubiKey authentication required
    static constexpr uint8_t FLAG_YUBIKEY_REQUIRED = 0x02;

    /// Minimum acceptable Reed-Solomon redundancy percentage
    static constexpr uint8_t MIN_RS_REDUNDANCY = 1;

    /// Maximum acceptable Reed-Solomon redundancy percentage
    static constexpr uint8_t MAX_RS_REDUNDANCY = 100;

    /// Maximum vault size (for validation)
    static constexpr size_t MAX_VAULT_SIZE = 1024 * 1024 * 1024;  // 1 GB

    /// Big-endian bit shift values
    static constexpr int BIGENDIAN_SHIFT_24 = 24;
    static constexpr int BIGENDIAN_SHIFT_16 = 16;
    static constexpr int BIGENDIAN_SHIFT_8 = 8;

    VaultFormat() = delete;  ///< Static-only class, no instances
    ~VaultFormat() = delete;  ///< Static-only class, no instances
    VaultFormat(const VaultFormat&) = delete;  ///< No copy
    VaultFormat& operator=(const VaultFormat&) = delete;  ///< No copy
    VaultFormat(VaultFormat&&) = delete;  ///< No move
    VaultFormat& operator=(VaultFormat&&) = delete;  ///< No move
};

}  // namespace KeepTower

#endif  // KEEPTOWER_VAULT_FORMAT_H
