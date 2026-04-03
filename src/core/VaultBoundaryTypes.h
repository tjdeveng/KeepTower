// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file VaultBoundaryTypes.h
 * @brief Protobuf-free boundary models for VaultManager-facing APIs
 *
 * These types are intended for public service and facade boundaries where the
 * code should not depend directly on protobuf-generated vault messages.
 *
 * The vault storage format and internal serialization remain protobuf-based.
 * This header exists to stop protobuf details leaking across higher-level
 * interfaces while preserving the existing on-disk schema.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace KeepTower {

/**
 * @brief Group membership metadata for an account in a read model.
 */
struct GroupMembershipView {
    std::string group_id;
    int32_t display_order{-1};
};

/**
 * @brief Protobuf-free account list model for tree, search, and grouping views.
 */
struct AccountListItem {
    std::string id;
    std::string account_name;
    std::string user_name;
    std::string email;
    std::string website;
    std::string notes;
    std::vector<std::string> tags;
    std::vector<GroupMembershipView> groups;
    bool is_favorite{false};
    bool is_archived{false};
    int32_t global_display_order{-1};
};

/**
 * @brief Protobuf-free account detail model for editor-style views.
 *
 * This starts with the fields currently surfaced through VaultManager callers
 * and can be extended deliberately as more protobuf leakage is removed.
 */
struct AccountDetail {
    std::string id;
    std::string account_name;
    std::string user_name;
    std::string password;
    std::string email;
    std::string website;
    std::string notes;
    std::vector<std::string> tags;
    std::vector<GroupMembershipView> groups;
    bool is_favorite{false};
    bool is_archived{false};
    bool is_admin_only_viewable{false};
    bool is_admin_only_deletable{false};
    int32_t global_display_order{-1};
    int64_t created_at{0};
    int64_t modified_at{0};
    int64_t password_changed_at{0};
};

/**
 * @brief Protobuf-free group model for navigation and organization features.
 */
struct GroupView {
    std::string group_id;
    std::string group_name;
    std::string description;
    std::string color;
    std::string icon;
    int32_t display_order{0};
    bool is_expanded{true};
    bool is_system_group{false};
};

/**
 * @brief Protobuf-free YubiKey model for configured-key management UI.
 */
struct YubiKeyView {
    std::string serial;
    std::string name;
    int64_t added_at{0};
};

}  // namespace KeepTower