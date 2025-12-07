// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "ReedSolomon.h"

extern "C" {
#include <correct.h>
}

#include <stdexcept>
#include <cstring>
#include <algorithm>

ReedSolomon::ReedSolomon(uint8_t redundancy_percent)
    : m_redundancy_percent(redundancy_percent) {
    if (redundancy_percent < MIN_REDUNDANCY || redundancy_percent > MAX_REDUNDANCY) {
        throw std::invalid_argument(
            "Redundancy percentage must be between " +
            std::to_string(MIN_REDUNDANCY) + " and " +
            std::to_string(MAX_REDUNDANCY)
        );
    }
}

ReedSolomon::~ReedSolomon() = default;

ReedSolomon::ReedSolomon(ReedSolomon&&) noexcept = default;
ReedSolomon& ReedSolomon::operator=(ReedSolomon&&) noexcept = default;

bool ReedSolomon::set_redundancy_percent(uint8_t percent) {
    if (percent < MIN_REDUNDANCY || percent > MAX_REDUNDANCY) {
        return false;
    }
    m_redundancy_percent = percent;
    return true;
}

size_t ReedSolomon::calculate_encoded_size(size_t input_size) const {
    // Calculate number of blocks needed
    size_t num_blocks = (input_size + OPTIMAL_BLOCK_SIZE - 1) / OPTIMAL_BLOCK_SIZE;
    size_t padded_size = num_blocks * OPTIMAL_BLOCK_SIZE;

    // Add parity based on redundancy percentage
    size_t parity_size = (padded_size * m_redundancy_percent + 99) / 100;

    return padded_size + parity_size;
}

uint32_t ReedSolomon::calculate_parity_blocks(uint32_t data_blocks) const {
    // Calculate parity blocks based on redundancy percentage
    return (data_blocks * m_redundancy_percent + 99) / 100;
}

std::vector<uint8_t> ReedSolomon::pad_data(const std::vector<uint8_t>& data) const {
    size_t num_blocks = (data.size() + OPTIMAL_BLOCK_SIZE - 1) / OPTIMAL_BLOCK_SIZE;
    size_t padded_size = num_blocks * OPTIMAL_BLOCK_SIZE;

    std::vector<uint8_t> padded(padded_size, 0);
    std::copy(data.begin(), data.end(), padded.begin());

    return padded;
}

std::vector<uint8_t> ReedSolomon::unpad_data(const std::vector<uint8_t>& data, uint32_t original_size) const {
    if (original_size > data.size()) {
        return {};
    }

    return std::vector<uint8_t>(data.begin(), data.begin() + original_size);
}

std::expected<ReedSolomon::EncodedData, ReedSolomon::Error>
ReedSolomon::encode(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return std::unexpected(Error::INVALID_DATA);
    }

    // Pad data to block boundaries
    auto padded = pad_data(data);

    // Calculate block configuration
    uint32_t num_data_blocks = padded.size() / OPTIMAL_BLOCK_SIZE;
    uint32_t num_parity_blocks = calculate_parity_blocks(num_data_blocks);

    // Use RS(255, 223) encoding
    // 223 data bytes + 32 parity bytes per block
    const size_t RS_BLOCK_SIZE = 255;
    const size_t RS_DATA_SIZE = 223;
    const size_t RS_PARITY_SIZE = RS_BLOCK_SIZE - RS_DATA_SIZE;

    // Create RS encoder
    correct_reed_solomon* rs = correct_reed_solomon_create(
        correct_rs_primitive_polynomial_ccsds,
        1,  // first consecutive root
        1,  // generator root gap
        RS_PARITY_SIZE  // number of parity bytes
    );

    if (!rs) {
        return std::unexpected(Error::LIBCORRECT_ERROR);
    }

    // Allocate output buffer
    size_t encoded_size = num_data_blocks * RS_BLOCK_SIZE;
    std::vector<uint8_t> encoded_data(encoded_size);

    // Encode each block
    for (uint32_t i = 0; i < num_data_blocks; ++i) {
        const uint8_t* data_block = padded.data() + (i * RS_DATA_SIZE);
        uint8_t* encoded_block = encoded_data.data() + (i * RS_BLOCK_SIZE);

        // Copy data portion
        std::memcpy(encoded_block, data_block, RS_DATA_SIZE);

        // Encode to generate parity
        ssize_t result = correct_reed_solomon_encode(
            rs,
            data_block,
            RS_DATA_SIZE,
            encoded_block
        );

        if (result < 0) {
            correct_reed_solomon_destroy(rs);
            return std::unexpected(Error::ENCODING_FAILED);
        }
    }

    correct_reed_solomon_destroy(rs);

    EncodedData result;
    result.data = std::move(encoded_data);
    result.original_size = data.size();
    result.redundancy_percent = m_redundancy_percent;
    result.block_size = RS_BLOCK_SIZE;
    result.num_data_blocks = num_data_blocks;
    result.num_parity_blocks = num_parity_blocks;

    return result;
}

std::expected<std::vector<uint8_t>, ReedSolomon::Error>
ReedSolomon::decode(const EncodedData& encoded) {
    if (encoded.data.empty() || encoded.original_size == 0) {
        return std::unexpected(Error::INVALID_DATA);
    }

    const size_t RS_BLOCK_SIZE = encoded.block_size;
    const size_t RS_DATA_SIZE = 223;
    const size_t RS_PARITY_SIZE = RS_BLOCK_SIZE - RS_DATA_SIZE;

    // Create RS decoder
    correct_reed_solomon* rs = correct_reed_solomon_create(
        correct_rs_primitive_polynomial_ccsds,
        1,
        1,
        RS_PARITY_SIZE
    );

    if (!rs) {
        return std::unexpected(Error::LIBCORRECT_ERROR);
    }

    // Allocate output buffer
    std::vector<uint8_t> decoded_data(encoded.num_data_blocks * RS_DATA_SIZE);

    // Decode each block
    for (uint32_t i = 0; i < encoded.num_data_blocks; ++i) {
        const uint8_t* encoded_block = encoded.data.data() + (i * RS_BLOCK_SIZE);
        uint8_t* decoded_block = decoded_data.data() + (i * RS_DATA_SIZE);

        ssize_t result = correct_reed_solomon_decode(
            rs,
            encoded_block,
            RS_BLOCK_SIZE,
            decoded_block
        );

        if (result < 0) {
            correct_reed_solomon_destroy(rs);
            return std::unexpected(Error::DECODING_FAILED);
        }
    }

    correct_reed_solomon_destroy(rs);

    // Remove padding and return original data
    return unpad_data(decoded_data, encoded.original_size);
}

std::string ReedSolomon::error_to_string(Error error) {
    switch (error) {
        case Error::INVALID_REDUNDANCY:
            return "Invalid redundancy percentage (must be 5-50%)";
        case Error::ENCODING_FAILED:
            return "Reed-Solomon encoding failed";
        case Error::DECODING_FAILED:
            return "Reed-Solomon decoding failed - data too corrupted";
        case Error::INVALID_DATA:
            return "Invalid or empty data";
        case Error::BLOCK_SIZE_TOO_LARGE:
            return "Data size exceeds maximum Reed-Solomon block size";
        case Error::LIBCORRECT_ERROR:
            return "Libcorrect library error";
        default:
            return "Unknown error";
    }
}
