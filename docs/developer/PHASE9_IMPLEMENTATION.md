# Phase 9: User Password History Tracking

**Status:** Planning
**Priority:** High
**Estimated Effort:** 2-3 days
**Dependencies:** Phases 1-8 (all completed)

---

## Overview

Implement password history tracking for vault users to prevent password reuse. This enhances security by ensuring users cannot cycle through a small set of passwords, which is a common security weakness.

**Important:** This tracks **vault user login passwords**, not account passwords (which already have history tracking implemented).

---

## Goals

1. Track configurable number of previous passwords per user (default: 5, range: 0-24)
2. Securely hash and store password history
3. Prevent password reuse during password changes
4. Provide admin tools to clear password history when needed
5. Add global vault policy for password history depth
6. Integrate seamlessly with existing password change flows

---

## Technical Design

### 1. Protobuf Schema Changes

**File:** `src/record.proto`

Add password history to `UserKeySlot`:

```protobuf
message UserKeySlot {
  bytes encrypted_master_key = 1;
  bytes salt = 2;
  uint32 iterations = 3;
  string username = 4;
  UserRole role = 5;
  bool is_active = 6;
  string display_name = 7;

  // Password history (for preventing reuse)
  repeated bytes password_hashes = 8;  // Argon2id hashes of previous passwords
}
```

Add global policy to `VaultHeader`:

```protobuf
message VaultHeader {
  uint32 version = 1;
  uint64 created_timestamp = 2;
  string created_by = 3;
  bytes master_key_check = 4;
  repeated UserKeySlot key_slots = 5;
  SecurityPolicy security_policy = 6;

  // New: Password history policy
  PasswordHistoryPolicy password_history_policy = 7;
}

message PasswordHistoryPolicy {
  uint32 history_depth = 1;  // Number of previous passwords to remember (0-24, default 5)
  bool enforce_for_admins = 2;  // Whether admins are subject to history (default true)
  bool enforce_for_users = 3;   // Whether standard users are subject to history (default true)
}
```

### 2. VaultManager Methods

**New Methods:**

```cpp
/**
 * @brief Check if password was used previously by user
 *
 * Compares new password against stored password history using secure
 * constant-time comparison of Argon2id hashes.
 *
 * @param slot_index User's key slot index
 * @param new_password Password to check
 * @return true if password found in history (reuse detected)
 * @return false if password is new/acceptable
 * @throws std::runtime_error If vault not open or slot invalid
 */
[[nodiscard]] bool is_password_in_history(
    size_t slot_index,
    const std::string& new_password) const;

/**
 * @brief Add password to user's history during password change
 *
 * Hashes the old password with Argon2id and adds to history ring buffer.
 * Automatically maintains configured history depth (FIFO eviction).
 *
 * @param slot_index User's key slot index
 * @param old_password Password to add to history
 * @throws std::runtime_error If vault not open or slot invalid
 */
void add_password_to_history(
    size_t slot_index,
    const std::string& old_password);

/**
 * @brief Clear password history for a user (admin operation)
 *
 * Removes all stored password hashes from user's history.
 * Used for account recovery or security policy changes.
 *
 * @param slot_index User's key slot index
 * @throws std::runtime_error If vault not open, slot invalid, or insufficient permissions
 * @pre Current user must be administrator
 */
void clear_user_password_history(size_t slot_index);

/**
 * @brief Get password history policy
 *
 * @return Current password history configuration
 * @throws std::runtime_error If vault not open
 */
[[nodiscard]] PasswordHistoryPolicy get_password_history_policy() const;

/**
 * @brief Set password history policy (admin only)
 *
 * @param policy New password history configuration
 * @throws std::runtime_error If vault not open or insufficient permissions
 * @pre Current user must be administrator
 */
void set_password_history_policy(const PasswordHistoryPolicy& policy);
```

**Modified Methods:**

Update `change_user_password()` to integrate history checking:

```cpp
bool VaultManager::change_user_password(
    const std::string& current_password,
    const std::string& new_password)
{
    // ... existing authentication ...

    // Check password history (if policy enabled)
    const auto policy = get_password_history_policy();
    const bool enforce = (current_role == UserRole::ADMINISTRATOR)
        ? policy.enforce_for_admins()
        : policy.enforce_for_users();

    if (enforce && policy.history_depth() > 0) {
        if (is_password_in_history(current_slot_index, new_password)) {
            Log::warning("Password change rejected: password found in history");
            return false;  // Password reuse detected
        }
    }

    // Add old password to history before changing
    if (enforce && policy.history_depth() > 0) {
        add_password_to_history(current_slot_index, current_password);
    }

    // ... existing password change logic ...
}
```

### 3. Password Hashing Strategy

**Use Argon2id for history hashes:**

```cpp
// Different from KEK derivation - use separate context
struct PasswordHistoryHash {
    static constexpr uint32_t MEMORY_COST = 65536;   // 64 MB
    static constexpr uint32_t TIME_COST = 3;          // 3 iterations
    static constexpr uint32_t PARALLELISM = 1;        // Single thread
    static constexpr size_t HASH_LENGTH = 32;         // 256 bits

    // Use random salt per hash for additional security
    static constexpr size_t SALT_LENGTH = 16;
};

// Store format in password_hashes field:
// [salt (16 bytes)][hash (32 bytes)] = 48 bytes per entry
```

**Security rationale:**
- Separate from KEK derivation (different parameters)
- Random salt prevents rainbow table attacks
- Lower parameters than KEK (faster comparison, still secure)
- Constant-time comparison prevents timing attacks

### 4. Default Policy

```cpp
constexpr PasswordHistoryPolicy DEFAULT_PASSWORD_HISTORY_POLICY = {
    .history_depth = 5,           // Remember last 5 passwords
    .enforce_for_admins = true,   // Admins subject to policy
    .enforce_for_users = true     // Users subject to policy
};
```

### 5. UI Integration

#### 5.1 Password History Settings (PreferencesWindow)

Add new section in Security preferences:

```
┌─ Password History ─────────────────────────────┐
│                                                 │
│ Remember previous passwords: [✓]                │
│                                                 │
│ History depth: [5 passwords ▾]                  │
│ ├─ 0 (disabled)                                 │
│ ├─ 3 passwords                                  │
│ ├─ 5 passwords (default)                        │
│ ├─ 10 passwords                                 │
│ └─ 24 passwords                                 │
│                                                 │
│ Enforce for:                                    │
│ [✓] Administrators                              │
│ [✓] Standard users                              │
│                                                 │
│ ℹ️  Prevents users from reusing previous vault  │
│    login passwords. Does not affect account     │
│    password history (managed separately).       │
│                                                 │
│ 🔧 Admin only: [Clear All History...]          │
│                                                 │
└─────────────────────────────────────────────────┘
```

**Implementation notes:**
- Only show if V2 vault format
- Disable controls if not administrator
- "Clear All History" button opens confirmation dialog
- Tooltip explains difference from account password history

#### 5.2 Change Password Dialog Updates

Enhance `ChangePasswordDialog` error handling:

```cpp
// In on_ok_button_clicked():
if (!vault_manager_->change_user_password(current_pw, new_pw)) {
    // Check specific error type
    if (vault_manager_->is_password_in_history(slot, new_pw)) {
        show_error_message(
            "Password Previously Used",
            "This password was recently used. Please choose a different password.\n\n"
            "Password history prevents reusing your last 5 passwords for security."
        );
    } else {
        show_error_message("Password Change Failed", "Authentication error.");
    }
    return;
}
```

#### 5.3 User Management Dialog Enhancement

Add "Clear Password History" option in user context menu (admin only):

```
Right-click on user →
  ├─ Reset Password...
  ├─ Remove User...
  └─ Clear Password History... ← NEW
```

Confirmation dialog:
```
┌─ Clear Password History ───────────────────┐
│                                             │
│ Clear password history for user:           │
│ john.doe                                    │
│                                             │
│ This will allow the user to reuse any      │
│ previous password on their next change.    │
│                                             │
│ This action cannot be undone.              │
│                                             │
│ [Cancel]              [Clear History]      │
└─────────────────────────────────────────────┘
```

---

## Implementation Steps

### Step 1: Protobuf Schema (1 hour)
- [ ] Add `password_hashes` field to `UserKeySlot`
- [ ] Add `PasswordHistoryPolicy` message
- [ ] Add `password_history_policy` to `VaultHeader`
- [ ] Regenerate protobuf code
- [ ] Update default vault creation to include default policy

### Step 2: Core VaultManager Methods (4 hours)
- [ ] Implement `is_password_in_history()` with constant-time comparison
- [ ] Implement `add_password_to_history()` with ring buffer logic
- [ ] Implement `clear_user_password_history()`
- [ ] Implement `get_password_history_policy()`
- [ ] Implement `set_password_history_policy()`
- [ ] Update `change_user_password()` integration
- [ ] Add Argon2id hashing helper for password history

### Step 3: VaultManager Migration Support (2 hours)
- [ ] Add version check in vault loading
- [ ] Initialize empty history for existing V2 vaults
- [ ] Add default policy if missing
- [ ] Ensure backward compatibility

### Step 4: Unit Tests (4 hours)
- [ ] Test password history addition and ring buffer
- [ ] Test password reuse detection (positive/negative cases)
- [ ] Test history clearing
- [ ] Test policy enforcement (admin vs user)
- [ ] Test policy get/set operations
- [ ] Test migration from vaults without history
- [ ] Test edge cases (disabled history, depth 0, depth 24)
- [ ] Test constant-time comparison (timing analysis)

### Step 5: UI - Preferences Window (3 hours)
- [ ] Add password history section to PreferencesWindow
- [ ] Add history depth dropdown
- [ ] Add enforcement checkboxes
- [ ] Add "Clear All History" admin button
- [ ] Add confirmation dialog for clearing
- [ ] Connect to VaultManager methods
- [ ] Add tooltips and help text
- [ ] Test with admin/user permissions

### Step 6: UI - Password Change Dialog (2 hours)
- [ ] Update error handling in ChangePasswordDialog
- [ ] Add specific message for password reuse
- [ ] Test error display and user flow
- [ ] Update documentation

### Step 7: UI - User Management Dialog (2 hours)
- [ ] Add "Clear Password History" context menu item
- [ ] Implement confirmation dialog
- [ ] Connect to VaultManager method
- [ ] Test with multiple users
- [ ] Ensure admin-only access

### Step 8: Integration Testing (2 hours)
- [ ] Test full password change flow with history
- [ ] Test policy changes affect new password changes
- [ ] Test admin clearing user history
- [ ] Test migration of old vaults
- [ ] Test with various policy configurations
- [ ] Verify no regressions in existing password flows

### Step 9: Documentation (1 hour)
- [ ] Update user documentation
- [ ] Add code comments and Doxygen
- [ ] Update ROADMAP.md to mark Phase 9 complete
- [ ] Create PHASE9_COMPLETE.md summary

**Total Estimated Time:** 21 hours (~3 days)

---

## Testing Strategy

### Unit Tests (VaultManager)

**File:** `tests/test_user_password_history.cc`

```cpp
TEST(UserPasswordHistory, AddPasswordToHistory) {
    // Test adding passwords to history
    // Verify ring buffer behavior (FIFO eviction)
    // Test with various depths (1, 5, 24)
}

TEST(UserPasswordHistory, DetectPasswordReuse) {
    // Test is_password_in_history() returns true for used passwords
    // Test returns false for new passwords
    // Test with various history depths
}

TEST(UserPasswordHistory, ClearHistory) {
    // Test clearing removes all hashes
    // Test user can reuse password after clear
}

TEST(UserPasswordHistory, PolicyEnforcement) {
    // Test admins subject to policy when enforce_for_admins=true
    // Test users subject to policy when enforce_for_users=true
    // Test bypass when enforce flags false
}

TEST(UserPasswordHistory, HistoryDepthZero) {
    // Test depth=0 disables history completely
    // Verify no passwords stored
    // Verify no reuse detection
}

TEST(UserPasswordHistory, ConstantTimeComparison) {
    // Timing analysis to verify no timing leak
    // Test with matching and non-matching passwords
}

TEST(UserPasswordHistory, Migration) {
    // Test loading vault without password_history_policy
    // Verify default policy applied
    // Test loading V2 vault without password_hashes
    // Verify empty history initialized
}
```

### Integration Tests

**File:** `tests/integration/test_password_history_flow.cc`

```cpp
TEST(PasswordHistoryFlow, ChangePasswordWithHistory) {
    // Create V2 vault with 2 users
    // Change password for user A
    // Attempt to reuse password - should fail
    // Change to different password - should succeed
    // Repeat until history full
    // Verify oldest password now allowed (rotated out)
}

TEST(PasswordHistoryFlow, AdminClearsUserHistory) {
    // User changes password 5 times
    // Admin clears user's history
    // User can now reuse any previous password
}
```

### Manual Testing Checklist

- [ ] Change password 6 times, verify 6th reuses password #1 successfully
- [ ] Attempt immediate password reuse, verify rejection with clear message
- [ ] Admin clears user history, verify user can reuse old password
- [ ] Change policy depth from 5 to 3, verify only last 3 enforced
- [ ] Disable enforcement for admins, verify admin can reuse passwords
- [ ] Open old V2 vault, verify history feature works seamlessly

---

## Security Considerations

### 1. Hash Storage
- **Threat:** Attacker with vault file access extracts password hashes
- **Mitigation:**
  - Use Argon2id with random salt per hash
  - Store alongside encrypted vault (same security boundary)
  - Hashes useless without decrypting vault first

### 2. Timing Attacks
- **Threat:** Time difference in comparison reveals password similarity
- **Mitigation:**
  - Use constant-time comparison (sodium_memcmp or similar)
  - Hash comparison, not plaintext comparison
  - Same time regardless of match position

### 3. History Depth Trade-offs
- **Too Low (1-2):** Minimal protection, easy to cycle
- **Too High (>24):** User frustration, forgotten passwords
- **Recommended:** 5-10 for balance

### 4. Admin Bypass
- **Risk:** Admins clearing history to circumvent policy
- **Mitigation:**
  - Log all history clears (future audit log)
  - Require admin authentication for clear operation
  - Clear button intentionally prominent (transparency)

### 5. Password Comparison
```cpp
// SECURE: Constant-time comparison
bool matches = (sodium_memcmp(hash1, hash2, 32) == 0);

// INSECURE: Early exit on mismatch (DO NOT USE)
bool matches = (memcmp(hash1, hash2, 32) == 0);  // Timing leak!
```

---

## Error Messages

### User-Facing Messages

**Password Reuse Detected:**
```
Title: Password Previously Used
Message: This password was recently used. Please choose a different password.

Password history prevents reusing your last 5 passwords for security.
```

**History Cleared (Admin):**
```
Title: Password History Cleared
Message: Password history cleared for user: john.doe

The user can now change their password to any value, including previously used passwords.
```

**Policy Updated:**
```
Title: Password History Policy Updated
Message: Password history policy has been changed.

New settings will apply to all future password changes.
```

---

## Migration Strategy

### Loading V2 Vaults Without History

```cpp
void VaultManager::load_vault(const std::string& path) {
    // ... existing load logic ...

    // Initialize password history for V2 vaults
    if (header_.version() == 2) {
        // Add default policy if missing
        if (!header_.has_password_history_policy()) {
            auto* policy = header_.mutable_password_history_policy();
            policy->set_history_depth(5);
            policy->set_enforce_for_admins(true);
            policy->set_enforce_for_users(true);
            needs_save_ = true;
        }

        // Initialize empty password_hashes for all users
        for (auto& slot : *header_.mutable_key_slots()) {
            if (slot.password_hashes_size() == 0) {
                // Empty history - nothing to do, field exists
            }
        }
    }
}
```

---

## Performance Considerations

### Hash Computation
- **Operation:** Argon2id hash per password check
- **Cost:** ~100ms on modern hardware (acceptable for password change)
- **Frequency:** Only during password changes (rare operation)

### Storage Overhead
- **Per Password:** 48 bytes (16 byte salt + 32 byte hash)
- **Per User (depth=5):** 240 bytes
- **For 10 Users:** 2.4 KB (negligible)

### Comparison Performance
- **Operation:** Constant-time memcmp
- **Cost:** < 1µs per comparison
- **Frequency:** Up to N comparisons per password change (N = history depth)

**Conclusion:** Performance impact is negligible.

---

## Future Enhancements (Post-Phase 9)

1. **Audit Logging:** Log all password changes and history clears
2. **Password Age Tracking:** Track when each password was used
3. **Similarity Checks:** Detect passwords that are too similar (Levenshtein distance)
4. **Expiration Policy:** Force password change after N days
5. **Breach Detection:** Check passwords against HaveIBeenPwned database
6. **Multi-Factor History:** Track MFA method changes

---

## Definition of Done

- [ ] All protobuf schema changes implemented
- [ ] All VaultManager methods implemented with full documentation
- [ ] All unit tests passing (>80% coverage for new code)
- [ ] Integration tests passing
- [ ] UI components implemented and functional
- [ ] Manual testing checklist completed
- [ ] Security review completed
- [ ] Documentation updated
- [ ] Code review approved
- [ ] Phase 9 marked complete in ROADMAP.md
- [ ] PHASE9_COMPLETE.md created

---

## Dependencies

**Required Before Starting:**
- ✅ Phase 1-8 complete (all prerequisite infrastructure in place)
- ✅ V2 vault format with protobuf
- ✅ User authentication and password change flows
- ✅ Admin permission checks

**No Blockers:** Ready to implement.

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Timing attack in comparison | Medium | High | Use constant-time comparison |
| Migration breaks old vaults | Low | High | Thorough testing, graceful defaults |
| User frustration with policy | Medium | Medium | Configurable depth, clear messaging |
| Performance impact | Low | Low | Hash computation only on password change |
| Policy bypass by admins | Low | Medium | Audit logging (future), transparency |

**Overall Risk:** Low - Well-understood problem domain with established solutions.

---

## References

- NIST SP 800-63B: Password Guidelines (history recommendation)
- OWASP Password Storage Cheat Sheet
- Argon2 RFC 9106
- Existing Phase 1-8 implementation patterns
- Account password history implementation (for reference)

---

**Document Version:** 1.0
**Last Updated:** December 25, 2025
**Next Review:** Upon Phase 9 completion
