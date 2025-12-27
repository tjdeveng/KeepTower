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
   - [x] Clipboard timeout spin button (5-300 seconds)

   **Section 2: Account Password History**
   - [x] "Prevent account password reuse" checkbox
   - [x] History limit spin button (0-24)

   **Section 3: Undo/Redo**
   - [x] "Enable undo/redo" checkbox
   - [x] History limit spin button (1-100)
   - [x] Memory security warning visible

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
2. Open preferences → Vault Security tab
3. Verify **three visible sections**:

   **Section 1: Auto-Lock**
   - [x] "Enable auto-lock after inactivity" checkbox
   - [x] Lock timeout spin button (60-3600 seconds)

   **Section 2: User Password History (Default for new vaults)**
   - [x] Spin button (0-24) for default vault policy
   - [x] Label: "0 = disabled (password reuse allowed)"

   **Section 3: FIPS-140-3 Compliance**
   - [x] "Enable FIPS-140-3 mode" checkbox
   - [x] Status indicator (✓ available or ⚠️ not available)
   - [x] Restart warning visible

**Expected Result**: ✅ Only auto-lock and FIPS sections visible, no password history
**Actual Result**: ___________
**Status**: [x] Pass [ ] Fail

---

### Test 5: Vault Security Tab - With Vault Open

**Objective**: Verify password history section changes when vault is open.

**Steps**:
1. Open a test vault
2. Open preferences → Vault Security tab
3. Verify **User Password History** section now shows:
   - [x] "Current vault policy: X passwords" (read-only, shows vault's setting)
   - [x] "Logged in as: USERNAME"
   - [x] "Password history: Y entries" (your history count)
   - [x] "Clear My Password History" button
   - [x] Warning about permanent deletion

4. Verify information is accurate:
   - [x] Vault policy matches actual vault setting
   - [x] Username matches logged-in user
   - [x] History count is reasonable (0-24)

5. Test clear button state:
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
   - [ ] Icon: Warning icon
   - [ ] Message: "This will permanently delete all saved password history for user 'USERNAME'"
   - [ ] Secondary text: "This action cannot be undone."
   - [ ] Buttons: "Cancel" and "OK"

5. Click "Cancel":
   - [ ] Dialog closes
   - [ ] History count unchanged
   - [ ] No confirmation message

**Expected Result**: ✅ Confirmation dialog appears, cancel works
**Actual Result**: ___________
**Status**: [ ] Pass [ ] Fail

---

### Test 7: Clear Password History - Successful Clear

**Objective**: Verify clear operation deletes history and updates UI.

**Prerequisites**: Vault open with at least 1 password history entry

**Steps**:
1. Note current history count: _______
2. Open preferences → Vault Security tab
3. Click "Clear My Password History" button
4. Click "OK" in confirmation dialog
5. Verify success message appears:
   - [ ] Title: "Password history cleared"
   - [ ] Message: "Password history for 'USERNAME' has been cleared."
   - [ ] Button: "OK"
6. Click "OK" on success message
7. Verify UI updates:
   - [ ] History count now shows "0 entries"
   - [ ] Clear button becomes disabled (grayed)

8. Close preferences and reopen:
   - [ ] History count still shows "0 entries"
   - [ ] Clear button still disabled

9. Close and reopen vault:
   - [ ] History count remains "0 entries"
   - [ ] Change is persistent

**Expected Result**: ✅ History cleared, UI updates, change persists
**Actual Result**: ___________
**Status**: [ ] Pass [ ] Fail

---

### Test 8: Clear Password History - Error Handling

**Objective**: Verify error dialog if clear operation fails.

**Steps** (requires forcing an error - may skip if difficult):
1. Attempt to trigger error condition (e.g., file permissions)
2. Click "Clear My Password History" → OK
3. Verify error dialog appears:
   - [ ] Title: "Failed to clear password history"
   - [ ] Icon: Error icon
   - [ ] Message: Error description
   - [ ] Button: "OK"
4. Click "OK"
5. Verify:
   - [ ] History count unchanged
   - [ ] No corruption or data loss

**Expected Result**: ✅ Error handled gracefully with clear message
**Actual Result**: ___________ (or SKIPPED if cannot trigger error)
**Status**: [ ] Pass [ ] Fail [ ] Skip

---

### Test 9: Storage Tab (Unchanged)

**Objective**: Verify Storage tab still works correctly.

**Steps**:
1. Click "Storage" tab
2. Verify **two sections** visible:

   **Section 1: Error Correction**
   - [ ] Reed-Solomon checkbox and redundancy spin button
   - [ ] "Apply to current vault" checkbox (if vault open)

   **Section 2: Automatic Backups**
   - [ ] Enable backups checkbox
   - [ ] Backup count spin button

3. Test settings save and persist across reopen

**Expected Result**: ✅ Storage tab unchanged and functional
**Actual Result**: ___________
**Status**: [ ] Pass [ ] Fail

---

### Test 10: Account Password Reuse Detection

**Objective**: Verify account password history prevents password reuse for account entries.

**Steps**:
1. Open preferences → Account Security → Enable "Prevent account password reuse" with limit 3
2. Edit an existing account and change password to "NewPass123!"
3. Try to change password back to original:
   - [ ] Error dialog: "Password reuse detected!"
   - [ ] Account NOT saved
4. Change to a different unused password:
   - [ ] Change succeeds
5. Change password 3 more times (filling history to limit)
6. Try to reuse one of the 3 recent passwords:
   - [ ] Error dialog appears
7. Try to reuse the oldest password (beyond limit):
   - [ ] Change SUCCEEDS (limit working correctly)

**Expected Result**: ✅ Password reuse prevented for last N passwords (within limit)
**Actual Result**: ___________
**Status**: [ ] Pass [ ] Fail

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
   - [ ] Appearance: "Dark" selected
   - [ ] Account Security: Clipboard = 45s, Account password history = 10, Undo = 30
   - [ ] Vault Security: Auto-lock enabled, 600s
   - [ ] Storage: Backups = 7

**Expected Result**: ✅ All settings persist after close/reopen
**Actual Result**: ___________
**Status**: [ ] Pass [ ] Fail

---

### Test 12: Admin vs Non-Admin View (V2 Vaults Only)

**Objective**: Verify both admin and standard users can access Vault Security tab.

**Steps**:
1. Log in as **administrator** → Open preferences → Vault Security
   - [ ] All sections visible (Auto-Lock, User Password History, FIPS)

2. Log in as **standard user** → Open preferences → Vault Security
   - [ ] All sections visible (same as admin)
   - [ ] Can manage own password history

**Note**: Admin-only restrictions apply at the page level (user management), not preferences.

**Expected Result**: ✅ Both users see full Vault Security tab
**Actual Result**: ___________
**Status**: [ ] Pass [ ] Fail [ ] Skip (no V2 vault)

---

### Test 13: Vault State Changes

**Objective**: Verify password history section shows/hides when vault opens/closes.

**Steps**:
1. With NO vault open:
   - Open preferences → Vault Security
   - [ ] Password history section HIDDEN

2. Open a vault:
   - Preferences should still be open
   - [ ] Password history section NOW VISIBLE
   - [ ] Shows correct vault policy and user info

3. Close the vault (lock or close):
   - Preferences should still be open
   - [ ] Password history section NOW HIDDEN again

**Expected Result**: ✅ Section dynamically shows/hides based on vault state
**Actual Result**: ___________
**Status**: [ ] Pass [ ] Fail

---

### Test 14: Cancel Button Behavior

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
| 11 | Settings Persistence | [ ] Pass [ ] Fail | |
| 12 | Admin vs Non-Admin | [ ] Pass [ ] Fail [ ] Skip | |
| 13 | Vault State Changes | [ ] Pass [ ] Fail | |
| 14 | Cancel Button | [ ] Pass [ ] Fail | |

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
