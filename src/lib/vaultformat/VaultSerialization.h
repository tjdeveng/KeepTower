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

class VaultSerialization {
public:
    static VaultResult<std::vector<uint8_t>> serialize(const keeptower::VaultData& vault_data);
    static VaultResult<keeptower::VaultData> deserialize(const std::vector<uint8_t>& data);
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