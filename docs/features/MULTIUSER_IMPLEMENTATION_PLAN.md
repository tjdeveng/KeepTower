# Multi-User Implementation Plan - KeepTower v0.3.x

**Planning Date:** 22 December 2025
**Target Version:** v0.3.0-beta
**Estimated Duration:** 2-3 weeks
**Status:** Planning Phase

---

## Table of Contents
1. [Executive Summary](#executive-summary)
2. [Current Architecture Analysis](#current-architecture-analysis)
3. [Proposed Multi-User Architecture](#proposed-multi-user-architecture)
4. [Vault Format Changes](#vault-format-changes)
5. [Migration Strategy](#migration-strategy)
6. [Implementation Phases](#implementation-phases)
7. [Security Considerations](#security-considerations)
8. [Testing Strategy](#testing-strategy)
9. [Risk Assessment](#risk-assessment)

---

## Executive Summary

### Objectives
- Enable multiple user accounts per vault for team/organizational use
- Implement role-based permissions (Administrator vs Standard User)
- Maintain backward compatibility with existing single-user vaults
- Restrict security settings and plaintext exports to administrators
- Allow administrators to manage user accounts (add/remove users)

### Key Requirements
- **All users** can view/edit all accounts (except private ones)
- **Standard users** cannot modify security settings or export to plaintext
- **Administrators** have full access including user management
- **Optional:** Account-level privacy flags (admin-only viewable/deletable)
- **Critical:** Seamless migration from single-user to multi-user vaults

### Non-Breaking Constraints
- Existing single-user vaults must continue to work
- No changes to master password encryption (vault-level authentication)
- Preserve all existing features (groups, tags, favorites, etc.)

---

## Current Architecture Analysis

### Authentication Flow (Current)
```
1. User opens vault file
2. Enters master password
3. PBKDF2 derives encryption key from password + salt
4. (Optional) YubiKey challenge-response XORed with key
5. Decrypt vault with AES-256-GCM
6. Parse protobuf VaultData
7. Full access to all vault operations
```

**Key Observation:** Current system has **vault-level authentication only**. Once unlocked, there are no user roles or permissions.

### Vault Format (Current - record.proto)
```protobuf
message VaultData {
  repeated AccountRecord accounts = 1;
  VaultMetadata metadata = 2;
  YubiKeyConfig yubikey_config = 16;
  bool fec_enabled = 17;
  int32 fec_redundancy_percent = 18;
  repeated AccountGroup groups = 32;

  // Reserved: 3-15, 19-31, 33-47
}

message VaultMetadata {
  int32 schema_version = 1;
  int64 created_at = 2;
  int64 last_modified = 3;
  int64 last_accessed = 4;
  string name = 5;
  string description = 6;
  int64 access_count = 7;

  // Reserved: 8-15
}

message AccountRecord {
  // Standard fields: id, account_name, username, password, etc.
  // Groups: repeated GroupMembership groups = 38
  // No privacy/ownership fields currently
}
```

### Code Architecture (Current)
- **VaultManager** (src/core/VaultManager.h/cc):
  - Handles encryption/decryption
  - PBKDF2 key derivation: `derive_key(password, salt, key)`
  - Vault operations: `open_vault()`, `save_vault()`, `create_vault()`
  - Account management: `add_account()`, `delete_account()`, etc.

- **MainWindow** (src/ui/windows/MainWindow.h/cc):
  - UI controller with full vault access
  - Preferences dialog for security settings
  - Export functionality (CSV, JSON, XML)

- **PreferencesDialog** (src/ui/dialogs/PreferencesDialog.h/cc):
  - Security tab: encryption, FIPS mode, backup settings
  - No access control currently

---

## Proposed Multi-User Architecture

### Design Principles (LUKS-Style Key Slots)
1. **No "master password"** - Each user has their own password to unlock vault
2. **Single authentication step** - User password directly unlocks vault (better UX)
3. **Key slots like LUKS** - Multiple passwords can decrypt same master key
4. **Role-based access control (RBAC)** - Permissions enforced in application layer
5. **Backward compatible** - Legacy vaults = single key slot (full admin access)
6. **FIPS-140-3 compliant** - Uses NIST-approved key wrapping (AES-256-KW)

### LUKS-Style Key Slot Architecture
```
Vault File Structure:
┌────────────────────────────────────────┐
│ Header (unencrypted)                   │
├────────────────────────────────────────┤
│ Key Slot 0: User "alice" (Admin)      │
│   ├─ Salt (32 bytes)                   │
│   ├─ Username (encrypted)              │
│   ├─ Role (encrypted)                  │
│   ├─ Wrapped KEK (encrypted DEK)       │
│   └─ Optional: YubiKey challenge       │
├────────────────────────────────────────┤
│ Key Slot 1: User "bob" (Standard)     │
│   ├─ Salt (32 bytes)                   │
│   ├─ Username (encrypted)              │
│   ├─ Role (encrypted)                  │
│   ├─ Wrapped KEK (encrypted DEK)       │
│   └─ Optional: YubiKey challenge       │
├────────────────────────────────────────┤
│ ... (up to N key slots)                │
├────────────────────────────────────────┤
│ Encrypted Vault Data (with DEK)       │
│   └─ Encrypted with Master DEK         │
└────────────────────────────────────────┘

Authentication Flow:
1. User enters username + password
2. Find key slot for that username
3. PBKDF2(password, slot_salt) → KEK (Key Encryption Key)
4. (Optional) YubiKey challenge-response XORed with KEK
5. Unwrap DEK from key slot: AES-KW(KEK, wrapped_DEK) → DEK
6. Decrypt user metadata (role, permissions) with KEK
7. Decrypt vault data with DEK
8. Load session with role-based permissions

Key Concepts:
- DEK (Data Encryption Key): Master key that encrypts vault data (NEVER changes)
- KEK (Key Encryption Key): Derived from user password, wraps the DEK
- Each user has unique: password, salt, KEK, wrapped_DEK
- All users unwrap to SAME DEK (can decrypt same vault data)
- Adding/removing users = add/remove key slots (vault data unchanged)
```

### User Roles

#### Administrator
**Permissions:**
- ✅ View/edit/delete all accounts (including private)
- ✅ Modify security settings (encryption, FIPS, backup)
- ✅ Export vault to plaintext (CSV, JSON, XML)
- ✅ Add/remove user accounts (manage key slots)
- ✅ Change user roles
- ✅ Access all preferences
- ✅ Regenerate vault DEK (re-wrap all key slots)

**Authentication:** Username + password (+ optional YubiKey)

#### Standard User
**Permissions:**
- ✅ View/edit normal accounts
- ✅ Add new accounts
- ✅ Use vault for daily password management
- ❌ Modify security settings
- ❌ Export to plaintext
- ❌ Add/remove users
- ❌ View/delete private accounts (if privacy enabled)

**Authentication:** Username + password (+ optional YubiKey)

**Note:** Unlike the previous two-layer design, each user authenticates ONCE with their own password. There is no separate "master password" - each user password directly unlocks the vault.

### Account Privacy (Optional Feature)
```protobuf
message AccountRecord {
  // ... existing fields ...

  // Privacy flags (NEW - field 48)
  bool is_private = 48;               // Only admins can view/edit
  bool is_admin_only_deletable = 49;  // All can view, only admins can delete

  // Audit trail (NEW - field 50)
  string created_by_user = 50;  // Username who created this account
  string modified_by_user = 51; // Username who last modified
}
```

---

## Vault Format Changes

### Schema Version Update
- **Current:** `schema_version = 1` (single key derivation, no key slots)
- **Multi-user:** `schema_version = 2` (key slots with wrapped DEKs)

### Binary Header Format (Outside Protobuf)

**Current Header:**
```
┌──────────────────────────────────────┐
│ Magic: "KTVAULT\0" (8 bytes)        │
│ Version: uint32 (4 bytes)            │
│ PBKDF2 iterations: uint32 (4 bytes)  │
│ Salt: 32 bytes                       │
│ IV: 16 bytes (GCM nonce)             │
│ YubiKey config (if enabled)          │
│ FEC data (if enabled)                │
│ Encrypted vault data                 │
└──────────────────────────────────────┘
```

**New Header with Key Slots:**
```
┌──────────────────────────────────────┐
│ Magic: "KTVAULT\0" (8 bytes)        │
│ Version: uint32 = 2 (4 bytes)        │
│ Number of key slots: uint32          │
│ DEK size: uint32 (always 32 for AES-256) │
├──────────────────────────────────────┤
│ Key Slot 0:                          │
│   ├─ Slot active: uint8 (0/1)        │
│   ├─ Username length: uint16         │
│   ├─ Username: variable bytes        │
│   ├─ Salt: 32 bytes                  │
│   ├─ PBKDF2 iterations: uint32       │
│   ├─ Wrapped DEK: 40 bytes           │
│   │   (AES-256-KW output for 32-byte DEK)
│   ├─ Role: uint8 (0=standard, 1=admin)
│   └─ YubiKey config: optional        │
├──────────────────────────────────────┤
│ Key Slot 1: (same structure)         │
├──────────────────────────────────────┤
│ ... (up to MAX_KEY_SLOTS = 32)       │
├──────────────────────────────────────┤
│ Master IV: 16 bytes (GCM nonce)      │
│ FEC config (if enabled)              │
│ Encrypted vault data (with DEK)      │
└──────────────────────────────────────┘
```

### Key Wrapping (FIPS-140-3 Compliant)

**Algorithm:** AES-256-KW (RFC 3394 - NIST SP 800-38F)
- **Purpose:** Securely wrap DEK with user's KEK
- **Input:** 32-byte DEK (plaintext master key)
- **Output:** 40-byte wrapped DEK (includes 8-byte IV)
- **FIPS status:** Approved for key wrapping

**Workflow:**
```cpp
// Creating a new user key slot
1. Generate random DEK (32 bytes) - ONLY ONCE per vault
   DEK = random_bytes(32);

2. Derive user's KEK from password:
   KEK = PBKDF2(user_password, user_salt, iterations, 32);

3. (Optional) Mix YubiKey response:
   KEK = KEK XOR yubikey_response;

4. Wrap DEK with KEK:
   wrapped_DEK = AES_KW_encrypt(KEK, DEK);  // 40 bytes output

5. Store in key slot:
   slot.username = username;
   slot.salt = user_salt;
   slot.wrapped_dek = wrapped_DEK;
   slot.role = role;

// Opening vault with user password
1. User enters username + password
2. Find key slot for username
3. Derive KEK:
   KEK = PBKDF2(password, slot.salt, slot.iterations, 32);

4. (Optional) Mix YubiKey:
   KEK = KEK XOR yubikey_response;

5. Unwrap DEK:
   DEK = AES_KW_decrypt(KEK, slot.wrapped_dek);

6. Decrypt vault data with DEK:
   vault_data = AES_GCM_decrypt(DEK, master_iv, ciphertext);
```

### Protobuf Changes (Minimal)

**No changes to VaultData needed!** Key slots are in binary header.

**Optional: Add audit trail to AccountRecord**
```protobuf
message AccountRecord {
  // ... existing fields ...

  // Privacy flags (NEW - field 48)
  bool is_private = 48;               // Only admins can view/edit
  bool is_admin_only_deletable = 49;  // All can view, only admins can delete

  // Audit trail (NEW - field 50)
  string created_by_user = 50;  // Username who created this account
  string modified_by_user = 51; // Username who last modified
  int64 last_modified_at = 52;  // Timestamp of last modification
}
```
## Implementation Phases

### Phase 1: Protobuf Schema Extension (2 days)
**Goal:** Add multi-user structures to vault format

**Tasks:**
1. Add `VaultUser`, `MultiUserConfig`, `UserRole` to record.proto
2. Add `multiuser_config` field to `VaultData` (field 48)
3. Add privacy fields to `AccountRecord` (fields 48-51)
4. Regenerate protobuf C++ code (`protoc`)
5. Update VaultManager to handle new fields
6. Ensure backward compatibility (optional fields)

**Deliverables:**
- Updated record.proto
- Generated record.pb.h/cc
- Unit tests for protobuf serialization

**Testing:**
- Load legacy vault → No errors
- Save legacy vault → No multiuser_config added
- Create new vault → Can add multiuser_config

---

### Phase 2: User Management Backend (3-4 days)
**Goal:** Implement user authentication and session management

**Tasks:**
1. Add Argon2id library dependency (libsodium or standalone)
2. Create `UserManager` class:
   ```cpp
   class UserManager {
   public:
       // Hash password with Argon2id
       std::string hash_password(const std::string& password);

       // Verify password against hash
       bool verify_password(const std::string& password, const std::string& hash);

       // Create new user
       bool add_user(const std::string& username, const std::string& password, UserRole role);

       // Remove user (prevent if last admin)
       bool remove_user(const std::string& username);

       // Authenticate user
       std::optional<VaultUser> authenticate(const std::string& username, const std::string& password);

       // Check role
       bool is_admin(const std::string& username);
   };
   ```

3. Add `VaultSession` class:
   ```cpp
   class VaultSession {
   public:
       VaultSession(const VaultUser& user);

       // Permission checks
       bool can_modify_security_settings() const;
       bool can_export_plaintext() const;
       bool can_manage_users() const;
       bool can_view_account(const AccountRecord& account) const;
       bool can_delete_account(const AccountRecord& account) const;

       // Getters
       std::string username() const;
       UserRole role() const;
       bool is_admin() const;

   private:
       VaultUser m_user;
   };
   ```

4. Integrate with VaultManager:
   ```cpp
   class VaultManager {
       // ... existing members ...

       std::unique_ptr<VaultSession> m_session;  // Current user session

   public:
       // Check if vault has multi-user
       bool is_multiuser_enabled() const;

       // Get user list (for selection dialog)
       std::vector<std::string> get_usernames() const;

       // Authenticate and create session
       bool authenticate_user(const std::string& username, const std::string& password);

       // Get current session
       const VaultSession* get_session() const;

       // User management (admin only)
       bool add_user(const std::string& username, const std::string& password, UserRole role);
       bool remove_user(const std::string& username);
       bool change_user_role(const std::string& username, UserRole new_role);
   };
   ```

**Deliverables:**
- UserManager class with Argon2id hashing
- VaultSession class with permission checks
- VaultManager integration
- Unit tests for authentication

**Testing:**
- Hash password → Verify succeeds
- Hash password → Wrong password fails
- Add admin user → Succeeds
- Remove last admin → Fails with error
- Authenticate → Creates session
- Permission checks return correct results

---

### Phase 3: User Selection Dialog (2 days)
**Goal:** Create UI for user authentication

**Tasks:**
1. Create `UserSelectionDialog` class:
   - Dropdown/combobox for username selection
   - Password entry field
   - Login/Cancel buttons
   - Error message display

2. Integrate with MainWindow:
   ```cpp
   bool MainWindow::open_vault(const std::string& path) {
       // 1. Open vault with master password (existing flow)
       if (!m_vault_manager->open_vault(path, master_password)) {
           show_error("Incorrect master password");
           return false;
       }

       // 2. Check if multi-user
       if (m_vault_manager->is_multiuser_enabled()) {
           // Show user selection dialog
           UserSelectionDialog dialog(*this, m_vault_manager->get_usernames());
           int result = dialog.run();

           if (result != Gtk::ResponseType::OK) {
               m_vault_manager->close_vault();
               return false;
           }

           // Authenticate user
           std::string username = dialog.get_username();
           std::string password = dialog.get_password();

           if (!m_vault_manager->authenticate_user(username, password)) {
               show_error("Incorrect username or password");
               m_vault_manager->close_vault();
               return false;
           }
       }

       // 3. Vault is now fully authenticated
       update_ui_for_session();
       return true;
   }
   ```

**Deliverables:**
- UserSelectionDialog UI
- Integration with vault open flow
- Error handling

**Testing:**
- Open legacy vault → No dialog shown
- Open multi-user vault → Dialog appears
- Enter correct credentials → Login succeeds
- Enter wrong password → Error shown
- Cancel dialog → Vault remains closed

---

### Phase 4: Permission Enforcement (3-4 days)
**Goal:** Lock UI elements based on user role

**Tasks:**
1. Update MainWindow:
   ```cpp
   void MainWindow::update_ui_for_session() {
       const auto* session = m_vault_manager->get_session();

       if (!session || session->is_admin()) {
           // Admin or legacy mode: full access
           enable_all_features();
       } else {
           // Standard user: restricted access
           disable_security_preferences();
           disable_export_plaintext();
           disable_user_management();
       }

       // Show user indicator in status bar
       if (session) {
           m_status_label.set_text(
               Glib::ustring::compose("Logged in as: %1 (%2)",
                   session->username(),
                   session->is_admin() ? "Administrator" : "Standard User")
           );
       }
   }
   ```

2. Lock PreferencesDialog security tab:
   ```cpp
   PreferencesDialog::PreferencesDialog() {
       // ... existing setup ...

       // Check permissions
       if (!can_access_security_settings()) {
           m_security_tab.set_sensitive(false);
           m_security_tab.set_tooltip_text(
               "Security settings can only be modified by administrators");
       }
   }

   bool PreferencesDialog::can_access_security_settings() {
       if (auto* vm = get_vault_manager()) {
           if (auto* session = vm->get_session()) {
               return session->can_modify_security_settings();
           }
       }
       return true;  // Legacy mode = full access
   }
   ```

3. Lock export functionality:
   ```cpp
   void MainWindow::on_export_csv() {
       if (!can_export_plaintext()) {
           show_error("Export to plaintext requires administrator access");
           return;
       }

       // ... existing export logic ...
   }
   ```

4. Add visual indicators:
   - Lock icon (🔒) next to restricted menu items
   - Grayed out / insensitive controls
   - Tooltip explanations

**Deliverables:**
- Permission checks in MainWindow
- Locked PreferencesDialog security tab
- Disabled export menu items
- Visual feedback for restrictions

**Testing:**
- Login as admin → All features enabled
- Login as standard user → Security tab locked
- Standard user clicks export → Error shown
- Tooltips explain restriction

---

### Phase 5: User Management Dialog (2-3 days)
**Goal:** Admin interface for managing users

**Tasks:**
1. Create `UserManagementDialog`:
   - ListBox showing all users (username, role, created date)
   - "Add User" button
   - "Remove User" button (disabled for last admin)
   - "Change Role" button
   - Current user highlighted

2. Add user creation dialog:
   - Username entry (validation: 3-50 chars, alphanumeric + underscore)
   - Password entry (validation: min 8 chars)
   - Password confirmation
   - Role selection (dropdown)

3. Integrate with MainWindow menu:
   ```cpp
   add_action("manage-users", sigc::mem_fun(*this, &MainWindow::on_manage_users));

   void MainWindow::on_manage_users() {
       if (!can_manage_users()) {
           show_error("User management requires administrator access");
           return;
       }

       UserManagementDialog dialog(*this, m_vault_manager);
       dialog.run();

       // Vault may have been modified
       mark_vault_modified();
   }
   ```

**Deliverables:**
- UserManagementDialog UI
- Add/remove/edit user functionality
- Integration with MainWindow
- Validation and error handling

**Testing:**
- Admin opens dialog → Shows all users
- Add new user → Appears in list
- Remove user → Removed from list
- Try to remove last admin → Error shown
- Standard user tries to open → Blocked

---

### Phase 6: Account Privacy (Optional - 1-2 days)
**Goal:** Mark accounts as private or admin-only deletable

**Tasks:**
1. Add checkboxes to AccountDetailWidget:
   ```
   [ ] Mark as private (only administrators can view)
   [ ] Protect from deletion (only administrators can delete)
   ```

2. Update account display filtering:
   ```cpp
   std::vector<AccountRecord> MainWindow::get_visible_accounts() {
       auto all_accounts = m_vault_manager->get_all_accounts();
       const auto* session = m_vault_manager->get_session();

       if (!session || session->is_admin()) {
           return all_accounts;  // Admins see everything
       }

       // Filter out private accounts for standard users
       std::vector<AccountRecord> visible;
       for (const auto& acc : all_accounts) {
           if (!acc.is_private()) {
               visible.push_back(acc);
           }
       }
       return visible;
   }
   ```

3. Enforce delete restrictions:
   ```cpp
   void MainWindow::on_delete_account() {
       const auto* session = m_vault_manager->get_session();
       const auto& account = get_current_account();

       if (!session || !session->can_delete_account(account)) {
           show_error("This account can only be deleted by administrators");
           return;
       }

       // ... existing delete logic ...
   }
   ```

**Deliverables:**
- Privacy checkboxes in account editor
- Filtered account display
- Delete protection enforcement

**Testing:**
- Admin marks account private → Standard user can't see it
- Admin marks account protected → Standard user can't delete
- Admin can still see/delete everything

---

### Phase 7: Migration UI (1-2 days)
**Goal:** Prompt users to enable multi-user on legacy vaults

**Tasks:**
1. Add info banner to MainWindow when legacy vault opened:
   ```cpp
   void MainWindow::show_multiuser_prompt() {
       // Create info bar at top of window
       auto info_bar = Gtk::make_managed<Gtk::InfoBar>();
       info_bar->set_message_type(Gtk::MessageType::INFO);
       info_bar->set_show_close_button(true);

       auto label = Gtk::make_managed<Gtk::Label>(
           "Enable multi-user support to add team members and control access.");
       info_bar->add_child(*label);

       info_bar->add_button("Enable Multi-User", 1);
       info_bar->add_button("Learn More", 2);

       info_bar->signal_response().connect([this](int response) {
           if (response == 1) {
               enable_multiuser_flow();
           } else if (response == 2) {
               show_multiuser_help();
           }
       });

       m_main_box.prepend(*info_bar);
   }
   ```

2. Create first admin wizard:
   ```cpp
   void MainWindow::enable_multiuser_flow() {
       // 1. Explain what will happen
       auto dialog = Gtk::MessageDialog(*this,
           "Enable Multi-User Support",
           false,
           Gtk::MessageType::QUESTION,
           Gtk::ButtonsType::OK_CANCEL);
       dialog.set_secondary_text(
           "This will create the first administrator account. "
           "You can add more users later.\n\n"
           "Recommendation: Create a backup before proceeding.");

       if (dialog.run() != Gtk::ResponseType::OK) {
           return;
       }

       // 2. Create first admin
       CreateAdminDialog admin_dialog(*this);
       if (admin_dialog.run() != Gtk::ResponseType::OK) {
           return;
       }

       // 3. Enable multi-user in vault
       std::string username = admin_dialog.get_username();
       std::string password = admin_dialog.get_password();

       if (!m_vault_manager->enable_multiuser(username, password)) {
           show_error("Failed to enable multi-user support");
           return;
       }

       // 4. Mark vault modified and prompt save
       mark_vault_modified();
       show_info("Multi-user support enabled! You are now logged in as: " + username);

       // Remove info bar
       remove_multiuser_prompt();
   }
   ```

**Deliverables:**
- Info banner for legacy vaults
- First admin wizard
- Migration flow
- Help/documentation

**Testing:**
- Open legacy vault → Info banner shows
- Click "Enable" → Wizard appears
- Complete wizard → Vault upgraded
- Reopen vault → User selection dialog shows

---

### Phase 8: Testing & Documentation (2-3 days)
**Goal:** Comprehensive testing and documentation

**Testing Tasks:**
1. Unit tests:
   - UserManager password hashing
   - VaultSession permission checks
   - User add/remove validation

2. Integration tests:
   - Open legacy vault → Legacy mode
   - Enable multi-user → Vault upgraded
   - Open multi-user vault → User auth
   - Admin operations → All succeed
   - Standard user operations → Restricted correctly

3. Migration tests:
   - Legacy vault → Enable multi-user → Reopen works
   - Multi-user vault → Legacy system → Error handling

4. Edge case tests:
   - Empty username/password
   - Duplicate usernames
   - Delete last admin
   - User password reset

**Documentation Tasks:**
1. Update README.md with multi-user feature
2. Create MULTIUSER_USER_GUIDE.md:
   - How to enable multi-user
   - Adding/removing users
   - Understanding roles
   - Privacy settings

3. Update FIPS_COMPLIANCE.md (no changes needed)
4. Update ROADMAP.md (mark feature complete)
5. Create CHANGELOG entry for v0.3.0

**Deliverables:**
- Full test suite
- User documentation
- Developer documentation
- Updated README

---

## Security Considerations

### Password Hashing
**Algorithm:** Argon2id (winner of Password Hashing Competition)
- **Parameters:**
  - Memory: 64 MB
  - Time cost: 3 iterations
  - Parallelism: 4 threads
  - Output: 32 bytes

**Why Argon2id:**
- Resistant to GPU/ASIC attacks
- Memory-hard (mitigates parallel attacks)
- NIST recommended (draft SP 800-63B)
- Better than bcrypt for new implementations

**Fallback:** bcrypt (if Argon2 not available)
- Cost factor: 12
- Still acceptable for user authentication

### Storage Security
**Where user data is stored:**
- Inside encrypted vault (VaultData.multiuser_config)
- Protected by master password + PBKDF2 + AES-256-GCM
- User passwords hashed with Argon2id (never plaintext)

**Key points:**
- Master password ≠ User password
- Master password unlocks vault (encryption key)
- User password authenticates identity (role-based access)
- Both are required for full access

### Attack Scenarios

#### 1. Compromised Master Password
**Impact:** Attacker can decrypt vault
**Mitigation:** User passwords still protect individual accounts if privacy enabled
**Note:** This is same risk as current system

#### 2. Compromised User Password
**Impact:** Attacker gains user's role permissions
**Mitigation:**
- Admin passwords should be strong
- Standard users have limited damage potential
- Audit trail logs all modifications

#### 3. Insider Threat (Malicious Admin)
**Impact:** Admin can do anything
**Mitigation:**
- Multiple admins for oversight
- Audit logging (future feature)
- Regular backups

#### 4. Brute Force User Password
**Impact:** Offline attack on password hash
**Mitigation:**
- Argon2id is brute-force resistant
- Enforce strong password policy
- Rate limiting in UI (not enforceable offline)

### Recommendations
1. **Enforce password policies:**
   - Minimum 12 characters for admin accounts
   - Minimum 8 characters for standard users
   - Complexity requirements (optional)

2. **Audit logging:** (Future feature)
   - Log all user actions with timestamps
   - Track account modifications
   - Admin-only visibility

3. **Password reset flow:**
   - Admin can reset user passwords
   - Require master password for admin resets
   - Log all password changes

4. **Session management:**
   - No persistent sessions (re-auth on vault open)
   - Session tied to vault lifetime
   - Lock vault = end session

---

## Testing Strategy

### Unit Tests
```cpp
// test_user_manager.cc
TEST(UserManager, HashPassword) {
    UserManager um;
    std::string hash = um.hash_password("TestPassword123");
    ASSERT_FALSE(hash.empty());
    ASSERT_TRUE(um.verify_password("TestPassword123", hash));
    ASSERT_FALSE(um.verify_password("WrongPassword", hash));
}

TEST(UserManager, AddUser) {
    UserManager um;
    ASSERT_TRUE(um.add_user("admin", "password", UserRole::ADMINISTRATOR));
    ASSERT_FALSE(um.add_user("admin", "password", UserRole::STANDARD));  // Duplicate
}

TEST(VaultSession, PermissionChecks) {
    VaultUser admin_user;
    admin_user.set_role(UserRole::ADMINISTRATOR);
    VaultSession admin_session(admin_user);
    ASSERT_TRUE(admin_session.can_modify_security_settings());

    VaultUser standard_user;
    standard_user.set_role(UserRole::STANDARD);
    VaultSession standard_session(standard_user);
    ASSERT_FALSE(standard_session.can_modify_security_settings());
}
```

### Integration Tests
```cpp
// test_multiuser_integration.cc
TEST(MultiUser, LegacyVaultCompat) {
    VaultManager vm;
    ASSERT_TRUE(vm.create_vault("test.vault", "password"));
    ASSERT_FALSE(vm.is_multiuser_enabled());
    ASSERT_TRUE(vm.is_legacy_vault());
}

TEST(MultiUser, EnableOnLegacyVault) {
    VaultManager vm;
    vm.open_vault("legacy.vault", "password");
    ASSERT_TRUE(vm.enable_multiuser("admin", "adminpass"));
    ASSERT_TRUE(vm.is_multiuser_enabled());
    vm.save_vault();

    // Reopen
    VaultManager vm2;
    vm2.open_vault("legacy.vault", "password");
    ASSERT_TRUE(vm2.is_multiuser_enabled());
}
```

### Manual Testing Checklist
- [ ] Open legacy vault → No user selection dialog
- [ ] Enable multi-user → Wizard completes successfully
- [ ] Reopen vault → User selection dialog appears
- [ ] Login as admin → All features enabled
- [ ] Login as standard user → Security tab locked
- [ ] Standard user tries export → Error message shown
- [ ] Admin adds new user → User appears in list
- [ ] Admin removes user → User disappears
- [ ] Try to remove last admin → Error prevents deletion
- [ ] Mark account private → Standard user can't see it
- [ ] Standard user deletes normal account → Succeeds
- [ ] Standard user deletes protected account → Fails

---

## Risk Assessment

### Technical Risks

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| Data corruption during migration | HIGH | LOW | Backup prompt, transaction-like save |
| Password hash collision | HIGH | VERY LOW | Use Argon2id with unique salts |
| Permission bypass bug | HIGH | MEDIUM | Thorough testing, code review |
| UI doesn't lock correctly | MEDIUM | MEDIUM | Comprehensive permission checks |
| Performance degradation | LOW | LOW | Argon2 parameters tuned for speed |

### User Experience Risks

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| Users confused by two-layer auth | MEDIUM | MEDIUM | Clear UI, help documentation |
| Lost user password | HIGH | MEDIUM | Admin password reset feature |
| Accidental lockout | HIGH | LOW | Prevent deleting last admin |
| Migration anxiety | MEDIUM | HIGH | Optional feature, clear benefits |

### Backward Compatibility Risks

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| Legacy vaults break | CRITICAL | VERY LOW | Extensive testing, schema version check |
| Old KeepTower versions can't open | HIGH | CERTAIN | Document version requirements |
| Export/import compatibility | MEDIUM | LOW | CSV export includes user info |

---

## Open Questions

### Design Decisions Needed

1. **User password requirements:**
   - Enforce minimum length? (Recommend: 12 for admin, 8 for standard)
   - Require complexity? (Uppercase, numbers, symbols?)
   - Allow password change by user? (Yes, but require old password)

2. **Password reset flow:**
   - Admin can reset any user password? (Yes)
   - Require master password for admin resets? (Recommend: Yes)
   - Notify user of password change? (Logging only)

3. **Session timeout:**
   - Keep session for vault lifetime? (Yes - matches auto-lock behavior)
   - Allow "switch user" without closing vault? (Future feature)

4. **Account privacy:**
   - Implement privacy flags in Phase 1 or defer? (Defer to Phase 6 - optional)
   - Allow per-field privacy? (Too complex for v1)

5. **Audit logging:**
   - Include in v0.3.0 or defer? (Defer to v0.3.1)
   - Store in VaultData or separate file? (VaultData - encrypted)

6. **Emergency access:**
   - Master password override for locked accounts? (Yes - admin can add new admin)
   - Recovery codes? (Future feature)

### Technical Decisions Needed

1. **Argon2 library:**
   - Use libsodium (includes Argon2id)? (Recommend: Yes)
   - Or standalone Argon2 reference implementation? (More lightweight)

2. **Schema version migration:**
   - Auto-migrate old schemas? (No - explicit user action only)
   - Support downgrade (v2 → v1)? (No - data loss, not recommended)

3. **User storage format:**
   - Store users in VaultData (field 48) or VaultMetadata (field 8)? (Recommend: VaultData field 48)

4. **Password policy enforcement:**
   - Enforce in UI only or also in VaultManager? (Both - defense in depth)

5. **Multi-vault support:**
   - Each vault has independent user list? (Yes - per-vault users)
   - Share users across vaults? (No - too complex)

---

## Success Criteria

### Functional Requirements
- ✅ Legacy vaults open without issues
- ✅ Multi-user can be enabled on legacy vaults
- ✅ Multi-user vaults require user authentication
- ✅ Admin users have full access
- ✅ Standard users are restricted from security settings and export
- ✅ Admins can add/remove users
- ✅ Cannot delete last admin
- ✅ Account privacy flags work (if implemented)

### Non-Functional Requirements
- ✅ No performance degradation (< 500ms overhead for user auth)
- ✅ No data loss during migration
- ✅ Backward compatible with v0.2.x vaults
- ✅ Clear user documentation
- ✅ Comprehensive test coverage (> 80%)

### User Acceptance
- ✅ Users understand two-layer authentication
- ✅ Migration flow is smooth and non-scary
- ✅ Administrators can manage team effectively
- ✅ Standard users have sufficient access for daily use
- ✅ No confusion about restrictions

---

## Timeline Estimate

| Phase | Duration | Dependencies |
|-------|----------|--------------|
| Phase 1: Protobuf Schema | 2 days | None |
| Phase 2: User Management Backend | 3-4 days | Phase 1 |
| Phase 3: User Selection Dialog | 2 days | Phase 2 |
| Phase 4: Permission Enforcement | 3-4 days | Phase 2, 3 |
| Phase 5: User Management Dialog | 2-3 days | Phase 2, 4 |
| Phase 6: Account Privacy (Optional) | 1-2 days | Phase 4 |
| Phase 7: Migration UI | 1-2 days | Phase 3, 4 |
| Phase 8: Testing & Docs | 2-3 days | All phases |
| **Total** | **16-22 days** | **~3 weeks** |

**Minimum viable product (MVP):** Phases 1-5, 7, 8 = **13-18 days (2-2.5 weeks)**

**Full feature set:** All phases = **16-22 days (2.5-3 weeks)**

---

## Next Steps

1. **Review this plan** - Discuss design decisions and risks
2. **Make key decisions** - Password policy, audit logging, privacy flags
3. **Choose Argon2 library** - libsodium vs standalone
4. **Create feature branch** - `git checkout -b feature/multiuser`
5. **Start Phase 1** - Implement protobuf schema changes
6. **Iterate and refine** - Adjust plan based on discoveries

---

**Document Status:** Draft for review
**Author:** GitHub Copilot (Claude Sonnet 4.5)
**Date:** 22 December 2025
**Version:** 1.0
