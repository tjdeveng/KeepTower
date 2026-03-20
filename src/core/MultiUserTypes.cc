// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "MultiUserTypes.h"

#include <openssl/crypto.h>

namespace KeepTower {

// ============================================================================
// PasswordHistoryEntry Model Logic
// ============================================================================

PasswordHistoryEntry::~PasswordHistoryEntry() {
    // Securely clear the password hash to prevent memory dumps
    // Salt is not sensitive (it's stored in plaintext in vault)
    OPENSSL_cleanse(hash.data(), hash.size());
}
} // namespace KeepTower
