# Manual Test Script for Undo/Redo Fixes

This script guides you through manual testing of the undo/redo bug fixes.

## Prerequisites
- Build the application: `ninja -C build`
- Run the application: `GSETTINGS_SCHEMA_DIR=build/data ./build/src/keeptower`

## Test 1: Data Loss on Undo/Redo (Fixed)

### Steps:
1. Open or create a vault
2. Click the "Add Account" button (+ icon in toolbar)
3. A new account appears with "New Account" as the name
4. **Edit the account fields:**
   - Change name to: "Test Account"
   - Set username to: "testuser"
   - Set password to: "SecurePass123"
   - Set email to: "test@example.com"
   - Add note: "This is a test account for undo/redo"
5. Open the hamburger menu (≡) in the top right
6. Click "Undo" → Account should disappear
7. Click "Redo" → Account should reappear

### Expected Result (FIXED):
✅ Account reappears with **ALL fields intact**:
   - Name: "Test Account"
   - Username: "testuser"
   - Password: "SecurePass123"
   - Email: "test@example.com"
   - Note: "This is a test account for undo/redo"

### Previous Behavior (BUG):
❌ Account reappeared but only had:
   - Name: "New Account"
   - All other fields were empty
   - User edits were lost

---

## Test 2: Invalid Account Index Warning (Fixed)

### Steps:
1. Continue from Test 1 (or create multiple accounts)
2. Ensure you have at least 6 accounts in the vault
3. Select the last account (e.g., account #6)
4. Open the hamburger menu
5. Click "Undo" (to undo the last add operation)
6. Check the terminal console for warnings
7. Click "Redo"
8. Check the terminal console again

### Expected Result (FIXED):
✅ No warnings in the console
✅ UI updates smoothly
✅ Account selection updates correctly

### Previous Behavior (BUG):
❌ Console showed:
```
WARNING: Invalid account index 6 (total accounts: 6)
```

---

## Test 3: Undo/Redo Preference Toggle

### Steps:
1. Open the hamburger menu (≡)
2. Click "Preferences"
3. In the "Undo/Redo" section, **uncheck** "Enable undo/redo"
4. Close the preferences dialog
5. Add a new account
6. Try to open the hamburger menu

### Expected Result:
✅ "Undo" and "Redo" menu items are **greyed out/disabled**
✅ No undo history is recorded

### Steps (continued):
7. Go back to Preferences
8. **Check** "Enable undo/redo"
9. Close the preferences dialog
10. Add a new account
11. Open the hamburger menu

### Expected Result:
✅ "Undo" and "Redo" menu items are now **enabled** (if undo/redo is available)
✅ Undo/redo works normally

---

## Test 4: Multiple Undo/Redo Cycles

### Steps:
1. Start with an empty vault (or clear existing accounts)
2. Add account #1, edit fields (e.g., "Account A")
3. Add account #2, edit fields (e.g., "Account B")
4. Add account #3, edit fields (e.g., "Account C")
5. Click Undo 3 times → All accounts should disappear in reverse order
6. Click Redo 3 times → All accounts should reappear in forward order

### Expected Result:
✅ All accounts reappear with **complete data**:
   - Account A with all its edits
   - Account B with all its edits
   - Account C with all its edits
✅ No console warnings
✅ Selection and UI remain stable

---

## Test 5: Edit Existing Account (Expected Limitation)

### Steps:
1. Create a new account
2. Edit the account name from "New Account" to "Modified Name"
3. Open the hamburger menu
4. Check if "Undo" is enabled

### Expected Result:
❌ "Undo" is **NOT** enabled after editing an existing account
✅ This is **expected behavior** - ModifyAccountCommand not yet implemented

### Notes:
- Undo/redo ONLY works for:
  - Add Account
  - Delete Account
  - Toggle Favorite (star icon)
- Undo/redo does NOT work for:
  - Editing existing account fields
- This is documented as a known limitation, not a bug

---

## Test 6: Delete and Undo

### Steps:
1. Create a new account and edit it (e.g., "Delete Test Account")
2. Right-click the account and select "Delete"
3. Confirm deletion → Account disappears
4. Click "Undo"

### Expected Result:
✅ Account reappears with all data intact
✅ Account appears in the same position in the list

---

## Test 7: Toggle Favorite

### Steps:
1. Select any account
2. Click the star icon to mark as favorite
3. Account moves to top of list with star icon
4. Click "Undo" → Star should disappear, account moves back
5. Click "Redo" → Star should reappear, account moves to favorites section

### Expected Result:
✅ Favorite status toggles correctly
✅ List sorting updates properly
✅ All account data remains intact

---

## Success Criteria

All tests should pass with:
- ✅ No data loss during undo/redo cycles
- ✅ No console warnings about invalid indices
- ✅ Smooth UI updates
- ✅ Preference toggle works correctly
- ✅ All automated tests pass (17/17)

## Reporting Issues

If any test fails, please report:
1. Which test failed
2. Exact steps to reproduce
3. Expected vs actual behavior
4. Console output (if any warnings/errors)
5. Application version/commit hash
