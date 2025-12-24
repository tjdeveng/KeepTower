# LUKS-Style Multi-User Architecture - Implementation Update

**Date:** 22 December 2025
**Purpose:** Revised implementation plan using LUKS-style key slots

---

## Summary of Changes

### Previous Approach (Two-Layer Authentication)
- ❌ Master password + user password (two authentication steps)
- ❌ User passwords stored as Argon2id hashes in VaultData
- ❌ Complex UX (unlock vault, then select user)
- ❌ Master password + YubiKey + user password = 3 factors

### New Approach (LUKS-Style Key Slots)
- ✅ Single authentication: username + password
- ✅ No password hashes stored (only wrapped keys)
- ✅ Simple UX (one login screen)
- ✅ FIPS-140-3 compliant (AES-256-KW for key wrapping)
- ✅ Each user can have optional YubiKey
- ✅ Matches LUKS/dm-crypt security model

---

## Key Differences

| Aspect | Old Design | New Design (LUKS-Style) |
|--------|------------|-------------------------|
| **Authentication** | Master password → User password | Username + password directly |
| **Key Derivation** | PBKDF2 → Vault KEK, Argon2 → User hash | PBKDF2 → User KEK → Unwrap DEK |
| **Password Storage** | Argon2 hashes in VaultData | None! Only wrapped DEKs |
| **Master Password** | Required for all users | No master password concept |
| **YubiKey** | Vault-level only | Vault-level policy (admin-defined) |
| **First Login** | N/A | Mandatory password change |
| **Adding User** | Add hash to VaultData | Add key slot with wrapped DEK |
| **Removing User** | Delete from VaultData | Mark key slot inactive |
| **Password Change** | Update Argon2 hash | Re-wrap DEK with new KEK |
| **UX Steps** | 2 (unlock + login) | 1 (username + password) |
| **FIPS Compliance** | Argon2 not FIPS-approved | AES-KW is FIPS-approved |

---

## Security Analysis

### LUKS Model Security
✅ **Industry standard** - Used in Linux disk encryption since 2004
✅ **Battle-tested** - Millions of deployments, extensively audited
✅ **NIST compliant** - AES-256-KW approved (SP 800-38F)
✅ **No password hashes** - Reduces attack surface
✅ **Perfect forward secrecy** - Revoking user doesn't require re-encryption

### Attack Resistance

**Brute Force Attack:**
- Attacker must try: PBKDF2 + AES-KW unwrap + verify vault decryption
- Each attempt requires ~100,000 PBKDF2 iterations + cryptographic ops
- Comparable security to current single-password design

**Offline Attack:**
- Attacker has vault file and tries to crack one user's password
- Must derive KEK, unwrap DEK, decrypt vault (3 steps)
- No easier than current master password attack

**Multi-User Weakness?**
- Q: "More users = more attack surface?"
- A: No! Each key slot is independent. Success with one user ≠ access to others' keys
- Vault data encrypted with same DEK regardless of which user unlocks it

**Key Slot Metadata Exposure:**
- Usernames stored in plaintext (necessary for login UI)
- Roles can be encrypted with KEK if desired
- No password information leaked

### FIPS-140-3 Compliance

**APPROVED Algorithms:**
- ✅ PBKDF2-HMAC-SHA256 (key derivation)
- ✅ AES-256-KW (key wrapping - RFC 3394, NIST SP 800-38F)
- ✅ AES-256-GCM (data encryption)
- ✅ HMAC-SHA1 (YubiKey challenge-response)

**NOT REQUIRED:**
- ❌ Argon2 (not NIST-approved, but better password hashing)
- ✅ Replaced with PBKDF2 + AES-KW (both FIPS-approved)

**Result:** LUKS approach maintains full FIPS-140-3 compliance!

---

## Vault Security Policy (Admin-Controlled)

### Design Principle: Admin Sets Security Baseline
**Key Concept:** The administrator who creates the vault establishes the security profile that ALL users must meet. Users cannot opt out of security requirements.

### Vault-Level Settings (Set at Creation)
```cpp
struct VaultSecurityPolicy {
    bool require_yubikey;           // If true, ALL users must have YubiKey
    uint32_t min_password_length;   // Minimum password length (default: 12)
    uint32_t pbkdf2_iterations;     // Key derivation work factor (default: 100000)
    std::array<uint8_t, 64> yubikey_challenge;  // Shared challenge for all users
};
```

**Why Vault-Level YubiKey?**
- ✅ **Consistent security:** All users meet same authentication standard
- ✅ **Simpler management:** One YubiKey configuration for entire vault
- ✅ **Matches enterprise use:** IT sets policy, users comply
- ✅ **Like LUKS:** dm-crypt doesn't have per-user YubiKey settings
- ✅ **Easier backup:** Admin can configure backup YubiKeys for all users

**Shared YubiKey Challenge:**
- All users use the SAME challenge (like LUKS)
- Each user's YubiKey must be programmed with same HMAC secret
- Challenge-response is XORed with user's KEK
- Admin can program multiple YubiKeys (primary + backups)

### User Provisioning Workflow

#### 1. Admin Creates Vault (First Time)
```
1. Admin chooses security policy:
   ┌───────────────────────────────────────────┐
   │ Create New Vault                          │
   ├───────────────────────────────────────────┤
   │ Vault Name: [Company Passwords]           │
   │                                           │
   │ Security Policy:                          │
   │ [✓] Require YubiKey for all users        │
   │     Insert YubiKey to configure...        │
   │                                           │
   │ Min Password Length: [12______]           │
   │ PBKDF2 Iterations:  [100000___]          │
   │                                           │
   │ Your Administrator Account:               │
   │ Username: [alice_________________]        │
   │ Password: [**********************]        │
   │ Confirm:  [**********************]        │
   │                                           │
   │          [Cancel]  [Create Vault]         │
   └───────────────────────────────────────────┘

2. If YubiKey required:
   - Admin inserts YubiKey
   - System generates random challenge
   - Gets response from YubiKey (HMAC-SHA1)
   - Stores challenge in vault security policy

3. Admin's KEK = PBKDF2(password, salt) XOR yubikey_response
4. DEK wrapped with admin's KEK
5. First key slot created (admin, role=ADMINISTRATOR)
```

#### 2. Admin Adds New User
```
1. Admin selects "Add User" from menu

2. Dialog prompts for user details:
   ┌───────────────────────────────────────────┐
   │ Add User                                  │
   ├───────────────────────────────────────────┤
   │ Username: [bob___________________]        │
   │                                           │
   │ Temporary Password:                       │
   │ [ ] Auto-generate (recommended)           │
   │ Password: [**********************]        │
   │ Confirm:  [**********************]        │
   │                                           │
   │ ⓘ User must change password on first     │
   │   login                                   │
   │                                           │
   │ Role: [Standard User ▼]                  │
   │                                           │
   │ ⚠️  YubiKey required for this vault       │
   │    Give user a YubiKey programmed with:   │
   │    Challenge: a3f5...b2c1 (show full)     │
   │                                           │
   │          [Cancel]  [Add User]             │
   └───────────────────────────────────────────┘

3. System creates key slot for new user:
   - username = "bob"
   - Generates unique salt
   - Wraps DEK with KEK derived from temporary password
   - Sets must_change_password = true

4. Admin communicates to user:
   - Username: bob
   - Temporary password: [shown once or sent securely]
   - YubiKey serial: [if vault requires YubiKey]
```

#### 3. User's First Login
```
1. User enters username + temporary password (+ YubiKey if required)

2. Vault unlocks successfully

3. System detects must_change_password = true

4. Before showing vault, force password change:
   ┌───────────────────────────────────────────┐
   │ Password Change Required                  │
   ├───────────────────────────────────────────┤
   │ You must change your temporary password   │
   │ before accessing the vault.               │
   │                                           │
   │ Current Password: [**********************]│
   │                                           │
   │ New Password:     [____________________]  │
   │ Confirm New:      [____________________]  │
   │                                           │
   │ Password Requirements:                    │
   │ • Minimum 12 characters                   │
   │ • Use mix of letters, numbers, symbols    │
   │                                           │
   │              [Change Password]            │
   └───────────────────────────────────────────┘

5. System validates new password meets policy

6. Re-wraps DEK with new KEK (from new password)

7. Sets must_change_password = false

8. User gains access to vault
```

### Security Benefits

**Admin Control:**
- ✅ IT/security team sets vault-wide policy
- ✅ All users must comply (no opt-out)
- ✅ YubiKey requirement enforced uniformly
- ✅ Password standards enforced uniformly

**User Safety:**
- ✅ Temporary passwords prevent long-term exposure
- ✅ Forced password change on first use
- ✅ User creates their own strong password
- ✅ Admin never knows user's final password

**Operational:**
- ✅ Easy YubiKey deployment (same challenge for all)
- ✅ Admin can issue pre-programmed YubiKeys
- ✅ Backup YubiKeys work for all users
- ✅ Simpler troubleshooting (one configuration)

---

## Implementation Phases (Revised)

### Phase 1: Key Slot Infrastructure (3-4 days)
**Goal:** Implement binary header with key slots

**Tasks:**
1. Define vault security policy and key slot structures:
   ```cpp
   // Vault-level security policy (set by admin at creation)
   struct VaultSecurityPolicy {
       bool require_yubikey;           // Admin-defined: all users need YubiKey
       uint32_t min_password_length;   // Minimum password length (default: 12)
       uint32_t pbkdf2_iterations;     // Key derivation iterations
       std::array<uint8_t, 64> yubikey_challenge;  // Shared challenge (if required)
   };

   struct KeySlot {
       bool active;
       std::string username;
       std::array<uint8_t, 32> salt;           // Per-user salt
       std::array<uint8_t, 40> wrapped_dek;    // AES-KW output
       UserRole role;
       bool must_change_password;               // Force password change on next login
       int64_t password_changed_at;             // Timestamp of last password change
       int64_t last_login_at;                   // Last successful login
   };
   ```

2. Implement AES-256-KW (key wrapping):
   ```cpp
   // OpenSSL provides EVP_aes_256_wrap()
   bool wrap_key(const uint8_t* kek, const uint8_t* dek, uint8_t* wrapped);
   bool unwrap_key(const uint8_t* kek, const uint8_t* wrapped, uint8_t* dek);
   ```

3. Update VaultManager:
   ```cpp
   class VaultManager {
       // Key slot management
       std::vector<KeySlot> m_key_slots;
       std::array<uint8_t, 32> m_dek;  // Master data encryption key

       // Load key slots from binary header
       bool load_key_slots(std::istream& file);

       // Save key slots to binary header
       bool save_key_slots(std::ostream& file);

       // Find key slot by username
       std::optional<KeySlot> find_key_slot(const std::string& username);
   };
   ```

4. Implement vault version detection:
   ```cpp
   enum class VaultVersion {
       V1_LEGACY,    // Single password, direct PBKDF2
       V2_MULTIUSER  // Key slots with wrapped DEKs
   };

   VaultVersion detect_vault_version(const std::string& path);
   ```

**Deliverables:**
- KeySlot structure
- AES-KW wrapper functions
- Binary header read/write
- Version detection

**Testing:**
- Write key slot → Read key slot → Data matches
- Wrap DEK → Unwrap DEK → DEK recovered correctly
- Detect V1 vault → Returns V1_LEGACY
- Detect V2 vault → Returns V2_MULTIUSER

---

### Phase 2: Multi-User Authentication (3-4 days)
**Goal:** Implement LUKS-style authentication flow

**Tasks:**
1. Update `open_vault()` for V2 vaults with vault-level security policy:
   ```cpp
   bool VaultManager::open_vault_v2(const std::string& path,
                                     const std::string& username,
                                     const std::string& password) {
       // 1. Load vault security policy and key slots from file
       if (!load_vault_header(file, m_security_policy, m_key_slots)) return false;

       // 2. Find user's key slot
       auto slot = find_key_slot(username);
       if (!slot) {
           set_error("User not found");
           return false;
       }

       // 3. Derive KEK from password
       std::array<uint8_t, 32> kek;
       if (!derive_key(password, slot->salt, m_security_policy.pbkdf2_iterations, kek)) {
           return false;
       }

       // 4. Check vault-level YubiKey requirement (admin-defined policy)
       if (m_security_policy.require_yubikey) {
           auto response = yubikey_challenge_response(m_security_policy.yubikey_challenge);
           if (!response) {
               set_error("YubiKey required but not present");
               return false;
           }
           // Mix YubiKey response into KEK
           for (size_t i = 0; i < 32; ++i) {
               kek[i] ^= (*response)[i];
           }
       }

       // 5. Unwrap DEK
       if (!unwrap_key(kek.data(), slot->wrapped_dek.data(), m_dek.data())) {
           set_error("Incorrect password");
           return false;
       }

       // 6. Decrypt vault data with DEK
       if (!decrypt_vault_data(m_dek)) return false;

       // 7. Create session with user's role
       m_session = std::make_unique<VaultSession>(username, slot->role);

       // 8. Update last login timestamp
       slot->last_login_at = time(nullptr);
       m_vault_modified = true;

       // 9. Check if password change required (first login with admin-set password)
       if (slot->must_change_password) {
           m_session->set_password_change_required(true);
           // UI will prompt for password change before allowing vault access
       }

       return true;
   }
   ```

2. Implement legacy vault support:
   ```cpp
   bool VaultManager::open_vault_v1_legacy(const std::string& path,
                                             const std::string& password) {
       // Current implementation (unchanged)
       // Derive key directly, decrypt vault
       // Create admin session automatically
   }
   ```

3. Add user management methods with temporary password support:
   ```cpp
   // Add new user with admin-set temporary password (admin only)
   // User MUST change password on first login
   bool add_user(const std::string& username,
                 const std::string& temporary_password,
                 UserRole role) {
       // Validation
       if (!m_session || !m_session->is_admin()) {
           set_error("Administrator access required");
           return false;
       }

       if (temporary_password.length() < m_security_policy.min_password_length) {
           set_error("Password too short");
           return false;
       }

       if (find_key_slot(username).has_value()) {
           set_error("Username already exists");
           return false;
       }

       // Create new key slot
       KeySlot slot;
       slot.active = true;
       slot.username = username;
       generate_random(slot.salt.data(), 32);  // Unique salt for this user
       slot.role = role;
       slot.must_change_password = true;       // Force password change on first login
       slot.password_changed_at = 0;           // Never changed (uses temp password)
       slot.last_login_at = 0;                 // Never logged in

       // Derive KEK from temporary password
       std::array<uint8_t, 32> kek;
       derive_key(temporary_password, slot.salt, m_security_policy.pbkdf2_iterations, kek);

       // Wrap DEK with KEK
       wrap_key(kek.data(), m_dek.data(), slot.wrapped_dek.data());

       // Add to key slots
       m_key_slots.push_back(slot);
       m_vault_modified = true;

       return true;
   }

   // Remove user (admin only, prevent last admin)
   bool remove_user(const std::string& username) {
       if (!m_session || !m_session->is_admin()) {
           set_error("Administrator access required");
           return false;
       }

       // Count active admin users
       int admin_count = 0;
       for (const auto& slot : m_key_slots) {
           if (slot.active && slot.role == UserRole::ADMINISTRATOR) {
               admin_count++;
           }
       }

       // Find target user
       auto slot_it = std::find_if(m_key_slots.begin(), m_key_slots.end(),
           [&username](const KeySlot& s) { return s.username == username && s.active; });

       if (slot_it == m_key_slots.end()) {
           set_error("User not found");
           return false;
       }

       // Prevent deleting last admin
       if (slot_it->role == UserRole::ADMINISTRATOR && admin_count <= 1) {
           set_error("Cannot delete last administrator");
           return false;
       }

       // Mark slot inactive (don't remove - preserves audit trail)
       slot_it->active = false;
       m_vault_modified = true;

       return true;
   }

   // Change user password (self-service or admin)
   bool change_user_password(const std::string& username,
                              const std::string& old_password,
                              const std::string& new_password) {
       // Permission check: user can change own password, admin can change any
       if (m_session->username() != username && !m_session->is_admin()) {
           set_error("Permission denied");
           return false;
       }

       if (new_password.length() < m_security_policy.min_password_length) {
           set_error("New password too short");
           return false;
       }

       auto slot = find_key_slot(username);
       if (!slot) {
           set_error("User not found");
           return false;
       }

       // Verify old password (except when admin is resetting)
       if (m_session->username() == username || !m_session->is_admin()) {
           std::array<uint8_t, 32> old_kek;
           derive_key(old_password, slot->salt, m_security_policy.pbkdf2_iterations, old_kek);

           // Try to unwrap DEK to verify password
           std::array<uint8_t, 32> test_dek;
           if (!unwrap_key(old_kek.data(), slot->wrapped_dek.data(), test_dek.data())) {
               set_error("Incorrect old password");
               return false;
           }
       }

       // Generate new salt (best practice)
       generate_random(slot->salt.data(), 32);

       // Derive new KEK
       std::array<uint8_t, 32> new_kek;
       derive_key(new_password, slot->salt, m_security_policy.pbkdf2_iterations, new_kek);

       // Re-wrap DEK with new KEK
       wrap_key(new_kek.data(), m_dek.data(), slot->wrapped_dek.data());

       // Update metadata
       slot->must_change_password = false;
       slot->password_changed_at = time(nullptr);
       m_vault_modified = true;

       // Update session if user changed own password
       if (m_session->is_password_change_required() && m_session->username() == username) {
           m_session->set_password_change_required(false);
       }

       return true;
   }

   // Get list of usernames (for login UI)
   std::vector<std::string> get_usernames() const {
       std::vector<std::string> usernames;
       for (const auto& slot : m_key_slots) {
           if (slot.active) {
               usernames.push_back(slot.username);
           }
       }
       return usernames;
   }
   ```

4. Implement vault conversion (V1 → V2):
   ```cpp
   bool convert_to_multiuser(const std::string& username,
                              const std::string& current_password) {
       // 1. Vault must be open (already unlocked with current password)
       if (!is_vault_open()) return false;

       // 2. Generate new random DEK
       std::array<uint8_t, 32> new_dek;
       if (!generate_random(new_dek.data(), 32)) return false;

       // 3. Re-encrypt vault data with new DEK
       auto decrypted_data = m_vault_data;  // Already decrypted
       if (!encrypt_vault_data_with_dek(decrypted_data, new_dek)) return false;

       // 4. Create first key slot
       KeySlot first_slot;
       first_slot.active = true;
       first_slot.username = username;
       generate_random(first_slot.salt.data(), 32);
       first_slot.pbkdf2_iterations = m_pbkdf2_iterations;
       first_slot.role = UserRole::ADMINISTRATOR;

       // 5. Derive KEK from current password
       std::array<uint8_t, 32> kek;
       derive_key(current_password, first_slot.salt, kek);

       // 6. Wrap new DEK
       wrap_key(kek.data(), new_dek.data(), first_slot.wrapped_dek.data());

       // 7. Add key slot and update version
       m_key_slots.push_back(first_slot);
       m_dek = new_dek;
       m_vault_version = VaultVersion::V2_MULTIUSER;

       // 8. Mark modified
       m_vault_modified = true;

       return true;
   }
   ```

**Deliverables:**
- V2 authentication flow
- V1 legacy support (unchanged)
- User management functions
- Vault conversion function

**Testing:**
- Open V2 vault with correct password → Success
- Open V2 vault with wrong password → Fails
- Open V1 vault → Works as before (legacy mode)
- Convert V1 → V2 → Reopen as V2 → Success
- Add second user → Both users can unlock vault
- Remove user → That user can't unlock, others still work

---

### Phase 3: User Authentication Dialog (2 days)
**Goal:** Create UI for username + password entry

**Tasks:**
1. Create `UserAuthDialog` class:
   ```cpp
   class UserAuthDialog : public Gtk::Dialog {
   public:
       UserAuthDialog(Gtk::Window& parent,
                      const std::vector<std::string>& usernames);

       std::string get_username() const;
       std::string get_password() const;

   private:
       Gtk::ComboBoxText m_username_combo;  // Dropdown of usernames
       Gtk::Entry m_password_entry;
       Gtk::CheckButton m_remember_username;
       Gtk::Button m_unlock_button;
   };
   ```

2. Update MainWindow vault opening flow:
   ```cpp
   bool MainWindow::open_vault(const std::string& path) {
       // 1. Detect vault version
       auto version = m_vault_manager->detect_vault_version(path);

       if (version == VaultVersion::V1_LEGACY) {
           // Show password dialog (current behavior)
           PasswordDialog dialog(*this);
           if (dialog.run() != Gtk::ResponseType::OK) return false;

           std::string password = dialog.get_password();
           if (!m_vault_manager->open_vault_v1_legacy(path, password)) {
               show_error("Incorrect password");
               return false;
           }

           // Legacy mode: Show multi-user prompt
           show_multiuser_conversion_banner();

       } else {  // V2_MULTIUSER
           // Show user authentication dialog
           auto usernames = m_vault_manager->get_usernames_from_file(path);
           UserAuthDialog dialog(*this, usernames);

           if (dialog.run() != Gtk::ResponseType::OK) return false;

           std::string username = dialog.get_username();
           std::string password = dialog.get_password();

           if (!m_vault_manager->open_vault_v2(path, username, password)) {
               show_error("Incorrect username or password");
               return false;
           }
       }

       // Vault is now open
       update_ui_for_session();
       return true;
   }
   ```

**Deliverables:**
- UserAuthDialog UI
- Integration with MainWindow
- Version detection and routing

**Testing:**
- Open V1 vault → Password dialog shown
- Open V2 vault → User auth dialog shown with usernames
- Select user + enter password → Vault unlocks
- Cancel dialog → Vault remains closed

---

### Phase 4: Permission Enforcement (2-3 days)
**Goal:** Lock UI elements based on user role (unchanged from original plan)

---

### Phase 5: User Management Dialog (2-3 days)
**Goal:** Admin interface for managing key slots (modified for key slots)

**Tasks:**
1. Update UserManagementDialog to show key slots:
   - Display username, role, YubiKey status
   - "Add User" creates new key slot
   - "Remove User" deactivates key slot
   - "Reset Password" re-wraps DEK with new KEK

2. Add user flow:
   ```cpp
   void UserManagementDialog::on_add_user() {
       // Get new username and password
       AddUserDialog dialog(*this);
       if (dialog.run() != Gtk::ResponseType::OK) return;

       std::string username = dialog.get_username();
       std::string password = dialog.get_password();
       UserRole role = dialog.get_role();

       // VaultManager wraps DEK for new user
       if (!m_vault_manager->add_user(username, password, role)) {
           show_error("Failed to add user");
           return;
       }

       // Refresh list
       reload_user_list();
       mark_vault_modified();
   }
   ```

---

### Phase 6-8: Same as Original Plan
- Phase 6: Account Privacy (optional)
- Phase 7: Migration UI
- Phase 8: Testing & Documentation

---

## Benefits of LUKS Approach

### User Experience
✅ **Simpler:** One authentication step instead of two
✅ **Familiar:** Matches password manager UX (1Password, Bitwarden)
✅ **Flexible:** Each user can have YubiKey or not
✅ **Secure:** No "master password" to forget or share

### Security
✅ **No password hashes:** Can't crack hashes offline
✅ **Per-user salts:** Rainbow tables useless
✅ **FIPS-compliant:** All algorithms NIST-approved
✅ **Industry standard:** LUKS proven in production

### Implementation
✅ **Binary header only:** No protobuf changes needed
✅ **Cleaner code:** No Argon2 dependency
✅ **Better separation:** Key management separate from data

### Maintenance
✅ **User changes cheap:** Re-wrap DEK, don't re-encrypt vault
✅ **Password reset simple:** Generate new KEK, re-wrap DEK
✅ **User removal instant:** Mark key slot inactive

---

## Updated Timeline

| Phase | Old Duration | New Duration | Change |
|-------|--------------|--------------|--------|
| Phase 1: Schema | 2 days | 3-4 days | +1-2 days (key slots more complex) |
| Phase 2: Backend | 3-4 days | 3-4 days | Same (removed Argon2, added AES-KW) |
| Phase 3: UI | 2 days | 2 days | Same (simpler dialog) |
| Phase 4: Permissions | 3-4 days | 2-3 days | -1 day (no two-layer auth) |
| Phase 5: Management | 2-3 days | 2-3 days | Same |
| Phases 6-8 | 4-7 days | 4-7 days | Same |
| **Total** | **16-22 days** | **16-23 days** | ~Same |

**Net change:** Approximately same timeline, much better architecture!

---

## Conclusion

The LUKS-style key slot approach with vault-level security policy is **superior** to the two-layer authentication design:

**User Experience:**
- ✅ Single authentication step (username + password + optional YubiKey)
- ✅ Clear security requirements set by admin
- ✅ Safe onboarding with temporary passwords

**Security:**
- ✅ No password hashes stored (only wrapped DEKs)
- ✅ Vault-level security policy (admin-controlled)
- ✅ Mandatory password change on first use
- ✅ All users meet same security standard
- ✅ FIPS-140-3 compliant (all algorithms NIST-approved)

**Management:**
- ✅ Admin sets vault security profile (YubiKey, password policy)
- ✅ Simplified YubiKey deployment (shared challenge)
- ✅ User provisioning with temporary passwords
- ✅ Industry-proven design (LUKS/dm-crypt model)

**Implementation:**
- ✅ Same timeline as two-layer approach (~3 weeks)
- ✅ Cleaner architecture (binary header for key slots)
- ✅ Easier to maintain (re-wrap DEK vs re-encrypt vault)

**Recommendation:** Proceed with LUKS-style key slots with vault-level security policy.

**Key Design Decisions:**
1. ✅ YubiKey requirement is vault-wide (admin sets at creation)
2. ✅ All users share same YubiKey challenge
3. ✅ Users added with temporary passwords (must change on first login)
4. ✅ Admin cannot know users' final passwords (user sets during first login)
5. ✅ Security policy enforced uniformly (no per-user opt-outs)
