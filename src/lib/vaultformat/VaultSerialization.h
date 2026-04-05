// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 TJDev

/**
 * @file VaultSerialization.h
 * @brief Vault protobuf serialization and deserialization utilities
 */

#ifndef KEEPTOWER_VAULT_SERIALIZATION_H
#define KEEPTOWER_VAULT_SERIALIZATION_H

#include "core/VaultError.h"
#include "record.pb.h"
#include <vector>
#include <cstdint>
#include <string>

namespace KeepTower {

/**
 * @brief Stateless protobuf serialization helpers for vault payloads.
 *
 * VaultSerialization owns the low-level protobuf encode/decode and schema
 * migration mechanics used by higher-level workflow services.
 */
class VaultSerialization {
public:
    /**
     * @brief Serialize vault protobuf data into bytes.
     * @param vault_data Vault protobuf object to serialize.
     * @return Serialized byte buffer or an error.
     */
    static VaultResult<std::vector<uint8_t>> serialize(const keeptower::VaultData& vault_data);

    /**
     * @brief Deserialize vault protobuf data from bytes.
     * @param data Serialized vault payload bytes.
     * @return Parsed vault protobuf object or an error.
     */
    static VaultResult<keeptower::VaultData> deserialize(const std::vector<uint8_t>& data);

    /**
     * @brief Apply schema migrations to a parsed vault protobuf object.
     * @param vault_data Vault protobuf object to migrate.
     * @param modified Set to true when a migration changed the object.
     * @return True when migration succeeded.
     */
    static bool migrate_schema(keeptower::VaultData& vault_data, bool& modified);

private:
    static constexpr int32_t CURRENT_SCHEMA_VERSION = 2;

    VaultSerialization() = delete;
    ~VaultSerialization() = delete;
    VaultSerialization(const VaultSerialization&) = delete;
    VaultSerialization& operator=(const VaultSerialization&) = delete;
    VaultSerialization(VaultSerialization&&) = delete;
    VaultSerialization& operator=(VaultSerialization&&) = delete;
};

}  // namespace KeepTower

#endif  // KEEPTOWER_VAULT_SERIALIZATION_H