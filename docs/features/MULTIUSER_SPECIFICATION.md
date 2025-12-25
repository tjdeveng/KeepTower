# Multi-User Accounts Specification

**Status:** Planned for future implementation
**Priority:** After sort feature (A-Z/Z-A toggle)
**Estimated Effort:** 1-2 weeks
**Date Created:** 22 December 2025

## Overview

Add support for multiple user accounts with role-based permissions to enable team/organizational use of KeepTower while maintaining security controls.

## User Roles

### Administrator
- Full vault access and management
- Can add/remove user accounts (handle staff changes)
- Can modify all security settings (encryption, FIPS mode, etc.)
- Can export vault to plaintext
- Can delete any account (including private accounts)
- Can view all accounts (including those marked private)

### Standard User
- Can view all accounts in vault (except those marked as private)
- Can edit all accounts they have access to
- Can add new accounts to vault
- **Cannot** modify security features (encryption settings, FIPS mode, backup settings, etc.)
- **Cannot** export vault to plaintext
- **Cannot** delete accounts marked as "admin-only deletable"
- **Cannot** add/remove user accounts

## Account-Level Privacy (Optional Feature)

### Private Accounts
Accounts can optionally be marked with privacy flags:
- **Admin-only viewable:** Only administrators can view/edit this account
- **Admin-only deletable:** All users can view/edit, but only admins can delete

Use cases:
- Sensitive credentials (CEO accounts, payroll systems, etc.)
- Protected accounts that shouldn't be accidentally deleted by standard users

## Restricted Operations

Standard users are **explicitly restricted** from:

1. **Security Settings:**
   - Encryption algorithm changes
   - FIPS-140-3 mode toggle
   - Master password changes (may need admin approval workflow)
   - Backup configuration

2. **Export Operations:**
   - Export to JSON
   - Export to CSV
   - Export to XML
   - Any plaintext export format

3. **User Management:**
   - Add new users
   - Remove existing users
   - Change user roles (promote/demote)
   - Modify user permissions

4. **Vault Operations:**
   - Vault deletion
   - Vault format migration
   - Recovery key generation (if implemented)

## Implementation Considerations

### Vault Format Changes
- Add `users` section to vault structure
- Store username, password hash (bcrypt/Argon2), and role
- Current vault owner becomes first admin by default
- Migration path for existing vaults

### Authentication Flow
1. User opens vault with master password
2. Present user selection dialog (list of usernames)
3. User enters their username + user password
4. Authenticate against stored hash
5. Load UI with role-based restrictions

### UI Changes

**Affected Components:**
- PreferencesDialog: Lock security tab for standard users
- MainWindow: Disable export menu items for standard users
- User management dialog (new): Admin-only, add/remove users
- Account privacy flags (new): Checkbox for "Private" or "Admin-only delete"

**Visual Indicators:**
- Show current user and role in title bar or status bar
- Tooltip explanations for disabled features ("Administrator access required")
- Lock icons next to restricted menu items

### Permission Model

```cpp
enum class UserRole {
    ADMINISTRATOR,
    STANDARD_USER
};

class User {
    std::string username;
    std::string password_hash;  // bcrypt/Argon2
    UserRole role;
};

class VaultSession {
    User current_user;

    bool can_modify_security_settings() const {
        return current_user.role == UserRole::ADMINISTRATOR;
    }

    bool can_export_plaintext() const {
        return current_user.role == UserRole::ADMINISTRATOR;
    }

    bool can_manage_users() const {
        return current_user.role == UserRole::ADMINISTRATOR;
    }

    bool can_delete_account(const Account& account) const {
        if (current_user.role == UserRole::ADMINISTRATOR)
            return true;
        return !account.is_admin_only_deletable();
    }

    bool can_view_account(const Account& account) const {
        if (current_user.role == UserRole::ADMINISTRATOR)
            return true;
        return !account.is_private();
    }
};
```

### Security Considerations

1. **Password Storage:**
   - User passwords hashed with Argon2id or bcrypt
   - Separate from master vault password
   - Stored encrypted within vault structure

2. **Session Management:**
   - No persistent sessions (re-authenticate on vault open)
   - No "remember me" functionality
   - Session tied to vault lifetime

3. **Audit Trail:**
   - Log user actions (account modifications, exports, user changes)
   - Store in vault metadata
   - Admin-only visibility

4. **First Admin:**
   - Vault creator automatically becomes first admin
   - Cannot delete last admin (must have at least one)

### Backward Compatibility

- Existing vaults: No users defined = single-user mode (current behavior)
- User prompted on first open: "Add multi-user support? This vault currently has no users."
- Migration adds vault owner as sole admin
- Fully backward compatible with current vault format

## Implementation Phases

### Phase 1: Core Infrastructure (3-4 days)
- Vault format extension (users array)
- User authentication system
- Session management with role tracking
- Permission checking framework

### Phase 2: UI Restrictions (2-3 days)
- Lock PreferencesDialog security tab
- Disable export menu items
- Add user indicator in UI
- Tooltip explanations for disabled features

### Phase 3: User Management (2-3 days)
- User management dialog
- Add/remove users (admin only)
- Change passwords
- Role assignment

### Phase 4: Account Privacy (1-2 days) [Optional]
- Add privacy flags to Account class
- UI checkboxes for privacy settings
- Filter accounts based on user role
- Lock delete operation based on flags

### Phase 5: Testing & Documentation (1-2 days)
- Test all permission scenarios
- Migration testing (single-user → multi-user)
- Update user documentation
- Security audit

## Testing Scenarios

1. **Admin User:**
   - ✓ Can access all preferences
   - ✓ Can export to plaintext
   - ✓ Can add/remove users
   - ✓ Can view/edit/delete all accounts

2. **Standard User:**
   - ✓ Can view/edit normal accounts
   - ✗ Cannot access security preferences
   - ✗ Cannot export to plaintext
   - ✗ Cannot manage users
   - ✗ Cannot view private accounts
   - ✗ Cannot delete admin-protected accounts

3. **Migration:**
   - ✓ Old vault opens in single-user mode
   - ✓ Can upgrade to multi-user
   - ✓ Vault creator becomes first admin

## Open Questions

1. **Password Reset:** How do admins reset a standard user's password? Require master password?
2. **Lockout:** What happens if all admins are removed? Should system prevent this?
3. **Audit Detail:** How detailed should audit logging be? Every field edit?
4. **Emergency Access:** Should there be a "master override" with vault master password?

## Related Files

- `src/core/vault.h` - Vault structure definition
- `src/core/vault_manager.cc` - Vault operations
- `src/ui/preferences_dialog.cc` - Security settings UI
- `src/ui/main_window.cc` - Export operations
- Account privacy: New fields in `record.proto`

## References

- Similar feature in 1Password Teams, Bitwarden Organizations
- Role-based access control (RBAC) design patterns
- NIST SP 800-53 access control guidelines

---

**Note:** This specification was created on 22 December 2025 based on user requirements discussion. Review and refine before implementation begins.
