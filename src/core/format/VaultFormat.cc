// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 TJDev

#include "VaultFormat.h"
#include "../utils/Log.h"

namespace KeepTower {

VaultResult<ParsedVaultData>
VaultFormat::parse(const std::vector<uint8_t>& file_data) {
    ParsedVaultData result;

    // Validate minimum file size
    if (file_data.size() < SALT_LENGTH + IV_LENGTH) {
        return std::unexpected(VaultError::CorruptedFile);
    }

    // Extract salt
    result.metadata.salt.assign(file_data.begin(),
                                file_data.begin() + static_cast<std::ptrdiff_t>(SALT_LENGTH));

    // Extract IV
    result.metadata.iv.assign(
        file_data.begin() + static_cast<std::ptrdiff_t>(SALT_LENGTH),
        file_data.begin() + static_cast<std::ptrdiff_t>(SALT_LENGTH + IV_LENGTH));

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
                    // Validate serial_len to prevent integer overflow in size calculations
                    if (serial_len > 0 && serial_len <= 255 &&
                        data_offset + 1 + serial_len + YUBIKEY_CHALLENGE_SIZE <= file_data.size()) {
                        yk_metadata_size = 1 + serial_len + YUBIKEY_CHALLENGE_SIZE;
                    } else {
                        // Invalid serial length - treat as corrupted file
                        Log::warning("VaultFormat: Invalid YubiKey serial length in FEC header ({}) or insufficient data", serial_len);
                        return std::unexpected(VaultError::CorruptedFile);
                    }
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

                        // Validate serial_len is reasonable (max 255 bytes, but typically < 50)
                        // and we have enough data remaining
                        if (serial_len > 0 && serial_len <= 255 &&
                            ciphertext_offset + serial_len + YUBIKEY_CHALLENGE_SIZE <= file_data.size()) {
                            result.metadata.yubikey_serial.assign(
                                file_data.begin() + static_cast<std::ptrdiff_t>(ciphertext_offset),
                                file_data.begin() + static_cast<std::ptrdiff_t>(ciphertext_offset + serial_len));
                            ciphertext_offset += serial_len;
                            result.metadata.yubikey_challenge.assign(
                                file_data.begin() + static_cast<std::ptrdiff_t>(ciphertext_offset),
                                file_data.begin() + static_cast<std::ptrdiff_t>(ciphertext_offset + YUBIKEY_CHALLENGE_SIZE));
                            ciphertext_offset += YUBIKEY_CHALLENGE_SIZE;
                        } else {
                            // Invalid serial length or insufficient data - treat as corrupted
                            Log::warning("VaultFormat: Invalid YubiKey serial length ({}) or insufficient data", serial_len);
                            return std::unexpected(VaultError::CorruptedFile);
                        }
                    }

                    // Extract RS-encoded data
                    std::vector<uint8_t> encoded_data(
                        file_data.begin() + static_cast<std::ptrdiff_t>(ciphertext_offset),
                        file_data.end());

                    // Decode with Reed-Solomon
                    // Note: We need to pass ReedSolomon instance through, but for now create temporary
                    std::unique_ptr<ReedSolomon> temp_rs;
                    uint8_t temp_redundancy = 0;
                    auto decode_result = decode_with_reed_solomon(encoded_data, original_size, rs_redundancy,
                                                                   temp_rs, temp_redundancy);
                    if (!decode_result) {
                        return std::unexpected(decode_result.error());
                    }
                    result.ciphertext = std::move(decode_result.value());

                    Log::info("VaultFormat: Vault decoded with Reed-Solomon ({}% redundancy, {} -> {} bytes)",
                             rs_redundancy, encoded_data.size(), result.ciphertext.size());
                } else {
                    // Invalid size ratio - treat as legacy format
                    result.ciphertext.assign(file_data.begin() + static_cast<std::ptrdiff_t>(SALT_LENGTH + IV_LENGTH),
                                           file_data.end());
                }
            } else {
                // Invalid redundancy - treat as legacy format
                result.ciphertext.assign(file_data.begin() + static_cast<std::ptrdiff_t>(ciphertext_offset),
                                       file_data.end());
            }
        } else {
            // No RS encoding, extract normal ciphertext
            ciphertext_offset += 1;  // Skip flags byte

            // Read YubiKey metadata if required (after flags byte)
            if (yubikey_required && ciphertext_offset < file_data.size()) {
                uint8_t serial_len = file_data[ciphertext_offset++];

                // Validate serial_len is reasonable (max 255 bytes, but typically < 50)
                // and we have enough data remaining
                if (serial_len > 0 && serial_len <= 255 &&
                    ciphertext_offset + serial_len + YUBIKEY_CHALLENGE_SIZE <= file_data.size()) {
                    result.metadata.yubikey_serial.assign(
                        file_data.begin() + static_cast<std::ptrdiff_t>(ciphertext_offset),
                        file_data.begin() + static_cast<std::ptrdiff_t>(ciphertext_offset + serial_len));
                    ciphertext_offset += serial_len;
                    result.metadata.yubikey_challenge.assign(
                        file_data.begin() + static_cast<std::ptrdiff_t>(ciphertext_offset),
                        file_data.begin() + static_cast<std::ptrdiff_t>(ciphertext_offset + YUBIKEY_CHALLENGE_SIZE));
                    ciphertext_offset += YUBIKEY_CHALLENGE_SIZE;
                } else {
                    // Invalid serial length or insufficient data - treat as corrupted
                    Log::warning("VaultFormat: Invalid YubiKey serial length ({}) or insufficient data", serial_len);
                    return std::unexpected(VaultError::CorruptedFile);
                }
            }

            result.ciphertext.assign(file_data.begin() + static_cast<std::ptrdiff_t>(ciphertext_offset),
                                   file_data.end());
        }
    } else {
        // Legacy format without flags
        result.ciphertext.assign(file_data.begin() + static_cast<std::ptrdiff_t>(ciphertext_offset),
                                   file_data.end());
    }

    return result;
}

VaultResult<std::vector<uint8_t>>
VaultFormat::decode_with_reed_solomon(
    const std::vector<uint8_t>& encoded_data,
    uint32_t original_size,
    uint8_t redundancy,
    std::unique_ptr<ReedSolomon>& reed_solomon_instance,
    uint8_t& current_redundancy) {

    // Create ReedSolomon instance if needed
    if (!reed_solomon_instance || current_redundancy != redundancy) {
        reed_solomon_instance = std::make_unique<ReedSolomon>(redundancy);
        current_redundancy = redundancy;
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

    auto decode_result = reed_solomon_instance->decode(encoded_struct);
    if (!decode_result) {
        Log::error("VaultFormat: Reed-Solomon decoding failed: {}",
                  ReedSolomon::error_to_string(decode_result.error()));
        return std::unexpected(VaultError::DecodingFailed);
    }

    return decode_result.value();
}

}  // namespace KeepTower
