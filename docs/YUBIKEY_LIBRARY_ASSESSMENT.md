# YubiKey Library Assessment for FIPS-140-3 Compliance

**Date**: 2 January 2026
**Author**: Assessment for KeepTower v0.3.1+
**Requirement**: HMAC-SHA256 challenge-response support on Linux (Fedora/Ubuntu) + future Windows

---

## Executive Summary

**RECOMMENDATION: OpenSC via PCSC + Custom APDU Commands**

Neither `libyubikey` nor `OpenSC` directly provides HMAC challenge-response, but OpenSC provides the PCSC infrastructure we need. The winning approach is:
- **OpenSC's PCSC library** (pcscd + libpcsclite)
- **Custom APDU commands** for YubiKey HMAC operations
- Full control, SHA-256 support, FIPS-compliant

---

## Option Analysis

### 1. libyubikey (NOT RECOMMENDED)

**What it is**: Low-level Yubico OTP library

**Key Issues**:
- ❌ **Does NOT support HMAC challenge-response at all**
- ❌ Focused on Yubico OTP token generation only
- ❌ No SHA-256 support
- ❌ Legacy library, minimal maintenance

**Assessment**: **UNSUITABLE** - Wrong use case entirely. This is for OTP generation, not HMAC challenge-response.

**Verdict**: 🚫 **REJECT**

---

### 2. OpenSC (RECOMMENDED with caveats)

**What it is**: Smart card framework for PKCS#11 and PCSC

**Key Points**:
- ✅ Provides PCSC infrastructure (pcscd daemon)
- ✅ Well-maintained, active community
- ✅ Cross-platform (Linux, Windows, macOS)
- ✅ FIPS-compliant implementations available
- ⚠️ **Does NOT have built-in YubiKey HMAC support**

**How to use it**:
OpenSC provides the **PCSC layer**, but we need to send **raw APDU commands** to YubiKey for HMAC operations.

**Architecture**:
```
KeepTower → libpcsclite (from OpenSC) → pcscd → YubiKey
                ↓
         Custom APDU Commands
         (HMAC-SHA256 challenge)
```

**APDU Commands Needed**:
```cpp
// YubiKey APDU for HMAC-SHA256 Challenge-Response
// CLA INS P1  P2  Lc  Data                           Le
// 00  01  20  00  40  [64-byte challenge]           00

// Response: 32 bytes of HMAC-SHA256 output
```

**Verdict**: ✅ **ACCEPT** (with custom APDU implementation)

---

## Alternative: Direct PCSC without OpenSC

**Option**: Use `libpcsclite` directly (part of most Linux distros)

**Pros**:
- ✅ No OpenSC dependency needed
- ✅ Lighter weight
- ✅ Direct control
- ✅ Already installed on most systems

**Cons**:
- ⚠️ Need to implement APDU commands ourselves
- ⚠️ Slightly more low-level

**Assessment**: This is actually **BETTER** than full OpenSC package

---

## RECOMMENDED SOLUTION

### Use: libpcsclite + Custom APDU Implementation

**Dependencies**:
```meson
# Fedora/RHEL
pcsc-lite-devel

# Ubuntu/Debian
libpcsclite-dev

# Windows
WinSCard.dll (built-in)
```

**Implementation Strategy**:

1. **Use PCSC-lite for communication**:
   - `SCardEstablishContext()` - Initialize
   - `SCardListReaders()` - Find YubiKey
   - `SCardConnect()` - Connect to device
   - `SCardTransmit()` - Send APDU commands
   - `SCardDisconnect()` - Cleanup

2. **Send YubiKey-specific APDUs**:
   ```cpp
   // Select YubiKey OTP application
   APDU: 00 A4 04 00 08 A0 00 00 05 27 20 01 01

   // Send HMAC-SHA256 challenge (slot 2)
   APDU: 00 01 38 00 40 [64 bytes challenge] 00
   // Returns: 32 bytes HMAC-SHA256 response
   ```

3. **Platform abstraction**:
   - Linux: libpcsclite
   - Windows: WinSCard API (same interface!)
   - macOS: PCSC framework (same interface!)

**Key Advantages**:
- ✅ **Cross-platform** (PCSC is standardized)
- ✅ **FIPS-compliant** (SHA-256 native support)
- ✅ **No Python dependency**
- ✅ **Full control** over algorithms
- ✅ **Lightweight** (PCSC already on most systems)
- ✅ **Well-documented** APDU commands from Yubico

---

## Implementation Comparison

| Feature | ykpers (current) | OpenSC | Direct PCSC | yubikey-manager |
|---------|-----------------|--------|-------------|-----------------|
| SHA-256 Support | ❌ No | ⚠️ Via APDU | ✅ Via APDU | ✅ Yes |
| Native C/C++ | ✅ Yes | ✅ Yes | ✅ Yes | ❌ Python |
| Cross-platform | ✅ Yes | ✅ Yes | ✅ Yes | ✅ Yes |
| FIPS Compliant | ❌ No | ✅ Yes | ✅ Yes | ✅ Yes |
| Maintenance | ⚠️ Stale | ✅ Active | ✅ Standard | ✅ Active |
| Dependencies | Low | Medium | **Minimal** | High |
| Complexity | Low | Medium | Medium | Low |
| Documentation | Good | Excellent | Good | Excellent |

---

## FINAL RECOMMENDATION

### Phase 1: Implement Direct PCSC (Today - 2 Jan 2026)

**Library**: `libpcsclite` (PC/SC lite)

**Rationale**:
1. Minimal dependencies (already on most systems)
2. Native C/C++ integration
3. Full SHA-256 support via APDU
4. Cross-platform (Windows = WinSCard)
5. FIPS-compliant
6. Direct control over operations

**Implementation Steps**:
1. Replace `ykpers` calls with PCSC-lite API
2. Implement APDU command builder for YubiKey HMAC
3. Add SHA-256 challenge-response function
4. Test on YubiKey 5 FIPS device
5. Update build system (meson.build)

**Estimated Effort**: 3-4 hours

**Files to Modify**:
- `src/core/managers/YubiKeyManager.cc` - Replace ykpers with PCSC
- `src/core/managers/YubiKeyManager.h` - Update includes
- `meson.build` - Change dependency from ykpers to pcsc-lite
- `docs/BUILDING.md` - Update build instructions

**Testing**:
- Create vault with YubiKey + SHA-256
- Open old vaults (SHA-1 backward compat)
- Verify FIPS audit passes

---

## Reference Documentation

### PCSC-Lite
- **Website**: https://pcsclite.apdu.fr/
- **API Docs**: https://pcsclite.apdu.fr/api/
- **GitHub**: https://github.com/LudovicRousseau/PCSC

### YubiKey APDU Commands
- **Yubico APDU Spec**: https://developers.yubico.com/yubikey-manager/APDU.html
- **HMAC-SHA256**: OTP application, INS=0x01, P1=0x38 (slot 2)
- **Challenge Format**: 64 bytes (padded if needed)
- **Response Format**: 32 bytes (SHA-256) or 20 bytes (SHA-1)

### Cross-Platform PCSC
- **Linux**: libpcsclite (apt/dnf install)
- **Windows**: WinSCard.dll (built-in since Vista)
- **macOS**: PCSC.framework (built-in)

---

## Risk Assessment

| Risk | Severity | Mitigation |
|------|----------|------------|
| APDU command errors | Medium | Thorough testing, error handling |
| Platform differences | Low | PCSC is standardized |
| YubiKey firmware bugs | Low | Use official APDU spec |
| Performance overhead | Low | Direct PCSC is fast |
| Documentation gaps | Low | Yubico provides APDU docs |

---

## Conclusion

**Choose: Direct PCSC-lite with custom APDU commands**

This gives us:
- Full FIPS-140-3 compliance (SHA-256)
- Cross-platform support (Linux + future Windows)
- Native C++ performance
- Minimal dependencies
- Full control over implementation

**DO NOT use**:
- ❌ libyubikey - Wrong use case (OTP only)
- ⚠️ Full OpenSC - Unnecessary complexity
- ⚠️ yubikey-manager - Python dependency

**Proceed with**: PCSC-lite implementation today.
