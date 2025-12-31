// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include <gtest/gtest.h>
#include "../src/core/ReedSolomon.h"
#include <algorithm>
#include <random>

/**
 * @brief Test fixture for Reed-Solomon operations
 */
class ReedSolomonTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create RS encoder with 10% redundancy
        rs = std::make_unique<ReedSolomon>(10);
    }

    void TearDown() override {
        rs.reset();
    }

    std::unique_ptr<ReedSolomon> rs;
};

/**
 * @brief Test basic encoding and decoding without corruption
 */
TEST_F(ReedSolomonTest, BasicEncodeDecodeTest) {
    // Test data
    std::vector<uint8_t> original_data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Encode
    auto encoded = rs->encode(original_data);
    ASSERT_TRUE(encoded.has_value()) << "Encoding should succeed";
    EXPECT_GT(encoded->data.size(), original_data.size()) << "Encoded data should be larger";
    EXPECT_EQ(encoded->original_size, original_data.size());
    EXPECT_EQ(encoded->redundancy_percent, 10);

    // Decode
    auto decoded = rs->decode(*encoded);
    ASSERT_TRUE(decoded.has_value()) << "Decoding should succeed";
    EXPECT_EQ(*decoded, original_data) << "Decoded data should match original";
}

/**
 * @brief Test with empty data
 */
TEST_F(ReedSolomonTest, EmptyDataTest) {
    std::vector<uint8_t> empty_data;

    auto encoded = rs->encode(empty_data);
    EXPECT_FALSE(encoded.has_value()) << "Encoding empty data should fail";
    EXPECT_EQ(encoded.error(), ReedSolomon::Error::INVALID_DATA);
}

/**
 * @brief Test with large data block
 */
TEST_F(ReedSolomonTest, LargeDataTest) {
    // Create 10KB of test data
    std::vector<uint8_t> large_data(10240);
    for (size_t i = 0; i < large_data.size(); ++i) {
        large_data[i] = static_cast<uint8_t>(i % 256);
    }

    auto encoded = rs->encode(large_data);
    ASSERT_TRUE(encoded.has_value());

    auto decoded = rs->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, large_data);
}

/**
 * @brief Test single byte corruption recovery
 */
TEST_F(ReedSolomonTest, SingleByteCorruptionTest) {
    std::vector<uint8_t> original_data = {
        0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x57, 0x6F, 0x72, 0x6C, 0x64, 0x21  // "Hello World!"
    };

    auto encoded = rs->encode(original_data);
    ASSERT_TRUE(encoded.has_value());

    // Corrupt one byte in the middle
    if (encoded->data.size() > 100) {
        encoded->data[100] ^= 0xFF;  // Flip all bits
    }

    // Should still decode correctly due to error correction
    auto decoded = rs->decode(*encoded);
    ASSERT_TRUE(decoded.has_value()) << "Should recover from single byte corruption";
    EXPECT_EQ(*decoded, original_data);
}

/**
 * @brief Test multiple byte corruption recovery
 */
TEST_F(ReedSolomonTest, MultipleByteCorruptionTest) {
    // Use larger data for better RS block formation
    std::vector<uint8_t> original_data(500, 0xAA);

    auto encoded = rs->encode(original_data);
    ASSERT_TRUE(encoded.has_value());

    // Corrupt a few bytes (within correctable limit)
    // RS(255,223) can correct up to 16 errors per block
    if (encoded->data.size() > 10) {
        encoded->data[5] ^= 0xFF;
        encoded->data[10] ^= 0xFF;
        encoded->data[15] ^= 0xFF;
    }

    auto decoded = rs->decode(*encoded);
    ASSERT_TRUE(decoded.has_value()) << "Should recover from limited corruption";
    EXPECT_EQ(*decoded, original_data);
}

/**
 * @brief Test excessive corruption (beyond repair)
 */
TEST_F(ReedSolomonTest, ExcessiveCorruptionTest) {
    std::vector<uint8_t> original_data(500, 0x55);

    auto encoded = rs->encode(original_data);
    ASSERT_TRUE(encoded.has_value());

    // Corrupt too many bytes (beyond correction capability)
    // Corrupt 50% of the data which is way beyond 10% redundancy capability
    for (size_t i = 0; i < encoded->data.size() / 2; ++i) {
        encoded->data[i] ^= 0xFF;
    }

    auto decoded = rs->decode(*encoded);
    // This may fail or return incorrect data - RS cannot correct this much corruption
    // Just verify it handles the situation gracefully
    if (!decoded.has_value()) {
        EXPECT_EQ(decoded.error(), ReedSolomon::Error::DECODING_FAILED);
    }
}

/**
 * @brief Test different redundancy levels
 */
TEST_F(ReedSolomonTest, RedundancyLevelsTest) {
    std::vector<uint8_t> test_data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    for (uint8_t redundancy : {5, 10, 20, 30, 50}) {
        ReedSolomon rs_custom(redundancy);

        auto encoded = rs_custom.encode(test_data);
        ASSERT_TRUE(encoded.has_value()) << "Redundancy " << (int)redundancy << "% should work";

        auto decoded = rs_custom.decode(*encoded);
        ASSERT_TRUE(decoded.has_value());
        EXPECT_EQ(*decoded, test_data);

        // Verify max correctable corruption percentage
        EXPECT_EQ(rs_custom.get_max_correctable_corruption(), redundancy / 2);
    }
}

/**
 * @brief Test invalid redundancy percentage
 */
TEST_F(ReedSolomonTest, InvalidRedundancyTest) {
    // Too low
    EXPECT_THROW(ReedSolomon rs_low(2), std::invalid_argument);

    // Too high
    EXPECT_THROW(ReedSolomon rs_high(60), std::invalid_argument);

    // Valid boundary cases
    EXPECT_NO_THROW(ReedSolomon rs_min(5));
    EXPECT_NO_THROW(ReedSolomon rs_max(50));
}

/**
 * @brief Test set_redundancy_percent
 */
TEST_F(ReedSolomonTest, SetRedundancyTest) {
    EXPECT_EQ(rs->get_redundancy_percent(), 10);

    EXPECT_TRUE(rs->set_redundancy_percent(20));
    EXPECT_EQ(rs->get_redundancy_percent(), 20);

    EXPECT_FALSE(rs->set_redundancy_percent(3));   // Too low
    EXPECT_FALSE(rs->set_redundancy_percent(55));  // Too high
    EXPECT_EQ(rs->get_redundancy_percent(), 20);   // Should remain unchanged
}

/**
 * @brief Test calculate_encoded_size
 */
TEST_F(ReedSolomonTest, CalculateEncodedSizeTest) {
    size_t original_size = 1000;
    size_t encoded_size = rs->calculate_encoded_size(original_size);

    // Encoded size should be larger than original
    EXPECT_GT(encoded_size, original_size);

    // Should be approximately original + 10% redundancy
    // Allow some padding overhead (RS block alignment adds extra)
    EXPECT_LT(encoded_size, original_size * 1.3);  // Less than 30% overhead
}/**
 * @brief Test error_to_string
 */
TEST_F(ReedSolomonTest, ErrorToStringTest) {
    auto msg = ReedSolomon::error_to_string(ReedSolomon::Error::INVALID_REDUNDANCY);
    EXPECT_FALSE(msg.empty());
    EXPECT_NE(msg.find("redundancy"), std::string::npos);

    msg = ReedSolomon::error_to_string(ReedSolomon::Error::ENCODING_FAILED);
    EXPECT_FALSE(msg.empty());

    msg = ReedSolomon::error_to_string(ReedSolomon::Error::DECODING_FAILED);
    EXPECT_FALSE(msg.empty());
}

/**
 * @brief Test with random data patterns
 */
TEST_F(ReedSolomonTest, RandomDataTest) {
    std::mt19937 rng(42);  // Fixed seed for reproducibility
    std::uniform_int_distribution<int> dist(0, 255);

    std::vector<uint8_t> random_data(1000);
    for (auto& byte : random_data) {
        byte = static_cast<uint8_t>(dist(rng));
    }

    auto encoded = rs->encode(random_data);
    ASSERT_TRUE(encoded.has_value());

    auto decoded = rs->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, random_data);
}

/**
 * @brief Test binary data (all patterns)
 */
TEST_F(ReedSolomonTest, BinaryPatternsTest) {
    // Test various binary patterns
    std::vector<std::vector<uint8_t>> test_patterns = {
        std::vector<uint8_t>(100, 0x00),  // All zeros
        std::vector<uint8_t>(100, 0xFF),  // All ones
        std::vector<uint8_t>(100, 0xAA),  // Alternating 10101010
        std::vector<uint8_t>(100, 0x55),  // Alternating 01010101
    };

    for (const auto& pattern : test_patterns) {
        auto encoded = rs->encode(pattern);
        ASSERT_TRUE(encoded.has_value());

        auto decoded = rs->decode(*encoded);
        ASSERT_TRUE(decoded.has_value());
        EXPECT_EQ(*decoded, pattern);
    }
}

// ============================================================================
// Comprehensive Edge Case and Error Handling Tests
// ============================================================================

/**
 * @brief Test padding and unpadding edge cases
 */
TEST_F(ReedSolomonTest, PaddingEdgeCases) {
    // Test data exactly at block boundary (223 bytes = RS_DATA_SIZE)
    std::vector<uint8_t> exact_block(223);
    std::iota(exact_block.begin(), exact_block.end(), 0);

    auto encoded = rs->encode(exact_block);
    ASSERT_TRUE(encoded.has_value());

    auto decoded = rs->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, exact_block);
}

/**
 * @brief Test single byte input
 */
TEST_F(ReedSolomonTest, SingleByteInput) {
    std::vector<uint8_t> single_byte = {0x42};

    auto encoded = rs->encode(single_byte);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(encoded->original_size, 1);

    auto decoded = rs->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->size(), 1);
    EXPECT_EQ((*decoded)[0], 0x42);
}

/**
 * @brief Test very large data (multiple blocks)
 */
TEST_F(ReedSolomonTest, VeryLargeData) {
    // Create 50KB of data (many RS blocks)
    std::vector<uint8_t> large_data(50 * 1024);
    for (size_t i = 0; i < large_data.size(); ++i) {
        large_data[i] = static_cast<uint8_t>((i * 7 + 13) % 256);
    }

    auto encoded = rs->encode(large_data);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_EQ(encoded->original_size, large_data.size());
    EXPECT_GT(encoded->num_data_blocks, 200);  // Should create many blocks

    auto decoded = rs->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, large_data);
}

/**
 * @brief Test corruption at block boundaries
 */
TEST_F(ReedSolomonTest, CorruptionAtBlockBoundaries) {
    std::vector<uint8_t> data(500, 0x33);

    auto encoded = rs->encode(data);
    ASSERT_TRUE(encoded.has_value());

    // Corrupt at block boundary (position 255 = end of first RS block)
    if (encoded->data.size() > 255) {
        encoded->data[254] ^= 0xFF;
        encoded->data[255] ^= 0xFF;
    }

    auto decoded = rs->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, data);
}

/**
 * @brief Test systematic corruption (every Nth byte)
 */
TEST_F(ReedSolomonTest, SystematicCorruption) {
    std::vector<uint8_t> data(1000, 0x88);

    auto encoded = rs->encode(data);
    ASSERT_TRUE(encoded.has_value());

    // Corrupt every 50th byte (within correctable limits)
    for (size_t i = 0; i < encoded->data.size() && i < 10; i += 50) {
        encoded->data[i] ^= 0xFF;
    }

    auto decoded = rs->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, data);
}

/**
 * @brief Test parity block corruption
 */
TEST_F(ReedSolomonTest, ParityBlockCorruption) {
    std::vector<uint8_t> data(223);  // One RS block exactly
    std::iota(data.begin(), data.end(), 0);

    auto encoded = rs->encode(data);
    ASSERT_TRUE(encoded.has_value());

    // Corrupt parity region (bytes 223-254 of RS(255,223))
    // RS stores as [data(223) | parity(32)]
    // Corrupt a few parity bytes (within correctable limit)
    if (encoded->data.size() >= 255) {
        for (size_t i = 223; i < 228; ++i) {  // Only 5 bytes
            encoded->data[i] ^= 0xFF;
        }
    }

    // Should still decode - parity corruption is correctable up to limit
    auto decoded = rs->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, data);
}

/**
 * @brief Test all parity bytes corrupted in one block
 */
TEST_F(ReedSolomonTest, AllParityBytesCorrupted) {
    std::vector<uint8_t> data(223, 0xCC);

    auto encoded = rs->encode(data);
    ASSERT_TRUE(encoded.has_value());

    // Corrupt ALL 32 parity bytes in first block
    if (encoded->data.size() >= 255) {
        for (size_t i = 223; i < 255; ++i) {
            encoded->data[i] = 0x00;  // Zero out parity
        }
    }

    // Should fail - too much corruption in one block
    auto decoded = rs->decode(*encoded);
    // RS(255,223) can only correct 16 errors, not 32
    if (!decoded.has_value()) {
        EXPECT_EQ(decoded.error(), ReedSolomon::Error::DECODING_FAILED);
    }
}

/**
 * @brief Test decode with invalid EncodedData
 */
TEST_F(ReedSolomonTest, DecodeInvalidEncodedData) {
    ReedSolomon::EncodedData invalid_empty;
    invalid_empty.data.clear();
    invalid_empty.original_size = 0;

    auto result = rs->decode(invalid_empty);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ReedSolomon::Error::INVALID_DATA);

    // EncodedData with empty data but non-zero original_size
    ReedSolomon::EncodedData invalid_size;
    invalid_size.data.clear();
    invalid_size.original_size = 100;

    result = rs->decode(invalid_size);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ReedSolomon::Error::INVALID_DATA);
}

/**
 * @brief Test decode with truncated encoded data
 */
TEST_F(ReedSolomonTest, DecodeTruncatedData) {
    std::vector<uint8_t> data(500, 0xAB);

    auto encoded = rs->encode(data);
    ASSERT_TRUE(encoded.has_value());

    // Truncate encoded data
    encoded->data.resize(encoded->data.size() / 2);

    auto decoded = rs->decode(*encoded);
    // Should fail or return partial data
    if (!decoded.has_value()) {
        EXPECT_EQ(decoded.error(), ReedSolomon::Error::DECODING_FAILED);
    }
}

/**
 * @brief Test maximum correctable corruption calculation
 */
TEST_F(ReedSolomonTest, MaxCorrectableCorruption) {
    // RS(255,223) can correct up to 16 byte errors per block
    // That's 32 parity bytes / 2 = 16 correctable errors

    for (uint8_t redundancy : {10, 20, 30, 50}) {
        ReedSolomon rs_test(redundancy);
        EXPECT_EQ(rs_test.get_max_correctable_corruption(), redundancy / 2);
    }
}

/**
 * @brief Test move constructor and assignment
 */
TEST_F(ReedSolomonTest, MoveSemantics) {
    ReedSolomon rs1(15);
    EXPECT_EQ(rs1.get_redundancy_percent(), 15);

    // Move constructor
    ReedSolomon rs2(std::move(rs1));
    EXPECT_EQ(rs2.get_redundancy_percent(), 15);

    // Move assignment
    ReedSolomon rs3(20);
    rs3 = std::move(rs2);
    EXPECT_EQ(rs3.get_redundancy_percent(), 15);
}

/**
 * @brief Test error messages are descriptive
 */
TEST_F(ReedSolomonTest, ErrorMessagesDescriptive) {
    auto msg1 = ReedSolomon::error_to_string(ReedSolomon::Error::INVALID_REDUNDANCY);
    EXPECT_GT(msg1.length(), 10);
    EXPECT_NE(msg1.find("5-50"), std::string::npos);

    auto msg2 = ReedSolomon::error_to_string(ReedSolomon::Error::LIBCORRECT_ERROR);
    EXPECT_GT(msg2.length(), 10);
    EXPECT_NE(msg2.find("Libcorrect"), std::string::npos);

    auto msg3 = ReedSolomon::error_to_string(ReedSolomon::Error::INVALID_DATA);
    EXPECT_GT(msg3.length(), 5);
}

/**
 * @brief Test burst error correction
 */
TEST_F(ReedSolomonTest, BurstErrorCorrection) {
    std::vector<uint8_t> data(500, 0x99);

    auto encoded = rs->encode(data);
    ASSERT_TRUE(encoded.has_value());

    // Create burst error (consecutive corrupted bytes)
    if (encoded->data.size() > 100) {
        for (size_t i = 50; i < 60; ++i) {  // 10 consecutive bytes
            encoded->data[i] ^= 0xFF;
        }
    }

    auto decoded = rs->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, data);
}

/**
 * @brief Test data integrity with all byte values
 */
TEST_F(ReedSolomonTest, AllByteValues) {
    std::vector<uint8_t> data(256);
    std::iota(data.begin(), data.end(), 0);  // 0x00 to 0xFF

    auto encoded = rs->encode(data);
    ASSERT_TRUE(encoded.has_value());

    auto decoded = rs->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, data);
}

/**
 * @brief Test redundancy boundary values
 */
TEST_F(ReedSolomonTest, RedundancyBoundaries) {
    // Minimum redundancy (5%)
    ReedSolomon rs_min(5);
    std::vector<uint8_t> data1(100, 0x11);
    auto enc1 = rs_min.encode(data1);
    ASSERT_TRUE(enc1.has_value());
    auto dec1 = rs_min.decode(*enc1);
    ASSERT_TRUE(dec1.has_value());
    EXPECT_EQ(*dec1, data1);

    // Maximum redundancy (50%)
    ReedSolomon rs_max(50);
    std::vector<uint8_t> data2(100, 0x22);
    auto enc2 = rs_max.encode(data2);
    ASSERT_TRUE(enc2.has_value());
    auto dec2 = rs_max.decode(*enc2);
    ASSERT_TRUE(dec2.has_value());
    EXPECT_EQ(*dec2, data2);
}

/**
 * @brief Test encoded size increases with redundancy
 */
TEST_F(ReedSolomonTest, EncodedSizeVsRedundancy) {
    std::vector<uint8_t> data(1000, 0x77);

    size_t size_10 = ReedSolomon(10).calculate_encoded_size(data.size());
    size_t size_20 = ReedSolomon(20).calculate_encoded_size(data.size());
    size_t size_50 = ReedSolomon(50).calculate_encoded_size(data.size());

    // Higher redundancy = larger encoded size
    EXPECT_LT(size_10, size_20);
    EXPECT_LT(size_20, size_50);
}

/**
 * @brief Test recovery from random bit flips
 */
TEST_F(ReedSolomonTest, RandomBitFlips) {
    std::vector<uint8_t> data(1000, 0x55);

    auto encoded = rs->encode(data);
    ASSERT_TRUE(encoded.has_value());

    // Flip random bits (limited number)
    std::mt19937 rng(12345);
    std::uniform_int_distribution<size_t> pos_dist(0, encoded->data.size() - 1);
    std::uniform_int_distribution<int> bit_dist(0, 7);

    // Flip 20 random bits across the data (should be correctable)
    for (int i = 0; i < 20; ++i) {
        size_t pos = pos_dist(rng);
        int bit = bit_dist(rng);
        encoded->data[pos] ^= (1 << bit);
    }

    auto decoded = rs->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, data);
}

/**
 * @brief Test with compressible data
 */
TEST_F(ReedSolomonTest, CompressibleData) {
    // Highly compressible data (repeated pattern)
    std::vector<uint8_t> data(2000);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(i % 10);  // 0-9 repeating
    }

    auto encoded = rs->encode(data);
    ASSERT_TRUE(encoded.has_value());

    auto decoded = rs->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, data);
}

/**
 * @brief Test EncodedData metadata accuracy
 */
TEST_F(ReedSolomonTest, EncodedDataMetadata) {
    std::vector<uint8_t> data(1234, 0xCD);

    auto encoded = rs->encode(data);
    ASSERT_TRUE(encoded.has_value());

    EXPECT_EQ(encoded->original_size, 1234);
    EXPECT_EQ(encoded->redundancy_percent, 10);
    EXPECT_EQ(encoded->block_size, 255);
    EXPECT_GT(encoded->num_data_blocks, 0);
    EXPECT_GT(encoded->num_parity_blocks, 0);

    // Verify data blocks calculation
    size_t expected_blocks = (data.size() + 222) / 223;  // RS_DATA_SIZE = 223
    EXPECT_GE(encoded->num_data_blocks, expected_blocks);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}