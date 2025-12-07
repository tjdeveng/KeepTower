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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
