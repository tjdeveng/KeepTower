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

#include "core/MultiUserTypes.h"
#include "lib/fec/ReedSolomon.h"
#include "core/VaultError.h"
#include <vector>
#include <cstdint>
#include <expected>
#include <memory>

namespace KeepTower {

/**
 * @brief Stateless serializer/parser for the V2 vault file header format.
 *
 * VaultFormatV2 owns the on-disk framing rules for V2 headers, including
 * version detection, header serialization, and header-level FEC handling.
 */
class VaultFormatV2 {
public:
    static constexpr uint32_t VAULT_MAGIC = 0x4B505457;              ///< Magic number for KeepTower vault files.
    static constexpr uint32_t VAULT_VERSION_V2 = 2;                  ///< Supported V2 on-disk version.
    static constexpr uint8_t HEADER_FLAG_FEC_ENABLED = 0x01;         ///< Header bit flag indicating header FEC is enabled.
    static constexpr uint8_t MIN_HEADER_FEC_REDUNDANCY = 20;         ///< Minimum redundancy percent for header protection.
    static constexpr uint32_t MAX_HEADER_SIZE = 1024 * 1024;         ///< Maximum supported serialized header size in bytes.

    /**
     * @brief Parsed V2 file header plus authentication metadata.
     */
    struct V2FileHeader {
        uint32_t magic = VAULT_MAGIC;                  ///< File magic value.
        uint32_t version = VAULT_VERSION_V2;          ///< Parsed vault format version.
        uint32_t pbkdf2_iterations = 100000;          ///< Legacy/open-time PBKDF2 iteration hint.
        uint32_t header_size = 0;                     ///< Serialized protected header size in bytes.
        uint8_t header_flags = 0;                     ///< Header flags bitfield.
        uint8_t fec_redundancy_percent = 0;           ///< Stored header FEC redundancy percent.

        VaultHeaderV2 vault_header;                   ///< Structured security policy and key-slot header.

        std::array<uint8_t, 32> data_salt;           ///< Salt for data-encryption derivation.
        std::array<uint8_t, 12> data_iv;             ///< IV for encrypted vault payload.
    };

    /**
     * @brief Serialize a V2 header to on-disk bytes.
     * @param header Parsed header fields to serialize.
     * @param enable_header_fec True to protect the header with FEC.
     * @param user_fec_redundancy User-configured redundancy percent for data/header policy.
     * @return Encoded header bytes or an error.
     */
    [[nodiscard]] static KeepTower::VaultResult<std::vector<uint8_t>>
    write_header(const V2FileHeader& header,
                 bool enable_header_fec = true,
                 uint8_t user_fec_redundancy = 0);

    /**
     * @brief Parse a V2 header from raw file bytes.
     * @param file_data Complete file bytes or a prefix containing the header.
     * @return Parsed header and header byte length, or an error.
     */
    [[nodiscard]] static KeepTower::VaultResult<std::pair<V2FileHeader, size_t>>
    read_header(const std::vector<uint8_t>& file_data);

    /**
     * @brief Detect the vault format version from raw file bytes.
     * @param file_data Raw file bytes.
     * @return Parsed version number or an error.
     */
    [[nodiscard]] static KeepTower::VaultResult<uint32_t>
    detect_version(const std::vector<uint8_t>& file_data);

    /**
     * @brief Validate whether raw bytes appear to be a supported V2 vault.
     * @param file_data Raw file bytes.
     * @return True when the magic/version/header framing are valid for V2.
     */
    [[nodiscard]] static bool is_valid_v2_vault(const std::vector<uint8_t>& file_data);

private:
    /**
     * @brief Encode header bytes with FEC protection.
     * @param header_data Serialized header bytes.
     * @param encoding_redundancy Redundancy used when encoding.
     * @param stored_redundancy Redundancy value recorded in the file.
     * @return Protected header bytes or an error.
     */
    [[nodiscard]] static KeepTower::VaultResult<std::vector<uint8_t>>
    apply_header_fec(const std::vector<uint8_t>& header_data,
                     uint8_t encoding_redundancy,
                     uint8_t stored_redundancy);

    /**
     * @brief Decode FEC-protected header bytes.
     * @param protected_data Encoded header bytes with redundancy payload.
     * @param original_size Original unprotected header size.
     * @param redundancy Stored redundancy percent.
     * @return Decoded header bytes or an error.
     */
    [[nodiscard]] static KeepTower::VaultResult<std::vector<uint8_t>>
    remove_header_fec(const std::vector<uint8_t>& protected_data,
                      uint32_t original_size,
                      uint8_t redundancy);
};

} // namespace KeepTower

#endif // VAULTFORMATV2_H