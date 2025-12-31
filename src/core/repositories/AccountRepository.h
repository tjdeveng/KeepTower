// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file AccountRepository.h
 * @brief Concrete implementation of IAccountRepository
 *
 * Part of Phase 2 refactoring: Repository Pattern
 * Delegates to VaultManager for actual data access
 */

#pragma once

#include "IAccountRepository.h"
#include "../VaultManager.h"
#include <memory>

namespace KeepTower {

/**
 * @brief Concrete account repository implementation
 *
 * Wraps VaultManager to provide the IAccountRepository interface.
 * Handles V1/V2 vault differences and permission checking.
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
class AccountRepository : public IAccountRepository {
public:
    /**
     * @brief Construct repository with VaultManager reference
     * @param vault_manager Non-owning pointer to VaultManager
     * @throws std::invalid_argument if vault_manager is null
     */
    explicit AccountRepository(VaultManager* vault_manager);

    ~AccountRepository() override = default;

    // Non-copyable, non-movable (holds non-owning pointer)
    AccountRepository(const AccountRepository&) = delete;
    AccountRepository& operator=(const AccountRepository&) = delete;
    AccountRepository(AccountRepository&&) = delete;
    AccountRepository& operator=(AccountRepository&&) = delete;

    // IAccountRepository interface implementation
    [[nodiscard]] std::expected<void, RepositoryError>
        add(const keeptower::AccountRecord& account) override;

    [[nodiscard]] std::expected<keeptower::AccountRecord, RepositoryError>
        get(size_t index) const override;

    [[nodiscard]] std::expected<keeptower::AccountRecord, RepositoryError>
        get_by_id(std::string_view account_id) const override;

    [[nodiscard]] std::expected<std::vector<keeptower::AccountRecord>, RepositoryError>
        get_all() const override;

    [[nodiscard]] std::expected<void, RepositoryError>
        update(size_t index, const keeptower::AccountRecord& account) override;

    [[nodiscard]] std::expected<void, RepositoryError>
        remove(size_t index) override;

    [[nodiscard]] std::expected<size_t, RepositoryError>
        count() const override;

    [[nodiscard]] bool can_view(size_t index) const noexcept override;

    [[nodiscard]] bool can_modify(size_t index) const noexcept override;

    [[nodiscard]] bool is_vault_open() const noexcept override;

    [[nodiscard]] std::optional<size_t>
        find_index_by_id(std::string_view account_id) const noexcept override;

private:
    VaultManager* m_vault_manager;  ///< Non-owning pointer to vault manager
};

}  // namespace KeepTower
