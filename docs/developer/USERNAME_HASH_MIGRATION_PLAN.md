# Username Hash Algorithm Migration Plan

**Author:** GitHub Copilot
**Date:** 2026-01-21
**Status:** Design Phase
**Related:** USERNAME_HASHING_SECURITY_PLAN.md, KEK_DERIVATION_ALGORITHM_PLAN.md

## Executive Summary

This document describes the design and implementation strategy for migrating existing vaults from one username hashing algorithm to another. This is required when an administrator wants to upgrade vault security (e.g., SHA3-256 → Argon2id) without losing existing user accounts.

**Key Challenge:** Username hashes are used for authentication - changing the algorithm breaks existing logins unless we implement graceful migration.

**Solution:** Two-phase authentication with post-login migration. Users authenticate once with their old hash, then the system automatically upgrades them to the new algorithm.

---

## Problem Statement

### Current Behavior

When a vault is created, `VaultSecurityPolicy.username_hash_algorithm` is set (e.g., 0x01 for SHA3-256). Each user's KeySlot stores:

```cpp
std::array<uint8_t, 64> username_hash;     // Hash computed with policy algorithm
std::array<uint8_t, 16> username_salt;     // Per-user salt
uint8_t username_hash_size;                 // Size of hash (32, 48, or 64 bytes)
```

During authentication in `find_slot_by_username_hash()`:
1. User enters username "alice"
2. System reads `policy.username_hash_algorithm` (e.g., 0x01 = SHA3-256)
3. Computes: `hash = SHA3-256(username || username_salt)`
4. Compares `hash` against each KeySlot's `username_hash`
5. If match found → authentication proceeds

**If admin changes `policy.username_hash_algorithm` to 0x05 (Argon2id):**
- System computes `Argon2id(username || username_salt)` during login
- Compares against `SHA3-256(username || username_salt)` stored in KeySlot
- **MISMATCH** → "User not found" error
- User cannot login even with correct password

### YubiKey Implications

**Good News:** YubiKey FIDO2 credentials are NOT affected by username hash changes!

FIDO2 credentials use plaintext username as `user_id`:
```cpp
// YubiKeyManager.cc line 724
std::array<unsigned char, 32> user_id_hash{};
FIDO2::derive_salt_from_data(
    reinterpret_cast<const unsigned char*>(user_id.data()),  // Plaintext username
    user_id.size(),
    user_id_hash.data()
);
```

The credential ID is derived from SHA256(username), independent of `username_hash_algorithm`.

**However:** For defense-in-depth security, we may still want to offer optional YubiKey re-enrollment during migration (discussed below).

---

## Migration Architecture

### Design Principles

1. **Zero Data Loss:** All users must be able to login after algorithm change
2. **Transparent Migration:** Users don't need to know migration is happening
3. **Gradual Rollout:** Migration completes as users login naturally
4. **Rollback Support:** Admin can revert if issues arise
5. **Audit Trail:** Log all migration events for compliance

### Migration States

```
┌─────────────────────────────────────────────────────────────┐
│                    MIGRATION STATE MACHINE                   │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  [No Migration]  ───────────────────────────────────────┐   │
│        │                                                  │   │
│        │ Admin changes username_hash_algorithm            │   │
│        ▼                                                  │   │
│  [Migration Active]                                       │   │
│        │                                                  │   │
│        │ Users login one-by-one                           │   │
│        │  ├─> User A logs in ─> Migrated                 │   │
│        │  ├─> User B logs in ─> Migrated                 │   │
│        │  └─> User C logs in ─> Migrated                 │   │
│        │                                                  │   │
│        │ All users migrated                               │   │
│        ▼                                                  │   │
│  [Migration Complete]                                     │   │
│        │                                                  │   │
│        │ Admin confirms completion                        │   │
│        ▼                                                  │   │
│  [No Migration]  <───────────────────────────────────────┘   │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

### Data Structure Changes

#### VaultSecurityPolicy Extension

Add new fields to support migration:

```cpp
struct VaultSecurityPolicy {
    // ... existing fields ...

    uint8_t username_hash_algorithm = 0x01;  // Current/target algorithm

    // ========== MIGRATION SUPPORT (New) ==========

    /**
     * @brief Previous username hash algorithm (migration only)
     *
     * Set to the old algorithm when admin initiates migration.
     * Users authenticate using this algorithm until migrated.
     * Set to 0x00 when no migration is active.
     *
     * @note Only used during migration period
     * @note Must be 0x00 in non-migration state
     */
    uint8_t username_hash_algorithm_previous = 0x00;

    /**
     * @brief Migration timestamp (Unix epoch)
     *
     * When the migration was initiated. Used for:
     * - Audit logging
     * - Warning admins of stale migrations
     * - Rollback support
     *
     * @note 0 means no migration active
     */
    uint64_t migration_started_at = 0;

    /**
     * @brief Migration flags (bitfield)
     *
     * Bit 0: Migration active (1 = active, 0 = inactive)
     * Bit 1: Force YubiKey re-enrollment (1 = required, 0 = optional)
     * Bit 2: Reserved
     * Bit 3-7: Reserved
     *
     * @note All reserved bits must be 0
     */
    uint8_t migration_flags = 0x00;

    // Increase SERIALIZED_SIZE from 131 to 141 bytes (+10 bytes)
    static constexpr size_t SERIALIZED_SIZE = 141;
};
```

#### KeySlot Extension

Add per-user migration tracking:

```cpp
struct KeySlot {
    // ... existing fields ...

    /**
     * @brief Migration status for this user
     *
     * 0x00: Not migrated (using old algorithm)
     * 0x01: Migrated (using new algorithm)
     * 0xFF: Reserved for future use
     *
     * @note Only meaningful when migration is active
     */
    uint8_t migration_status = 0x00;

    /**
     * @brief Timestamp when this user was migrated
     *
     * Unix epoch timestamp. 0 means not yet migrated.
     * Used for audit trail and debugging.
     */
    uint64_t migrated_at = 0;
};
```

---

## Implementation Strategy

### Phase 1: Two-Phase Authentication

Modify `find_slot_by_username_hash()` to support fallback:

```cpp
static KeySlot* find_slot_by_username_hash(
    std::vector<KeySlot>& slots,
    const std::string& username,
    const VaultSecurityPolicy& policy) {

    // Determine which algorithms to try
    auto current_algo = static_cast<UsernameHashService::Algorithm>(
        policy.username_hash_algorithm);

    std::optional<UsernameHashService::Algorithm> fallback_algo;
    bool migration_active = (policy.migration_flags & 0x01) != 0;

    if (migration_active && policy.username_hash_algorithm_previous != 0x00) {
        fallback_algo = static_cast<UsernameHashService::Algorithm>(
            policy.username_hash_algorithm_previous);
    }

    // Phase 1: Try current algorithm first (for already-migrated users)
    for (auto& slot : slots) {
        if (!slot.active) continue;

        // Skip users who haven't migrated yet (they use old algorithm)
        if (migration_active && slot.migration_status == 0x00) {
            continue;  // Will be checked in Phase 2
        }

        std::span<const uint8_t> stored_hash(slot.username_hash.data(),
                                              slot.username_hash_size);
        if (UsernameHashService::verify_username(username, stored_hash,
                                                   current_algo, slot.username_salt)) {
            slot.username = username;  // Populate for UI
            return &slot;
        }
    }

    // Phase 2: Try fallback algorithm (for not-yet-migrated users)
    if (migration_active && fallback_algo.has_value()) {
        for (auto& slot : slots) {
            if (!slot.active) continue;
            if (slot.migration_status != 0x00) continue;  // Already migrated

            std::span<const uint8_t> stored_hash(slot.username_hash.data(),
                                                  slot.username_hash_size);
            if (UsernameHashService::verify_username(username, stored_hash,
                                                       *fallback_algo, slot.username_salt)) {
                slot.username = username;
                // Mark that this user needs migration
                slot.migration_status = 0xFF;  // Temporary flag: "pending migration"
                return &slot;
            }
        }
    }

    return nullptr;  // User not found
}
```

**Key Points:**
- Migrated users (status=0x01) authenticate with NEW algorithm
- Not-yet-migrated users (status=0x00) authenticate with OLD algorithm
- Temporary status=0xFF indicates "just authenticated with old algo, needs migration"

### Phase 2: Post-Login Migration

After successful authentication in `open_vault_v2()`, check for pending migration:

```cpp
KeepTower::VaultResult<KeepTower::UserSession> VaultManager::open_vault_v2(
    const std::string& path,
    const Glib::ustring& username,
    const Glib::ustring& password,
    const std::string& yubikey_serial) {

    // ... existing authentication code ...

    // Check if user needs migration
    bool migration_active = (m_v2_header->security_policy.migration_flags & 0x01) != 0;
    if (migration_active && user_slot->migration_status == 0xFF) {
        Log::info("VaultManager: User {} requires username hash migration", username.raw());

        // Schedule post-login migration
        // This must happen AFTER vault is fully opened but BEFORE user can access data
        auto migration_result = migrate_user_hash(user_slot, username.raw(), password.raw());

        if (!migration_result) {
            Log::error("VaultManager: Failed to migrate user {}: {}",
                      username.raw(), to_string(migration_result.error()));
            // Don't fail login - user can still access vault
            // Migration can be retried on next login
        }
    }

    // ... rest of authentication code ...
}
```

### Phase 3: User Hash Migration

Implement the actual migration logic:

```cpp
VaultResult<> VaultManager::migrate_user_hash(
    KeySlot* user_slot,
    const std::string& username,
    const std::string& password) {

    if (!user_slot || !m_v2_header) {
        return std::unexpected(VaultError::InvalidState);
    }

    Log::info("VaultManager: Starting username hash migration for user: {}", username);

    // Get new algorithm from policy
    auto new_algo = static_cast<UsernameHashService::Algorithm>(
        m_v2_header->security_policy.username_hash_algorithm);

    // Generate new salt for username (best practice: don't reuse salt)
    std::vector<uint8_t> new_username_salt_vec = VaultCrypto::generate_random_bytes(16);
    std::array<uint8_t, 16> new_username_salt{};
    std::copy_n(new_username_salt_vec.begin(), 16, new_username_salt.begin());

    // Compute new username hash with new algorithm
    auto new_hash_result = UsernameHashService::hash_username(
        username, new_algo, new_username_salt);

    if (!new_hash_result) {
        Log::error("VaultManager: Failed to compute new username hash");
        return std::unexpected(VaultError::CryptoError);
    }

    // Update KeySlot
    const auto& new_hash_vec = new_hash_result.value();
    std::fill(user_slot->username_hash.begin(), user_slot->username_hash.end(), 0);
    std::copy_n(new_hash_vec.begin(),
                std::min(new_hash_vec.size(), size_t(64)),
                user_slot->username_hash.begin());
    user_slot->username_hash_size = static_cast<uint8_t>(new_hash_vec.size());
    user_slot->username_salt = new_username_salt;

    // Mark as migrated
    user_slot->migration_status = 0x01;
    user_slot->migrated_at = std::time(nullptr);

    // Save vault immediately (critical!)
    if (!save_vault(true)) {  // Create backup before migration
        Log::error("VaultManager: Failed to save vault after migration");
        return std::unexpected(VaultError::FileWriteError);
    }

    Log::info("VaultManager: Successfully migrated user {} to algorithm 0x{:02x}",
             username, m_v2_header->security_policy.username_hash_algorithm);

    return {};  // Success
}
```

**Critical Security Note:** We do NOT need to re-derive the KEK or re-wrap the DEK! The username hash is only used for authentication lookup. The password → KEK derivation is separate and uses `KeySlot.kek_derivation_algorithm`.

### Phase 4: YubiKey Re-Enrollment (Optional)

If `migration_flags` bit 1 is set (force YubiKey re-enrollment):

```cpp
VaultResult<> VaultManager::migrate_user_hash(
    KeySlot* user_slot,
    const std::string& username,
    const std::string& password) {

    // ... username hash migration (above) ...

    // Check if YubiKey re-enrollment is required
    bool force_yubikey_reenroll = (m_v2_header->security_policy.migration_flags & 0x02) != 0;

    if (force_yubikey_reenroll && user_slot->yubikey_enrolled) {
        Log::info("VaultManager: YubiKey re-enrollment required for user: {}", username);

        // Mark user for forced YubiKey re-enrollment
        // This will be handled by UI layer (show re-enrollment dialog)
        user_slot->must_change_password = true;  // Reuse this flag to trigger UI flow

        // Alternative: Add new flag "yubikey_reenroll_required"
        // user_slot->yubikey_reenroll_required = true;
    }

    return {};
}
```

**Why might we want YubiKey re-enrollment?**
- Defense-in-depth: If username algorithm is being upgraded for security, might as well refresh YubiKey too
- Regulatory compliance: Some standards require periodic re-enrollment
- User management: Detect and remove inactive YubiKeys

**Why we probably don't need it:**
- FIDO2 credentials are already cryptographically strong
- Re-enrollment requires user interaction (disruptive)
- Credential ID doesn't depend on username hash algorithm

**Recommendation:** Make it optional via migration flag, default to OFF.

---

## Admin UI Flow

### Initiating Migration

Add UI in Admin Settings dialog:

```
┌─────────────────────────────────────────────────────────────┐
│ Vault Security Settings (Admin Only)                        │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│ Current Username Hash Algorithm: SHA3-256                    │
│                                                               │
│ Change Algorithm: [Argon2id ▼]  [Migrate All Users]        │
│                                                               │
│ ⚠ Warning: This will require all users to re-authenticate   │
│   on their next login. Migration happens automatically.      │
│                                                               │
│ Migration Options:                                           │
│   ☐ Force YubiKey re-enrollment (more secure, disruptive)  │
│   ☑ Create backup before migration                          │
│                                                               │
│ Estimated Impact:                                            │
│   - 5 users need migration                                   │
│   - Average migration time: < 1 second per user             │
│   - No vault data re-encryption required                     │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

### Confirmation Dialog

```
┌─────────────────────────────────────────────────────────────┐
│ Confirm Username Hash Migration                              │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│ You are about to change the username hash algorithm from:    │
│                                                               │
│   SHA3-256 (current) → Argon2id (new)                       │
│                                                               │
│ What will happen:                                            │
│   ✓ All 5 users can still login with their passwords        │
│   ✓ Users will be automatically upgraded on next login       │
│   ✓ Migration is transparent (users won't notice)           │
│   ✓ Backup will be created before changes                    │
│                                                               │
│ What you need to do:                                         │
│   • Monitor migration progress in Admin dashboard            │
│   • Ensure all users login within 30 days                    │
│   • Do not change algorithm again until complete             │
│                                                               │
│ [Cancel]                              [Start Migration]     │
└─────────────────────────────────────────────────────────────┘
```

### Migration Progress Dashboard

```
┌─────────────────────────────────────────────────────────────┐
│ Username Hash Migration Progress                             │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│ Status: In Progress                                          │
│ Started: 2026-01-20 14:32:15                                │
│ Algorithm: SHA3-256 → Argon2id                              │
│                                                               │
│ Progress: ████████████░░░░░░░░ 60% (3 of 5 users)           │
│                                                               │
│ User Status:                                                 │
│   ✓ alice    - Migrated (2026-01-20 15:05:32)              │
│   ✓ bob      - Migrated (2026-01-20 16:22:18)              │
│   ✓ charlie  - Migrated (2026-01-21 09:15:44)              │
│   ⏳ david    - Pending (last login: 2026-01-15)            │
│   ⏳ eve      - Pending (last login: 2026-01-18)            │
│                                                               │
│ Actions:                                                     │
│   [Send Reminder Email to Pending Users]                     │
│   [Force Complete Migration] (migrates all users now)        │
│   [Rollback Migration] (revert to SHA3-256)                  │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

---

## Edge Cases and Error Handling

### 1. User Never Logs In

**Problem:** User "david" doesn't login for months. Migration stalls.

**Solutions:**
- **Option A:** Admin can force-complete migration (generates new hash with random salt)
  - Risk: If David has the old password, authentication will fail
  - Mitigation: Require admin to reset David's password

- **Option B:** Migration timeout (after 90 days, auto-migrate with warning)
  - Safer: Keep old algorithm as fallback forever
  - Cleaner: Force migration and require password reset for stragglers

**Recommendation:** Admin-initiated "Force Complete" with password reset option.

### 2. Migration Interrupted (Crash/Power Loss)

**Problem:** Vault crashes during migration, leaving KeySlot in inconsistent state.

**Protection:**
1. Use `migration_status = 0xFF` as temporary state during migration
2. Check for status=0xFF on vault open → resume migration
3. Backup vault before migration starts
4. Use atomic write (write to temp file, then rename)

```cpp
// In open_vault_v2(), after loading header
for (auto& slot : m_v2_header->key_slots) {
    if (slot.migration_status == 0xFF) {
        Log::warning("VaultManager: Detected interrupted migration for user {}", slot.username);
        // Mark for immediate re-migration attempt
        // This is safe because user authenticated successfully
    }
}
```

### 3. Argon2id → SHA3-256 (Downgrade)

**Problem:** Admin wants to downgrade from Argon2id to SHA3-256 (bad idea, but possible).

**Handling:**
- Show warning: "This reduces security. Are you sure?"
- Require reason/justification (audit log)
- Proceed with same migration mechanism
- Mark migration as "downgrade" in logs

### 4. Multiple Rapid Changes

**Problem:** Admin changes SHA3 → Argon2id → PBKDF2 in quick succession.

**Protection:**
- Only allow one migration at a time
- Check `migration_flags` before starting new migration
- Show error: "Migration already in progress. Please wait for completion."

### 5. Concurrent Login During Migration

**Problem:** User logs in while their KeySlot is being migrated by another session.

**Protection:**
- Use vault-level locking (already exists)
- Migration happens inside `open_vault_v2()` which has exclusive access
- No concurrent modification possible (vault is locked during auth)

---

## Testing Strategy

### Unit Tests

```cpp
// Test: Two-phase authentication
TEST(VaultManagerV2Test, MigrationTwoPhaseAuth) {
    // Setup: Create vault with SHA3-256
    VaultManager vm;
    vm.create_vault_v2(..., policy_with_sha3);

    // Add user "alice"
    vm.add_user("alice", "password123", ...);

    // Change policy to Argon2id
    vm.initiate_username_hash_migration(0x05);  // Argon2id

    // Test: Alice can still login
    auto result = vm.open_vault_v2(path, "alice", "password123");
    EXPECT_TRUE(result.has_value());

    // Verify: Alice's KeySlot was migrated
    auto slot = vm.get_user_slot("alice");
    EXPECT_EQ(slot->migration_status, 0x01);
}

// Test: YubiKey not affected
TEST(VaultManagerV2Test, MigrationYubiKeyUnaffected) {
    // Setup: Create vault with YubiKey
    VaultManager vm;
    vm.create_vault_v2(..., policy_with_yubikey);

    // Store credential ID
    auto original_cred_id = vm.get_user_slot("alice")->yubikey_credential_id;

    // Migrate algorithm
    vm.initiate_username_hash_migration(0x05);
    vm.open_vault_v2(...);  // Trigger migration

    // Verify: Credential ID unchanged
    auto new_cred_id = vm.get_user_slot("alice")->yubikey_credential_id;
    EXPECT_EQ(original_cred_id, new_cred_id);
}
```

### Integration Tests

1. **Happy Path:** SHA3-256 → Argon2id, all users migrate successfully
2. **Rollback:** Start migration, rollback before completion
3. **Partial Migration:** 3 of 5 users migrate, verify both algorithms work
4. **Crash Recovery:** Interrupt migration, verify recovery on next open
5. **Performance:** Measure migration time with 100 users

### Manual Testing Checklist

- [ ] Create vault with SHA3-256
- [ ] Add 3 users (alice, bob, charlie)
- [ ] Initiate migration to Argon2id
- [ ] Login as alice → verify migration
- [ ] Login as bob → verify migration
- [ ] Check admin dashboard shows 2/3 complete
- [ ] Close vault, reopen
- [ ] Login as charlie → verify migration
- [ ] Verify admin dashboard shows "Migration Complete"
- [ ] Confirm no password/YubiKey re-entry needed

---

## Migration Checklist (for Implementation)

### Data Structures
- [ ] Add `username_hash_algorithm_previous` to VaultSecurityPolicy
- [ ] Add `migration_started_at` to VaultSecurityPolicy
- [ ] Add `migration_flags` to VaultSecurityPolicy
- [ ] Add `migration_status` to KeySlot
- [ ] Add `migrated_at` to KeySlot
- [ ] Update serialization/deserialization for both structs
- [ ] Increase `VaultSecurityPolicy::SERIALIZED_SIZE` to 141 bytes
- [ ] Update `KeySlot::SERIALIZED_SIZE` accordingly

### Core Logic
- [ ] Modify `find_slot_by_username_hash()` for two-phase auth
- [ ] Implement `migrate_user_hash()` in VaultManager
- [ ] Add migration check in `open_vault_v2()`
- [ ] Implement crash recovery (check for status=0xFF on vault open)
- [ ] Add vault backup before migration
- [ ] Add migration completion detection

### Admin UI
- [ ] Add "Change Username Hash Algorithm" button to Admin Settings
- [ ] Show confirmation dialog with impact analysis
- [ ] Add migration progress dashboard
- [ ] Add "Force Complete Migration" action
- [ ] Add "Rollback Migration" action
- [ ] Show warning if migration is stale (>30 days)

### User UI
- [ ] No changes needed (migration is transparent)
- [ ] Optional: Show "Upgrading security..." toast after login

### Testing
- [ ] Unit tests for two-phase authentication
- [ ] Unit tests for migration logic
- [ ] Integration test for full migration flow
- [ ] Crash recovery test
- [ ] Performance test with many users

### Documentation
- [ ] Update USERNAME_HASHING_SECURITY_PLAN.md
- [ ] Add admin guide section on migrations
- [ ] Add troubleshooting guide for stalled migrations
- [ ] Update API documentation

---

## Security Considerations

### Threat Model

**Threat 1: Attacker observes migration in progress**
- Risk: Attacker might try to login during migration window
- Mitigation: Both algorithms are cryptographically strong. No timing advantage.

**Threat 2: Rollback attack**
- Risk: Attacker forces vault to use weaker old algorithm
- Mitigation: Migration is one-way. Old algorithm only works for not-yet-migrated users.

**Threat 3: Side-channel timing attack**
- Risk: Different hash verification times reveal which algorithm was used
- Mitigation: Both paths use constant-time comparison. No timing leak.

**Threat 4: Partial migration DoS**
- Risk: Attacker prevents users from logging in, stalling migration
- Mitigation: Admin can force-complete migration. Backup allows rollback.

### Cryptographic Guarantees

- **Username hash strength:** Unchanged (algorithm is being upgraded, not downgraded)
- **KEK derivation:** Unchanged (separate from username hashing)
- **Vault DEK:** Unchanged (no re-encryption needed)
- **YubiKey credentials:** Unchanged (independent of username algorithm)

### Audit Trail

Log all migration events:
```
2026-01-20 14:32:15 [ADMIN] User 'admin' initiated username hash migration SHA3-256→Argon2id
2026-01-20 15:05:32 [MIGRATION] User 'alice' migrated to Argon2id
2026-01-20 16:22:18 [MIGRATION] User 'bob' migrated to Argon2id
2026-01-21 09:15:44 [MIGRATION] User 'charlie' migrated to Argon2id
2026-01-21 09:15:45 [MIGRATION] All users migrated. Marking migration complete.
```

---

## Performance Impact

### Migration Cost Per User

```
Operation                          Time (typical)
────────────────────────────────────────────────
Generate new salt (16 bytes)       < 1ms
Compute SHA3-256 hash              ~1ms
Compute Argon2id hash (256MB)      ~50ms
Update KeySlot fields              < 1ms
Save vault to disk                 ~10ms
────────────────────────────────────────────────
Total per user                     ~62ms
```

**For 100 users:** ~6.2 seconds total migration time (but spread across individual logins).

### Login Performance During Migration

**Case 1: User already migrated**
- Normal login time (no extra cost)

**Case 2: User not yet migrated**
- Try new algorithm first: +50ms (Argon2id verification fails)
- Try old algorithm: +1ms (SHA3-256 verification succeeds)
- Perform migration: +62ms
- **Total overhead:** ~113ms (acceptable for one-time migration)

**Case 3: No migration active**
- Normal login time (no extra cost)

---

## Alternatives Considered

### Alternative 1: Forced Password Reset

**Approach:** Change algorithm, invalidate all passwords, force reset **without validating old password**.

**How it would work:**
1. Admin changes username hash algorithm
2. System marks all KeySlots as "password invalid"
3. Users cannot login with existing passwords
4. Users must use "Forgot Password" flow or admin reset
5. New password is hashed with new algorithm

**Critical Detail:** This approach does NOT validate the old password first. It's equivalent to locking everyone out and forcing them through password recovery. We cannot validate the old password because:
- We only have the hash, not the password
- Even if we could validate, we'd have the plaintext password and could just migrate it (Alternative 4)

**🚨 SECURITY RISK:** Without authentication, there's no proof the user is legitimate:
- Attacker could trigger password reset flow
- User loses access until admin manually intervenes
- Potential for denial-of-service attack
- No cryptographic proof of user identity before reset

**Pros:**
- Simplest implementation (just mark slots invalid)
- Forces password rotation (good security practice)
- Immediate migration completion (no waiting for users)

**Cons:**
- **SECURITY RISK:** No authentication before password reset
- Extremely disruptive to users (cannot access their data)
- High support burden ("I forgot my password" tickets)
- Users might choose weaker passwords under pressure
- Security team would need to manually reset all passwords
- Risk of data access loss if admin is unavailable

**Verdict:** ❌ **Rejected. Security risk and operationally dangerous.**

### Alternative 2: Dual-Hash Storage

**Approach:** Store both old and new hashes simultaneously.

**Pros:**
- No migration window
- Instant rollback possible

**Cons:**
- Doubles storage per user
- Never know when to remove old hash
- Potential downgrade attack surface

**Verdict:** Rejected. Storage bloat and unclear lifecycle.

### Alternative 3: Background Migration (Proactive)

**Approach:** Admin triggers migration, system re-hashes all users immediately.

**Pros:**
- No per-user migration delay
- Completes in seconds

**Cons:**
- Can't verify old hash (we don't have passwords!)
- Would need to generate new random salt (breaks authentication)
- Not actually possible without user passwords

**Verdict:** Rejected. Cryptographically impossible.

### Alternative 4: Chosen Solution (Two-Phase with Post-Login Migration)

**Approach:** User enters password **ONCE** during normal login, system automatically migrates in background.

**How it works (user perspective):**
1. User opens vault: "alice" / "password123" ← **ONLY PASSWORD ENTRY**
2. User accesses their data normally
3. User logs out
4. ✅ Migration happened silently during step 1 (user unaware)

**How it works (system perspective):**
1. User submits username="alice", password="password123"
2. System tries new algorithm (Argon2id) - fails (user not migrated yet)
3. System tries old algorithm (SHA3-256) - succeeds! (authentication OK)
4. **🔒 SECURITY CHECKPOINT:** Authentication succeeded - user is legitimate
5. **KEY POINT:** System now has plaintext password in memory temporarily
6. System computes new_hash = Argon2id("alice" || new_salt)
7. System updates KeySlot with new hash and new salt
8. System saves vault (migration complete for alice)
9. System clears password from memory (security)
10. User is now authenticated and accessing vault normally

**Critical Security Detail:** Migration ONLY happens after successful authentication (step 4). If authentication fails at step 3:
- ❌ No migration occurs
- ❌ Hash is NOT updated
- ❌ Attacker gains nothing
- ✅ Vault remains secure

This ensures only legitimate users with correct passwords trigger migration. An attacker guessing passwords will never cause migration to occur.

**Critical Detail:** The user's password is entered **exactly once** during their normal login. No additional password prompt. No user action required. The migration happens automatically in the background between steps 3 and 9 above.

**Pros:**
- **Completely transparent to users** (zero disruption)
- No data loss (authentication always succeeds)
- Gradual, safe rollout (users migrate as they login)
- Fully auditable (every migration logged)
- No password changes required (same password works)
- Works with YubiKey (no re-enrollment needed)

**Cons:**
- Slight complexity in auth logic (two-phase lookup)
- Requires tracking migration state (per-user flag)
- Migration completion depends on users logging in

**Verdict:** **SELECTED.** Best balance of security, usability, and implementability.

---

## Future Enhancements

### 1. Migration Scheduler

Instead of waiting for user login, admin could:
- Send email: "Please login by [date] to complete security upgrade"
- Show countdown in UI: "Migration completes in 7 days"
- Auto-reset passwords for users who don't migrate in time

### 2. Algorithm Deprecation Warnings

- Show warning if vault uses SHA3-256: "Consider upgrading to Argon2id"
- Add "Security Health Score" that factors in algorithm choice
- Periodic reminders to keep security current

### 3. Batch Admin Operations

- "Migrate All Now" button (requires all user passwords - not practical)
- "Reset Unmigrated Users" button (force password reset for stragglers)
- "Export Migration Report" (CSV with user migration status)

### 4. FIPS Migration

If FIPS mode changes (disabled → enabled), automatically:
- Block non-FIPS algorithms
- Force migration to PBKDF2 (only FIPS-approved option)
- Show compliance dashboard

---

## Conclusion

Username hash algorithm migration is achievable with a two-phase authentication approach:

1. **Phase 1:** Admin changes algorithm, system enters migration mode
2. **Phase 2:** Users login normally, system detects old hash, migrates transparently
3. **Phase 3:** After all users migrate, admin marks migration complete

**Key Benefits:**
- ✅ Zero user disruption
- ✅ No password changes required
- ✅ YubiKey credentials preserved
- ✅ Full audit trail
- ✅ Rollback support

**Next Steps:**
1. Review this design with security team
2. Implement data structure changes
3. Add unit tests for two-phase auth
4. Implement UI for admin controls
5. Conduct integration testing
6. Document in user manual

**Estimated Implementation Time:** 3-5 days for core logic + UI + tests.

