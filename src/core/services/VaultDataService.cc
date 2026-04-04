// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 KeepTower Contributors

#include "VaultDataService.h"
#include "lib/vaultformat/VaultSerialization.h"

namespace KeepTower {

VaultResult<std::vector<uint8_t>> VaultDataService::serialize_vault_data(
    const keeptower::VaultData& vault_data) {
    return VaultSerialization::serialize(vault_data);
}

VaultResult<keeptower::VaultData> VaultDataService::deserialize_vault_data(
    const std::vector<uint8_t>& data) {
    return VaultSerialization::deserialize(data);
}

bool VaultDataService::migrate_vault_schema(
    keeptower::VaultData& vault_data,
    bool& modified) {
    return VaultSerialization::migrate_schema(vault_data, modified);
}

}  // namespace KeepTower
