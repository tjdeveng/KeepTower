// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file VaultFormatV2.h
 * @brief Version 2 vault file format with multi-user support
 *
 * V2 format introduces LUKS-style key slots for multi-user authentication.
 * The vault header (security policy + key slots) is FEC-protected separately
 * from encrypted data to ensure critical authentication data survives corruption.
 *
 * @section v2_format V2 File Format
 * @code
 * +------------------+
 * | Magic: 0x4B505457| 4 bytes  ("KPTW" = KeepTower)
 * | Version: 2       | 4 bytes
 * | PBKDF2 Iters     | 4 bytes
 * | Header Size      | 4 bytes  (size of FEC-protected header)
 * +------------------+
 * | Header Flags     | 1 byte   (FEC enabled, etc.)
 * | [FEC metadata]   | Variable (if FEC enabled)
 * | Header Data      | Variable (security policy + key slots)
 * | [FEC Parity]     | Variable (if FEC enabled)
 * +------------------+
 * | Data Salt        | 32 bytes (for encrypting vault data)
 * | Data IV          | 12 bytes (for encrypting vault data)
 * | [Encrypted Data] | Variable (protobuf-serialized accounts)
 * | [Data FEC]       | Variable (if FEC enabled for data)
 * +------------------+
 * @endcode
 *
 * @section fec_strategy FEC Protection Strategy
 * - **Header FEC**: Protects security policy and key slots (critical for authentication)
 *   - Minimum 20% redundancy (can recover from ~10% corruption)
 *   - Uses max(20%, user_preference) to respect higher user settings
 *   - If user sets 30% or 50% for vault data, header gets same protection
 * - **Data FEC**: Protects encrypted account data (user-configurable)
 * - Both can be enabled/disabled independently
 */

#ifndef VAULTFORMATV2_H
#define VAULTFORMATV2_H

#include "MultiUserTypes.h"
#include "ReedSolomon.h"
#include "VaultError.h"
#include <vector>
#include <cstdint>
#include <expected>
#include <memory>

namespace KeepTower {

/**
 * @brief V2 vault file format handler
 *
 * Manages reading and writing of V2 vault files with:
 * - Multi-user key slots
 * - FEC-protected headers
 * - Backward compatibility with V1 format
 */
class VaultFormatV2 {
public:
    /** @brief Magic number for vault files: "KPTW" (KeepTower) */
    static constexpr uint32_t VAULT_MAGIC = 0x4B505457;

    /** @brief Vault format version 2 (multi-user) */
    static constexpr uint32_t VAULT_VERSION_V2 = 2;

    /** @brief Vault format version 1 (legacy, single-user) */
    static constexpr uint32_t VAULT_VERSION_V1 = 1;

    /** @brief FEC enabled flag for header */
    static constexpr uint8_t HEADER_FLAG_FEC_ENABLED = 0x01;

    /** @brief Minimum header FEC redundancy percentage (20% = ~10% corruption recovery) */
    static constexpr uint8_t MIN_HEADER_FEC_REDUNDANCY = 20;

    /** @brief Maximum reasonable header size (1MB) to prevent DoS attacks */
    static constexpr uint32_t MAX_HEADER_SIZE = 1024 * 1024;

    /**
     * @brief V2 vault file header
     */
    struct V2FileHeader {
        uint32_t magic = VAULT_MAGIC;              ///< Magic: "KPTW"
        uint32_t version = VAULT_VERSION_V2;       ///< Version: 2
        uint32_t pbkdf2_iterations = 100000;       ///< PBKDF2 iteration count
        uint32_t header_size = 0;                  ///< Size of (FEC-protected header data)
        uint8_t header_flags = 0;                  ///< FEC enabled, etc.
        uint8_t fec_redundancy_percent = 0;        ///< FEC redundancy percentage (if enabled)

        VaultHeaderV2 vault_header;                ///< Security policy + key slots

        std::array<uint8_t, 32> data_salt;         ///< Salt for encrypting vault data
        std::array<uint8_t, 12> data_iv;           ///< IV for encrypting vault data
    };

    /**
     * @brief Write V2 vault header to binary format
     *
     * Serializes the vault header (security policy + key slots) and applies
     * FEC protection if enabled. The result is a complete V2 file header ready
     * to be written to disk followed by encrypted vault data.
     *
     * The header FEC redundancy uses max(20%, user_preference) to ensure critical
     * authentication data has minimum protection while respecting higher user settings.
     *
     * @param header V2 file header
     * @param enable_header_fec Enable FEC protection for header (recommended)
     * @param user_fec_redundancy User's FEC preference percentage (0-50, default 0 = use minimum)
     * @return Binary representation of header, or error
     */
    [[nodiscard]] static KeepTower::VaultResult<std::vector<uint8_t>>
    write_header(const V2FileHeader& header,
                 bool enable_header_fec = true,
                 uint8_t user_fec_redundancy = 0);

    /**
     * @brief Read V2 vault header from binary format
     *
     * Parses and validates V2 vault header, applying FEC decoding if enabled.
     * Returns the deserialized header and offset to encrypted data.
     *
     * @param file_data Complete vault file data
     * @return Pair of (header, data_offset), or error
     */
    [[nodiscard]] static KeepTower::VaultResult<std::pair<V2FileHeader, size_t>>
    read_header(const std::vector<uint8_t>& file_data);

    /**
     * @brief Detect vault file version
     *
     * Reads magic number and version from file without full parsing.
     *
     * @param file_data Vault file data (at least 8 bytes)
     * @return Vault version (1 or 2), or error
     */
    [[nodiscard]] static KeepTower::VaultResult<uint32_t>
    detect_version(const std::vector<uint8_t>& file_data);

    /**
     * @brief Check if file is a valid V2 vault
     *
     * Quick validation without full deserialization.
     *
     * @param file_data Vault file data
     * @return true if V2 format is valid
     */
    [[nodiscard]] static bool is_valid_v2_vault(const std::vector<uint8_t>& file_data);

private:
    /**
     * @brief Apply FEC protection to header data
     *
     * @param header_data Serialized header (security policy + key slots)
     * @param encoding_redundancy Redundancy percentage for encoding (actual protection level)
     * @param stored_redundancy Redundancy percentage to store in header (user preference)
     * @return FEC-protected data (header + parity), or error
     */
    [[nodiscard]] static KeepTower::VaultResult<std::vector<uint8_t>>
    apply_header_fec(const std::vector<uint8_t>& header_data,
                     uint8_t encoding_redundancy,
                     uint8_t stored_redundancy);

    /**
     * @brief Remove FEC protection from header data
     *
     * @param protected_data FEC-protected header data
     * @param original_size Original header size (before FEC)
     * @param redundancy FEC redundancy percentage used
     * @return Recovered header data, or error
     */
    [[nodiscard]] static KeepTower::VaultResult<std::vector<uint8_t>>
    remove_header_fec(const std::vector<uint8_t>& protected_data,
                      uint32_t original_size,
                      uint8_t redundancy);
};

} // namespace KeepTower

#endif // VAULTFORMATV2_H
