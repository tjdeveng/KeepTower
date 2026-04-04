// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 KeepTower Contributors

#ifndef KEEPTOWER_VAULT_DATA_SERVICE_H
#define KEEPTOWER_VAULT_DATA_SERVICE_H

#include "../VaultError.h"
#include "../record.pb.h"
#include <vector>

namespace KeepTower {

/**
 * @brief Manager-facing protobuf payload facade for V2 vault workflows.
 *
 * VaultDataService exposes the workflow-level operations needed to convert
 * between keeptower::VaultData and its protobuf byte representation while
 * keeping VaultManager and VaultCreationOrchestrator out of the lower-level
 * VaultSerialization utility.
 *
 * **Responsibilities:**
 * - Serialize vault payload protobufs for save/create flows
 * - Deserialize vault payload protobufs for open flows
 * - Apply schema migration rules and modification tracking
 *
 * **NOT Responsible For:**
 * - File I/O or on-disk header handling (VaultFileService)
 * - Encryption/decryption (VaultCryptoService)
 * - Business workflow coordination (VaultManager, VaultCreationOrchestrator)
 */
class VaultDataService {
public:
    [[nodiscard]] static VaultResult<std::vector<uint8_t>> serialize_vault_data(
        const keeptower::VaultData& vault_data);

    [[nodiscard]] static VaultResult<keeptower::VaultData> deserialize_vault_data(
        const std::vector<uint8_t>& data);

    [[nodiscard]] static bool migrate_vault_schema(
        keeptower::VaultData& vault_data,
        bool& modified);

private:
    VaultDataService() = delete;
    ~VaultDataService() = delete;
    VaultDataService(const VaultDataService&) = delete;
    VaultDataService& operator=(const VaultDataService&) = delete;
    VaultDataService(VaultDataService&&) = delete;
    VaultDataService& operator=(VaultDataService&&) = delete;
};

}  // namespace KeepTower

#endif  // KEEPTOWER_VAULT_DATA_SERVICE_H
