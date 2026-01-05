// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "VaultFormatV2.h"
#include "../utils/Log.h"
#include <cstring>
#include <algorithm>

namespace KeepTower {

// ============================================================================
// Version Detection
// ============================================================================

KeepTower::VaultResult<uint32_t> VaultFormatV2::detect_version(const std::vector<uint8_t>& file_data) {
    if (file_data.size() < 8) {
        return std::unexpected(VaultError::CorruptedFile);
    }

    // Read magic (4 bytes, little-endian)
    uint32_t magic = 0;
    std::memcpy(&magic, file_data.data(), sizeof(magic));

    if (magic != VAULT_MAGIC) {
        return std::unexpected(VaultError::CorruptedFile);
    }

    // Read version (4 bytes, little-endian)
    uint32_t version = 0;
    std::memcpy(&version, file_data.data() + 4, sizeof(version));

    if (version != VAULT_VERSION_V1 && version != VAULT_VERSION_V2) {
        Log::error("VaultFormatV2: Unsupported vault version: {}", version);
        return std::unexpected(VaultError::UnsupportedVersion);
    }

    return version;
}

bool VaultFormatV2::is_valid_v2_vault(const std::vector<uint8_t>& file_data) {
    auto version_result = detect_version(file_data);
    if (!version_result) {
        return false;
    }
    return version_result.value() == VAULT_VERSION_V2;
}

// ============================================================================
// FEC Operations
// ============================================================================

KeepTower::VaultResult<std::vector<uint8_t>>
VaultFormatV2::apply_header_fec(const std::vector<uint8_t>& header_data,
                                uint8_t encoding_redundancy,
                                uint8_t stored_redundancy) {
    // Create ReedSolomon encoder with the effective encoding redundancy
    ReedSolomon rs(encoding_redundancy);

    // Encode header data
    auto encode_result = rs.encode(header_data);
    if (!encode_result) {
        Log::error("VaultFormatV2: Header FEC encoding failed: {}",
                   ReedSolomon::error_to_string(encode_result.error()));
        return std::unexpected(VaultError::FECEncodingFailed);
    }

    const auto& encoded = encode_result.value();

    // Build FEC-protected header:
    // [redundancy(1)][original_size(4)][encoded_data]
    std::vector<uint8_t> result;
    result.reserve(1 + 4 + encoded.data.size());

    // Store user's preference redundancy (not the effective encoding redundancy)
    result.push_back(stored_redundancy);

    // Original size (4 bytes, big-endian)
    // Check for integer overflow (size_t -> uint32_t)
    if (header_data.size() > UINT32_MAX) {
        Log::error("VaultFormatV2: Header data too large: {} bytes (max: {})",
                   header_data.size(), UINT32_MAX);
        return std::unexpected(VaultError::InvalidData);
    }
    uint32_t original_size = static_cast<uint32_t>(header_data.size());
    result.push_back((original_size >> 24) & 0xFF);
    result.push_back((original_size >> 16) & 0xFF);
    result.push_back((original_size >> 8) & 0xFF);
    result.push_back(original_size & 0xFF);

    // Encoded data (data + parity)
    result.insert(result.end(), encoded.data.begin(), encoded.data.end());

    Log::info("VaultFormatV2: Header FEC applied (encoding: {}%, stored: {}%, {} -> {} bytes)",
              encoding_redundancy, stored_redundancy, header_data.size(), encoded.data.size());

    return result;
}

KeepTower::VaultResult<std::vector<uint8_t>>
VaultFormatV2::remove_header_fec(const std::vector<uint8_t>& protected_data,
                                  uint32_t original_size,
                                  uint8_t redundancy) {
    // Create ReedSolomon decoder
    ReedSolomon rs(redundancy);

    // Build EncodedData structure
    ReedSolomon::EncodedData encoded;
    encoded.data = protected_data;
    encoded.original_size = original_size;
    encoded.redundancy_percent = redundancy;

    // Calculate block structure (ReedSolomon will handle this internally)
    // We just need to provide the encoded data

    // Decode header data
    auto decode_result = rs.decode(encoded);
    if (!decode_result) {
        Log::error("VaultFormatV2: Header FEC decoding failed: {}",
                   ReedSolomon::error_to_string(decode_result.error()));
        return std::unexpected(VaultError::FECDecodingFailed);
    }

    Log::info("VaultFormatV2: Header FEC decoded successfully (recovered {} bytes)", original_size);

    return decode_result.value();
}

// ============================================================================
// Header Writing
// ============================================================================

KeepTower::VaultResult<std::vector<uint8_t>>
VaultFormatV2::write_header(const V2FileHeader& header,
                             bool enable_header_fec,
                             uint8_t user_fec_redundancy) {
    std::vector<uint8_t> result;
    result.reserve(4096); // Reserve typical header size

    // Serialize vault header (security policy + key slots)
    std::vector<uint8_t> vault_header_data = header.vault_header.serialize();
    if (vault_header_data.empty()) {
        Log::error("VaultFormatV2: Failed to serialize vault header");
        return std::unexpected(VaultError::SerializationFailed);
    }

    // Prepare header data for FEC protection (if enabled)
    std::vector<uint8_t> header_data_section;
    uint8_t header_flags = 0;

    if (enable_header_fec) {
        header_flags |= HEADER_FLAG_FEC_ENABLED;

        // Header encoding uses max(20%, user_preference) for critical data protection
        // But we store the user's actual preference so it can be read back
        uint8_t effective_redundancy = std::max(MIN_HEADER_FEC_REDUNDANCY, user_fec_redundancy);

        // Apply FEC: encode with effective_redundancy, store user_fec_redundancy
        auto fec_result = apply_header_fec(vault_header_data, effective_redundancy, user_fec_redundancy);
        if (!fec_result) {
            return std::unexpected(fec_result.error());
        }
        header_data_section = std::move(fec_result.value());
    } else {
        // No FEC, just use raw header data
        header_data_section = vault_header_data;
    }

    // Calculate header size (flags + header data)
    uint32_t header_size = 1 + header_data_section.size();

    // Write file header: [magic][version][pbkdf2_iters][header_size]
    uint32_t magic = VAULT_MAGIC;
    uint32_t version = VAULT_VERSION_V2;
    uint32_t pbkdf2_iters = header.pbkdf2_iterations;

    Log::info("VaultFormatV2: Writing header with version={}, pbkdf2={}, header_size={}",
              version, pbkdf2_iters, header_size);

    // Magic (4 bytes, little-endian)
    result.push_back(magic & 0xFF);
    result.push_back((magic >> 8) & 0xFF);
    result.push_back((magic >> 16) & 0xFF);
    result.push_back((magic >> 24) & 0xFF);

    // Version (4 bytes, little-endian)
    result.push_back(version & 0xFF);
    result.push_back((version >> 8) & 0xFF);
    result.push_back((version >> 16) & 0xFF);
    result.push_back((version >> 24) & 0xFF);

    // PBKDF2 iterations (4 bytes, little-endian)
    result.push_back(pbkdf2_iters & 0xFF);
    result.push_back((pbkdf2_iters >> 8) & 0xFF);
    result.push_back((pbkdf2_iters >> 16) & 0xFF);
    result.push_back((pbkdf2_iters >> 24) & 0xFF);

    // Header size (4 bytes, little-endian)
    result.push_back(header_size & 0xFF);
    result.push_back((header_size >> 8) & 0xFF);
    result.push_back((header_size >> 16) & 0xFF);
    result.push_back((header_size >> 24) & 0xFF);

    // Header flags (1 byte)
    result.push_back(header_flags);

    // Header data (FEC-protected or raw)
    result.insert(result.end(), header_data_section.begin(), header_data_section.end());

    // Data salt (32 bytes)
    result.insert(result.end(), header.data_salt.begin(), header.data_salt.end());

    // Data IV (12 bytes)
    result.insert(result.end(), header.data_iv.begin(), header.data_iv.end());

    Log::info("VaultFormatV2: Header written ({} bytes, FEC: {})",
              result.size(), enable_header_fec ? "enabled" : "disabled");

    return result;
}

// ============================================================================
// Header Reading
// ============================================================================

KeepTower::VaultResult<std::pair<VaultFormatV2::V2FileHeader, size_t>>
VaultFormatV2::read_header(const std::vector<uint8_t>& file_data) {
    // Minimum size: magic(4) + version(4) + pbkdf2(4) + header_size(4) = 16 bytes
    if (file_data.size() < 16) {
        return std::unexpected(VaultError::CorruptedFile);
    }

    size_t offset = 0;
    V2FileHeader header;

    // Read magic (4 bytes, little-endian)
    std::memcpy(&header.magic, file_data.data() + offset, sizeof(header.magic));
    offset += 4;

    if (header.magic != VAULT_MAGIC) {
        Log::error("VaultFormatV2: Invalid magic: 0x{:08X}", header.magic);
        return std::unexpected(VaultError::CorruptedFile);
    }

    // Read version (4 bytes, little-endian)
    std::memcpy(&header.version, file_data.data() + offset, sizeof(header.version));
    offset += 4;

    if (header.version != VAULT_VERSION_V2) {
        Log::error("VaultFormatV2: Expected version 2, got {}", header.version);
        return std::unexpected(VaultError::UnsupportedVersion);
    }

    // Read PBKDF2 iterations (4 bytes, little-endian)
    std::memcpy(&header.pbkdf2_iterations, file_data.data() + offset, sizeof(header.pbkdf2_iterations));
    offset += 4;

    // Read header size (4 bytes, little-endian)
    std::memcpy(&header.header_size, file_data.data() + offset, sizeof(header.header_size));
    offset += 4;

    // Validate header size
    if (header.header_size == 0 ||
        header.header_size > MAX_HEADER_SIZE ||
        header.header_size > file_data.size() - offset) {
        Log::error("VaultFormatV2: Invalid header size: {} (max: {})",
                   header.header_size, MAX_HEADER_SIZE);
        return std::unexpected(VaultError::CorruptedFile);
    }

    // Read header flags (1 byte)
    header.header_flags = file_data[offset++];

    bool fec_enabled = (header.header_flags & HEADER_FLAG_FEC_ENABLED) != 0;

    // Calculate remaining header data size (excluding flags byte)
    uint32_t header_data_size = header.header_size - 1;

    // Check if we have enough data
    if (offset + header_data_size + 32 + 12 > file_data.size()) {
        Log::error("VaultFormatV2: File too small for header (need {}, have {})",
                   offset + header_data_size + 32 + 12, file_data.size());
        return std::unexpected(VaultError::CorruptedFile);
    }

    // Extract header data section
    std::vector<uint8_t> header_data_section(
        file_data.begin() + offset,
        file_data.begin() + offset + header_data_size);
    offset += header_data_size;

    // Decode header data (apply FEC if enabled)
    std::vector<uint8_t> vault_header_data;

    if (fec_enabled) {
        // FEC-protected header: [redundancy(1)][original_size(4)][encoded_data]
        if (header_data_section.size() < 5) {
            Log::error("VaultFormatV2: FEC header too small");
            return std::unexpected(VaultError::CorruptedFile);
        }

        uint8_t redundancy = header_data_section[0];
        header.fec_redundancy_percent = redundancy;  // Store user preference for caller

        uint32_t original_size = (static_cast<uint32_t>(header_data_section[1]) << 24) |
                                 (static_cast<uint32_t>(header_data_section[2]) << 16) |
                                 (static_cast<uint32_t>(header_data_section[3]) << 8) |
                                 static_cast<uint32_t>(header_data_section[4]);

        // Extract encoded data (skip redundancy + size)
        std::vector<uint8_t> encoded_data(
            header_data_section.begin() + 5,
            header_data_section.end());

        // Decode with effective redundancy (max of 20% and stored value)
        // This matches the encoding redundancy used during write
        uint8_t decoding_redundancy = std::max(MIN_HEADER_FEC_REDUNDANCY, redundancy);
        auto decode_result = remove_header_fec(encoded_data, original_size, decoding_redundancy);
        if (!decode_result) {
            return std::unexpected(decode_result.error());
        }

        vault_header_data = std::move(decode_result.value());

        Log::info("VaultFormatV2: Header FEC decoded successfully (recovered {} bytes, encoded: {}%, stored: {}%)",
                  vault_header_data.size(), decoding_redundancy, redundancy);
    } else {
        // No FEC, use raw header data
        vault_header_data = header_data_section;
    }

    // Deserialize vault header (security policy + key slots)
    auto vault_header_opt = VaultHeaderV2::deserialize(vault_header_data);
    if (!vault_header_opt) {
        Log::error("VaultFormatV2: Failed to deserialize vault header");
        return std::unexpected(VaultError::CorruptedFile);
    }

    header.vault_header = std::move(vault_header_opt.value());

    // Read data salt (32 bytes)
    std::copy(file_data.begin() + offset, file_data.begin() + offset + 32, header.data_salt.begin());
    offset += 32;

    // Read data IV (12 bytes)
    std::copy(file_data.begin() + offset, file_data.begin() + offset + 12, header.data_iv.begin());
    offset += 12;

    if (fec_enabled) {
        // Calculate effective redundancy for display (same as encoding)
        uint8_t effective_redundancy = std::max(MIN_HEADER_FEC_REDUNDANCY, header.fec_redundancy_percent);
        Log::info("VaultFormatV2: Header read successfully ({} key slots, FEC: enabled, encoded: {}%, user setting: {}%)",
                  header.vault_header.key_slots.size(), effective_redundancy, header.fec_redundancy_percent);
    } else {
        Log::info("VaultFormatV2: Header read successfully ({} key slots, FEC: disabled)",
                  header.vault_header.key_slots.size());
    }

    return std::make_pair(header, offset);
}

} // namespace KeepTower
