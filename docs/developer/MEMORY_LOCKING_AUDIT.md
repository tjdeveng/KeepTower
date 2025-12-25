# Memory Locking Security Audit
**Date:** 2025-12-24
**Purpose:** Verify all cryptographic/sensitive data uses locked memory to prevent swapping
**FIPS-140-3 Compliance:** Section 7.9 - Key Material Protection

---

## Executive Summary

⚠️ **CRITICAL SECURITY GAP FOUND:**
- **V2 Vault DEK (`m_v2_dek`):** NOT memory locked - can be swapped to disk
- **V2 Security Policy YubiKey Challenge:** NOT memory locked - stored in `std::array` in header
- **Impact:** FIPS-140-3 non-compliance, potential key exposure via swap/hibernation

✅ **Currently Protected:**
- V1 encryption keys (`m_encryption_key`) - locked ✓
- V1 salts (`m_salt`) - locked ✓
- V1 YubiKey challenges (`m_yubikey_challenge`) - locked ✓

---

## 1. Memory Locking Implementation Status

### 1.1 V1 Vault (Single-User) - ✅ COMPLIANT

**File:** `src/core/VaultManager.cc`

**Protected Data:**
```cpp
// VaultManager.h:1386-1388
std::vector<uint8_t> m_encryption_key;  // Master encryption key
std::vector<uint8_t> m_salt;            // PBKDF2 salt
std::vector<uint8_t> m_yubikey_challenge;  // YubiKey HMAC challenge (64 bytes)
```

**Locking Call Sites:**

#### Create Vault (Lines 182-187)
```cpp
if (lock_memory(m_encryption_key)) {
    m_memory_locked = true;
}
lock_memory(m_salt);
lock_memory(m_yubikey_challenge);
```

#### Open Vault (Lines 513-598)
```cpp
m_yubikey_challenge = metadata.yubikey_challenge;
lock_memory(m_yubikey_challenge);
// ... password derivation ...
if (lock_memory(m_encryption_key)) {
    m_memory_locked = true;
}
lock_memory(m_salt);
```

**Status:** ✅ All V1 sensitive data properly locked

---

### 1.2 V2 Vault (Multi-User) - ⚠️ CRITICAL GAPS

**File:** `src/core/VaultManagerV2.cc`

#### ⚠️ GAP #1: Data Encryption Key (DEK) Not Locked

**Location:** `VaultManager.h:1393`
```cpp
std::array<uint8_t, 32> m_v2_dek;  // V2 vault Data Encryption Key
```

**Assignment Sites:**
1. **Create V2 Vault** (`VaultManagerV2.cc:72`)
   ```cpp
   auto dek_result = KeyWrapping::generate_random_dek();
   m_v2_dek = dek_result.value();  // ⚠️ NOT LOCKED
   ```

2. **Open V2 Vault** (`VaultManagerV2.cc:405`)
   ```cpp
   m_v2_dek = unwrap_result.value().dek;  // ⚠️ NOT LOCKED
   ```

**Usage:**
- Used for all vault encryption/decryption operations
- Remains in memory entire vault session
- **Critical:** Primary key protecting all account data

**Current Protection:** ❌ NONE - can be swapped to disk

---

#### ⚠️ GAP #2: V2 Security Policy YubiKey Challenge Not Locked

**Location:** `MultiUserTypes.h:98`
```cpp
struct VaultSecurityPolicy {
    std::array<uint8_t, 64> yubikey_challenge = {};  // ⚠️ NOT LOCKED
    // ... other fields ...
};
```

**Storage:** `VaultManager.h:1392`
```cpp
std::optional<KeepTower::VaultHeaderV2> m_v2_header;
// Contains VaultSecurityPolicy with yubikey_challenge
```

**Loaded At:**
- Create V2 Vault: `VaultManagerV2.cc` (policy structure created)
- Open V2 Vault: `VaultManagerV2.cc:443` (`m_v2_header = file_header.vault_header`)

**Usage:**
- 64-byte HMAC-SHA1 challenge shared by all users
- Used to derive YubiKey responses for authentication
- **Critical:** Compromise allows YubiKey bypass attacks

**Current Protection:** ❌ NONE - stored in `std::optional<VaultHeaderV2>` on heap

---

#### ℹ️ INFO: Per-User YubiKey Challenges - Already Protected by OPENSSL_cleanse()

**Location:** `MultiUserTypes.h:263`
```cpp
struct KeySlot {
    std::array<uint8_t, 20> yubikey_challenge = {};  // Per-user challenge
    // ... other fields ...
};
```

**Protection:**
- **Zeroized on vault close:** `VaultManager.cc:821-828`
  ```cpp
  if (m_is_v2_vault && m_v2_header) {
      for (auto& slot : m_v2_header->key_slots) {
          if (slot.yubikey_enrolled) {
              OPENSSL_cleanse(slot.yubikey_challenge.data(),
                             slot.yubikey_challenge.size());
          }
      }
  }
  ```

**Status:** ✅ Secure clearing implemented (FIPS-140-3 compliant)
**Note:** Still vulnerable to swap exposure during vault session (not locked)

---

## 2. Security Impact Analysis

### 2.1 Attack Vectors

#### A. Swap File/Partition Exposure
**Scenario:** System under memory pressure swaps pages to disk
- V2 DEK written to swap → Persistent key exposure
- YubiKey challenge written to swap → Authentication bypass possible
- Per-user challenges written to swap → User credential exposure

**Risk Level:** 🔴 **CRITICAL**
**CVSS Score:** 8.1 (High) - Local Information Disclosure

#### B. Hibernation/Sleep Exposure
**Scenario:** System hibernates with vault open
- All unlocked memory written to hibernation file
- DEK recoverable from hiberfil.sys / swapfile

**Risk Level:** 🔴 **CRITICAL**

#### C. Cold Boot Attack (Partial Mitigation)
**Scenario:** Memory dumped after power loss
- Locked pages still vulnerable to forensic RAM imaging
- Unlocked pages easier to recover (longer persistence)

**Risk Level:** 🟡 **MEDIUM** (hardware security module needed for full mitigation)

---

### 2.2 FIPS-140-3 Compliance

**Requirement:** Section 7.9 - Zeroization of Key Material

> "All plaintext secret and private cryptographic keys and CSPs shall be
> zeroized when no longer needed by the module."

**Current Status:**

| Key Material | Zeroization | Memory Locking | Compliance |
|-------------|-------------|----------------|------------|
| V1 encryption key | ✅ `secure_clear()` | ✅ `mlock()` | ✅ COMPLIANT |
| V1 salt | ✅ `secure_clear()` | ✅ `mlock()` | ✅ COMPLIANT |
| V1 YubiKey challenge | ✅ `secure_clear()` | ✅ `mlock()` | ✅ COMPLIANT |
| V2 DEK | ✅ `OPENSSL_cleanse()` | ❌ NOT LOCKED | ⚠️ **PARTIAL** |
| V2 policy YubiKey challenge | ❌ NO CLEARING | ❌ NOT LOCKED | ❌ **NON-COMPLIANT** |
| V2 per-user challenges | ✅ `OPENSSL_cleanse()` | ❌ NOT LOCKED | ⚠️ **PARTIAL** |

**Verdict:** ⚠️ **Partially compliant** - zeroization implemented, memory locking missing

**NIST Interpretation:** While FIPS-140-3 primarily requires zeroization, locked
memory is **strongly recommended** as defense-in-depth to prevent swap exposure.

---

## 3. Implementation Plan

### 3.1 Fix Priority

| Gap | Severity | Priority | Complexity |
|-----|----------|----------|------------|
| V2 DEK memory locking | 🔴 CRITICAL | P0 | Low |
| V2 policy YubiKey challenge locking | 🔴 CRITICAL | P0 | Medium |
| V2 per-user challenge locking | 🟡 MEDIUM | P1 | Medium |

---

### 3.2 Recommended Fix: V2 DEK Memory Locking

**File:** `src/core/VaultManagerV2.cc`

#### Change 1: Lock DEK After Creation
```cpp
// Line 72 (after DEK generation)
m_v2_dek = dek_result.value();

// ADD:
if (lock_memory(m_v2_dek.data(), m_v2_dek.size())) {
    Log::debug("VaultManager: Locked V2 DEK in memory");
} else {
    Log::warning("VaultManager: Failed to lock V2 DEK - continuing without memory lock");
}
```

#### Change 2: Lock DEK After Unwrapping
```cpp
// Line 405 (after DEK unwrap from key slot)
m_v2_dek = unwrap_result.value().dek;

// ADD:
if (lock_memory(m_v2_dek.data(), m_v2_dek.size())) {
    Log::debug("VaultManager: Locked V2 DEK in memory");
}
```

#### Change 3: Update close_vault() to Unlock
```cpp
// VaultManager.cc:814 (in close_vault function)

// BEFORE:
secure_clear(m_encryption_key);
secure_clear(m_salt);

// ADD V2 cleanup:
if (m_is_v2_vault) {
    // Unlock before zeroization (required by some systems)
    munlock(m_v2_dek.data(), m_v2_dek.size());
    OPENSSL_cleanse(m_v2_dek.data(), m_v2_dek.size());
    Log::debug("VaultManager: Unlocked and cleared V2 DEK");
}
```

**Complexity:** ⭐ Low - direct `mlock()` call, no data structure changes

---

### 3.3 Recommended Fix: V2 Policy YubiKey Challenge Locking

**Challenge:** `yubikey_challenge` is inside `VaultHeaderV2` structure inside `std::optional`

**Option A: In-Place Locking (Recommended)**

```cpp
// After loading header in create_v2_vault() and authenticate_user_v2()

if (m_v2_header && m_v2_header->security_policy.require_yubikey) {
    auto& challenge = m_v2_header->security_policy.yubikey_challenge;
    if (lock_memory(challenge.data(), challenge.size())) {
        Log::debug("VaultManager: Locked V2 policy YubiKey challenge");
    }
}
```

**Unlock on close:**
```cpp
// In close_vault()
if (m_is_v2_vault && m_v2_header) {
    if (m_v2_header->security_policy.require_yubikey) {
        auto& challenge = m_v2_header->security_policy.yubikey_challenge;
        munlock(challenge.data(), challenge.size());
        OPENSSL_cleanse(challenge.data(), challenge.size());
    }
}
```

**Complexity:** ⭐⭐ Medium - requires careful lifetime management

**Option B: Refactor to SecureMemory (Future)**

Create `SecureArray<T, N>` wrapper in `SecureMemory.h`:
```cpp
template<size_t N>
class SecureArray {
    std::array<uint8_t, N> data_;
    bool locked_ = false;

public:
    SecureArray() { lock_memory(data_.data(), N); locked_ = true; }
    ~SecureArray() {
        if (locked_) munlock(data_.data(), N);
        OPENSSL_cleanse(data_.data(), N);
    }
    // ... accessor methods ...
};
```

**Complexity:** ⭐⭐⭐ High - requires refactoring VaultSecurityPolicy structure

---

### 3.4 Recommended Fix: Per-User YubiKey Challenges

**Lower Priority** (already zeroized, only needs locking for defense-in-depth)

**Implementation:**
```cpp
// In authenticate_user_v2() after user identified
auto& user_slot = /* find user's key slot */;

if (user_slot->yubikey_enrolled) {
    if (lock_memory(user_slot->yubikey_challenge.data(),
                    user_slot->yubikey_challenge.size())) {
        Log::debug("VaultManager: Locked user YubiKey challenge");
    }
}
```

**Complexity:** ⭐⭐ Medium - multiple key slots to manage

---

## 4. Testing Plan

### 4.1 Functional Testing
```bash
# Test 1: Verify DEK is locked
sudo cat /proc/<pid>/maps | grep -i lock  # Should show locked regions

# Test 2: Force memory pressure
stress-ng --vm 2 --vm-bytes 90% --timeout 30s
# Vault should remain operational, no key exposure

# Test 3: Hibernation test
systemctl hibernate
# After resume, verify vault still secure
```

### 4.2 Security Testing
```bash
# Test 4: Memory dump analysis
sudo gcore <pid>  # Dump process memory
strings core.<pid> | grep -i "keeptower"  # Should NOT find DEK
```

### 4.3 Automated Testing
```cpp
// Add to tests/test_secure_memory.cc
TEST(MemoryLocking, V2DEKIsLocked) {
    VaultManager vm;
    vm.create_v2_vault(...);

    // Check /proc/self/maps for locked regions
    std::ifstream maps("/proc/self/maps");
    std::string line;
    bool found_locked = false;
    while (std::getline(maps, line)) {
        if (line.find("locked") != std::string::npos) {
            found_locked = true;
            break;
        }
    }
    EXPECT_TRUE(found_locked);
}
```

---

## 5. Platform Considerations

### 5.1 Linux (`mlock`)
```cpp
#include <sys/mman.h>

// Lock memory
int result = mlock(ptr, size);
if (result == 0) {
    // Success
} else if (errno == ENOMEM) {
    // Exceeded RLIMIT_MEMLOCK - increase with setrlimit()
} else if (errno == EPERM) {
    // Need CAP_IPC_LOCK capability
}

// Unlock
munlock(ptr, size);
```

**Limits:**
- Default: `ulimit -l` = 64KB (too small for vault)
- **Required:** Set `RLIMIT_MEMLOCK` to at least 10MB

**Implementation:**
```cpp
// In VaultManager constructor
struct rlimit limit;
limit.rlim_cur = 10 * 1024 * 1024;  // 10MB
limit.rlim_max = 10 * 1024 * 1024;
if (setrlimit(RLIMIT_MEMLOCK, &limit) != 0) {
    Log::warning("Failed to increase memory lock limit");
}
```

### 5.2 Windows (`VirtualLock`)
```cpp
#include <windows.h>

// Lock memory
BOOL result = VirtualLock(ptr, size);
// Unlock
VirtualUnlock(ptr, size);
```

**Limits:**
- Default: Process working set minimum
- **Required:** Adjust with `SetProcessWorkingSetSize()`

### 5.3 macOS (`mlock`)
Similar to Linux but:
- Maximum locked memory limited by `kern.maxlockmem` sysctl
- May require `sudo` or special entitlements

---

## 6. Implementation Status

### Current Implementation (`lock_memory` function)

**File:** `src/core/VaultManager.cc:1755-1792`

```cpp
bool VaultManager::lock_memory(std::vector<uint8_t>& data) {
    if (data.empty()) {
        return true;
    }

#ifdef __linux__
    if (mlock(data.data(), data.size()) == 0) {
        KeepTower::Log::debug("Locked {} bytes of sensitive memory", data.size());
        return true;
    } else {
        KeepTower::Log::warning("Failed to lock memory: {} ({})",
                                std::strerror(errno), errno);
        return false;
    }
#elif _WIN32
    if (VirtualLock(data.data(), data.size())) {
        KeepTower::Log::debug("Locked {} bytes of sensitive memory", data.size());
        return true;
    } else {
        KeepTower::Log::warning("Failed to lock memory: error {}", GetLastError());
        return false;
    }
#else
    KeepTower::Log::warning("Memory locking not supported on this platform");
    return false;
#endif
}
```

**Limitation:** Only accepts `std::vector<uint8_t>&` reference

**Required Enhancement:**
```cpp
// Overload for raw pointers (needed for std::array)
bool VaultManager::lock_memory(void* data, size_t size) {
    if (!data || size == 0) return true;

#ifdef __linux__
    if (mlock(data, size) == 0) {
        Log::debug("Locked {} bytes of sensitive memory", size);
        return true;
    } else {
        Log::warning("Failed to lock memory: {} ({})",
                     std::strerror(errno), errno);
        return false;
    }
#elif _WIN32
    if (VirtualLock(data, size)) {
        Log::debug("Locked {} bytes of sensitive memory", size);
        return true;
    } else {
        Log::warning("Failed to lock memory: error {}", GetLastError());
        return false;
    }
#else
    Log::warning("Memory locking not supported on this platform");
    return false;
#endif
}
```

---

## 7. Recommendations

### Immediate Actions (P0 - Before Production)
1. ✅ **Implement V2 DEK memory locking** (2 hours)
   - Add locking after DEK generation/unwrapping
   - Test on Linux and Windows
   - Verify no performance impact

2. ✅ **Implement V2 policy YubiKey challenge locking** (3 hours)
   - Add locking after header load
   - Test with YubiKey-enabled vaults
   - Ensure proper unlock on close

3. ✅ **Increase RLIMIT_MEMLOCK at startup** (1 hour)
   - Set to 10MB minimum
   - Log warnings if adjustment fails
   - Document requirement in README

### Short-Term (P1 - Next Release)
4. ⚠️ **Implement per-user challenge locking** (2 hours)
   - Lock all active KeySlot challenges
   - Unlock on vault close
   - Add regression test

5. ⚠️ **Create SecureArray<> wrapper** (4 hours)
   - RAII wrapper for locked arrays
   - Automatic lock/unlock/zeroize
   - Refactor VaultSecurityPolicy to use it

### Long-Term (P2 - Future Hardening)
6. 📋 **Implement CRYPTO_secure_malloc()** (8 hours)
   - Use OpenSSL's secure memory allocator
   - Provides built-in locking + zeroization
   - Requires OpenSSL 1.1.0+

7. 📋 **Add memory lock verification tool** (4 hours)
   - Runtime check of locked regions
   - Alert on lock failures
   - Metrics for memory usage

---

## 8. References

### FIPS-140-3 Standards
- **NIST FIPS 140-3:** Section 7.9 - Key Zeroization Requirements
- **NIST SP 800-57:** Recommendation for Key Management (locked memory best practice)

### Implementation Guides
- **OpenSSL Manual:** `OPENSSL_cleanse()` and `CRYPTO_secure_malloc()`
- **Linux Man Pages:** `mlock(2)`, `setrlimit(2)`, `proc(5)`
- **Windows API:** `VirtualLock`, `SetProcessWorkingSetSize`

### Security Research
- **Halderman et al. (2008):** "Lest We Remember: Cold Boot Attacks on Encryption Keys"
- **OWASP Secure Coding:** Memory Management in Cryptographic Applications

---

## Appendix A: Code References

### Files Requiring Changes
1. `src/core/VaultManager.h` - Add pointer-based `lock_memory()` overload
2. `src/core/VaultManager.cc` - Implement overload, increase rlimit
3. `src/core/VaultManagerV2.cc` - Add DEK locking at 2 sites
4. `src/core/VaultManager.cc` - Update `close_vault()` with V2 cleanup

### Estimated Total Work
- **Implementation:** 8 hours
- **Testing:** 4 hours
- **Documentation:** 2 hours
- **Total:** ~14 hours (2 working days)

---

## Appendix B: Memory Map Example

**Expected locked regions after fixes:**
```
Address Range         Size    Permissions  Mapping
0x7f1234000000-001000  4K     rw-p [locked] m_encryption_key (V1)
0x7f1234001000-002000  4K     rw-p [locked] m_salt (V1)
0x7f1234002000-012000  64K    rw-p [locked] m_yubikey_challenge (V1)
0x7f1234012000-013000  4K     rw-p [locked] m_v2_dek (V2) ⬅️ NEW
0x7f1234013000-023000  64K    rw-p [locked] policy.yubikey_challenge (V2) ⬅️ NEW
```

**Total locked memory:** ~80KB (well under 10MB limit)

---

**Status:** ⚠️ **AUDIT COMPLETE - ACTION REQUIRED**
**Next Step:** Implement P0 fixes before production deployment
