// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file GroupRepository.h
 * @brief Concrete implementation of IGroupRepository
 *
 * Part of Phase 2 refactoring: Repository Pattern
 * Delegates to VaultManager for actual data access
 */

#pragma once

#include "IGroupRepository.h"
#include "../VaultManager.h"
#include <memory>

namespace KeepTower {

/**
 * @brief Concrete group repository implementation
 *
 * Wraps VaultManager to provide the IGroupRepository interface.
 * Manages account groups and account-group associations.
 *
 * This is a transitional implementation during refactoring:
 * - Phase 2a: Wraps existing VaultManager methods
 * - Phase 2b: Will move logic from VaultManager into this class
 * - Phase 3: VaultManager becomes thin coordinator
 *
 * Thread Safety:
 * - Same as underlying VaultManager
 * - Not thread-safe by default
 * - All calls must be from same thread
 */
class GroupRepository : public IGroupRepository {
public:
    /**
     * @brief Construct repository with VaultManager reference
     * @param vault_manager Non-owning pointer to VaultManager
     * @throws std::invalid_argument if vault_manager is null
     */
    explicit GroupRepository(VaultManager* vault_manager);

    ~GroupRepository() override = default;

    // Non-copyable, non-movable (holds non-owning pointer)
    GroupRepository(const GroupRepository&) = delete;
    GroupRepository& operator=(const GroupRepository&) = delete;
    GroupRepository(GroupRepository&&) = delete;
    GroupRepository& operator=(GroupRepository&&) = delete;

    // IGroupRepository interface implementation
    [[nodiscard]] std::expected<std::string, RepositoryError>
        create(std::string_view name) override;

    [[nodiscard]] std::expected<keeptower::AccountGroup, RepositoryError>
        get(std::string_view group_id) const override;

    [[nodiscard]] std::expected<std::vector<keeptower::AccountGroup>, RepositoryError>
        get_all() const override;

    [[nodiscard]] std::expected<void, RepositoryError>
        update(const keeptower::AccountGroup& group) override;

    [[nodiscard]] std::expected<void, RepositoryError>
        remove(std::string_view group_id) override;

    [[nodiscard]] std::expected<size_t, RepositoryError>
        count() const override;

    [[nodiscard]] std::expected<void, RepositoryError>
        add_account_to_group(size_t account_index, std::string_view group_id) override;

    [[nodiscard]] std::expected<void, RepositoryError>
        remove_account_from_group(size_t account_index, std::string_view group_id) override;

    [[nodiscard]] std::expected<std::vector<size_t>, RepositoryError>
        get_accounts_in_group(std::string_view group_id) const override;

    [[nodiscard]] bool is_vault_open() const noexcept override;

    [[nodiscard]] bool exists(std::string_view group_id) const noexcept override;

private:
    VaultManager* m_vault_manager;  ///< Non-owning pointer to vault manager
};

}  // namespace KeepTower
