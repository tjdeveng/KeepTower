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
    std::string group_id;       ///< Stable group identifier.
    int32_t display_order{-1};  ///< Display order within the group.
};

/**
 * @brief Protobuf-free account list model for tree, search, and grouping views.
 */
struct AccountListItem {
    std::string id;                             ///< Stable account identifier.
    std::string account_name;                   ///< User-facing account label.
    std::string user_name;                      ///< Stored username/login value.
    std::string email;                          ///< Stored email address.
    std::string website;                        ///< Related website or URL.
    std::string notes;                          ///< Notes shown in list/search contexts.
    std::vector<std::string> tags;              ///< Assigned tag labels.
    std::vector<GroupMembershipView> groups;    ///< Group memberships for navigation.
    bool is_favorite{false};                    ///< True when marked as a favorite.
    bool is_archived{false};                    ///< True when archived from normal views.
    int32_t global_display_order{-1};           ///< Cross-group ordering hint.
};

/**
 * @brief Protobuf-free account detail model for editor-style views.
 *
 * This starts with the fields currently surfaced through VaultManager callers
 * and can be extended deliberately as more protobuf leakage is removed.
 */
struct AccountDetail {
    std::string id;                              ///< Stable account identifier.
    std::string account_name;                    ///< User-facing account label.
    std::string user_name;                       ///< Stored username/login value.
    std::string password;                        ///< Current password secret.
    std::string email;                           ///< Stored email address.
    std::string website;                         ///< Related website or URL.
    std::string notes;                           ///< Full notes/body text.
    std::vector<std::string> tags;               ///< Assigned tag labels.
    std::vector<std::string> password_history;   ///< Historical passwords when retained.
    std::vector<GroupMembershipView> groups;     ///< Group memberships for the account.
    bool is_favorite{false};                     ///< True when marked as a favorite.
    bool is_archived{false};                     ///< True when archived from normal views.
    bool is_admin_only_viewable{false};          ///< True when only admins may view details.
    bool is_admin_only_deletable{false};         ///< True when only admins may delete the account.
    int32_t global_display_order{-1};            ///< Cross-group ordering hint.
    int64_t created_at{0};                       ///< Unix timestamp when created.
    int64_t modified_at{0};                      ///< Unix timestamp of last modification.
    int64_t password_changed_at{0};              ///< Unix timestamp of last password change.
};

/**
 * @brief Protobuf-free group model for navigation and organization features.
 */
struct GroupView {
    std::string group_id;         ///< Stable group identifier.
    std::string group_name;       ///< User-facing group name.
    std::string description;      ///< Optional descriptive text.
    std::string color;            ///< Optional color token or hex value.
    std::string icon;             ///< Icon name used in UI lists.
    int32_t display_order{0};     ///< Order among peer groups.
    bool is_expanded{true};       ///< Preferred initial expansion state.
    bool is_system_group{false};  ///< True for built-in/system-managed groups.
};

/**
 * @brief Protobuf-free YubiKey model for configured-key management UI.
 */
struct YubiKeyView {
    std::string serial;   ///< Device serial or stable key identifier.
    std::string name;     ///< User-facing label for the key.
    int64_t added_at{0};  ///< Unix timestamp when the key was enrolled.
};

}  // namespace KeepTower