# YubiKey Unenrollment Async Migration Plan

**Date:** 11 January 2026
**Status:** Planning - Implementation Tomorrow
**Related Commits:** ad11dc1 (voluntary password change async fix)

## Overview

Complete the YubiKey async refactoring by adding async wrapper with progress callbacks for `unenroll_yubikey_for_user()`. This operation currently blocks the UI during YubiKey hardware verification, inconsistent with the async patterns established for enrollment and password change.

## Motivation

**Current State:**
- ✅ `enroll_yubikey_for_user_async()` - Shows dual prompts (credential creation + challenge-response)
- ✅ `change_user_password_async()` - Shows dual prompts (verify old + combine new)
- ❌ `unenroll_yubikey_for_user()` - **Sync only** - Blocks UI during verification touch

**Problem:**
- Single YubiKey touch operation (line ~1709 in VaultManagerV2.cc) blocks GTK main thread
- No progress reporting during hardware interaction
- Inconsistent UX compared to other YubiKey operations
- Violates async architecture pattern established in codebase

**User Experience Impact:**
- UI freezes during YubiKey verification (~2-3 seconds)
- No visual feedback that YubiKey touch is required
- "Application Not Responding" errors on slower systems
- User may not realize they need to touch their YubiKey

## Architecture Principles (from CONTRIBUTING.md)

### Single Responsibility Principle (SRP)
- **Sync Method**: Core business logic, optional progress callbacks for internal use
- **Async Wrapper**: Thread management, GTK callback marshalling, no business logic duplication
- **UI Layer**: Dialog management, user interaction, calls async wrapper

### Composition Over Inheritance
- Async wrapper **composes** sync method via function call
- No code duplication - async delegates to sync implementation
- Progress callbacks **composed** through wrapper layer

### Separation of Concerns
```
┌─────────────────────────────────────────────────────────────┐
│ UI Layer (UserAccountHandler, Settings, etc.)              │
│ - Dialog lifecycle management                              │
│ - User interaction                                         │
│ - Calls async wrapper                                      │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│ Async Wrapper (unenroll_yubikey_for_user_async)           │
│ - std::thread for background execution                     │
│ - Glib::signal_idle() for GTK thread safety               │
│ - Marshals callbacks between threads                       │
│ - NO business logic                                        │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│ Sync Method (unenroll_yubikey_for_user)                   │
│ - Core business logic                                      │
│ - YubiKey verification                                     │
│ - DEK re-wrapping                                          │
│ - Optional progress_callback parameter                     │
└─────────────────────────────────────────────────────────────┘
```

## Established Async Pattern (Reference Implementation)

Based on recent refactors (enroll_yubikey, change_password):

### Pattern Components:

1. **Sync Method Enhancement** (backward compatible):
   ```cpp
   VaultResult<> sync_operation(
       params...,
       std::function<void(const std::string&)> progress_callback = nullptr);
   ```
   - Add optional progress_callback parameter (default nullptr)
   - Call progress_callback before hardware operations
   - No other changes to business logic

2. **Async Wrapper** (new method):
   ```cpp
   void sync_operation_async(
       params...,
       std::function<void(const std::string&)> progress_callback,
       std::function<void(const VaultResult<>&)> completion_callback);
   ```
   - Wrap progress_callback with Glib::signal_idle() for GTK thread safety
   - Wrap completion_callback with Glib::signal_idle()
   - Launch std::thread, call sync method with wrapped callbacks
   - Thread detaches after completion

3. **Progress Messages**:
   - Clear, specific descriptions of each hardware operation
   - Format: "Touch X of Y: [Specific operation description]"
   - Helps user understand why touch is needed

## Implementation Plan

### 1. Core Layer Changes

#### File: `src/core/VaultManager.h`

**Location 1: Sync method signature** (~line 748)

**BEFORE:**
```cpp
[[nodiscard]] KeepTower::VaultResult<> unenroll_yubikey_for_user(
    const Glib::ustring& username,
    const Glib::ustring& password);
```

**AFTER:**
```cpp
[[nodiscard]] KeepTower::VaultResult<> unenroll_yubikey_for_user(
    const Glib::ustring& username,
    const Glib::ustring& password,
    std::function<void(const std::string&)> progress_callback = nullptr);
```

**Location 2: Add async wrapper declaration** (after line 750)

**INSERT:**
```cpp
/**
 * @brief Async version of unenroll_yubikey_for_user with progress reporting
 * @param username Username to unenroll YubiKey from
 * @param password User's current password (for verification)
 * @param progress_callback Callback for YubiKey touch progress message
 * @param completion_callback Called with result when unenrollment completes
 *
 * Runs unenrollment in background thread, reports progress when YubiKey
 * verification touch is required. Callbacks are invoked on GTK main thread
 * via Glib::signal_idle().
 *
 * Progress message:
 * - "Verifying current password with YubiKey (touch required)..."
 *
 * @code
 * // Async unenrollment with progress dialog
 * vault_manager->unenroll_yubikey_for_user_async(
 *     "alice", "password123",
 *     [dialog](const std::string& msg) {
 *         dialog->update_message(msg);
 *         dialog->present();
 *     },
 *     [this, dialog](const auto& result) {
 *         dialog->hide();
 *         if (!result) {
 *             show_error("Unenrollment failed");
 *             return;
 *         }
 *         vault_manager->save_vault();
 *         show_success("YubiKey removed successfully");
 *     });
 * @endcode
 */
void unenroll_yubikey_for_user_async(
    const Glib::ustring& username,
    const Glib::ustring& password,
    std::function<void(const std::string&)> progress_callback,
    std::function<void(const KeepTower::VaultResult<>&)> completion_callback);
```

#### File: `src/core/VaultManagerV2.cc`

**Location 1: Update sync method signature** (~line 1639)

**BEFORE:**
```cpp
KeepTower::VaultResult<> VaultManager::unenroll_yubikey_for_user(
    const Glib::ustring& username,
    const Glib::ustring& password) {
```

**AFTER:**
```cpp
KeepTower::VaultResult<> VaultManager::unenroll_yubikey_for_user(
    const Glib::ustring& username,
    const Glib::ustring& password,
    std::function<void(const std::string&)> progress_callback) {
```

**Location 2: Add progress callback before YubiKey operation** (~line 1705)

**FIND:** (around line 1705 after YubiKey presence check)
```cpp
    // Verify password+YubiKey by unwrapping DEK
    auto kek_result = KeyWrapping::derive_kek_from_password(
        password,
        user_slot->salt,
        m_v2_header->security_policy.pbkdf2_iterations);
```

**INSERT BEFORE:**
```cpp
    // Report progress before YubiKey verification touch
    if (progress_callback) {
        progress_callback("Verifying current password with YubiKey (touch required)...");
    }

```

**Location 3: Add async wrapper implementation** (after sync method, before Session section ~line 1780)

**INSERT:**
```cpp

// ============================================================================
// YubiKey Unenrollment - Async Wrapper
// ============================================================================

void VaultManager::unenroll_yubikey_for_user_async(
    const Glib::ustring& username,
    const Glib::ustring& password,
    std::function<void(const std::string&)> progress_callback,
    std::function<void(const KeepTower::VaultResult<>&)> completion_callback) {

    Log::info("VaultManager: Starting async YubiKey unenrollment for user: {}", username.raw());

    // Wrap progress callback for GTK thread safety
    auto wrapped_progress = [progress_callback](const std::string& message) {
        if (progress_callback) {
            Glib::signal_idle().connect_once([progress_callback, message]() {
                progress_callback(message);
            });
        }
    };

    // Wrap completion callback for GTK thread safety
    auto wrapped_completion = [completion_callback](KeepTower::VaultResult<> result) {
        if (completion_callback) {
            Glib::signal_idle().connect_once([completion_callback, result]() {
                completion_callback(result);
            });
        }
    };

    // Launch background thread for YubiKey unenrollment
    std::thread([this, username, password, wrapped_progress, wrapped_completion]() {
        // Execute synchronous unenrollment on background thread
        // Progress callback will report before YubiKey verification touch
        auto result = unenroll_yubikey_for_user(username, password, wrapped_progress);

        // Report completion on GTK thread
        wrapped_completion(result);
    }).detach();

    Log::debug("VaultManager: Async YubiKey unenrollment thread launched");
}
```

### 2. UI Layer Changes (Future - No Changes Required Immediately)

**Note:** Current codebase analysis shows **no existing UI code** calls `unenroll_yubikey_for_user()`. This is likely a not-yet-implemented feature in the UI.

**When UI is implemented**, follow this pattern:

#### Example: Hypothetical UserAccountHandler or Settings Dialog

```cpp
void UserAccountHandler::handle_unenroll_yubikey_request(
    const std::string& username,
    const Glib::ustring& password) {

    // Create YubiKey prompt dialog (reusable pattern)
    auto touch_dialog = std::make_shared<YubiKeyPromptDialog*>(
        new YubiKeyPromptDialog(*m_parent_window));

    // Progress callback - update dialog with specific message
    auto progress_callback = [touch_dialog](const std::string& message) {
        if (*touch_dialog) {
            (*touch_dialog)->update_message(message);
            (*touch_dialog)->present();
        }
    };

    // Completion callback - handle result
    auto completion_callback = [this, touch_dialog, username](const auto& result) {
        // Hide dialog
        if (*touch_dialog) {
            (*touch_dialog)->hide();
            delete *touch_dialog;
            *touch_dialog = nullptr;
        }

        if (!result) {
            std::string error_msg = "Failed to remove YubiKey enrollment";
            if (result.error() == KeepTower::VaultError::YubiKeyNotPresent) {
                error_msg = "YubiKey not detected. Please connect your YubiKey.";
            } else if (result.error() == KeepTower::VaultError::AuthenticationFailed) {
                error_msg = "Password verification failed. Please try again.";
            }

            show_error_dialog(error_msg);
            return;
        }

        // Save vault to persist changes
        auto save_result = m_vault_manager->save_vault();
        if (!save_result) {
            show_error_dialog("YubiKey removed but failed to save vault");
            return;
        }

        show_success_dialog("YubiKey enrollment removed successfully.\n"
                          "You can now log in with password only.");

        // Refresh user settings display
        refresh_user_security_settings();
    };

    // Call async method - non-blocking
    m_vault_manager->unenroll_yubikey_for_user_async(
        username, password, progress_callback, completion_callback);
}
```

### 3. Testing Requirements

#### Unit Tests

**File:** `tests/test_vault_manager.cc` (or new test file)

Add test case:
```cpp
TEST(VaultManagerV2, UnenrollYubiKeyProgressCallback) {
    // Test that progress callback is invoked before YubiKey operation
    VaultManager vm;
    // ... setup vault with enrolled YubiKey user ...

    bool progress_called = false;
    std::string progress_message;

    auto result = vm.unenroll_yubikey_for_user(
        "testuser", "password",
        [&](const std::string& msg) {
            progress_called = true;
            progress_message = msg;
        });

    ASSERT_TRUE(progress_called);
    EXPECT_THAT(progress_message, testing::HasSubstr("Verifying"));
    EXPECT_THAT(progress_message, testing::HasSubstr("YubiKey"));
    EXPECT_THAT(progress_message, testing::HasSubstr("touch"));
}
```

#### Manual Testing Checklist

1. **Happy Path:**
   - [ ] User with enrolled YubiKey requests unenrollment
   - [ ] Dialog appears with "Verifying current password with YubiKey (touch required)..."
   - [ ] Touch YubiKey → Dialog updates/closes
   - [ ] Success message shown
   - [ ] User can log in with password only (no YubiKey required)

2. **Error Cases:**
   - [ ] YubiKey not present → Clear error message
   - [ ] Wrong password → Authentication failed error
   - [ ] YubiKey not enrolled → Appropriate error
   - [ ] YubiKey removed during operation → Connection error

3. **UI Responsiveness:**
   - [ ] Application remains responsive during operation
   - [ ] Can move window, click other UI elements
   - [ ] No "Not Responding" freezes
   - [ ] Progress dialog updates in real-time

4. **Thread Safety:**
   - [ ] No GTK warnings in console
   - [ ] No crashes during concurrent operations
   - [ ] Proper cleanup on dialog close mid-operation

### 4. Code Review Checklist

**SRP Compliance:**
- [ ] Sync method contains ONLY business logic (no threading)
- [ ] Async wrapper contains ONLY threading/marshalling (no business logic)
- [ ] UI layer contains ONLY dialog management (no crypto operations)
- [ ] No code duplication between sync and async implementations

**Async Pattern Consistency:**
- [ ] Matches enroll_yubikey_for_user_async() pattern
- [ ] Matches change_user_password_async() pattern
- [ ] Progress callback signature: `std::function<void(const std::string&)>`
- [ ] Completion callback signature: `std::function<void(const VaultResult<>&)>`
- [ ] Both callbacks wrapped with Glib::signal_idle()

**Progress Messages:**
- [ ] Clear, user-friendly language
- [ ] Explicitly mentions "touch required"
- [ ] Consistent terminology with other YubiKey operations
- [ ] No technical jargon

**Backward Compatibility:**
- [ ] progress_callback parameter is optional (default nullptr)
- [ ] Existing sync callers continue to work
- [ ] No breaking changes to public API

**Thread Safety:**
- [ ] All GTK calls wrapped with Glib::signal_idle()
- [ ] Progress callback invoked on GTK thread
- [ ] Completion callback invoked on GTK thread
- [ ] No data races on shared state

**Error Handling:**
- [ ] All error paths preserved from sync implementation
- [ ] Completion callback receives errors properly
- [ ] No exceptions thrown across thread boundaries

**Documentation:**
- [ ] Doxygen comments for async wrapper
- [ ] Code examples in header comments
- [ ] Progress message format documented
- [ ] Thread safety guarantees documented

### 5. Build and Test Commands

```bash
# 1. Build with changes
cd /home/tjdev/Projects/KeepTower
meson compile -C build

# 2. Run full test suite
meson test -C build

# 3. Run specific YubiKey tests (if new tests added)
meson test -C build test_vault_manager

# 4. Check for GTK threading warnings
./build/src/keeptower 2>&1 | grep -i "gtk\|thread\|critical"
```

### 6. Commit Strategy

Follow established pattern from recent commits:

**Commit Message:**
```
Add async wrapper for YubiKey unenrollment with progress reporting

- Add progress_callback parameter to sync unenroll_yubikey_for_user()
- Report progress before YubiKey verification touch
- Implement unenroll_yubikey_for_user_async() wrapper
- Follow established async pattern (enrollment, password change)

Now YubiKey unenrollment shows specific touch prompt matching the
behavior of enrollment and password change operations.

Completes async refactoring for all YubiKey operations. UI no longer
blocks during hardware verification, providing better responsiveness.

Related: ad11dc1 (voluntary password change async fix)
```

**Files to Stage:**
```bash
git add src/core/VaultManager.h
git add src/core/VaultManagerV2.cc
git add tests/test_vault_manager.cc  # if tests added
```

## Benefits

### User Experience
- ✅ No UI freezing during YubiKey verification
- ✅ Clear progress message: "Verifying current password with YubiKey (touch required)..."
- ✅ Consistent UX across all YubiKey operations
- ✅ Better user understanding of when/why touch is needed

### Code Quality
- ✅ Follows SRP: sync (business logic) vs async (threading)
- ✅ No code duplication: async wraps sync
- ✅ Consistent pattern across all YubiKey operations
- ✅ Backward compatible: optional progress_callback
- ✅ Thread-safe: proper GTK callback marshalling

### Maintainability
- ✅ Easy to test: sync method testable independently
- ✅ Clear separation of concerns
- ✅ Follows established codebase patterns
- ✅ Well-documented with examples

## Risks and Mitigations

### Risk 1: Thread Safety Issues
**Mitigation:** Use exact same pattern as enrollment/password change (proven working)

### Risk 2: UI Not Yet Implemented
**Mitigation:** No immediate impact; async wrapper ready when UI is added

### Risk 3: Test Coverage Gaps
**Mitigation:** Add unit tests for progress callback invocation

### Risk 4: Breaking Changes
**Mitigation:** Optional progress_callback parameter maintains backward compatibility

## Success Criteria

- [ ] Sync method accepts optional progress_callback parameter
- [ ] Progress callback invoked before YubiKey verification
- [ ] Async wrapper implemented following established pattern
- [ ] All 42 tests pass (or more if new tests added)
- [ ] Build succeeds with no warnings (deprecation warnings OK)
- [ ] Code review checklist items all pass
- [ ] Consistent with enroll/password change async patterns
- [ ] Thread-safe GTK callback marshalling verified

## Timeline

**Tomorrow (12 January 2026):**
1. Morning: Implement core layer changes (VaultManager.h, VaultManagerV2.cc)
2. Afternoon: Add tests, build, verify all tests pass
3. Evening: Code review against checklist, commit changes

**Estimated Time:** 2-3 hours
- Core implementation: 1 hour
- Testing and verification: 1 hour
- Code review and commit: 30 minutes

## References

- **Recent async refactor commits:**
  - ad11dc1: Voluntary password change async fix
  - Previous: Forced password change async fix

- **Related files:**
  - src/core/VaultManager.h: API declarations
  - src/core/VaultManagerV2.cc: Implementation
  - src/ui/managers/UserAccountHandler.cc: Example UI pattern
  - src/ui/managers/V2AuthenticationHandler.cc: Example async usage

- **CONTRIBUTING.md:** Lines 124-170 (SRP, async patterns)
- **Test examples:** tests/test_vault_manager.cc (existing YubiKey tests)

## Notes

- No UI code currently calls unenrollment - likely future feature
- Async wrapper will be ready when UI is implemented
- Follows exact same pattern as successful enrollment/password change refactors
- Single YubiKey touch (verification only), simpler than enrollment (2 touches)
