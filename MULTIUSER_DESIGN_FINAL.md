# Multi-User Architecture - Key Design Decisions

**Date:** 22 December 2025
**Status:** Final Architecture Approved

---

## Core Design: LUKS-Style Key Slots

### Single Authentication
- Username + Password (+ optional YubiKey)
- No "master password" - each user directly unlocks vault
- One authentication step (not two-layer)

### Key Wrapping Model
```
User Password → PBKDF2 → KEK (Key Encryption Key)
KEK + YubiKey response → Unwraps → DEK (Data Encryption Key)
DEK → Decrypts → Vault Data
```

- Each user has unique salt and wrapped DEK
- All users unwrap to SAME DEK
- Adding/removing users = add/remove key slots (vault data unchanged)

---

## Vault-Level Security Policy (Admin-Controlled)

### Admin Sets Security Baseline at Vault Creation
```cpp
struct VaultSecurityPolicy {
    bool require_yubikey;           // If true, ALL users must use YubiKey
    uint32_t min_password_length;   // Minimum password length (default: 12)
    uint32_t pbkdf2_iterations;     // Key derivation iterations (default: 100000)
    std::array<uint8_t, 64> yubikey_challenge;  // Shared challenge for all users
};
```

### Why Vault-Level YubiKey?
✅ Consistent security standard for all users
✅ Simpler management (one configuration)
✅ Matches enterprise IT policies
✅ Like LUKS (no per-user YubiKey settings)
✅ Easier backup YubiKey deployment

### Shared YubiKey Challenge
- All users use SAME challenge (stored in vault security policy)
- Each user's YubiKey programmed with same HMAC secret
- Challenge-response XORed with user's KEK
- Admin can program multiple backup YubiKeys

---

## User Provisioning Workflow

### 1. Admin Creates Vault
```
1. Admin sets security policy:
   - Require YubiKey? (yes/no)
   - Min password length (default: 12)
   - PBKDF2 iterations (default: 100000)

2. If YubiKey required:
   - Insert YubiKey
   - Generate random challenge
   - Get HMAC-SHA1 response
   - Store challenge in vault policy

3. Admin creates own account:
   - Choose username
   - Set password (no temporary - admin knows own password)
   - Role = ADMINISTRATOR
```

### 2. Admin Adds New User
```
1. Admin provides:
   - Username (must be unique)
   - Temporary password (auto-generated or manual)
   - Role (Administrator or Standard User)

2. System creates key slot:
   - Unique salt generated
   - DEK wrapped with KEK from temporary password
   - must_change_password = true
   - password_changed_at = 0

3. Admin communicates to user:
   - Username: [username]
   - Temporary password: [shown once or sent securely]
   - YubiKey serial: [if vault requires YubiKey]
```

### 3. User's First Login
```
1. User enters username + temporary password (+ YubiKey if required)
2. Vault unlocks
3. System detects must_change_password = true
4. BEFORE accessing vault, force password change dialog:
   - Current password: [temporary password]
   - New password: [user sets own password]
   - Confirm new password

5. System validates new password meets policy
6. Re-wraps DEK with new KEK (from new password)
7. Sets must_change_password = false
8. Sets password_changed_at = current timestamp
9. User gains full vault access
```

---

## Security Benefits

### Admin Control
✅ IT/security team sets vault-wide security policy
✅ All users must comply (no opt-outs)
✅ YubiKey requirement enforced uniformly
✅ Password standards enforced uniformly

### User Safety
✅ Temporary passwords prevent long-term exposure
✅ Forced password change on first use
✅ User creates own strong password
✅ Admin never knows user's final password

### Operational
✅ Easy YubiKey deployment (same challenge for all)
✅ Admin can issue pre-programmed YubiKeys
✅ Backup YubiKeys work for all users
✅ Simpler troubleshooting (one configuration)

---

## Key Slot Structure

```cpp
struct KeySlot {
    bool active;                        // Slot in use
    std::string username;               // User identifier (plaintext)
    std::array<uint8_t, 32> salt;       // Unique per-user salt
    std::array<uint8_t, 40> wrapped_dek; // AES-KW wrapped DEK (40 bytes)
    UserRole role;                      // ADMINISTRATOR or STANDARD_USER
    bool must_change_password;          // Force password change on next login
    int64_t password_changed_at;        // Timestamp of last password change
    int64_t last_login_at;              // Last successful login timestamp
};
```

**Stored in:** Binary vault header (not in protobuf)
**Max slots:** 32 (configurable)

---

## FIPS-140-3 Compliance

All algorithms are NIST-approved:

| Algorithm | Purpose | NIST Status |
|-----------|---------|-------------|
| PBKDF2-HMAC-SHA256 | Key derivation from password | Approved |
| AES-256-KW | Key wrapping (RFC 3394) | Approved (SP 800-38F) |
| AES-256-GCM | Vault data encryption | Approved |
| HMAC-SHA1 | YubiKey challenge-response | Approved |

**Result:** Full FIPS-140-3 compliance maintained.

---

## Implementation Summary

### Phase 1: Key Slot Infrastructure (3-4 days)
- Binary header with vault security policy
- Key slot read/write
- AES-256-KW wrapping/unwrapping
- Version detection (V1 legacy vs V2 multi-user)

### Phase 2: Authentication (3-4 days)
- V2 vault authentication with key slots
- Vault security policy enforcement
- User management (add/remove/password change)
- Temporary password support

### Phase 3: UI (2 days)
- User authentication dialog (username + password)
- Password change dialog (forced on first login)
- Vault creation wizard (security policy selection)

### Phase 4: Permissions (2-3 days)
- Lock UI based on role
- Admin vs Standard User restrictions

### Phase 5: User Management (2-3 days)
- User management dialog
- Add user with temporary password
- Remove user (prevent last admin)
- Reset user password

### Phases 6-8: Polish (4-7 days)
- Account privacy (optional)
- Migration UI (legacy → multi-user)
- Testing & documentation

**Total: 16-23 days (~3 weeks)**

---

## Key Design Decisions (Final)

1. ✅ **YubiKey is vault-wide:** Admin sets at creation, applies to ALL users
2. ✅ **Shared YubiKey challenge:** All users use same challenge (easier management)
3. ✅ **Temporary passwords:** Users added with admin-set temp password
4. ✅ **Mandatory password change:** User MUST change password on first login
5. ✅ **Admin never knows final passwords:** Users set during first login
6. ✅ **Security policy enforced uniformly:** No per-user opt-outs
7. ✅ **FIPS-140-3 maintained:** All algorithms NIST-approved
8. ✅ **LUKS-style architecture:** Industry-proven key slot model

---

## Advantages Over Two-Layer Design

| Aspect | Two-Layer | LUKS-Style (Final) |
|--------|-----------|-------------------|
| **Authentication steps** | 2 (master + user) | 1 (username + password) |
| **Password storage** | Argon2 hashes | None (only wrapped keys) |
| **YubiKey config** | Vault-level | Vault-level (same) |
| **User passwords** | Set by user | Temporary → user changes |
| **Security policy** | Per-user options | Vault-wide (admin-controlled) |
| **FIPS compliance** | Argon2 not approved | All algorithms approved |
| **Management** | Complex (2 auth flows) | Simple (1 auth flow) |
| **UX** | Unlock → Select user → Login | Username + password → Unlock |

**Winner:** LUKS-style with vault-level policy

---

## Next Steps

1. ✅ Architecture approved
2. ⏭️ Create feature branch: `git checkout -b feature/multiuser-luks`
3. ⏭️ Start Phase 1: Binary header and key slot infrastructure
4. ⏭️ Implement AES-256-KW wrapping (OpenSSL EVP API)
5. ⏭️ Add vault security policy structure
6. ⏭️ Proceed through phases 2-8

**Target:** KeepTower v0.3.0-beta (Q1 2026)
