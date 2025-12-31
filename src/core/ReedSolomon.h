// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file ReedSolomon.h
 * @brief Reed-Solomon forward error correction wrapper
 */

#ifndef REEDSOLOMON_H
#define REEDSOLOMON_H

#include <vector>
#include <cstdint>
#include <memory>
#include <expected>
#include <string>

/**
 * @brief Reed-Solomon error correction wrapper using libcorrect
 *
 * Provides forward error correction capabilities for vault data, allowing
 * recovery from partial file corruption. Uses Reed-Solomon codes to add
 * redundant parity data that enables reconstruction of corrupted blocks.
 *
 * @section usage Usage Example
 * @code
 * ReedSolomon rs(10); // 10% redundancy
 *
 * // Encoding
 * std::vector<uint8_t> data = {1, 2, 3, 4};
 * auto encoded = rs.encode(data);
 * if (encoded) {
 *     // Write encoded->data to disk
 * }
 *
 * // Decoding (with potential corruption recovery)
 * auto decoded = rs.decode(*encoded);
 * if (decoded) {
 *     // Use recovered data
 * }
 * @endcode
 *
 * @section format Encoded Data Format
 * - Original data blocks
 * - Parity blocks (size based on redundancy percentage)
 *
 * @note The redundancy percentage directly affects file size overhead.
 *       10% redundancy adds approximately 10% to file size.
 */
class ReedSolomon {
public:
    /**
     * @brief Encoded data containing original data and parity information
     */
    struct EncodedData {
        std::vector<uint8_t> data;           ///< Combined data and parity blocks
        uint32_t original_size;               ///< Size of original data before encoding
        uint8_t redundancy_percent;          ///< Redundancy percentage used (0-50)
        uint32_t block_size;                 ///< Block size used for encoding
        uint32_t num_data_blocks;            ///< Number of data blocks
        uint32_t num_parity_blocks;          ///< Number of parity blocks
    };

    /**
     * @brief Error types for Reed-Solomon operations
     */
    enum class Error {
        INVALID_REDUNDANCY,      ///< Redundancy percentage out of range (5-50%)
        ENCODING_FAILED,         ///< RS encoding operation failed
        DECODING_FAILED,         ///< RS decoding operation failed (too much corruption)
        INVALID_DATA,            ///< Input data is invalid or corrupted beyond repair
        BLOCK_SIZE_TOO_LARGE,    ///< Data size exceeds maximum RS block size
        LIBCORRECT_ERROR         ///< Underlying libcorrect library error
    };

    /**
     * @brief Construct Reed-Solomon encoder/decoder
     *
     * @param redundancy_percent Percentage of redundancy to add (5-50%)
     * @throws std::invalid_argument if redundancy is out of range
     */
    explicit ReedSolomon(uint8_t redundancy_percent = 10);

    /**
     * @brief Destructor
     */
    ~ReedSolomon();

    // Non-copyable but moveable
    ReedSolomon(const ReedSolomon&) = delete;
    ReedSolomon& operator=(const ReedSolomon&) = delete;

    /** @brief Move constructor - transfers Reed-Solomon context */
    ReedSolomon(ReedSolomon&&) noexcept;

    /** @brief Move assignment - transfers Reed-Solomon context */
    ReedSolomon& operator=(ReedSolomon&&) noexcept;

    /**
     * @brief Encode data with Reed-Solomon error correction
     *
     * Adds parity blocks to the data that enable recovery from corruption.
     * The output size will be approximately (100 + redundancy_percent)% of input size.
     *
     * @param data Input data to encode
     * @return Encoded data with parity, or error
     */
    std::expected<EncodedData, Error> encode(const std::vector<uint8_t>& data);

    /**
     * @brief Decode and potentially repair corrupted data
     *
     * Attempts to recover the original data from encoded data, even if
     * some blocks are corrupted. Can recover from up to (redundancy_percent / 2)
     * corruption.
     *
     * @param encoded Encoded data (possibly corrupted)
     * @return Recovered original data, or error if corruption is too severe
     */
    std::expected<std::vector<uint8_t>, Error> decode(const EncodedData& encoded);

    /**
     * @brief Get current redundancy percentage
     * @return Redundancy percentage (5-50)
     */
    uint8_t get_redundancy_percent() const { return m_redundancy_percent; }

    /**
     * @brief Set redundancy percentage for future encoding operations
     * @param percent New redundancy percentage (5-50)
     * @return true if valid, false if out of range
     */
    bool set_redundancy_percent(uint8_t percent);

    /**
     * @brief Calculate output size for given input size
     * @param input_size Size of data to encode
     * @return Expected size after encoding with current redundancy
     */
    size_t calculate_encoded_size(size_t input_size) const;

    /**
     * @brief Get maximum correctable corruption percentage
     * @return Percentage of data that can be corrupted and still recovered
     */
    uint8_t get_max_correctable_corruption() const {
        // RS can typically correct up to half the redundancy percentage
        return m_redundancy_percent / 2;
    }

    /**
     * @brief Convert error to human-readable string
     * @param error Error code
     * @return Error description
     */
    static std::string error_to_string(Error error);

private:
    uint8_t m_redundancy_percent;          ///< Redundancy percentage (5-50)
    static constexpr uint8_t MIN_REDUNDANCY = 5;    ///< Minimum redundancy (5%)
    static constexpr uint8_t MAX_REDUNDANCY = 50;   ///< Maximum redundancy (50%)
    static constexpr size_t MAX_BLOCK_SIZE = 255;   ///< Maximum RS block size (GF(256) limitation)
    static constexpr size_t OPTIMAL_BLOCK_SIZE = 223; ///< Optimal block size for RS(255,223)

    /**
     * @brief Calculate number of parity blocks needed
     * @param data_blocks Number of data blocks
     * @return Number of parity blocks
     */
    uint32_t calculate_parity_blocks(uint32_t data_blocks) const;

    /**
     * @brief Pad data to align with block boundaries
     * @param data Input data
     * @return Padded data
     */
    std::vector<uint8_t> pad_data(const std::vector<uint8_t>& data) const;

    /**
     * @brief Remove padding from decoded data
     * @param data Padded data
     * @param original_size Original size before padding
     * @return Data with padding removed
     */
    std::vector<uint8_t> unpad_data(const std::vector<uint8_t>& data, uint32_t original_size) const;
};

#endif // REEDSOLOMON_H
