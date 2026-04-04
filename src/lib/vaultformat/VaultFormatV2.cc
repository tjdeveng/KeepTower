// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "VaultFormatV2.h"
#include "../../utils/Log.h"
#include <cstring>
#include <algorithm>

namespace KeepTower {

KeepTower::VaultResult<uint32_t> VaultFormatV2::detect_version(const std::vector<uint8_t>& file_data) {
    if (file_data.size() < 8) {
        return std::unexpected(VaultError::CorruptedFile);
    }

    uint32_t magic = 0;
    std::memcpy(&magic, file_data.data(), sizeof(magic));

    if (magic != VAULT_MAGIC) {
        return std::unexpected(VaultError::CorruptedFile);
    }

    uint32_t version = 0;
    std::memcpy(&version, file_data.data() + 4, sizeof(version));

    if (version != VAULT_VERSION_V2) {
        Log::error("VaultFormatV2: Unsupported vault version: {}", version);
        return std::unexpected(VaultError::UnsupportedVersion);
    }

    return VAULT_VERSION_V2;
}

bool VaultFormatV2::is_valid_v2_vault(const std::vector<uint8_t>& file_data) {
    auto version_result = detect_version(file_data);
    if (!version_result) {
        return false;
    }
    return version_result.value() == VAULT_VERSION_V2;
}

KeepTower::VaultResult<std::vector<uint8_t>>
VaultFormatV2::apply_header_fec(const std::vector<uint8_t>& header_data,
                                uint8_t encoding_redundancy,
                                uint8_t stored_redundancy) {
    ReedSolomon rs(encoding_redundancy);

    auto encode_result = rs.encode(header_data);
    if (!encode_result) {
        Log::error("VaultFormatV2: Header FEC encoding failed: {}",
                   ReedSolomon::error_to_string(encode_result.error()));
        return std::unexpected(VaultError::FECEncodingFailed);
    }

    const auto& encoded = encode_result.value();

    std::vector<uint8_t> result;
    result.reserve(1 + 4 + encoded.data.size());
    result.push_back(stored_redundancy);

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
    result.insert(result.end(), encoded.data.begin(), encoded.data.end());

    Log::info("VaultFormatV2: Header FEC applied (encoding: {}%, stored: {}%, {} -> {} bytes)",
              encoding_redundancy, stored_redundancy, header_data.size(), encoded.data.size());

    return result;
}

KeepTower::VaultResult<std::vector<uint8_t>>
VaultFormatV2::remove_header_fec(const std::vector<uint8_t>& protected_data,
                                 uint32_t original_size,
                                 uint8_t redundancy) {
    ReedSolomon rs(redundancy);

    ReedSolomon::EncodedData encoded;
    encoded.data = protected_data;
    encoded.original_size = original_size;
    encoded.redundancy_percent = redundancy;

    auto decode_result = rs.decode(encoded);
    if (!decode_result) {
        Log::error("VaultFormatV2: Header FEC decoding failed: {}",
                   ReedSolomon::error_to_string(decode_result.error()));
        return std::unexpected(VaultError::FECDecodingFailed);
    }

    Log::info("VaultFormatV2: Header FEC decoded successfully (recovered {} bytes)", original_size);
    return decode_result.value();
}

KeepTower::VaultResult<std::vector<uint8_t>>
VaultFormatV2::write_header(const V2FileHeader& header,
                            bool enable_header_fec,
                            uint8_t user_fec_redundancy) {
    std::vector<uint8_t> result;
    result.reserve(4096);

    std::vector<uint8_t> vault_header_data = header.vault_header.serialize();
    if (vault_header_data.empty()) {
        Log::error("VaultFormatV2: Failed to serialize vault header");
        return std::unexpected(VaultError::SerializationFailed);
    }

    std::vector<uint8_t> header_data_section;
    uint8_t header_flags = 0;

    if (enable_header_fec) {
        header_flags |= HEADER_FLAG_FEC_ENABLED;
        uint8_t effective_redundancy = std::max(MIN_HEADER_FEC_REDUNDANCY, user_fec_redundancy);
        auto fec_result = apply_header_fec(vault_header_data, effective_redundancy, user_fec_redundancy);
        if (!fec_result) {
            return std::unexpected(fec_result.error());
        }
        header_data_section = std::move(fec_result.value());
    } else {
        header_data_section = vault_header_data;
    }

    uint32_t header_size = 1 + header_data_section.size();
    uint32_t magic = VAULT_MAGIC;
    uint32_t version = VAULT_VERSION_V2;
    uint32_t pbkdf2_iters = header.pbkdf2_iterations;

    Log::info("VaultFormatV2: Writing header with version={}, pbkdf2={}, header_size={}",
              version, pbkdf2_iters, header_size);

    result.push_back(magic & 0xFF);
    result.push_back((magic >> 8) & 0xFF);
    result.push_back((magic >> 16) & 0xFF);
    result.push_back((magic >> 24) & 0xFF);
    result.push_back(version & 0xFF);
    result.push_back((version >> 8) & 0xFF);
    result.push_back((version >> 16) & 0xFF);
    result.push_back((version >> 24) & 0xFF);
    result.push_back(pbkdf2_iters & 0xFF);
    result.push_back((pbkdf2_iters >> 8) & 0xFF);
    result.push_back((pbkdf2_iters >> 16) & 0xFF);
    result.push_back((pbkdf2_iters >> 24) & 0xFF);
    result.push_back(header_size & 0xFF);
    result.push_back((header_size >> 8) & 0xFF);
    result.push_back((header_size >> 16) & 0xFF);
    result.push_back((header_size >> 24) & 0xFF);
    result.push_back(header_flags);
    result.insert(result.end(), header_data_section.begin(), header_data_section.end());
    result.insert(result.end(), header.data_salt.begin(), header.data_salt.end());
    result.insert(result.end(), header.data_iv.begin(), header.data_iv.end());

    if (enable_header_fec) {
        uint8_t effective_redundancy = std::max(MIN_HEADER_FEC_REDUNDANCY, user_fec_redundancy);
        Log::info("VaultFormatV2: Header written ({} bytes, FEC: {}% redundancy)",
                  result.size(), effective_redundancy);
    } else {
        Log::info("VaultFormatV2: Header written ({} bytes, FEC: disabled)", result.size());
    }

    return result;
}

KeepTower::VaultResult<std::pair<VaultFormatV2::V2FileHeader, size_t>>
VaultFormatV2::read_header(const std::vector<uint8_t>& file_data) {
    if (file_data.size() < 16) {
        return std::unexpected(VaultError::CorruptedFile);
    }

    size_t offset = 0;
    V2FileHeader header;
    std::memcpy(&header.magic, file_data.data() + offset, sizeof(header.magic));
    offset += 4;

    if (header.magic != VAULT_MAGIC) {
        Log::error("VaultFormatV2: Invalid magic: 0x{:08X}", header.magic);
        return std::unexpected(VaultError::CorruptedFile);
    }

    std::memcpy(&header.version, file_data.data() + offset, sizeof(header.version));
    offset += 4;
    if (header.version != VAULT_VERSION_V2) {
        Log::error("VaultFormatV2: Expected version 2, got {}", header.version);
        return std::unexpected(VaultError::UnsupportedVersion);
    }

    std::memcpy(&header.pbkdf2_iterations, file_data.data() + offset, sizeof(header.pbkdf2_iterations));
    offset += 4;
    std::memcpy(&header.header_size, file_data.data() + offset, sizeof(header.header_size));
    offset += 4;

    if (header.header_size == 0 ||
        header.header_size > MAX_HEADER_SIZE ||
        offset > file_data.size() ||
        static_cast<size_t>(header.header_size) > (file_data.size() - offset)) {
        Log::error("VaultFormatV2: Invalid header size: {} (max: {})",
                   header.header_size, MAX_HEADER_SIZE);
        return std::unexpected(VaultError::CorruptedFile);
    }

    header.header_flags = file_data[offset++];
    bool fec_enabled = (header.header_flags & HEADER_FLAG_FEC_ENABLED) != 0;
    uint32_t header_data_size = header.header_size - 1;
    const size_t required_after_offset = static_cast<size_t>(header_data_size) + 32 + 12;
    if (offset > file_data.size() || (file_data.size() - offset) < required_after_offset) {
        Log::error("VaultFormatV2: File too small for header (need {}, have {})",
                   offset + required_after_offset, file_data.size());
        return std::unexpected(VaultError::CorruptedFile);
    }

    std::vector<uint8_t> header_data_section(
        file_data.begin() + offset,
        file_data.begin() + offset + header_data_size);
    offset += header_data_size;

    std::vector<uint8_t> vault_header_data;
    if (fec_enabled) {
        if (header_data_section.size() < 5) {
            Log::error("VaultFormatV2: FEC header too small");
            return std::unexpected(VaultError::CorruptedFile);
        }

        uint8_t redundancy = header_data_section[0];
        header.fec_redundancy_percent = redundancy;
        uint32_t original_size = (static_cast<uint32_t>(header_data_section[1]) << 24) |
                                 (static_cast<uint32_t>(header_data_section[2]) << 16) |
                                 (static_cast<uint32_t>(header_data_section[3]) << 8) |
                                 static_cast<uint32_t>(header_data_section[4]);
        std::vector<uint8_t> encoded_data(header_data_section.begin() + 5, header_data_section.end());
        uint8_t decoding_redundancy = std::max(MIN_HEADER_FEC_REDUNDANCY, redundancy);
        auto decode_result = remove_header_fec(encoded_data, original_size, decoding_redundancy);
        if (!decode_result) {
            return std::unexpected(decode_result.error());
        }

        vault_header_data = std::move(decode_result.value());
        Log::info("VaultFormatV2: Header FEC decoded successfully (recovered {} bytes, encoded: {}%, stored: {}%)",
                  vault_header_data.size(), decoding_redundancy, redundancy);
    } else {
        vault_header_data = header_data_section;
    }

    auto vault_header_opt = VaultHeaderV2::deserialize(vault_header_data);
    if (!vault_header_opt) {
        Log::error("VaultFormatV2: Failed to deserialize vault header");
        return std::unexpected(VaultError::CorruptedFile);
    }

    header.vault_header = std::move(vault_header_opt.value());
    std::copy(file_data.begin() + offset, file_data.begin() + offset + 32, header.data_salt.begin());
    offset += 32;
    std::copy(file_data.begin() + offset, file_data.begin() + offset + 12, header.data_iv.begin());
    offset += 12;

    if (fec_enabled) {
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