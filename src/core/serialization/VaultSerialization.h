// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 TJDev
/**
 * @file VaultSerialization.h
 * @brief Vault protobuf serialization and deserialization utilities
 *
 * This module provides utilities for serializing and deserializing vault data
 * using Protocol Buffers, as well as schema migration functionality.
 */

#ifndef KEEPTOWER_VAULT_SERIALIZATION_H
#define KEEPTOWER_VAULT_SERIALIZATION_H

#include "../VaultError.h"
#include "../record.pb.h"
#include <vector>
#include <cstdint>
#include <string>

namespace KeepTower {

/**
 * @class VaultSerialization
 * @brief Static utility class for vault data serialization and schema migration
 *
 * This class provides methods for:
 * - Serializing VaultData protobuf messages to binary format
 * - Deserializing binary data to VaultData protobuf messages
 * - Migrating vault schemas between versions
 *
 * All methods are static as they perform stateless transformations.
 *
 * ## Thread Safety
 * All methods are thread-safe as they operate on the data provided as parameters
 * and do not maintain any shared state.
 *
 * ## Example Usage
 * @code
 * // Serialize vault data
 * keeptower::VaultData vault_data;
 * // ... populate vault_data ...
 * auto result = VaultSerialization::serialize(vault_data);
 * if (result) {
 *     std::vector<uint8_t> binary_data = std::move(result.value());
 * }
 *
 * // Deserialize vault data
 * std::vector<uint8_t> binary_data = ...;
 * auto vault_result = VaultSerialization::deserialize(binary_data);
 * if (vault_result) {
 *     keeptower::VaultData vault_data = std::move(vault_result.value());
 * }
 *
 * // Migrate schema
 * keeptower::VaultData vault_data;
 * bool modified = false;
 * if (VaultSerialization::migrate_schema(vault_data, modified)) {
 *     // Schema migrated successfully
 * }
 * @endcode
 */
class VaultSerialization {
public:
    /**
     * @brief Serialize VaultData protobuf to binary format
     *
     * Converts a VaultData protobuf message to a binary byte array suitable
     * for encryption and storage.
     *
     * @param vault_data The VaultData protobuf message to serialize
     * @return VaultResult containing the serialized binary data, or VaultError::SerializationFailed
     *
     * @note The returned data is unencrypted and should be encrypted before storage
     * @see deserialize
     */
    static VaultResult<std::vector<uint8_t>> serialize(const keeptower::VaultData& vault_data);

    /**
     * @brief Deserialize binary data to VaultData protobuf
     *
     * Converts a binary byte array to a VaultData protobuf message.
     * The input data should be decrypted before calling this function.
     *
     * @param data The binary data to deserialize
     * @return VaultResult containing the deserialized VaultData, or VaultError::InvalidProtobuf
     *
     * @note The input data must be valid protobuf format
     * @see serialize
     */
    static VaultResult<keeptower::VaultData> deserialize(const std::vector<uint8_t>& data);

    /**
     * @brief Migrate vault schema to current version
     *
     * Performs schema migration on a VaultData structure, upgrading it from
     * older schema versions to the current version (v2). This function handles:
     * - Migration from schema v1 to v2
     * - Initialization of metadata for new vaults
     * - Update of access tracking for current-version vaults
     *
     * ## Schema Versions
     * - **v1**: Legacy format with direct fields in VaultData
     * - **v2**: Current format with VaultMetadata sub-message
     *
     * ## Migration Details
     * - v1 → v2: Protobuf field numbers remain compatible, metadata is initialized
     * - Access tracking: Increments access_count and updates last_accessed timestamp
     *
     * @param vault_data The VaultData structure to migrate (modified in-place)
     * @param modified Output parameter: set to true if vault_data was modified
     * @return true if migration was successful or vault is already current, false otherwise
     *
     * @note For v1 vaults, protobuf ensures field compatibility by field number
     * @note The function is idempotent - safe to call multiple times
     *
     * @see keeptower::VaultData
     * @see keeptower::VaultMetadata
     */
    static bool migrate_schema(keeptower::VaultData& vault_data, bool& modified);

private:
    /// Current schema version
    static constexpr int32_t CURRENT_SCHEMA_VERSION = 2;

    VaultSerialization() = delete;  ///< Static-only class, no instances
    ~VaultSerialization() = delete;  ///< Static-only class, no instances
    VaultSerialization(const VaultSerialization&) = delete;  ///< No copy
    VaultSerialization& operator=(const VaultSerialization&) = delete;  ///< No copy
    VaultSerialization(VaultSerialization&&) = delete;  ///< No move
    VaultSerialization& operator=(VaultSerialization&&) = delete;  ///< No move
};

}  // namespace KeepTower

#endif  // KEEPTOWER_VAULT_SERIALIZATION_H
