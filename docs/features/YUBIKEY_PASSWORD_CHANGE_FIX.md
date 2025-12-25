# YubiKey Password Change Fix

## Problem
When changing a password for a user with an enrolled YubiKey, the YubiKey prompt was not shown. This caused:
1. No visual indication that user needs to touch their YubiKey
2. System reporting application as non-responsive during 15-second timeout
3. Eventual timeout with "yk_challenge_response failed" error

## Root Cause
The `change_user_password()` method in VaultManagerV2.cc directly calls `yk_manager.challenge_response()` which blocks the UI thread. No YubiKey prompt dialog was being shown before this blocking operation.

## Solution
Added YubiKey touch prompt dialog display before calling `change_user_password()` in both password change flows:

1. **Forced Password Change** (password change required on login)
   - Location: `MainWindow::handle_password_change_required()`
   - Lines: ~3500-3530

2. **Voluntary Password Change** (user settings)
   - Location: `MainWindow::on_change_password()`
   - Lines: ~3785-3815

### Implementation Details
- Check if user has `yubikey_enrolled` flag set via `list_users()`
- If enrolled, show `YubiKeyPromptDialog` with `TOUCH` prompt type
- Process GTK events to ensure dialog is rendered (150ms delay)
- Perform password change operation
- Hide dialog after operation completes

## Testing Checklist
- [ ] Standard user with YubiKey - forced password change (must_change_password flag)
- [ ] Standard user with YubiKey - voluntary password change (settings menu)
- [ ] Admin user with YubiKey - forced password change
- [ ] Admin user with YubiKey - voluntary password change
- [ ] User without YubiKey - password change (should work without prompt)
- [ ] Verify no "not responding" message during YubiKey wait
- [ ] Verify YubiKey timeout (15s) still shows appropriate error if not touched
- [ ] Verify correct password with YubiKey touch succeeds

## Files Modified
- `src/ui/windows/MainWindow.cc`:
  - Lines ~3502-3532: Added YubiKey prompt for forced password change
  - Lines ~3787-3817: Added YubiKey prompt for voluntary password change

## Related Issues
- Similar to vault opening YubiKey prompt implementation
- Follows same pattern used in:
  - V1 vault opening (lines 573-590)
  - V2 vault opening (lines 754-779)
  - YubiKey enrollment (lines 2135+)
