# Debug Session - Clipboard Preservation Regression
**Date**: 2026-01-17
**Status**: ⚠️ BROKEN - Segfault in ChangePasswordDialog
**Decision**: DO NOT COMMIT - May need to rollback clipboard preservation entirely

---

## What Was Implemented (Working)
**Feature**: Smart clipboard preservation for admin→standard user workflow

**Files Modified** (7 total):
1. `src/ui/controllers/ClipboardManager.h` - Added `preserve_on_vault_close()`, `cancel_preservation()`
2. `src/ui/controllers/ClipboardManager.cc` - Implemented 60s preservation window
3. `src/ui/dialogs/UserManagementDialog.h` - Added clipboard_manager pointer
4. `src/ui/dialogs/UserManagementDialog.cc` - Call preserve on temp password copy
5. `src/ui/managers/UserAccountHandler.h` - Thread clipboard_manager through
6. `src/ui/managers/UserAccountHandler.cc` - Pass clipboard_manager to dialog
7. `src/ui/windows/MainWindow.cc` - Pass clipboard_manager, call cancel_preservation()

**Testing Before Regression**: All 42 tests passed, clipboard preservation working correctly

---

## Regression Discovered
**Symptom**: Segmentation fault when ChangePasswordDialog is shown during forced password change

**Workflow Triggering Crash**:
1. Admin creates vault as user "dev"
2. Admin adds standard user "dev1" with temp password
3. Admin copies temp password (preservation enabled ✓)
4. Admin closes vault (clipboard NOT cleared ✓)
5. User logs in as "dev1" with pasted temp password ✓
6. **CRASH**: ChangePasswordDialog segfaults during rendering

**Log Evidence**:
```
[2026-01-17 11:51:11.628] INFO : V2AuthenticationHandler: Password change required
(keeptower:12127): Gtk-DEBUG: 11:51:11.680: snapshot symbolic icon using mask
(keeptower:12127): Gtk-DEBUG: 11:51:11.680: snapshot symbolic icon using mask
(keeptower:12127): Gtk-DEBUG: 11:51:11.680: snapshot symbolic icon using mask
./run_debug.sh: line 6: 12127 Segmentation fault
```

**User Observation**: "YubiKey open vault (single touch) prompt is appearing before the change password dialog appears"

---

## Root Cause Analysis

### Initial Hypothesis (WRONG)
Thought it was Unicode emoji `👁` (\U0001F441) used in ToggleButton constructors causing rendering crash.

**What We Tried**:
- Removed emoji from `m_current_password_show_button` and `m_yubikey_pin_show_button`
- Added `set_icon_name("view-reveal-symbolic")` calls in constructor
- Files modified: `ChangePasswordDialog.h`, `ChangePasswordDialog.cc`
- **Result**: Still crashes - hypothesis was wrong

### Current Hypothesis (LIKELY)
Timing issue with `cancel_preservation()` being called during GTK dialog lifecycle.

**Evidence**:
- Crash occurs during "snapshot symbolic icon" (GTK rendering phase)
- In `MainWindow::complete_vault_opening()`, we call `cancel_preservation()`
- This happens DURING authentication flow while dialogs are being constructed
- GTK event loop may be disrupted by clipboard operations during widget creation

**Code Flow**:
```
handle_v2_vault_open()
  → V2AuthenticationHandler::handle_vault_open()
    → Success callback: complete_vault_opening()  ← CALLS cancel_preservation()
      → Meanwhile: ChangePasswordDialog constructor + show()  ← CRASHES HERE
```

**Latest Change** (NOT TESTED):
Removed `cancel_preservation()` from `complete_vault_opening()` to avoid interfering with dialog lifecycle.

---

## Files Currently Modified (Uncommitted)

### Functional Changes (Clipboard Preservation)
- ✅ ClipboardManager.h/cc - Working
- ✅ UserManagementDialog.h/cc - Working
- ✅ UserAccountHandler.h/cc - Working
- ⚠️ MainWindow.cc - Modified to remove cancel_preservation() call

### Debug/Fix Attempts (May Need Rollback)
- ⚠️ ChangePasswordDialog.h - Removed emoji, added comment "Icon set in constructor"
- ⚠️ ChangePasswordDialog.cc - Added set_icon_name() calls, added Log.h include, added defensive logging
- ⚠️ V2AuthenticationHandler.cc - Added extensive debug logging (not in git log output)

---

## Next Steps When Resuming

### Option 1: Test Current Fix
1. Run the workflow again with current build
2. Check if removing `cancel_preservation()` from `complete_vault_opening()` fixed it
3. If YES: Find proper place to cancel preservation (after all dialogs complete)
4. If NO: Proceed to Option 2

### Option 2: Rollback Clipboard Preservation
**If crash persists, revert all clipboard preservation work**:

```bash
# Revert to last known good state (before clipboard preservation)
git checkout HEAD -- src/ui/controllers/ClipboardManager.h
git checkout HEAD -- src/ui/controllers/ClipboardManager.cc
git checkout HEAD -- src/ui/dialogs/UserManagementDialog.h
git checkout HEAD -- src/ui/dialogs/UserManagementDialog.cc
git checkout HEAD -- src/ui/managers/UserAccountHandler.h
git checkout HEAD -- src/ui/managers/UserAccountHandler.cc
git checkout HEAD -- src/ui/windows/MainWindow.cc
git checkout HEAD -- src/ui/dialogs/ChangePasswordDialog.h
git checkout HEAD -- src/ui/dialogs/ChangePasswordDialog.cc
git checkout HEAD -- src/ui/managers/V2AuthenticationHandler.cc

# Rebuild and test
meson compile -C build
./run_debug.sh
```

### Option 3: Deep Investigation
If we keep clipboard preservation, investigate:
1. **GTK Event Loop**: Is cancel_preservation() causing event loop issues?
2. **Widget Lifecycle**: Are we accessing clipboard during unsafe GTK phases?
3. **Async Operations**: Is YubiKey async code racing with dialog creation?
4. **Architecture**: Did we violate SRP by making MainWindow manage clipboard during auth?

**Potential Proper Fix**:
- Move clipboard management into V2AuthenticationHandler (SRP principle)
- Cancel preservation AFTER password change + YubiKey enrollment complete
- Use Glib::signal_idle() to defer clipboard operations outside rendering phase

---

## Architecture Concerns (User's Valid Points)

**User Quote**: "Have we made these changes to the mainwindow in the correct place, we previously refactored the MainWindow code which had become a god object, are we recreating a god object with this code change?"

**Answer**: YES, we likely violated SRP:
- MainWindow should NOT manage clipboard state during authentication flows
- This is V2AuthenticationHandler's responsibility
- We added clipboard coupling back into MainWindow (god object anti-pattern)

**Proper Architecture**:
```
ClipboardManager (owns clipboard state)
     ↑
     |
UserManagementDialog (enables preservation on copy)
     ↑
     |
V2AuthenticationHandler (cancels preservation after auth completes)
```

NOT:
```
MainWindow (god object - manages everything)
  → ClipboardManager
  → V2AuthenticationHandler
  → UserManagementDialog
```

---

## Test Status
- Unit Tests: 42/42 passing (before crash testing)
- Integration Test: ❌ FAILED - Segfault during user login workflow
- Clipboard Preservation: ✅ WORKING (when it doesn't crash)

---

## Recommendation
1. **Test current build** first (cancel_preservation removed from complete_vault_opening)
2. **If still broken**: Revert entire clipboard preservation feature (Option 2)
3. **If fixed**: Refactor to move clipboard management into V2AuthenticationHandler (proper SRP)
4. **Alternative**: Implement clipboard preservation differently - use signal/slot pattern to decouple

---

## Files to Check for Proper Revert
```bash
git diff src/ui/controllers/ClipboardManager.h
git diff src/ui/controllers/ClipboardManager.cc
git diff src/ui/dialogs/UserManagementDialog.h
git diff src/ui/dialogs/UserManagementDialog.cc
git diff src/ui/managers/UserAccountHandler.h
git diff src/ui/managers/UserAccountHandler.cc
git diff src/ui/windows/MainWindow.cc
git diff src/ui/dialogs/ChangePasswordDialog.h
git diff src/ui/dialogs/ChangePasswordDialog.cc
git diff src/ui/managers/V2AuthenticationHandler.cc
```

Save clean patches before reverting:
```bash
git diff > /tmp/clipboard_preservation_attempt.patch
```
