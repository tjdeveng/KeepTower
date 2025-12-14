# Undo/Redo Bug Fixes

## Issues Addressed

### 1. Data Loss on Undo/Redo
**Problem**: When a user:
1. Added a new account
2. Edited the account fields
3. Clicked undo (account was deleted)
4. Clicked redo (account reappeared but all edits were lost)

**Root Cause**: `AddAccountCommand` stored the initial empty `AccountRecord` that was passed to it during construction. When the user edited the account after creation, those changes were not captured. When undo was executed, it deleted the account. When redo was executed, it restored the original empty account, not the edited version.

**Solution**: Modified `AddAccountCommand::undo()` to capture the current state of the account before deletion:

```cpp
[[nodiscard]] bool undo() override {
    if (!m_vault_manager || m_added_index < 0) {
        return false;
    }

    // Before deleting, save the current state of the account (in case user edited it)
    const auto* current_account = m_vault_manager->get_account(m_added_index);
    if (current_account) {
        // Update our stored account with current state for proper redo
        m_account.CopyFrom(*current_account);
    }

    // Delete the account we added
    bool success = m_vault_manager->delete_account(m_added_index);

    if (success && m_ui_callback) {
        m_ui_callback();
    }

    return success;
}
```

This ensures that when redo is called, it restores the account with all user edits intact.

### 2. Invalid Account Index Warning
**Problem**: During redo operations, the console displayed:
```
WARNING: Invalid account index 6 (total accounts: 6)
```

This indicated an off-by-one error where code was trying to access index 6 when only indices 0-5 were valid (for 6 accounts).

**Root Cause**: When `AddAccountCommand`'s UI callback was invoked after undo/redo, it called `update_account_list()` which triggered selection changes. The previous `m_selected_account_index` was still pointing to the deleted account's index, causing `save_current_account()` to attempt saving an invalid account.

**Solution**: Modified the UI callback in `on_add_account()` to clear the selection before updating the account list:

```cpp
auto ui_callback = [this]() {
    // Clear search filter so new account is visible
    m_search_entry.set_text("");

    // Clear selection before updating list to avoid stale index issues
    clear_account_details();  // This sets m_selected_account_index = -1

    // Update the display
    update_account_list();

    // ... rest of callback
};
```

This ensures `m_selected_account_index` is always valid when UI updates occur.

## Files Modified

### src/core/commands/AccountCommands.h
- **Lines 93-100**: Added state capture in `AddAccountCommand::undo()`
- Uses protobuf's `CopyFrom()` to deep copy the current account state
- Preserves all user edits for proper redo behavior

### src/ui/windows/MainWindow.cc
- **Line 850**: Added `clear_account_details()` call before `update_account_list()`
- **Lines 3018-3033**: Removed debug logging from `update_undo_redo_sensitivity()`
- Cleaned up production code by removing temporary debugging statements

## Testing Performed

The following scenario now works correctly:

1. ✅ Open vault with existing accounts
2. ✅ Click "Add Account" button
3. ✅ Edit account fields (name, username, password, email, etc.)
4. ✅ Click Undo → account is deleted
5. ✅ Click Redo → account reappears with ALL edits preserved
6. ✅ No invalid index warnings in console
7. ✅ Selection state correctly managed

## Known Limitations

### ModifyAccountCommand Not Implemented
Currently, editing an existing account does NOT create an undo history entry. Only these operations support undo/redo:
- ✅ Add Account
- ✅ Delete Account
- ✅ Toggle Favorite (star/unstar)
- ❌ Modify Account (edit existing account fields)

**Reason**: `ModifyAccountCommand` has not been implemented yet. This is expected behavior, not a bug.

**Future Work**: Implement `ModifyAccountCommand` to track field changes:
- Store old and new account states
- Execute: Update vault with new state
- Undo: Restore old state
- Redo: Apply new state again
- Trigger: Connect to field change signals in MainWindow

## Implementation Details

### Security Considerations
The fix maintains security best practices:
- Passwords are still securely cleared in `AddAccountCommand` destructor
- Uses protobuf's `CopyFrom()` which handles memory management safely
- No plaintext password logging or exposure

### Performance Impact
Minimal - the additional `CopyFrom()` call during undo is negligible:
- Only executed when user performs undo operation
- Uses efficient protobuf serialization
- No impact on normal add/edit operations

## Related Files
- [AccountCommands.h](src/core/commands/AccountCommands.h) - Command implementations
- [MainWindow.cc](src/ui/windows/MainWindow.cc) - UI callbacks and selection management
- [UNDO_REDO_FEATURE.md](UNDO_REDO_FEATURE.md) - Original feature documentation
- [UNDO_REDO_PREFERENCES_SUMMARY.md](UNDO_REDO_PREFERENCES_SUMMARY.md) - Preferences feature

## Conclusion

Both bugs have been resolved:
1. ✅ User edits are preserved during undo/redo cycles
2. ✅ No invalid index warnings during redo operations
3. ✅ Clean production code with debug logging removed

The undo/redo feature now works correctly for Add, Delete, and Toggle Favorite operations. Future enhancement would include ModifyAccountCommand for tracking field edits.
