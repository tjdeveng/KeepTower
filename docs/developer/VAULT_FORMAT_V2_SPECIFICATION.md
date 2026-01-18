# KeepTower Vault Format V2 - Technical Specification

**Version:** 2.0
**Date:** 2026-01-18
**Status:** PRODUCTION
**Security Level:** FIPS 140-3 Compliant

---

## Table of Contents

1. [Overview](#overview)
2. [File Structure](#file-structure)
3. [Binary Format Specification](#binary-format-specification)
4. [Data Structures](#data-structures)
5. [Serialization Details](#serialization-details)
6. [FEC Protection](#fec-protection)
7. [Security Considerations](#security-considerations)
8. [Compatibility Notes](#compatibility-notes)

---

## Overview

### Purpose

The KeepTower Vault Format V2 is a FIPS 140-3 compliant binary file format designed for secure storage of encrypted password vault data with multi-user support, username hashing, and Forward Error Correction (FEC).

### Key Features

- **Multi-User Support**: LUKS-style key slots with independent user credentials
- **Username Hashing**: Cryptographic username hashing prevents enumeration attacks
- **FIPS 140-3 Compliance**: All cryptographic operations use FIPS-approved algorithms
- **Forward Error Correction**: Reed-Solomon encoding protects critical header data
- **YubiKey Integration**: FIDO2/WebAuthn hardware authentication support
- **Password History**: Prevents password reuse with configurable depth
- **Secure Serialization**: Plaintext usernames never written to disk

### Design Principles

1. **Defense in Depth**: Multiple layers of encryption and integrity protection
2. **Zero Trust**: Assume file can be read by attackers; rely on cryptographic strength
3. **Fail-Safe**: FEC ensures vault remains accessible even with minor corruption
4. **Audit Trail**: Timestamps and history support compliance requirements

---

## File Structure

### High-Level Layout

```
┌──────────────────────────────────────────────────────────┐
│ File Header (16 bytes)                                   │
│  - Magic Number (4 bytes)                                │
│  - Version (4 bytes)                                      │
│  - PBKDF2 Iterations (4 bytes)                           │
│  - Header Size (4 bytes)                                 │
├──────────────────────────────────────────────────────────┤
│ FEC-Protected Vault Header (variable size)               │
│  ┌────────────────────────────────────────────────────┐  │
│  │ FEC Metadata (5 bytes)                             │  │
│  │  - Redundancy Percentage (1 byte)                  │  │
│  │  - Original Header Size (4 bytes, big-endian)      │  │
│  ├────────────────────────────────────────────────────┤  │
│  │ Reed-Solomon Encoded Data (variable size)          │  │
│  │  ┌──────────────────────────────────────────────┐  │  │
│  │  │ Vault Header V2 (decoded, variable size)     │  │  │
│  │  │  - VaultSecurityPolicy (122 bytes)           │  │  │
│  │  │  - Key Slot Count (1 byte)                   │  │  │
│  │  │  - KeySlot[0] (variable size)                │  │  │
│  │  │  - KeySlot[1] (variable size)                │  │  │
│  │  │  - ... (up to 16 slots)                      │  │  │
│  │  └──────────────────────────────────────────────┘  │  │
│  └────────────────────────────────────────────────────┘  │
├──────────────────────────────────────────────────────────┤
│ Data IV (12 bytes)                                       │
├──────────────────────────────────────────────────────────┤
│ Encrypted Vault Data (variable size)                     │
│  - AES-256-GCM encrypted protobuf payload                │
│  - Contains accounts, groups, tags, metadata             │
│  - Optionally FEC-protected (user configurable)          │
└──────────────────────────────────────────────────────────┘
```

### Size Calculations

- **Minimum File Size**: ~650 bytes (empty vault, 1 user, no YubiKey)
- **Typical File Size**: 1-2 KB (2-3 users with YubiKey)
- **Maximum Header Size**: ~50 KB (16 users, full password history, YubiKey)

---

## Binary Format Specification

### File Header (16 bytes, little-endian)

| Offset | Size | Type     | Field             | Description                              |
|--------|------|----------|-------------------|------------------------------------------|
| 0x00   | 4    | uint32   | magic             | Magic number: `0x4B505457` ("WTPK")      |
| 0x04   | 4    | uint32   | version           | Format version: `0x00000002` (V2)        |
| 0x08   | 4    | uint32   | pbkdf2_iterations | PBKDF2 iterations (default: 100,000)     |
| 0x0C   | 4    | uint32   | header_size       | Total header section size (bytes)        |

**Notes:**
- All integers are **little-endian**
- `header_size` includes FEC metadata + encoded header data
- Magic number `0x4B505457` = "WTPK" in ASCII (reversed due to little-endian)

### FEC Protection Layer (5+ bytes)

| Offset | Size | Type     | Field              | Description                                    |
|--------|------|----------|--------------------|------------------------------------------------|
| 0x00   | 1    | uint8    | redundancy_percent | User's FEC preference (0-50%, 0=disabled)      |
| 0x01   | 4    | uint32   | original_size      | Original header size before encoding (big-endian) |
| 0x05   | var  | bytes    | encoded_data       | Reed-Solomon encoded header (data + parity)    |

**FEC Encoding Rules:**
- **Minimum redundancy**: 20% (for critical vault header, enforced)
- **User preference**: Stored in `redundancy_percent` but not enforced for header
- **Actual encoding**: `max(20%, redundancy_percent)` always applied to header
- **Original size**: Big-endian uint32, allows up to 4GB headers (theoretical)

**Example:**
- User sets 0% FEC → Header still encoded at 20%, stored as `0x00`
- User sets 30% FEC → Header encoded at 30%, stored as `0x1E`
- Original header: 524 bytes → Encoded size: 629 bytes (20% FEC)

---

## Data Structures

### VaultSecurityPolicy (122 bytes, fixed size)

Defines vault-wide security settings applied to all users.

| Offset | Size | Type      | Field                         | Description                              |
|--------|------|-----------|-------------------------------|------------------------------------------|
| 0x00   | 1    | bool      | require_yubikey               | `0x00`=optional, `0x01`=required         |
| 0x01   | 1    | uint8     | yubikey_algorithm             | `0x00`=legacy, `0x01`=FIPS HMAC-SHA256   |
| 0x02   | 4    | uint32    | min_password_length           | Minimum password length (big-endian)     |
| 0x06   | 4    | uint32    | max_password_age_days         | Password expiry (0=never, big-endian)    |
| 0x0A   | 1    | bool      | require_uppercase             | Require uppercase letters                |
| 0x0B   | 1    | bool      | require_lowercase             | Require lowercase letters                |
| 0x0C   | 1    | bool      | require_digit                 | Require digits                           |
| 0x0D   | 1    | bool      | require_special               | Require special characters               |
| 0x0E   | 4    | uint32    | pbkdf2_iterations             | PBKDF2 iterations (big-endian)           |
| 0x12   | 4    | uint32    | session_timeout_minutes       | Auto-lock timeout (big-endian)           |
| 0x16   | 1    | uint8     | password_history_depth        | Number of old passwords to remember      |
| 0x17   | 1    | bool      | fec_enabled                   | Enable FEC for data section              |
| 0x18   | 1    | uint8     | fec_redundancy_percent        | FEC redundancy (0-50%)                   |
| 0x19   | 4    | uint32    | vault_timeout_hours           | Vault auto-close timeout (big-endian)    |
| 0x1D   | 1    | bool      | clipboard_clear_enabled       | Auto-clear clipboard                     |
| 0x1E   | 4    | uint32    | clipboard_clear_seconds       | Clipboard timeout (big-endian)           |
| 0x22   | 1    | bool      | show_passwords_default        | Default password visibility              |
| 0x23   | 4    | uint32    | backup_retention_days         | Backup retention period (big-endian)     |
| 0x27   | 4    | uint32    | max_failed_attempts           | Max failed auth attempts (big-endian)    |
| 0x2B   | 4    | uint32    | lockout_duration_minutes      | Lockout duration (big-endian)            |
| 0x2F   | 1    | bool      | require_2fa_for_export        | Require 2FA for vault export             |
| 0x30   | 4    | uint32    | min_yubikey_touches           | Minimum YubiKey touches required         |
| 0x34   | 1    | bool      | allow_password_hints          | Allow password hints (discouraged)       |
| 0x35   | 4    | uint32    | max_hint_length               | Maximum hint length (big-endian)         |
| 0x39   | 1    | bool      | enforce_unique_passwords      | Enforce unique passwords per account     |
| 0x3A   | 8    | uint64    | created_at                    | Policy creation timestamp (big-endian)   |
| 0x42   | 8    | uint64    | modified_at                   | Last modification timestamp (big-endian) |
| 0x4A   | 32   | bytes     | reserved                      | Reserved for future use (zeros)          |
| 0x6A   | 1    | uint8     | data_fec_redundancy_percent   | Data section FEC redundancy              |
| 0x6B   | 1    | uint8     | fec_algorithm                 | `0x00`=Reed-Solomon                      |
| 0x6C   | 8    | uint64    | last_backup_at                | Last backup timestamp (big-endian)       |
| 0x74   | 1    | bool      | auto_backup_enabled           | Enable automatic backups                 |
| 0x75   | 4    | uint32    | auto_backup_interval_hours    | Backup interval (big-endian)             |
| 0x79   | 1    | uint8     | username_hash_algorithm       | **CRITICAL**: Username hash algorithm    |

**Username Hash Algorithm Values:**
- `0x00`: Plaintext (LEGACY, INSECURE - do not use)
- `0x01`: SHA3-256 (32 bytes)
- `0x02`: SHA3-384 (48 bytes)
- `0x03`: SHA3-512 (64 bytes)
- `0x04`: PBKDF2-SHA256 (32 bytes, 600k iterations)
- `0x05`: Argon2id (32 bytes, recommended)

**Total Size:** 122 bytes (0x7A)

### KeySlot Structure (variable size)

Represents a single user's authentication credentials and metadata.

#### Fixed-Size Fields (172 bytes base)

| Offset | Size | Type      | Field                  | Description                                    |
|--------|------|-----------|------------------------|------------------------------------------------|
| 0x00   | 1    | bool      | active                 | `0x00`=inactive, `0x01`=active                 |
| 0x01   | 64   | bytes     | username_hash          | Cryptographic hash of username                 |
| 0x41   | 1    | uint8     | username_hash_size     | Actual hash size used (1-64)                   |
| 0x42   | 16   | bytes     | username_salt          | Random salt for username hashing               |
| 0x52   | 32   | bytes     | salt                   | Random salt for password KEK derivation        |
| 0x72   | 40   | bytes     | wrapped_dek            | AES-256-GCM wrapped Data Encryption Key        |
| 0x9A   | 1    | uint8     | role                   | `0x00`=admin, `0x01`=standard                  |
| 0x9B   | 1    | bool      | must_change_password   | Force password change on next login            |
| 0x9C   | 8    | int64     | password_changed_at    | Unix timestamp (big-endian, seconds)           |
| 0xA4   | 8    | int64     | last_login_at          | Unix timestamp (big-endian, seconds)           |

#### YubiKey Fields (variable size)

| Offset | Size | Type      | Field                     | Description                                 |
|--------|------|-----------|---------------------------|---------------------------------------------|
| 0xAC   | 1    | bool      | yubikey_enrolled          | YubiKey enrollment status                   |
| 0xAD   | 32   | bytes     | yubikey_challenge         | FIDO2 HMAC-SHA256 challenge (32 bytes)      |
| 0xCD   | 1    | uint8     | yubikey_serial_length     | Length of serial string (0-255)             |
| 0xCE   | var  | string    | yubikey_serial            | Device path (e.g., "/dev/hidraw2")          |
| +N     | 8    | int64     | yubikey_enrolled_at       | Enrollment timestamp (big-endian)           |
| +8     | 2    | uint16    | encrypted_pin_length      | Encrypted PIN length (big-endian)           |
| +10    | var  | bytes     | yubikey_encrypted_pin     | AES-256-GCM encrypted FIDO2 PIN             |
| +M     | 2    | uint16    | credential_id_length      | FIDO2 credential ID length (big-endian)     |
| +2     | var  | bytes     | yubikey_credential_id     | FIDO2 credential ID (typically 48-128 bytes)|

**Notes:**
- `yubikey_serial` is **NOT** the username (security-critical distinction)
- Encrypted PIN format: `[IV(12)][ciphertext][auth_tag(16)]`
- Credential ID is opaque binary data from FIDO2 device

#### Password History (variable size)

| Offset | Size | Type      | Field                      | Description                              |
|--------|------|-----------|----------------------------|------------------------------------------|
| +N     | 1    | uint8     | password_history_count     | Number of history entries (0-255)        |
| +1     | var  | entries   | password_history_entries   | Array of PasswordHistoryEntry (88 bytes each) |

### PasswordHistoryEntry (88 bytes, fixed size)

| Offset | Size | Type      | Field         | Description                                      |
|--------|------|-----------|---------------|--------------------------------------------------|
| 0x00   | 8    | int64     | timestamp     | Password set timestamp (big-endian, seconds)     |
| 0x08   | 32   | bytes     | salt          | PBKDF2 salt (FIPS-approved DRBG)                 |
| 0x28   | 48   | bytes     | hash          | PBKDF2-HMAC-SHA512 hash (600k iterations)        |

**Total Size:** 88 bytes (0x58)

---

## Serialization Details

### VaultHeaderV2 Serialization Order

1. **VaultSecurityPolicy** (122 bytes) - serialize policy
2. **Key Slot Count** (1 byte) - number of slots (0-16)
3. **For each KeySlot** - serialize in order:
   - Fixed fields (172 bytes base)
   - YubiKey serial string (length-prefixed)
   - YubiKey enrollment timestamp
   - Encrypted PIN (length-prefixed)
   - Credential ID (length-prefixed)
   - Password history count
   - Password history entries (88 bytes each)

### KeySlot Serialization Algorithm

```
function serialize_keyslot(slot):
    result = []

    # Fixed fields
    result.append(slot.active ? 0x01 : 0x00)
    result.append_bytes(slot.username_hash, 64)
    result.append(slot.username_hash_size)
    result.append_bytes(slot.username_salt, 16)
    result.append_bytes(slot.salt, 32)
    result.append_bytes(slot.wrapped_dek, 40)
    result.append(slot.role as uint8)
    result.append(slot.must_change_password ? 0x01 : 0x00)
    result.append_big_endian_64(slot.password_changed_at)
    result.append_big_endian_64(slot.last_login_at)

    # YubiKey fields
    result.append(slot.yubikey_enrolled ? 0x01 : 0x00)
    result.append_bytes(slot.yubikey_challenge, 32)

    result.append(len(slot.yubikey_serial) as uint8)
    result.append_string(slot.yubikey_serial)

    result.append_big_endian_64(slot.yubikey_enrolled_at)

    result.append_big_endian_16(len(slot.yubikey_encrypted_pin))
    result.append_bytes(slot.yubikey_encrypted_pin)

    result.append_big_endian_16(len(slot.yubikey_credential_id))
    result.append_bytes(slot.yubikey_credential_id)

    # Password history
    result.append(len(slot.password_history) as uint8)
    for entry in slot.password_history:
        result.append_bytes(entry.serialize(), 88)

    return result
```

### Critical Serialization Rules

1. **Username Field**: The `username` field in `KeySlot` structure is **NEVER** serialized to disk
   - Used only for in-memory UI display
   - Populated after successful authentication
   - Must be securely cleared before serialization using `OPENSSL_cleanse()`

2. **Byte Order**:
   - File header fields: **little-endian**
   - FEC original size: **big-endian**
   - All policy/slot integers: **big-endian**
   - Rationale: Network byte order for protocol consistency

3. **String Encoding**:
   - All strings are UTF-8 encoded
   - Length-prefixed (1 or 2 bytes depending on field)
   - No null terminators in binary format

4. **Alignment**: No padding bytes; all fields are packed

---

## FEC Protection

### Reed-Solomon Encoding

**Algorithm:** Reed-Solomon (RS) error correction
**Implementation:** Cauchy-based RS over GF(2^8)
**Block Size:** Configurable based on data size
**Redundancy:** 20-50% (header: minimum 20%, enforced)

### Encoding Process

```
function apply_fec(data, encoding_redundancy, stored_redundancy):
    # Create Reed-Solomon encoder
    rs = ReedSolomon(encoding_redundancy)

    # Encode data
    encoded = rs.encode(data)

    # Build FEC-protected structure
    result = []
    result.append(stored_redundancy as uint8)  # User preference
    result.append_big_endian_32(len(data))     # Original size
    result.append_bytes(encoded.data)           # Data + parity

    return result
```

### Decoding Process

```
function remove_fec(protected_data):
    # Parse FEC header
    redundancy = protected_data[0]
    original_size = big_endian_32(protected_data[1:5])
    encoded_data = protected_data[5:]

    # Calculate effective redundancy used during encoding
    effective_redundancy = max(20, redundancy)

    # Create Reed-Solomon decoder
    rs = ReedSolomon(effective_redundancy)

    # Decode and error-correct
    decoded = rs.decode(encoded_data, original_size)

    if decoded is error:
        return error("FEC decoding failed")

    return (decoded.data, redundancy, effective_redundancy)
```

### FEC Coverage

| Section          | FEC Required | Min Redundancy | User Configurable |
|------------------|--------------|----------------|-------------------|
| File Header      | No           | N/A            | No                |
| Vault Header V2  | **Yes**      | **20%**        | Yes (stored only) |
| Data IV          | No           | N/A            | No                |
| Encrypted Data   | Optional     | 0%             | Yes               |

**Rationale:**
- **Header FEC (mandatory 20%)**: Protects critical authentication data (key slots, policy)
- **Data FEC (optional)**: User can choose based on storage/performance tradeoffs
- **Minimum 20%**: OWASP recommendation for critical data integrity

---

## Security Considerations

### Cryptographic Algorithms

#### FIPS 140-3 Approved Operations

| Operation           | Algorithm           | Key/Output Size | Notes                    |
|---------------------|---------------------|-----------------|--------------------------|
| Password KEK        | PBKDF2-HMAC-SHA256  | 256 bits        | 100k iterations (default)|
| Username Hashing    | Argon2id (recommended) | 256 bits     | Memory-hard              |
| Username Hashing    | SHA3-256/384/512    | 256-512 bits    | Fallback option          |
| DEK Wrapping        | AES-256-GCM         | 256 bits        | NIST SP 800-38D          |
| Data Encryption     | AES-256-GCM         | 256 bits        | NIST SP 800-38D          |
| Password History    | PBKDF2-HMAC-SHA512  | 384 bits        | 600k iterations          |
| YubiKey HMAC        | HMAC-SHA256         | 256 bits        | FIDO2 hmac-secret        |
| Random Generation   | OpenSSL DRBG        | N/A             | FIPS 186-4 compliant     |

#### Non-FIPS Operations (Integrity Only)

| Operation           | Algorithm           | Purpose                           |
|---------------------|---------------------|-----------------------------------|
| FEC                 | Reed-Solomon        | Error correction (not crypto)     |

### Attack Surface Analysis

#### Threats Mitigated

1. **Username Enumeration**:
   - Usernames cryptographically hashed with random salt
   - Constant-time hash comparison prevents timing attacks
   - No plaintext usernames in vault file

2. **Offline Dictionary Attacks**:
   - High PBKDF2 iteration count (100k+ for KEK)
   - Random per-user salts
   - Hardware token option (YubiKey) for 2FA

3. **Password Reuse**:
   - Password history with PBKDF2-HMAC-SHA512
   - Configurable depth (0-255 passwords)

4. **File Corruption**:
   - Reed-Solomon FEC on critical header
   - 20% redundancy allows recovery from sector damage

5. **Replay Attacks**:
   - Timestamps on all sensitive operations
   - Session timeout enforcement

#### Threats NOT Mitigated

1. **Memory Attacks**:
   - DEK and passwords in RAM when vault open
   - Mitigation: mlock(), secure memory wiping, auto-lock

2. **Malware/Keyloggers**:
   - Cannot protect against compromised OS
   - Mitigation: Require YubiKey for additional layer

3. **Side-Channel Attacks**:
   - Timing, power analysis not addressed
   - Relies on OpenSSL's constant-time operations

4. **Quantum Attacks**:
   - AES-256 offers 128-bit quantum security
   - PBKDF2/SHA-3 vulnerable to Grover's algorithm
   - Future: Post-quantum key encapsulation

### Key Derivation Flow

```
┌──────────────────┐
│ User Password    │
└────────┬─────────┘
         │
         ├─────────────────────────────────────┐
         │                                     │
         ▼                                     ▼
    ┌─────────────┐                    ┌──────────────┐
    │ PBKDF2-SHA256│                    │ Username +   │
    │ 100k iters  │                    │ Salt         │
    │ (KEK derive)│                    └──────┬───────┘
    └──────┬──────┘                           │
           │                                  ▼
           │                           ┌──────────────┐
           ▼                           │ Argon2id /   │
    ┌──────────────┐                  │ SHA3-256     │
    │ KEK (256-bit)│                  │ (Hash derive)│
    └──────┬───────┘                  └──────┬───────┘
           │                                  │
           │ (Optional YubiKey)               │
           │                                  ▼
           ▼                           ┌──────────────┐
    ┌──────────────┐                  │Username Hash │
    │ HMAC-SHA256  │                  │(auth token)  │
    │ with YubiKey │                  └──────────────┘
    └──────┬───────┘
           │
           ▼
    ┌──────────────┐
    │ Enhanced KEK │
    │ (YubiKey+PWD)│
    └──────┬───────┘
           │
           ▼
    ┌──────────────┐
    │ AES-256-GCM  │
    │ Unwrap DEK   │
    └──────┬───────┘
           │
           ▼
    ┌──────────────┐
    │ DEK (256-bit)│
    │ (Data Decrypt)│
    └──────────────┘
```

### Secure Memory Handling

**Requirements:**
1. All sensitive data cleared with `OPENSSL_cleanse()` before deallocation
2. Passwords and DEK locked in RAM with `mlock()` when supported
3. Username field in KeySlot cleared before serialization
4. No sensitive data in exception messages or logs

**Implementation Checklist:**
- [x] Username cleared before `VaultHeaderV2::serialize()`
- [x] Password cleared after KEK derivation
- [x] DEK cleared on vault close
- [x] Temporary buffers wiped after use
- [x] Stack-allocated secrets overwritten on scope exit

---

## Compatibility Notes

### Version Migration

**V1 → V2 Migration:**
- Not supported directly (breaking changes in multi-user architecture)
- Export/import workflow required
- Username hashing applied during import

**V2.0 → V2.x Forward Compatibility:**
- New fields added to reserved space
- Backward-compatible deserialization
- Version detection via security policy size

### Backward Compatibility Guarantees

1. **File Magic**: Always `0x4B505457` ("WTPK")
2. **Version Field**: Strictly versioned (no assumptions)
3. **Reserved Fields**: Always zero in current version
4. **FEC Algorithm**: Reed-Solomon only (field for future algos)

### Known Incompatibilities

| Change                          | Introduced | Impact                     | Workaround          |
|---------------------------------|------------|----------------------------|---------------------|
| Username hashing mandatory      | V2.0       | Cannot read V1 vaults      | Export from V1      |
| FIPS-only mode                  | V2.0       | Legacy crypto disabled     | Rebuild with FIPS   |
| Password history size increased | V2.1 (TBD) | Older clients can't parse  | Update client       |

---

## Appendix: Example Vault Structures

### Minimal Vault (1 User, No YubiKey, No History)

```
File Header (16 bytes):
  Magic:       57 54 50 4B (WTPK)
  Version:     02 00 00 00
  PBKDF2:      A0 86 01 00 (100,000)
  Header Size: 01 05 00 00 (1281 bytes with FEC)

FEC Header (5 bytes):
  Redundancy:  00          (0% user preference, 20% actual)
  Orig Size:   00 00 02 0C (524 bytes)

Vault Header V2 (524 bytes decoded):
  Security Policy (122 bytes)
  Key Slot Count: 01
  KeySlot[0]:
    Active:              01
    Username Hash:       [32 bytes SHA3-256]
    Hash Size:           20
    Username Salt:       [16 bytes random]
    Salt:                [32 bytes random]
    Wrapped DEK:         [40 bytes AES-GCM]
    Role:                00 (admin)
    Must Change:         00
    Password Changed:    00 00 00 00 00 00 00 00
    Last Login:          00 00 00 00 00 00 00 00
    YubiKey Enrolled:    00
    YubiKey Challenge:   [32 bytes zeros]
    Serial Length:       00
    Enrolled At:         00 00 00 00 00 00 00 00
    PIN Length:          00 00
    Cred ID Length:      00 00
    History Count:       00

Data IV (12 bytes): [random]
Encrypted Data: [AES-256-GCM ciphertext]
```

**Total Size:** ~650 bytes (FEC adds ~125 bytes to header)

### Production Vault (2 Users, YubiKey, Password History)

**Structure:**
- File Header: 16 bytes
- FEC Header + Encoded: ~1020 bytes (831 bytes original)
- Data IV: 12 bytes
- Encrypted Data: Variable

**Header Contents:**
- VaultSecurityPolicy: 122 bytes
- Slot Count: 1 byte
- Admin Slot:
  - Base: 172 bytes
  - YubiKey serial: 13 bytes ("/dev/hidraw2")
  - Encrypted PIN: 34 bytes
  - Credential ID: 48 bytes
  - History: 88 bytes × 3 = 264 bytes
  - **Subtotal:** ~531 bytes
- Standard User Slot:
  - Base: 172 bytes
  - YubiKey: ~95 bytes
  - History: 88 bytes
  - **Subtotal:** ~355 bytes

**Total Header:** 122 + 1 + 531 + 355 = 1009 bytes (1020 with padding/alignment)

---

## Revision History

| Version | Date       | Author  | Changes                                  |
|---------|------------|---------|------------------------------------------|
| 2.0     | 2026-01-18 | tjdev   | Initial V2 specification                 |
|         |            |         | Username hashing security implementation |
|         |            |         | FEC mandatory 20% for header             |
|         |            |         | FIPS 140-3 compliance documented         |

---

## References

- NIST SP 800-38D: GCM Mode for AES
- NIST SP 800-132: PBKDF2 Recommendations
- NIST FIPS 186-4: Digital Signature Standard (DRBG)
- FIDO2 CTAP Specification
- RFC 9106: Argon2 Memory-Hard Function
- OWASP Password Storage Cheat Sheet
- Reed-Solomon Error Correction (Cauchy construction)

---

**Document Status:** ACTIVE
**Security Classification:** PUBLIC (design specification, no secrets)
**Last Reviewed:** 2026-01-18
