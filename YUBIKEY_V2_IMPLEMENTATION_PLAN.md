# YubiKey V2 Implementation Plan

## Overview
Implement YubiKey HMAC-SHA1 challenge-response support for V2 multi-user vaults, mirroring the V1 implementation but adapted for the key slot architecture.

## Current State

### ✅ V1 Implementation (Reference)
- **Location**: `VaultManager.cc` lines 100-220
- **Enrollment**: During vault creation
- **Challenge**: Random 20-byte value stored in vault
- **KEK Derivation**: `KEK_final = KEK_password XOR YubiKey_Response`
- **Storage**: Challenge stored in vault metadata (protobuf)
- **Change Password**: Challenge persists, user gets YubiKey prompt

### ❌ V2 Status (NOT Implemented)
- **TODO markers**:
  - `VaultManagerV2.cc:93` - Vault creation enrollment
  - `VaultManagerV2.cc:261-262` - Authentication integration
- **Infrastructure**:
  - ✅ `KeyWrapping::combine_with_yubikey()` exists
  - ✅ YubiKey manager available
  - ❌ No challenge storage in KeySlot structure
  - ❌ No UI integration

## Architecture Decisions

### Challenge Storage Strategy

**Option 1: Global Challenge (V1 Style)**
- Single challenge stored in V2VaultHeader
- All users share same challenge
- Simpler implementation
- Matches LUKS2 YubiKey integration pattern
- **Pro**: One YubiKey protects entire vault
- **Pro**: Straightforward enrollment during creation
- **Con**: All users must use same YubiKey ❌ Doesn't work for distributed teams

**Option 2: Per-User Challenge** ⭐ SELECTED
- Each KeySlot stores own YubiKey challenge
- Allows different YubiKeys per user
- **Pro**: Multi-YubiKey support (field engineers in different locations)
- **Pro**: Admin doesn't need user's YubiKey
- **Pro**: User controls their own 2FA enrollment
- **Pro**: Matches V1 multi-YubiKey backup capability
- **Con**: Slightly more complex enrollment workflow
- **Con**: More data in header (~20 bytes per active user)

**DECISION**: Use Option 2 (Per-User Challenge) for distributed team support.

**Use Case**: Field engineers at different sites each have their own YubiKey. Admin creates account with temporary password, user enrolls their own YubiKey on first login.

### Security Policy Integration

The vault security policy controls whether YubiKey is **required** or **optional**:

```cpp
struct VaultSecurityPolicy {
    uint32_t min_password_length = 8;
    uint32_t pbkdf2_iterations = 100000;
    bool require_yubikey = false;           // If true, ALL users must have YubiKey
    // Note: yubikey_challenge is per-user, stored in KeySlot
};
```

**Policy Options**:
- `require_yubikey = false`: YubiKey optional, users choose during enrollment
- `require_yubikey = true`: YubiKey mandatory, enforced at user creation

### KeySlot Structure Update

Each user's KeySlot stores their own YubiKey data:

```cpp
struct KeySlot {
    bool active = false;
    std::string username;
    std::array<uint8_t, 32> salt = {};
    std::array<uint8_t, 40> wrapped_dek = {};
    UserRole role = UserRole::STANDARD_USER;
    bool must_change_password = false;
    int64_t password_changed_at = 0;
    int64_t last_login_at = 0;

    // YubiKey per-user enrollment
    bool yubikey_enrolled = false;                      // ← ADD: Has this user enrolled YubiKey?
    std::array<uint8_t, 20> yubikey_challenge = {};     // ← ADD: User's unique challenge
    std::string yubikey_serial;                         // ← ADD: Device serial (optional)
    int64_t yubikey_enrolled_at = 0;                    // ← ADD: Enrollment timestamp

    // Serialization methods
    std::vector<uint8_t> serialize() const;
    static std::optional<std::pair<KeySlot, size_t>> deserialize(...);
    size_t calculate_serialized_size() const;
};
```

## Implementation Tasks

### Phase 1: Core Infrastructure (Priority: HIGH)

#### Task 1.1: Add YubiKey Fields to KeySlot
**File**: `src/core/MultiUserTypes.h`

Add per-user YubiKey fields to KeySlot structure:

```cpp
struct KeySlot {
    bool active = false;
    std::string username;
    std::array<uint8_t, 32> salt = {};
    std::array<uint8_t, 40> wrapped_dek = {};
    UserRole role = UserRole::STANDARD_USER;
    bool must_change_password = false;
    int64_t password_changed_at = 0;
    int64_t last_login_at = 0;

    // YubiKey per-user enrollment (NEW)
    bool yubikey_enrolled = false;                   // Has user enrolled YubiKey?
    std::array<uint8_t, 20> yubikey_challenge = {};  // User's unique 20-byte challenge
    std::string yubikey_serial;                      // Device serial (optional, for audit)
    int64_t yubikey_enrolled_at = 0;                 // Enrollment timestamp

    std::vector<uint8_t> serialize() const;
    static std::optional<std::pair<KeySlot, size_t>> deserialize(...);
    size_t calculate_serialized_size() const;
};
```

**Update serialization in `MultiUserTypes.cc`**:
- Add 4 new fields to `serialize()`: yubikey_enrolled (1 byte), challenge (20 bytes), serial (variable), timestamp (8 bytes)
- Update `deserialize()` to read new fields (with version check for backward compatibility)
- Update `calculate_serialized_size()` to account for new fields

**Backward Compatibility**:
- Old vaults (without YubiKey fields): All fields default to empty/false
- New vaults: Fields populated when user enrolls YubiKey

#### Task 1.2: Admin Creates Vault with Optional YubiKey
**File**: `src/core/VaultManagerV2.cc` - Line 93 (create_vault_v2)

**Workflow**: Admin creating vault can optionally enroll their own YubiKey.

```cpp
// Current code derives KEK from admin password
auto kek_result = KeyWrapping::derive_kek_from_password(
    admin_password,
    salt_result.value(),
    policy.pbkdf2_iterations);
if (!kek_result) {
    Log::error("VaultManager: Failed to derive KEK from admin password");
    return std::unexpected(VaultError::CryptoError);
}

std::array<uint8_t, 32> final_kek = kek_result.value();

// NEW: Admin YubiKey enrollment (optional, based on policy.require_yubikey flag)
bool admin_yubikey_enrolled = false;
std::array<uint8_t, 20> admin_yubikey_challenge = {};
std::string admin_yubikey_serial;

#ifdef HAVE_YUBIKEY_SUPPORT
if (policy.require_yubikey) {
    Log::info("VaultManager: Enrolling admin's YubiKey");

    // Generate unique challenge for admin
    auto challenge_salt = KeyWrapping::generate_random_salt();
    if (!challenge_salt) {
        Log::error("VaultManager: Failed to generate YubiKey challenge");
        return std::unexpected(VaultError::CryptoError);
    }

    // Use first 20 bytes as challenge
    std::copy_n(challenge_salt.value().begin(), 20, admin_yubikey_challenge.begin());

    // Get YubiKey response
    YubiKeyManager yk_manager;
    if (!yk_manager.initialize()) {
        Log::error("VaultManager: Failed to initialize YubiKey");
        return std::unexpected(VaultError::YubiKeyError);
    }

    if (!yk_manager.is_yubikey_present()) {
        Log::error("VaultManager: YubiKey not present but required");
        return std::unexpected(VaultError::YubiKeyNotPresent);
    }

    auto response = yk_manager.challenge_response(
        std::span<const unsigned char>(admin_yubikey_challenge.data(),
                                      admin_yubikey_challenge.size()),
        false,  // don't require touch for vault operations
        5000    // 5 second timeout
    );

    if (!response.success) {
        Log::error("VaultManager: YubiKey challenge-response failed: {}",
                   response.error_message);
        return std::unexpected(VaultError::YubiKeyError);
    }

    // Get device serial for audit trail
    auto device_info = yk_manager.get_device_info();
    if (device_info) {
        admin_yubikey_serial = std::to_string(device_info->serial_number);
        Log::info("VaultManager: Admin YubiKey enrolled with serial: {}",
                  admin_yubikey_serial);
    }

    // Combine KEK with YubiKey response
    std::array<uint8_t, 20> yk_response_array;
    std::copy_n(response.response.begin(), 20, yk_response_array.begin());
    final_kek = KeyWrapping::combine_with_yubikey(final_kek, yk_response_array);

    admin_yubikey_enrolled = true;
    Log::info("VaultManager: Admin KEK combined with YubiKey response");
}
#else
if (policy.require_yubikey) {
    Log::error("VaultManager: YubiKey support not compiled in");
    return std::unexpected(VaultError::YubiKeyError);
}
#endif

// Wrap DEK with admin's final KEK (password or password+YubiKey)
auto wrapped_result = KeyWrapping::wrap_key(final_kek, m_v2_dek);
if (!wrapped_result) {
    Log::error("VaultManager: Failed to wrap DEK with admin KEK");
    return std::unexpected(VaultError::CryptoError);
}

// Create admin key slot (NOW WITH YUBIKEY FIELDS)
KeySlot admin_slot;
admin_slot.active = true;
admin_slot.username = admin_username.raw();
admin_slot.salt = salt_result.value();
admin_slot.wrapped_dek = wrapped_result.value().wrapped_key;
admin_slot.role = UserRole::ADMINISTRATOR;
admin_slot.must_change_password = false;
admin_slot.password_changed_at = std::chrono::system_clock::now().time_since_epoch().count();
// NEW: YubiKey enrollment data
admin_slot.yubikey_enrolled = admin_yubikey_enrolled;
admin_slot.yubikey_challenge = admin_yubikey_challenge;
admin_slot.yubikey_serial = admin_yubikey_serial;
if (admin_yubikey_enrolled) {
    admin_slot.yubikey_enrolled_at = std::chrono::system_clock::now().time_since_epoch().count();
}
```

#### Task 1.3: Implement Authentication with Per-User YubiKey
**File**: `src/core/VaultManagerV2.cc` - Line 261 (open_vault_v2)

**Workflow**: Check if THIS user has YubiKey enrolled, if so require it.

```cpp
// Derive KEK from password
auto kek_result = KeyWrapping::derive_kek_from_password(
    password,
    user_slot->salt,
    file_header.pbkdf2_iterations);
if (!kek_result) {
    Log::error("VaultManager: Failed to derive KEK");
    return std::unexpected(VaultError::CryptoError);
}

std::array<uint8_t, 32> final_kek = kek_result.value();

// NEW: Check if THIS user has YubiKey enrolled
#ifdef HAVE_YUBIKEY_SUPPORT
if (user_slot->yubikey_enrolled) {
    Log::info("VaultManager: User {} has YubiKey enrolled, requiring device",
              username.raw());

    YubiKeyManager yk_manager;
    if (!yk_manager.initialize()) {
        Log::error("VaultManager: Failed to initialize YubiKey");
        return std::unexpected(VaultError::YubiKeyError);
    }

    if (!yk_manager.is_yubikey_present()) {
        Log::error("VaultManager: YubiKey not present but required for user {}",
                   username.raw());
        return std::unexpected(VaultError::YubiKeyNotPresent);
    }

    // Optional: Verify YubiKey serial matches enrolled device
    if (!user_slot->yubikey_serial.empty()) {
        auto device_info = yk_manager.get_device_info();
        if (device_info) {
            std::string current_serial = std::to_string(device_info->serial_number);
            if (current_serial != user_slot->yubikey_serial) {
                Log::warning("VaultManager: YubiKey serial mismatch - expected: {}, got: {}",
                           user_slot->yubikey_serial, current_serial);
                // Don't fail here - serial is informational, challenge-response is the auth
            }
        }
    }

    // Use user's unique challenge
    const auto& challenge = user_slot->yubikey_challenge;

    auto response = yk_manager.challenge_response(
        std::span<const unsigned char>(challenge.data(), challenge.size()),
        false,  // don't require touch
        5000    // 5 second timeout
    );

    if (!response.success) {
        Log::error("VaultManager: YubiKey challenge-response failed: {}",
                   response.error_message);
        return std::unexpected(VaultError::YubiKeyError);
    }

    // Combine KEK with YubiKey response
    std::array<uint8_t, 20> yk_response_array;
    std::copy_n(response.response.begin(), 20, yk_response_array.begin());
    final_kek = KeyWrapping::combine_with_yubikey(final_kek, yk_response_array);

    Log::info("VaultManager: YubiKey authentication successful for user {}",
              username.raw());
}
#endif

// Unwrap DEK (verifies password correctness, and YubiKey if enrolled)
auto unwrap_result = KeyWrapping::unwrap_key(final_kek, user_slot->wrapped_dek);
if (!unwrap_result) {
    if (user_slot->yubikey_enrolled) {
        Log::error("VaultManager: Failed to unwrap DEK - incorrect password or YubiKey");
    } else {
        Log::error("VaultManager: Failed to unwrap DEK - incorrect password");
    }
    return std::unexpected(VaultError::AuthenticationFailed);
}
#### Task 2.1: Update change_user_password() for YubiKey
**File**: `src/core/VaultManagerV2.cc` - Line 530 (change_user_password)

**Issue**: When user changes password, they need YubiKey for:
1. Unwrapping DEK with old password+YubiKey
2. Wrapping DEK with new password+YubiKey

**Solution**: Add YubiKey integration to both steps:

```cpp
// Verify old password by unwrapping DEK
auto old_kek_result = KeyWrapping::derive_kek_from_password(
    old_password,
    user_slot->salt,
    m_v2_header->security_policy.pbkdf2_iterations);

std::array<uint8_t, 32> old_final_kek = old_kek_result.value();

#ifdef HAVE_YUBIKEY_SUPPORT
if (m_v2_header->security_policy.require_yubikey) {
    // Get YubiKey response for old password verification
    YubiKeyManager yk_manager;
    if (!yk_manager.initialize() || !yk_manager.is_yubikey_present()) {
        return std::unexpected(VaultError::YubiKeyNotPresent);
    }

    const auto& challenge = m_v2_header->security_policy.yubikey_challenge;
    auto response = yk_manager.challenge_response(
        std::span<const unsigned char>(challenge.data(), challenge.size()),
        false, 5000);

    if (!response.success) {
        return std::unexpected(VaultError::YubiKeyError);
    }

    std::array<uint8_t, 20> yk_response_array;
    std::copy_n(response.response.begin(), 20, yk_response_array.begin());
    old_final_kek = KeyWrapping::combine_with_yubikey(old_final_kek, yk_response_array);
}
#endif

auto verify_unwrap = KeyWrapping::unwrap_key(old_final_kek, user_slot->wrapped_dek);
// ... verify old password ...

// Derive new KEK and wrap DEK
auto new_kek_result = KeyWrapping::derive_kek_from_password(
    new_password,
    new_salt_result.value(),
    m_v2_header->security_policy.pbkdf2_iterations);

std::array<uint8_t, 32> new_final_kek = new_kek_result.value();

#ifdef HAVE_YUBIKEY_SUPPORT
if (m_v2_header->security_policy.require_yubikey) {
    // Use SAME YubiKey response for new password
    // (Challenge hasn't changed, just re-deriving with new password)
    YubiKeyManager yk_manager;
    yk_manager.initialize();

    const auto& challenge = m_v2_header->security_policy.yubikey_challenge;
    auto response = yk_manager.challenge_response(
        std::span<const unsigned char>(challenge.data(), challenge.size()),
        false, 5000);

    std::array<uint8_t, 20> yk_response_array;
    std::copy_n(response.response.begin(), 20, yk_response_array.begin());
    new_final_kek = KeyWrapping::combine_with_yubikey(new_final_kek, yk_response_array);
}
#endif

auto new_wrap_result = KeyWrapping::wrap_key(new_final_kek, m_v2_dek);
```

**UPDATE THIS SECTION** - Replace with per-user YubiKey logic:

```cpp
// Verify old password by unwrapping DEK
auto old_kek_result = KeyWrapping::derive_kek_from_password(
    old_password,
    user_slot->salt,
    m_v2_header->security_policy.pbkdf2_iterations);

std::array<uint8_t, 32> old_final_kek = old_kek_result.value();

#ifdef HAVE_YUBIKEY_SUPPORT
if (user_slot->yubikey_enrolled) {  // ← CHECK PER-USER, NOT POLICY
    Log::info("VaultManager: User has YubiKey enrolled, requiring for password verification");

    YubiKeyManager yk_manager;
    if (!yk_manager.initialize() || !yk_manager.is_yubikey_present()) {
        Log::error("VaultManager: YubiKey required but not present");
        return std::unexpected(VaultError::YubiKeyNotPresent);
    }

    const auto& challenge = user_slot->yubikey_challenge;  // ← USER'S CHALLENGE
    auto response = yk_manager.challenge_response(
        std::span<const unsigned char>(challenge.data(), challenge.size()),
        false, 5000);

    if (!response.success) {
        Log::error("VaultManager: YubiKey challenge-response failed");
        return std::unexpected(VaultError::YubiKeyError);
    }

    std::array<uint8_t, 20> yk_response_array;
    std::copy_n(response.response.begin(), 20, yk_response_array.begin());
    old_final_kek = KeyWrapping::combine_with_yubikey(old_final_kek, yk_response_array);
}
#endif

// Verify old password+YubiKey
auto verify_unwrap = KeyWrapping::unwrap_key(old_final_kek, user_slot->wrapped_dek);
// ... (rest of password change logic) ...
```

#### Task 2.2: Add YubiKey Enrollment Method
**New Method**: `enroll_yubikey_for_user(username, password)`

Users can enroll their own YubiKey after account creation:

```cpp
KeepTower::VaultResult<> VaultManager::enroll_yubikey_for_user(
    const Glib::ustring& username,
    const Glib::ustring& password) {

    // Permission check: user enrolling own YubiKey OR admin
    bool is_self = (m_current_session && m_current_session->username == username.raw());
    bool is_admin = (m_current_session && m_current_session->role == UserRole::ADMINISTRATOR);
    if (!is_self && !is_admin) {
        return std::unexpected(VaultError::PermissionDenied);
    }

    // Find user slot
    KeySlot* user_slot = nullptr;
    for (auto& slot : m_v2_header->key_slots) {
        if (slot.active && slot.username == username.raw()) {
            user_slot = &slot;
            break;
        }
    }

    if (!user_slot) {
        return std::unexpected(VaultError::UserNotFound);
    }

    if (user_slot->yubikey_enrolled) {
        Log::warning("VaultManager: User already has YubiKey enrolled");
        return std::unexpected(VaultError::AlreadyEnrolled);
    }

#ifdef HAVE_YUBIKEY_SUPPORT
    // Generate unique challenge for this user
    auto challenge_result = KeyWrapping::generate_random_salt();
    if (!challenge_result) {
        return std::unexpected(VaultError::CryptoError);
    }

    std::array<uint8_t, 20> yubikey_challenge;
    std::copy_n(challenge_result.value().begin(), 20, yubikey_challenge.begin());

    // Get YubiKey response
    YubiKeyManager yk_manager;
    if (!yk_manager.initialize() || !yk_manager.is_yubikey_present()) {
        return std::unexpected(VaultError::YubiKeyNotPresent);
    }

    auto response = yk_manager.challenge_response(
        std::span<const unsigned char>(yubikey_challenge.data(), yubikey_challenge.size()),
        true,  // require touch for enrollment security
        10000  // 10 second timeout
    );

    if (!response.success) {
        return std::unexpected(VaultError::YubiKeyError);
    }

    // Get device serial for audit
    std::string yubikey_serial;
    auto device_info = yk_manager.get_device_info();
    if (device_info) {
        yubikey_serial = std::to_string(device_info->serial_number);
    }

    // Re-wrap DEK with password+YubiKey
    // 1. Verify current password and unwrap DEK (password-only)
    auto kek_result = KeyWrapping::derive_kek_from_password(
        password, user_slot->salt, m_v2_header->security_policy.pbkdf2_iterations);

    auto unwrap_result = KeyWrapping::unwrap_key(kek_result.value(), user_slot->wrapped_dek);
    if (!unwrap_result) {
        Log::error("VaultManager: Password verification failed during YubiKey enrollment");
        return std::unexpected(VaultError::AuthenticationFailed);
    }

    // 2. Combine KEK with YubiKey response
    std::array<uint8_t, 20> yk_response_array;
    std::copy_n(response.response.begin(), 20, yk_response_array.begin());
    auto final_kek = KeyWrapping::combine_with_yubikey(kek_result.value(), yk_response_array);

    // 3. Re-wrap DEK with password+YubiKey
    auto wrap_result = KeyWrapping::wrap_key(final_kek, unwrap_result.value().dek);
    if (!wrap_result) {
        return std::unexpected(VaultError::CryptoError);
    }

    // 4. Update key slot
    user_slot->wrapped_dek = wrap_result.value().wrapped_key;
    user_slot->yubikey_enrolled = true;
    user_slot->yubikey_challenge = yubikey_challenge;
    user_slot->yubikey_serial = yubikey_serial;
    user_slot->yubikey_enrolled_at = std::chrono::system_clock::now().time_since_epoch().count();

    m_modified = true;
    Log::info("VaultManager: YubiKey enrolled for user: {}", username.raw());
    return {};
#else
    return std::unexpected(VaultError::YubiKeyError);
#endif
}
```

#### Task 2.3: Add YubiKey Unenrollment Method (Optional)
**New Method**: `unenroll_yubikey_for_user(username, password)`

Allow users to remove YubiKey protection:

```cpp
KeepTower::VaultResult<> VaultManager::unenroll_yubikey_for_user(
    const Glib::ustring& username,
    const Glib::ustring& password) {

    // Permission check
    bool is_self = (m_current_session && m_current_session->username == username.raw());
    bool is_admin = (m_current_session && m_current_session->role == UserRole::ADMINISTRATOR);
    if (!is_self && !is_admin) {
        return std::unexpected(VaultError::PermissionDenied);
    }

    // Check policy: if vault requires YubiKey, can't unenroll
    if (m_v2_header->security_policy.require_yubikey) {
        Log::error("VaultManager: Cannot unenroll YubiKey - vault policy requires it");
        return std::unexpected(VaultError::PolicyViolation);
    }

    KeySlot* user_slot = /* find slot */;

    if (!user_slot->yubikey_enrolled) {
        return std::unexpected(VaultError::YubiKeyNotEnrolled);
    }

#ifdef HAVE_YUBIKEY_SUPPORT
    YubiKeyManager yk_manager;
    yk_manager.initialize();

    // 1. Unwrap DEK with current password+YubiKey
    auto kek_result = KeyWrapping::derive_kek_from_password(
        password, user_slot->salt, m_v2_header->security_policy.pbkdf2_iterations);

    auto response = yk_manager.challenge_response(/* user's challenge */);
    std::array<uint8_t, 20> yk_response_array;
    std::copy_n(response.response.begin(), 20, yk_response_array.begin());
    auto current_kek = KeyWrapping::combine_with_yubikey(kek_result.value(), yk_response_array);

    auto unwrap_result = KeyWrapping::unwrap_key(current_kek, user_slot->wrapped_dek);
    if (!unwrap_result) {
        return std::unexpected(VaultError::AuthenticationFailed);
    }

    // 2. Re-wrap DEK with password-only (no YubiKey)
    auto wrap_result = KeyWrapping::wrap_key(kek_result.value(), unwrap_result.value().dek);
    if (!wrap_result) {
        return std::unexpected(VaultError::CryptoError);
    }

    // 3. Update key slot - remove YubiKey
    user_slot->wrapped_dek = wrap_result.value().wrapped_key;
    user_slot->yubikey_enrolled = false;
    user_slot->yubikey_challenge = {};
    user_slot->yubikey_serial = "";
    user_slot->yubikey_enrolled_at = 0;

    m_modified = true;
    Log::info("VaultManager: YubiKey unenrolled for user: {}", username.raw());
    return {};
#endif
}
```

### Phase 3: User Management Integration (Priority: MEDIUM)

#### Task 3.1: Update add_user() for YubiKey Vaults
**File**: `src/core/VaultManagerV2.cc` - `add_user()`

**Important**: Admin creates user WITHOUT YubiKey. User enrolls their own YubiKey later.

**Workflow**:
1. Admin creates user with temporary password
2. User logs in with temporary password (no YubiKey yet)
3. User sets their own password (must_change_password flow)
4. User optionally enrolls their own YubiKey

```cpp
// In add_user() - Create new user WITHOUT YubiKey
// (User will enroll their own device later)

auto kek_result = KeyWrapping::derive_kek_from_password(
    temp_password,
    new_salt.value(),
    m_v2_header->security_policy.pbkdf2_iterations);

// DO NOT combine with YubiKey here - user doesn't have their device yet
// Admin can't enroll YubiKey for user (they don't have user's physical device)

auto wrap_result = KeyWrapping::wrap_key(kek_result.value(), m_v2_dek);

// Create new key slot
KeySlot new_slot;
new_slot.active = true;
new_slot.username = new_username.raw();
new_slot.salt = new_salt.value();
new_slot.wrapped_dek = wrap_result.value().wrapped_key;
new_slot.role = new_role;
new_slot.must_change_password = true;  // User must change temp password
new_slot.password_changed_at = 0;       // Not set yet
new_slot.yubikey_enrolled = false;      // User will enroll own YubiKey later
new_slot.yubikey_challenge = {};
new_slot.yubikey_serial = "";
new_slot.yubikey_enrolled_at = 0;

Log::info("VaultManager: Created user {} without YubiKey (user will enroll own device)",
          new_username.raw());
```

**Policy Enforcement**:
- If `require_yubikey = true`: User MUST enroll YubiKey before accessing vault
- Check in `open_vault_v2()`: If policy requires YubiKey and user not enrolled → block access

#### Task 3.2: Enforce YubiKey Policy at Login
**File**: `src/core/VaultManagerV2.cc` - `open_vault_v2()`

After successful authentication, check if user meets YubiKey requirements:

```cpp
// After unwrapping DEK successfully
auto unwrap_result = KeyWrapping::unwrap_key(final_kek, user_slot->wrapped_dek);
if (!unwrap_result) {
    return std::unexpected(VaultError::AuthenticationFailed);
}

m_v2_dek = unwrap_result.value().dek;

// NEW: Check YubiKey policy compliance
if (m_v2_header->security_policy.require_yubikey && !user_slot->yubikey_enrolled) {
    Log::warning("VaultManager: User {} must enroll YubiKey (vault policy)", username.raw());

    // Create session but flag as incomplete
    UserSession session;
    session.username = username.raw();
    session.role = user_slot->role;
    session.requires_yubikey_enrollment = true;  // ← NEW FLAG

    m_current_session = session;
    m_is_v2_vault = true;
    m_vault_open = true;

    // Return success BUT with special status
    return session;  // UI will show "Enroll YubiKey" dialog
}

// Normal session creation
UserSession session;
session.username = username.raw();
session.role = user_slot->role;
session.requires_yubikey_enrollment = false;
// ... etc ...
```

#### Task 3.3: Add YubiKey Enrollment Dialog Flow
**New UI Flow**: After login, if `requires_yubikey_enrollment = true`:

1. Show dialog: "Vault Policy Requires YubiKey"
2. Prompt user to connect their YubiKey
3. Call `enroll_yubikey_for_user(username, password)`
4. On success: Save vault, reload session, grant full access
5. On failure: Keep vault locked, allow retry

```cpp
enum class VaultError {
    // ... existing errors ...
    YubiKeyError,           // YubiKey operation failed
    YubiKeyNotPresent,      // YubiKey required but not connected
    YubiKeyWrongDevice,     // Different YubiKey serial detected
};
```

#### Task 4.2: Update MainWindow.cc YubiKey Prompts
**File**: `src/ui/windows/MainWindow.cc`

The YubiKey touch dialog is already implemented for vault creation (lines 570-580).
Needs to be added for:
- Vault opening (V2UserLoginDialog integration)
- Password changes (ChangePasswordDialog integration)

**V2UserLoginDialog Integration:**
```cpp
// After user enters password but before calling open_vault_v2:
if (vault_requires_yubikey(vault_path)) {
    auto touch_dialog = Gtk::make_managed<YubiKeyPromptDialog>(*this,
        YubiKeyPromptDialog::PromptType::TOUCH);
    touch_dialog->present();
    // ... wait briefly for user to touch YubiKey ...
    touch_dialog->hide();
}

auto result = vault_manager->open_vault_v2(path, username, password, "");
```

#### Task 4.3: Update ChangePasswordDialog for YubiKey
**File**: `src/ui/dialogs/ChangePasswordDialog.cc`

Add YubiKey warning label when vault requires YubiKey:
```cpp
if (vault_requires_yubikey) {
    auto* yk_label = Gtk::make_managed<Gtk::Label>();
    yk_label->set_markup("<b>Note:</b> This vault requires YubiKey. "
                         "Ensure your YubiKey is connected during password change.");
    yk_label->set_wrap(true);
    content_box->append(*yk_label);
}
```

### Phase 5: Testing (Priority: HIGH)

#### Test Case 1: YubiKey Vault Creation
```cpp
// Test creating V2 vault with YubiKey
VaultSecurityPolicy policy;
policy.require_yubikey = true;
auto result = vm.create_vault_v2("test.vault", "admin", "password", policy);
EXPECT_TRUE(result);
EXPECT_TRUE(policy.yubikey_challenge.size() == 20);
EXPECT_FALSE(policy.yubikey_serial.empty());
```

#### Test Case 2: YubiKey Authentication
```cpp
// Test opening YubiKey-protected vault
auto result = vm.open_vault_v2("test.vault", "admin", "password", "");
EXPECT_TRUE(result);
EXPECT_EQ(result->role, UserRole::ADMINISTRATOR);
```

#### Test Case 3: Password Change with YubiKey
```cpp
// Test changing password on YubiKey vault
auto result = vm.change_user_password("admin", "old_pwd", "new_pwd");
EXPECT_TRUE(result);

// Verify can login with new password
vm.close_vault();
auto login = vm.open_vault_v2("test.vault", "admin", "new_pwd", "");
EXPECT_TRUE(login);
```

#### Test Case 4: YubiKey Not Present Error
```cpp
// Test error when YubiKey required but not connected
// (Requires mocking YubiKeyManager)
auto result = vm.open_vault_v2("yubikey_vault.vault", "admin", "password", "");
EXPECT_FALSE(result);
EXPECT_EQ(result.error(), VaultError::YubiKeyNotPresent);
```

## Implementation Order

### Sprint 1 (Core Functionality)
1. ✅ Fix dialog workflow (DONE)
2. Add YubiKey fields to VaultSecurityPolicy
3. Update serialization/deserialization
4. Implement vault creation enrollment (Task 1.2)
5. Implement authentication integration (Task 1.3)
6. Basic testing

### Sprint 2 (Password Changes)
7. Implement change_user_password YubiKey support (Task 2.1)
8. Add YubiKey UI hints to ChangePasswordDialog
9. Test password change workflow

### Sprint 3 (User Management)
10. Implement add_user YubiKey support (Task 3.1)
11. Add YubiKey detection to V2UserLoginDialog
12. Test multi-user workflows

### Sprint 4 (Polish & Testing)
13. Add comprehensive error messages
14. Add YubiKey serial verification (optional)
15. Update documentation
16. Integration testing
17. Manual testing with real YubiKey

## Security Considerations

### ✅ Benefits
- **Two-factor authentication**: Password + physical device
- **Matches V1 behavior**: Consistent user experience
- **FIPS-compliant**: Uses approved HMAC-SHA1
- **No password in plaintext**: Challenge-response keeps secrets secure

### ⚠️ Limitations (Same as V1)
- **Single YubiKey**: All users share same YubiKey (Phase 1)
- **No backup YubiKey**: Losing YubiKey = vault inaccessible
- **No YubiKey rotation**: Challenge is permanent for vault lifetime
- **Touch not required**: Touch disabled for usability

### 🔮 Future Enhancements
- Per-user YubiKey challenges (Option 2 architecture)
- Multiple enrolled YubiKeys per vault (backup device)
- YubiKey serial verification and rotation
- Touch requirement for sensitive operations
- FIDO2/WebAuthn integration

## Compatibility

### Forward Compatibility
- V2 vaults without YubiKey: `require_yubikey = false`, challenge empty
- V2 vaults with YubiKey: `require_yubikey = true`, challenge stored
- New fields ignored by older versions (graceful degradation)

### Migration Path
- V1 → V2 conversion: YubiKey flag NOT preserved (by design)
- Reason: Different architecture (global KEK vs per-user KEK)
- User must manually enable YubiKey after migration if desired

## Risk Analysis

| Risk | Severity | Mitigation |
|------|----------|------------|
| YubiKey lost | HIGH | Document backup procedures, add multi-YubiKey support in future |
| Challenge corruption | MEDIUM | Reed-Solomon FEC protection on header |
| Wrong YubiKey used | LOW | Serial verification (optional feature) |
| Performance impact | LOW | Challenge-response is ~50ms, acceptable |
| Implementation bugs | MEDIUM | Extensive testing, reference V1 code |

## Success Criteria

✅ **Minimum Viable Product (MVP)**:
1. Admin creates vault and optionally enrolls their own YubiKey ✓
2. Admin creates users with temporary passwords (no YubiKey) ✓
3. User logs in with temporary password ✓
4. User changes password and enrolls their own YubiKey ✓
5. User logs in with password + their YubiKey ✓
6. User changes password (YubiKey stays enrolled) ✓
7. Error handling for missing YubiKey ✓
8. Each user has their own unique YubiKey challenge ✓
9. Manual testing with multiple YubiKeys (different users) ✓

🎯 **Stretch Goals**:
- YubiKey serial verification for device mismatch detection
- Multiple YubiKey enrollment per user (backup device)
- Touch requirement toggle for sensitive operations
- YubiKey status indicator in UI (enrolled/not enrolled per user)
- Bulk YubiKey policy enforcement (force all users to enroll)
- YubiKey unenrollment (remove 2FA protection)

## Documentation Updates Required

1. **README.md**: Document YubiKey support for V2 vaults
2. **MULTIUSER_DESIGN_FINAL.md**: Add YubiKey architecture section
3. **User Guide**: YubiKey setup and troubleshooting
4. **API Documentation**: Document YubiKey-related error codes

## Timeline Estimate

- **Phase 1 (Core)**: 4-6 hours
- **Phase 2 (Password)**: 2-3 hours
- **Phase 3 (Users)**: 2-3 hours
- **Phase 4 (UI/Errors)**: 2-3 hours
- **Phase 5 (Testing)**: 3-4 hours

**Total**: ~13-19 hours development + testing time

---

## Complete Workflow Examples

### Workflow 1: Admin Creates Vault with YubiKey
1. Admin selects "Create New Vault"
2. Enter admin username and password in combined dialog
3. Check "Require YubiKey" checkbox
4. Click "Create Vault"
5. System generates unique challenge for admin
6. YubiKey touch dialog appears
7. Vault created with admin KeySlot.yubikey_enrolled = true
8. Admin can now login with password + YubiKey

### Workflow 2: Admin Creates User (Per-User YubiKey Model)
1. Admin opens "Manage Users" dialog
2. Click "Add User", enter username for new field engineer
3. System generates temporary password and shows it to admin
4. New KeySlot created with:
   - `must_change_password = true`
   - `yubikey_enrolled = false` ← User will enroll own device
5. Admin gives temp password to user (secure channel)

### Workflow 3: Field Engineer First Login + YubiKey Enrollment
1. User opens vault, enters username + temp password
2. System authenticates with password-only (no YubiKey yet)
3. Forced password change dialog appears
4. User enters temp password and sets new password
5. System saves new password
6. If vault policy.require_yubikey = true:
   - **"Enroll Your YubiKey" dialog appears**
   - Message: "This vault requires two-factor authentication"
   - User connects THEIR OWN YubiKey
   - User clicks "Enroll YubiKey"
   - System generates unique challenge for THIS user
   - YubiKey touch required (secure enrollment)
   - DEK re-wrapped with new_password + user's_yubikey
   - KeySlot updated:
     - `yubikey_enrolled = true`
     - `yubikey_challenge = <unique 20 bytes>`
     - `yubikey_serial = <device serial>`
7. User gains full vault access

### Workflow 4: Field Engineer Daily Login (Different YubiKeys)
**Engineer A in California**:
1. Enters username_a + password_a
2. System checks KeySlot_A: yubikey_enrolled = true
3. Prompts for YubiKey touch
4. Uses KeySlot_A.yubikey_challenge (unique to Engineer A)
5. YubiKey_A responds with HMAC-SHA1(challenge_A)
6. KEK_A = password_KEK_A XOR yubikey_response_A
7. Unwraps DEK → Access granted

**Engineer B in New York** (same vault, different YubiKey):
1. Enters username_b + password_b
2. System checks KeySlot_B: yubikey_enrolled = true
3. Prompts for YubiKey touch
4. Uses KeySlot_B.yubikey_challenge (unique to Engineer B)
5. YubiKey_B responds with HMAC-SHA1(challenge_B)
6. KEK_B = password_KEK_B XOR yubikey_response_B
7. Unwraps SAME DEK → Access granted

**Key Point**: Different users, different YubiKeys, different challenges, but all unwrap to the SAME DEK (shared vault data).

### Workflow 5: Optional YubiKey (Policy Doesn't Require)
1. Vault created with `policy.require_yubikey = false`
2. Admin creates user with temp password
3. User logs in, changes password
4. No forced YubiKey enrollment (policy allows password-only)
5. **Later**: User decides to add 2FA
6. User opens Tools → Security Settings
7. Click "Enroll YubiKey for Two-Factor Authentication"
8. Enter current password for verification
9. Connect YubiKey, touch for enrollment
10. System re-wraps user's DEK with password + YubiKey
11. KeySlot.yubikey_enrolled = true
12. Future logins require both password + YubiKey

### Workflow 6: Password Change with YubiKey Enrolled
1. User clicks "Change Password"
2. Enter old password
3. System derives old KEK from old password
4. **YubiKey prompt**: "Touch YubiKey to verify identity"
5. Challenge-response with user's challenge
6. Combine: old_KEK = old_password_KEK XOR yubikey_response
7. Unwrap DEK (verifies old password + YubiKey)
8. Enter new password
9. Derive new KEK from new password
10. **Same YubiKey, same challenge** (challenge never changes)
11. Challenge-response again
12. Combine: new_KEK = new_password_KEK XOR yubikey_response
13. Re-wrap DEK with new KEK
14. Save updated KeySlot (challenge stays same, wrapped_dek updated)

---

## Additional Considerations

### UserSession Structure Update

Add flag for YubiKey enrollment enforcement:

```cpp
struct UserSession {
    std::string username;
    UserRole role;
    bool requires_password_change = false;           // Existing
    bool requires_yubikey_enrollment = false;        // ← NEW: Policy requires but not enrolled
};
```

### VaultError Additions

```cpp
enum class VaultError {
    // ... existing errors ...
    YubiKeyError,            // YubiKey operation failed
    YubiKeyNotPresent,       // YubiKey required but not connected
    YubiKeyWrongDevice,      // Wrong YubiKey serial (optional check)
    YubiKeyNotEnrolled,      // User hasn't enrolled YubiKey yet
    PolicyViolation,         // Action violates vault security policy
};
```

---

## Next Steps

1. ✅ Review plan with per-user YubiKey architecture
2. Start with Phase 1, Task 1.1 (Add YubiKey fields to KeySlot)
3. Update KeySlot serialization
4. Implement vault creation with optional admin YubiKey enrollment
5. Implement per-user authentication with YubiKey check
6. Add enroll_yubikey_for_user() method
7. Test with multiple YubiKeys (simulate field engineers)

**Ready to proceed?** The updated plan now supports:
- ✅ Multiple field engineers with different YubiKeys
- ✅ Per-user unique challenges
- ✅ Admin can't enroll YubiKey for users (they need physical device)
- ✅ Optional vs required YubiKey policies
- ✅ Password changes preserve YubiKey enrollment

Let me know if you'd like me to start implementation!
