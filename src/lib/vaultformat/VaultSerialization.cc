// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 TJDev

#include "VaultSerialization.h"
#include "../../utils/Log.h"
#include <ctime>
#include <limits>

namespace KeepTower {

VaultResult<std::vector<uint8_t>>
VaultSerialization::serialize(const keeptower::VaultData& vault_data) {
    std::string serialized_data;

    if (!vault_data.SerializeToString(&serialized_data)) {
        Log::error("VaultSerialization: Failed to serialize VaultData to protobuf");
        return std::unexpected(VaultError::SerializationFailed);
    }

    std::vector<uint8_t> result(serialized_data.begin(), serialized_data.end());
    return result;
}

VaultResult<keeptower::VaultData>
VaultSerialization::deserialize(const std::vector<uint8_t>& data) {
    constexpr size_t MAX_VAULT_SIZE = 100 * 1024 * 1024;

    if (data.size() > MAX_VAULT_SIZE) {
        Log::error("VaultSerialization: Vault data exceeds maximum size ({} bytes > {} bytes)",
                   data.size(), MAX_VAULT_SIZE);
        return std::unexpected(VaultError::InvalidProtobuf);
    }

    if (data.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        Log::error("VaultSerialization: Vault data size exceeds protobuf ParseFromArray limit ({} bytes)",
                   data.size());
        return std::unexpected(VaultError::InvalidProtobuf);
    }

    keeptower::VaultData vault_data;

    if (!vault_data.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
        Log::error("VaultSerialization: Failed to parse VaultData from protobuf");
        return std::unexpected(VaultError::InvalidProtobuf);
    }

    return vault_data;
}

bool VaultSerialization::migrate_schema(keeptower::VaultData& vault_data, bool& modified) {
    auto* metadata = vault_data.mutable_metadata();
    int32_t current_version = metadata->schema_version();

    if (current_version == 0 && vault_data.accounts_size() > 0) {
        Log::info("VaultSerialization: Migrating vault from schema v1 to v2");

        metadata->set_schema_version(CURRENT_SCHEMA_VERSION);
        metadata->set_created_at(std::time(nullptr));
        metadata->set_last_modified(std::time(nullptr));
        metadata->set_last_accessed(std::time(nullptr));
        metadata->set_access_count(1);
        modified = true;

        Log::info("VaultSerialization: Vault migrated successfully to schema v2");
        return true;
    }

    if (current_version == 0 && vault_data.accounts_size() == 0) {
        metadata->set_schema_version(CURRENT_SCHEMA_VERSION);
        metadata->set_created_at(std::time(nullptr));
        metadata->set_last_modified(std::time(nullptr));
        metadata->set_last_accessed(std::time(nullptr));
        metadata->set_access_count(1);
        return true;
    }

    if (current_version >= CURRENT_SCHEMA_VERSION) {
        metadata->set_last_accessed(std::time(nullptr));
        metadata->set_access_count(metadata->access_count() + 1);
        modified = true;
        return true;
    }

    Log::warning("VaultSerialization: Unknown vault schema version: {}", current_version);
    return false;
}

}  // namespace KeepTower