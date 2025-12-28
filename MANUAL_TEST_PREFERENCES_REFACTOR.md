# Manual Test Plan: Preferences Dialog Refactoring

**Test Date**: 26 December 2025
**Tester**: ____________
**Build Version**: 0.2.9-beta
**Feature**: Split Security tab into Account Security and Vault Security

## Overview

This document provides manual test procedures to verify the preferences dialog refactoring is working correctly. The main changes are:

1. **Security tab split** into two separate tabs:
   - **Account Security**: User behavior settings (clipboard, undo/redo)
   - **Vault Security**: Vault policy settings (auto-lock, FIPS, password history)

2. **Password history UI changes**:
   - Removed misleading "default settings" controls
   - Added read-only display of current vault's policy
   - Added "Clear My Password History" button

3. **Settings cleanup**:
   - Removed password-history-enabled and password-history-limit from PreferencesDialog
   - These settings only affect new vault creation (still in MainWindow)

## Prerequisites

- ✅ Application built successfully
- ✅ Test vault file available (or create new one)
- ✅ At least one user account in test vault

## Test Cases

### Test 1: Verify Tab Structure

**Objective**: Confirm all four tabs display correctly and in proper order.

**Steps**:
1. Launch application: `./build/src/keeptower`
2. Open preferences: Menu → Preferences (or Ctrl+,)
3. Verify left sidebar shows exactly 4 tabs:
   - [x] Appearance
   - [x] Account Security
   - [x] Vault Security
   - [x] Storage

**Expected Result**: ✅ All four tabs visible with clear labels
**Actual Result**: ___________
**Status**: [x] Pass [ ] Fail

---

### Test 2: Appearance Tab (Unchanged)

**Objective**: Verify Appearance tab still works correctly.

**Steps**:
1. Click "Appearance" tab
2. Verify color scheme dropdown is present
3. Test changing color scheme:
   - [x] Select "Light" → Apply → UI changes to light theme
   - [x] Select "Dark" → Apply → UI changes to dark theme
   - [x] Select "Default" → Apply → UI follows system preference

**Expected Result**: ✅ Color scheme changes apply immediately
**Actual Result**: ___________
**Status**: [x] Pass [ ] Fail

---

### Test 3: Account Security Tab (New)

**Objective**: Verify Account Security tab displays user behavior settings correctly.

**Steps**:
1. Click "Account Security" tab
2. Verify page contains **three sections**:

   **Section 1: Clipboard Protection**
   - [x] Title: "Clipboard Protection"
   - [x] Description about auto-clearing
   - [x] Label: "Clear clipboard after:"
   - [x] Spin button (range: 5-300 seconds)
   - [x] Suffix: " seconds"

   **Section 2: Account Password History**
   - [x] Title: "Account Password History"
   - [x] Description: "Prevent reusing passwords when updating account entries (Gmail, GitHub, etc.)"
   - [x] Checkbox: "Prevent account password reuse"
   - [x] Label: "Remember up to"
   - [x] Spin button (range: 0-24)
   - [x] Suffix: " previous passwords per account"

   **Section 3: Undo/Redo**
   - [x] Title: "Undo/Redo"
   - [x] Description about undo functionality
   - [x] Checkbox: "Enable undo/redo (Ctrl+Z/Ctrl+Shift+Z)"
   - [x] Label: "Keep up to"
   - [x] Spin button (range: 1-100)
   - [x] Suffix: " operations"
   - [x] Warning label about memory security

3. Test checkbox behavior:
   - [x] Uncheck "Prevent account password reuse" → spin button becomes insensitive (grayed)
   - [x] Check "Prevent account password reuse" → spin button becomes active
   - [x] Uncheck "Enable undo/redo" → spin button becomes insensitive (grayed)
   - [x] Check "Enable undo/redo" → spin button becomes active

4. Test settings persistence:
   - [x] Change clipboard timeout to 60 seconds
   - [x] Enable account password history, set limit to 10
   - [x] Set undo history to 25 operations
   - [x] Click Apply → Close dialog
   - [x] Reopen preferences → Values retained

**Expected Result**: ✅ Account Security tab shows clipboard and undo/redo settings
**Actual Result**: ___________
**Status**: [x] Pass [ ] Fail

---

### Test 4: Vault Security Tab - Without Vault Open

**Objective**: Verify Vault Security tab when no vault is open.

**Steps**:
1. Ensure NO vault is currently open
2. Open preferences
3. Click "Vault Security" tab
4. Verify page contains **three visible sections**:

   **Section 1: Auto-Lock**
   - [x] Title: "Auto-Lock"
   - [x] Checkbox: "Enable auto-lock after inactivity"
   - [x] Label: "Lock timeout:"
   - [x] Spin button (range: 60-3600 seconds)
   - [x] Suffix: " seconds"

   **Section 2: FIPS-140-3 Compliance**
   - [x] Title: "FIPS-140-3 Compliance"
   - [x] Description about FIPS mode
   - [x] Checkbox: "Enable FIPS-140-3 mode (requires restart)"
   - [x] Status label (✓ available or ⚠️ not available)
   - [x] Warning about restart requirement

5. Verify **Section 3: User Password History (Default for New Vaults)**:
   - [x] Title: "User Password History (Default for New Vaults)"
   - [x] Description: "Set default policy for preventing vault user authentication password reuse"
   - [x] Label present (for spin button)
   - [x] Spin button (range: 0-24, default: 5)
   - [x] Suffix present
   - [x] Help text: "0 = disabled (password reuse allowed)"
   - [x] No current vault info shown (no vault open)
   - [x] No username or history count shown
   - [x] No clear button visible

**Expected Result**: ✅ Auto-lock, FIPS, and password history sections all visible; history section shows default settings for new vault creation
**Actual Result**: ___________
**Status**: [x] Pass [ ] Fail

---

### Test 5: Vault Security Tab - With Vault Open

**Objective**: Verify password history section appears when vault is open.

**Steps**:
1. Open a test vault with known password
2. Open preferences
3. Click "Vault Security" tab
4. Verify page contains **three sections**:

   **Section 1: Auto-Lock** (same as Test 4)

   **Section 2: FIPS-140-3 Compliance** (same as Test 4)

   **Section 3: User Password History** (NEW!)
   - [x] Title: "User Password History"
   - [x] Description: "Manage your password history for this vault"
   - [x] Label: "Current vault policy: X passwords" (where X = vault's depth)
   - [x] Label: "Logged in as: USERNAME" (shows your username)
   - [x] Label: "Password history: Y entries" (where Y = your history count)
   - [x] Button: "Clear My Password History"
   - [x] Warning: "⚠ This will permanently delete all saved password history for your account"

5. Verify information is accurate:
   - [x] Vault policy matches actual vault setting
   - [x] Username matches logged-in user
   - [x] History count is reasonable (0-24)

6. Test clear button state:
   - If history count = 0:
     - [x] Clear button is disabled (grayed out)
   - If history count > 0:
     - [x] Clear button is enabled (clickable)

**Expected Result**: ✅ Password history section visible with accurate info
**Actual Result**: ___________
**Status**: [x] Pass [ ] Fail

---

### Test 6: Clear Password History - Confirmation Dialog

**Objective**: Verify clear button shows confirmation before deleting.

**Prerequisites**: Vault open with at least 1 password history entry

**Steps**:
1. Open preferences → Vault Security tab
2. Verify "Password history: 1+ entries" is shown
3. Click "Clear My Password History" button
4. Verify confirmation dialog appears:
   - [x] Title: "Clear Password History?"
   - [x] Icon: Warning icon
   - [x] Message: "This will permanently delete all saved password history for user 'USERNAME'"
   - [x] Secondary text: "This action cannot be undone."
   - [x] Buttons: "Cancel" and "OK"

5. Click "Cancel":
   - [x] Dialog closes
   - [x] History count unchanged
   - [x] No confirmation message

**Expected Result**: ✅ Confirmation dialog appears, cancel works
**Actual Result**: ___________
**Status**: [x] Pass [ ] Fail

---

### Test 7: Clear Password History - Successful Clear

**Objective**: Verify clear operation deletes history and updates UI.

**Prerequisites**: Vault open with at least 1 password history entry

**Steps**:
1. Note current history count: 2______
2. Open preferences → Vault Security tab
3. Click "Clear My Password History" button
4. Click "OK" in confirmation dialog
5. Verify success message appears:
   - [x] Title: "Password history cleared"
   - [x] Message: "Password history for 'USERNAME' has been cleared."
   - [x] Button: "OK"
6. Click "OK" on success message
7. Verify UI updates:
   - [x] History count now shows "0 entries"
   - [x] Clear button becomes disabled (grayed)

8. Close preferences and reopen:
   - [x] History count still shows "0 entries"
   - [x] Clear button still disabled

9. Close and reopen vault:
   - [x] History count remains "0 entries"
   - [x] Change is persistent

**Expected Result**: ✅ History cleared, UI updates, change persists
**Actual Result**: ___________
**Status**: [x] Pass [ ] Fail

---

### Test 8: Clear Password History - Error Handling

**Objective**: Verify error dialog if clear operation fails.

**Steps** (requires forcing an error - may skip if difficult):
1. Attempt to trigger error condition (e.g., file permissions)
2. Click "Clear My Password History" → OK
3. Verify error dialog appears:
   - [x] Title: "Failed to clear password history"
   - [x] Icon: Error icon
   - [x] Message: Error description
   - [x] Button: "OK"
4. Click "OK"
5. Verify:
   - [x] History count unchanged
   - [x] No corruption or data loss

**Expected Result**: ✅ Error handled gracefully with clear message
**Actual Result**: ___________ (or SKIPPED if cannot trigger error)
**Status**: [x] Pass [ ] Fail [ ] Skip

---

### Test 9: Storage Tab (Unchanged)

**Objective**: Verify Storage tab still works correctly.

**Steps**:
1. Click "Storage" tab
2. Verify page contains **two sections**:

   **Section 1: Error Correction**
   - [x] Title: "Error Correction"
   - [x] Reed-Solomon checkbox
   - [x] Redundancy spin button
   - [x] "Apply to current vault" checkbox (if vault open)

   **Section 2: Automatic Backups**
   - [x] Title: "Automatic Backups"
   - [x] Enable backups checkbox
   - [x] Backup count spin button

3. Test settings work:
   - [x] Change backup count → Apply → Setting saves
   - [x] Reopen preferences → Setting retained

**Expected Result**: ✅ Storage tab unchanged and functional
**Actual Result**: ___________
**Status**: [x] Pass [ ] Fail

---

### Test 10: Account Password Reuse Detection

**Objective**: Verify account password history prevents password reuse for account entries.

**Prerequisites**: Vault open with at least one account

**Steps**:
1. Open preferences → Account Security tab
2. Enable "Prevent account password reuse"
3. Set limit to 3 previous passwords
4. Click Apply
5. Close preferences
6. Edit an existing account (e.g., Gmail account):
   - Note current password: __________
   - Change password to "NewPass123!"
   - Save account
7. Edit the same account again:
   - Try to change password back to the original password
   - [x] Error dialog appears: "Password reuse detected!"
   - [x] Message explains password was used previously
   - [x] Account is NOT saved
8. Change to a different, unused password:
   - [x] Password change succeeds
   - [x] No error message
9. Continue changing password 3 more times (filling history)
10. Try to reuse one of the 3 recent passwords:
    - [x] Error dialog appears
11. Try to reuse the very first password (oldest, should be removed):
    - [x] Password change SUCCEEDS (history limit working correctly)

**Expected Result**: ✅ Password reuse prevented for last N passwords (within limit)
**Actual Result**: ___________
**Status**: [x] Pass [ ] Fail

---

### Test 11: Settings Persistence Across Tabs

**Objective**: Verify settings save correctly for all tabs.

**Steps**:
1. Make changes in each tab:
   - Appearance: Set to "Dark"
   - Account Security: Clipboard timeout = 45s, Account password history limit = 10, Undo limit = 30
   - Vault Security: Enable auto-lock, timeout = 600s
   - Storage: Backups = 7
2. Click "Apply"
3. Close preferences
4. Reopen preferences
5. Verify all changes persisted:
   - [x] Appearance: "Dark" selected
   - [x] Account Security: Clipboard = 45s, Account password history = 10, Undo = 30
   - [x] Vault Security: Auto-lock enabled, 600s
   - [x] Storage: Backups = 7

**Expected Result**: ✅ All settings persist after close/reopen
**Actual Result**: ___________
**Status**: [x] Pass [ ] Fail

---

### Test 11: Multi-User Security Restrictions (V2 Vaults Only)

**Objective**: Verify standard users cannot access security-related tabs in V2 vaults.

**Prerequisites**: V2 vault with admin and standard user accounts

**Steps**:
1. Log in as **administrator**
2. Open preferences
3. Verify all 4 tabs visible in sidebar:
   - [x] Appearance
   - [x] Account Security
   - [x] Vault Security
   - [x] Storage
4. Click each tab and verify all sections accessible

5. Log out and log in as **standard user**
6. Open preferences
7. Verify only 1 tab visible in sidebar:
   - [x] Appearance
8. Verify security tabs are HIDDEN:
   - [x] Account Security tab NOT in sidebar
   - [x] Vault Security tab NOT in sidebar
   - [x] Storage tab NOT in sidebar

**Note**: Standard users cannot modify vault-level security policies. Only administrators have access to Account Security, Vault Security, and Storage tabs.

**Expected Result**: ✅ Admins see all 4 tabs; standard users see only Appearance
**Actual Result**: ___________
**Status**: [x] Pass [ ] Fail [ ] Skip (no V2 vault)

---

### Test 12: Vault State Changes

**Objective**: Verify password history section updates when vault opens/closes.

**Steps**:
1. With NO vault open:
   - Open preferences → Vault Security
   - [x] Password history section VISIBLE
   - [x] Shows "User Password History (Default for New Vaults)"
   - [x] Shows spin button for setting default depth (0-24)
   - [x] No vault-specific info shown (no username, history count, or clear button)

2. Open a vault:
   - Preferences should still be open
   - [ ] Password history section UPDATES
   - [ ] Title changes to "User Password History"
   - [ ] Shows vault policy, username, and history count
   - [ ] Clear button appears (enabled if history > 0)

3. Close the vault (lock or close):
   - Preferences should still be open
   - [ ] Password history section UPDATES again
   - [ ] Title changes back to "User Password History (Default for New Vaults)"
   - [ ] Vault-specific info disappears (username, history count, clear button removed)
   - [ ] Spin button for defaults reappears

**Expected Result**: ✅ Section always visible but updates content based on vault state
**Actual Result**: ___________
**Status**: [ ] Pass [ ] Fail

---

### Test 13: Cancel Button Behavior

**Objective**: Verify Cancel button discards changes.

**Steps**:
1. Open preferences
2. Make changes in multiple tabs:
   - Account Security: Change clipboard timeout to 120s
   - Vault Security: Disable auto-lock
3. Click "Cancel" (don't click Apply)
4. Reopen preferences
5. Verify changes were NOT saved:
   - [ ] Clipboard timeout at original value
   - [ ] Auto-lock at original state

**Expected Result**: ✅ Cancel discards all changes
**Actual Result**: ___________
**Status**: [ ] Pass [ ] Fail

---

## Test Summary

| Test # | Test Name | Status | Notes |
|--------|-----------|--------|-------|
| 1 | Tab Structure | [ ] Pass [ ] Fail | |
| 2 | Appearance Tab | [ ] Pass [ ] Fail | |
| 3 | Account Security Tab | [ ] Pass [ ] Fail | |
| 4 | Vault Security (No Vault) | [ ] Pass [ ] Fail | |
| 5 | Vault Security (With Vault) | [ ] Pass [ ] Fail | |
| 6 | Clear Confirmation | [ ] Pass [ ] Fail | |
| 7 | Clear Success | [ ] Pass [ ] Fail | |
| 8 | Clear Error Handling | [ ] Pass [ ] Fail [ ] Skip | |
| 9 | Storage Tab | [ ] Pass [ ] Fail | |
| 10 | Account Password Reuse | [ ] Pass [ ] Fail | |
| 11 | Multi-User Security | [ ] Pass [ ] Fail [ ] Skip | |
| 12 | Vault State Changes | [ ] Pass [ ] Fail | |
| 13 | Cancel Button | [ ] Pass [ ] Fail | |

**Overall Result**: [ ] All Pass [ ] Some Failures
**Ready for Release**: [ ] Yes [ ] No (see notes)

---

## Known Issues / Notes

Record any issues discovered during testing:

1. ___________________________________________
2. ___________________________________________
3. ___________________________________________

---

## Regression Checks

Verify no regressions in other functionality:

- [ ] Main window still opens correctly
- [ ] Vault operations (create, open, save) still work
- [ ] Account add/edit/delete still works
- [ ] Search still works
- [ ] Password generation still works
- [ ] Favorites still work
- [ ] Groups still work

---

## Sign-Off

**Tester Name**: ___________________________
**Date**: ___________________________
**Signature**: ___________________________

**Approved for Release**: [ ] Yes [ ] No
**Approver**: ___________________________
**Date**: ___________________________
