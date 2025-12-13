# KeepTower v0.3.x Roadmap - Multi-User RBAC Feature

## Overview
Implement role-based access control (RBAC) with configurable YubiKey policies to support multi-user vault access with different permission levels.

## Primary Goals
1. Reduce insider threat risk (malicious or negligent users)
2. Support team/corporate vault sharing scenarios
3. Provide granular security policy control
4. Maintain single-file vault portability

## User Roles

### Administrator Role
- **Created by**: Vault creator (first user)
- **Permissions**:
  - Full export/import rights
  - Manage vault settings (FEC, backup configuration)
  - Add/remove users and assign roles
  - Change master password
  - Delete vault
  - Configure security policies
  - Manage YubiKey requirements per user
  - View audit logs

### Standard User Role
- **Created by**: Administrator invitation
- **Permissions**:
  - Open and view vault
  - Add new accounts
  - Edit/delete their own added accounts
  - Copy passwords to clipboard
  - Generate passwords
- **Restrictions**:
  - Cannot export vault data
  - Cannot modify vault settings (FEC, backup)
  - Cannot add/remove users
  - Cannot change security policies
  - May be required to use YubiKey (policy-dependent)

## Security Policies

### YubiKey Policy Options
Administrators can configure vault-wide YubiKey requirements:

1. **OPTIONAL** (Default)
   - YubiKey recommended but not required for any user
   - Suitable for personal use or small trusted teams

2. **ADMIN_ONLY**
   - Only administrator requires YubiKey
   - Standard users can use password-only
   - Use case: Admin wants extra security, users need convenience

3. **STANDARD_USERS_ONLY**
   - Standard users MUST have YubiKey
   - Administrator can access without YubiKey (emergency access)
   - Use case: Restrict standard users but allow admin flexibility

4. **ALL_USERS**
   - Everyone must use YubiKey for vault access
   - Highest security posture
   - Use case: High-security corporate environments

### Additional Policy Controls
- **Export Restrictions**: Allow/deny standard user exports
- **Password Rotation**: Force periodic password changes
- **Failed Attempt Lockout**: Lock user after N failed login attempts
- **Per-User Overrides**: Admin can override global policy for specific users

## Technical Implementation

### Protocol Buffer Schema Extensions

```protobuf
// Security policy configuration
message VaultPolicy {
  enum YubiKeyPolicy {
    OPTIONAL = 0;
    ADMIN_ONLY = 1;
    STANDARD_USERS_ONLY = 2;
    ALL_USERS = 3;
  }

  YubiKeyPolicy yubikey_policy = 1;
  bool allow_export_standard_users = 2;
  bool require_password_rotation = 3;
  uint32 password_rotation_days = 4;  // 0 = disabled
  uint32 max_failed_attempts = 5;     // 0 = unlimited
  uint32 lockout_duration_minutes = 6;
}

// User account entry
message UserEntry {
  string username = 1;
  string display_name = 2;

  enum Role {
    ADMIN = 0;
    STANDARD = 1;
  }
  Role role = 3;

  // Authentication
  bytes password_salt = 4;              // User-specific salt
  bytes password_hash = 5;              // PBKDF2 hash for verification
  repeated string yubikey_serials = 6;  // Registered YubiKey serial numbers
  bool yubikey_required_override = 7;   // Per-user policy override

  // Metadata
  uint64 created_at = 8;                // Unix timestamp
  uint64 last_login = 9;
  uint32 failed_attempts = 10;
  uint64 locked_until = 11;             // Unix timestamp, 0 = not locked
  uint64 password_changed_at = 12;
}

// Audit log entry
message AuditLogEntry {
  enum Action {
    VAULT_OPENED = 0;
    VAULT_CLOSED = 1;
    ACCOUNT_ADDED = 2;
    ACCOUNT_MODIFIED = 3;
    ACCOUNT_DELETED = 4;
    ACCOUNT_VIEWED = 5;
    PASSWORD_COPIED = 6;
    EXPORTED = 7;
    USER_ADDED = 8;
    USER_REMOVED = 9;
    POLICY_CHANGED = 10;
    SETTINGS_CHANGED = 11;
  }

  string username = 1;
  Action action = 2;
  uint64 timestamp = 3;
  string details = 4;  // JSON or human-readable description
  string account_name = 5;  // If action relates to specific account
}

// Add to VaultData
message VaultData {
  // ... existing fields ...

  VaultPolicy policy = 100;
  repeated UserEntry users = 101;
  repeated AuditLogEntry audit_log = 102;
}
```

### Cryptographic Design

#### Multi-User Key Derivation
Each user has their own password, but all users decrypt the same vault:

1. **Vault Master Key (VMK)**: Random 256-bit key, encrypts account data
2. **User Key Encryption Key (KEK)**: Derived from user password + YubiKey
3. **Encrypted VMK**: Each user has VMK encrypted with their KEK

```
User A Password + YubiKey → KEK_A → Decrypt VMK → Decrypt Vault
User B Password + YubiKey → KEK_B → Decrypt VMK → Decrypt Vault
```

#### Key Rotation
When admin changes policies or removes users:
- Generate new VMK
- Re-encrypt VMK for each remaining user
- Re-encrypt all account data with new VMK
- Old VMK securely erased

### UI Changes

#### New Dialogs
1. **User Management Dialog**
   - List users with roles
   - Add/remove users
   - Change user roles
   - View user login history
   - Reset failed attempts / unlock users

2. **Security Policy Dialog**
   - YubiKey policy dropdown
   - Export restrictions toggle
   - Password rotation settings
   - Failed attempt lockout settings

3. **User Login Dialog** (replaces simple password dialog)
   - Username field
   - Password field
   - YubiKey prompt (if required)
   - "Remember username" option

4. **Audit Log Viewer**
   - Filterable table of actions
   - Export audit log
   - Search/filter by user, action, date

#### Menu Changes
- **Admin-only menu items**:
  - "Manage Users..."
  - "Security Policy..."
  - "View Audit Log..."
  - "Export to CSV..."
- **Grayed out for standard users**

### VaultManager API Extensions

```cpp
// User management
bool add_user(const std::string& username,
              const std::string& password,
              UserRole role,
              const std::string& yubikey_serial = "");

bool remove_user(const std::string& username);

bool change_user_role(const std::string& username, UserRole new_role);

std::vector<UserEntry> get_users() const;

// Policy management
bool set_vault_policy(const VaultPolicy& policy);

VaultPolicy get_vault_policy() const;

// Authentication
bool authenticate_user(const std::string& username,
                      const std::string& password,
                      const std::string& yubikey_serial = "");

std::string get_current_username() const;

UserRole get_current_user_role() const;

bool has_permission(Permission perm) const;

// Audit logging
void log_action(AuditAction action,
               const std::string& details = "",
               const std::string& account_name = "");

std::vector<AuditLogEntry> get_audit_log(const AuditFilter& filter) const;
```

## Migration Path

### From v0.2.x to v0.3.x
- Existing vaults automatically get single admin user (vault creator)
- Default policy: OPTIONAL (backward compatible)
- No breaking changes to file format (new fields are optional)
- Users can continue using vaults without multi-user features

### Upgrade Process
1. Open vault with v0.3.x
2. Vault detects no user entries
3. Create default admin user with current password
4. Prompt: "Enable multi-user support?"
5. If yes: Show user management dialog
6. If no: Continue as single-user (can enable later)

## Testing Requirements

### Security Tests
- [ ] User isolation (users can't access each other's KEKs)
- [ ] Policy enforcement (standard users can't export)
- [ ] YubiKey policy enforcement (required users can't bypass)
- [ ] Failed attempt lockout
- [ ] Key rotation security

### Functional Tests
- [ ] Add/remove users
- [ ] Change user roles
- [ ] Policy changes take effect
- [ ] Audit log accuracy
- [ ] Multiple users can open same vault
- [ ] Password changes don't affect other users

### UI Tests
- [ ] Menu items enabled/disabled correctly
- [ ] Dialogs show appropriate options per role
- [ ] User management dialog CRUD operations
- [ ] Policy dialog updates settings

## Performance Considerations
- User list should scale to 10-20 users minimum
- Audit log should handle 10,000+ entries efficiently
- Key rotation should complete in <5 seconds for 1000 accounts
- Login should not be significantly slower than current

## Documentation Updates
- User guide: Multi-user vault setup
- Admin guide: Security policy configuration
- API documentation: New VaultManager methods
- Migration guide: v0.2.x → v0.3.x

## Future Enhancements (v0.4.x)
- Groups/teams within vault
- Temporary user access (expires after N days)
- Account-level permissions (user A can't see account B)
- Two-person rule for exports (requires two admins)
- Integration with LDAP/Active Directory
- Remote vault access with authentication server

## Implementation Priority
**Target**: v0.3.0-beta

**Phase 1** (v0.3.0-alpha):
- Protocol buffer schema
- VaultManager backend (user management, policy)
- Basic authentication flow

**Phase 2** (v0.3.0-beta):
- UI dialogs (user management, policy)
- Audit logging
- Migration from v0.2.x

**Phase 3** (v0.3.0):
- Complete testing
- Documentation
- Production-ready release
