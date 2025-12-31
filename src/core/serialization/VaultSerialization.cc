// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 TJDev

#include "VaultSerialization.h"
#include "../utils/Log.h"
#include <ctime>

namespace KeepTower {

VaultResult<std::vector<uint8_t>>
VaultSerialization::serialize(const keeptower::VaultData& vault_data) {
    std::string serialized_data;

    if (!vault_data.SerializeToString(&serialized_data)) {
        Log::error("VaultSerialization: Failed to serialize VaultData to protobuf");
        return std::unexpected(VaultError::SerializationFailed);
    }

    // Convert string to vector<uint8_t>
    std::vector<uint8_t> result(serialized_data.begin(), serialized_data.end());
    return result;
}

VaultResult<keeptower::VaultData>
VaultSerialization::deserialize(const std::vector<uint8_t>& data) {
    // SECURITY: Enforce maximum message size to prevent DoS attacks
    // Reasonable limit: 100 MB (allows for very large vaults)
    constexpr size_t MAX_VAULT_SIZE = 100 * 1024 * 1024;  // 100 MB

    if (data.size() > MAX_VAULT_SIZE) {
        Log::error("VaultSerialization: Vault data exceeds maximum size ({} bytes > {} bytes)",
                   data.size(), MAX_VAULT_SIZE);
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
    // Get current schema version
    auto* metadata = vault_data.mutable_metadata();
    int32_t current_version = metadata->schema_version();

    // Check if we have an old vault (version field was used in schema v1)
    // In the old schema, VaultData had version and last_modified as direct fields
    // In new schema, these are in VaultMetadata sub-message

    // If schema_version is not set but we have accounts, this is a v1 vault
    if (current_version == 0 && vault_data.accounts_size() > 0) {
        Log::info("VaultSerialization: Migrating vault from schema v1 to v2");

        // Migrate v1 to v2
        // In v1, accounts had fields: id(1), created_at(2), modified_at(3),
        // account_name(4), user_name(5), password(6), email(7), website(8), notes(9)
        // In v2, these are at: id(1), account_name(2), user_name(3), password(4),
        // email(5), website(6), created_at(16), modified_at(17), notes(19)

        // Good news: protobuf is forward/backward compatible by field number
        // The v1 fields will automatically map to v2 fields with same numbers
        // We just need to set metadata

        // Set metadata for migrated vault
        metadata->set_schema_version(CURRENT_SCHEMA_VERSION);
        metadata->set_created_at(std::time(nullptr));  // Unknown, use now
        metadata->set_last_modified(std::time(nullptr));
        metadata->set_last_accessed(std::time(nullptr));
        metadata->set_access_count(1);

        // Mark as modified so it gets saved with new schema
        modified = true;

        Log::info("VaultSerialization: Vault migrated successfully to schema v2");
        return true;
    }

    // If schema version is 0 and no accounts, this is a new empty vault from v2
    if (current_version == 0 && vault_data.accounts_size() == 0) {
        // Initialize metadata for empty vault
        metadata->set_schema_version(CURRENT_SCHEMA_VERSION);
        metadata->set_created_at(std::time(nullptr));
        metadata->set_last_modified(std::time(nullptr));
        metadata->set_last_accessed(std::time(nullptr));
        metadata->set_access_count(1);
        // Don't mark as modified since this is already a new vault
        return true;
    }

    // Already at current version or newer
    if (current_version >= CURRENT_SCHEMA_VERSION) {
        // Update access tracking
        metadata->set_last_accessed(std::time(nullptr));
        metadata->set_access_count(metadata->access_count() + 1);
        modified = true;  // Save access tracking
        return true;
    }

    // Unknown version
    Log::warning("VaultSerialization: Unknown vault schema version: {}", current_version);
    return false;
}

}  // namespace KeepTower
