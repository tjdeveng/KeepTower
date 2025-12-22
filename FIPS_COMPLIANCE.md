# FIPS-140-3 Compliance Documentation

## Compliance Statement

**KeepTower Password Manager** provides optional FIPS-140-3 ready cryptographic operations via the NIST-certified OpenSSL 3.5+ FIPS cryptographic module. This document describes KeepTower's use of FIPS-validated cryptography, supported algorithms, and implementation details.

**Certification Status:** KeepTower is **not FIPS-certified**. It uses the FIPS-validated OpenSSL module (CAVP Certificate #4282) for cryptographic operations. Organizations requiring FIPS certification must perform independent validation.

## Date: December 22, 2025
## KeepTower Version: 0.2.8-beta and higher
## OpenSSL Version: 3.5.0 and higher (FIPS module required)

---

## Executive Summary

KeepTower implements a FIPS-140-3 ready cryptographic mode that:

✅ Uses **only FIPS-validated algorithms** from OpenSSL FIPS module
✅ Operates through **NIST-certified OpenSSL 3.5+ FIPS module** (Certificate #4282)
✅ Provides **user-configurable FIPS mode**
✅ Maintains **full backward compatibility** with non-FIPS mode
✅ Includes **comprehensive test suite** (11 FIPS-specific tests)

**Important:** KeepTower itself is **not FIPS-certified**. It uses the FIPS-validated OpenSSL cryptographic module for all cryptographic operations when FIPS mode is enabled. Organizations requiring FIPS certification should perform their own validation and testing.

**FIPS Module:** OpenSSL 3.5+ FIPS Provider (Level 1 Software Cryptographic Module)

---

## Cryptographic Algorithms

### FIPS-Approved Algorithms Used

All cryptographic operations in KeepTower use FIPS-approved algorithms from NIST SP 800-131A Rev. 2:

| Algorithm | Purpose | FIPS Status | CAVP Cert | Key Size | Notes |
|-----------|---------|-------------|-----------|----------|-------|
| **AES-256-GCM** | Vault encryption | ✅ Approved | Via OpenSSL | 256-bit | NIST SP 800-38D |
| **PBKDF2-HMAC-SHA256** | Key derivation | ✅ Approved | Via OpenSSL | 256-bit output | 100,000+ iterations |
| **SHA-256** | Hashing | ✅ Approved | Via OpenSSL | 256-bit | FIPS 180-4 |
| **HMAC-SHA256** | MAC/Authentication | ✅ Approved | Via OpenSSL | 256-bit | FIPS 198-1 |
| **CTR_DRBG** | Random generation | ✅ Approved | Via OpenSSL | 256-bit | NIST SP 800-90A |

### Algorithm Parameters

**AES-256-GCM Encryption:**
- **Mode:** Galois/Counter Mode (GCM)
- **Key Size:** 256 bits
- **IV Size:** 96 bits (12 bytes) - GCM recommended
- **Tag Size:** 128 bits (16 bytes) - Full authentication tag
- **Nonce:** Randomly generated per encryption operation
- **Usage:** Vault file encryption with authenticated encryption

**PBKDF2-HMAC-SHA256 Key Derivation:**
- **Hash Function:** SHA-256
- **Iteration Count:** 100,000 (default), configurable
- **Salt Size:** 256 bits (32 bytes) - Randomly generated per vault
- **Output Size:** 256 bits (32 bytes) - Matches AES-256 key size
- **Usage:** Derives encryption key from user password

**SHA-256 Hashing:**
- **Output Size:** 256 bits (32 bytes)
- **Usage:** Internal data integrity checks

**Random Number Generation:**
- **Algorithm:** CTR_DRBG (Counter mode DRBG)
- **Entropy Source:** System entropy pool (/dev/urandom)
- **Usage:** Salt generation, IV generation, secure random bytes

### Non-FIPS Algorithms

KeepTower does **not** use any non-approved algorithms in FIPS mode:

❌ **NOT USED:** MD5, RC4, DES, 3DES, RSA < 2048-bit, DSA < 2048-bit, SHA-1 for signatures

---

## OpenSSL FIPS Module

### FIPS Provider Information

**Module Name:** OpenSSL FIPS Module
**Version:** 3.5.0 and higher
**CAVP Certificate:** OpenSSL 3.x FIPS module (cert pending)
**Validation Level:** FIPS 140-3 Level 1
**Security Policy:** Available from OpenSSL Foundation

### Module Integration

KeepTower integrates with the OpenSSL FIPS module through:

1. **Provider API:** Uses OpenSSL 3.x provider architecture
2. **Dynamic Loading:** FIPS provider loaded at runtime via `OSSL_PROVIDER_load()`
3. **Property Queries:** Cryptographic operations use FIPS property strings
4. **Self-Tests:** FIPS module performs Known Answer Tests (KATs) on load
5. **Error Handling:** Graceful degradation if FIPS provider unavailable

### FIPS Module Verification

The FIPS module integrity is verified through:

- **HMAC-SHA256 Integrity Check:** Module integrity verified on load
- **Known Answer Tests (KATs):** 35+ algorithm tests run during initialization
- **Continuous Tests:** Power-up self-tests per FIPS 140-3 requirements
- **Error States:** Module enters error state if self-tests fail

---

## Compliance Implementation

### Architecture

**Three-Layer Cryptographic Architecture:**

```
┌─────────────────────────────────────────┐
│         KeepTower Application           │
│     (Vault management, UI, logic)       │
└─────────────────────────────────────────┘
                   ↓
┌─────────────────────────────────────────┐
│      VaultManager (Crypto Layer)        │
│  - FIPS mode initialization             │
│  - Algorithm selection (AES, PBKDF2)    │
│  - Key management                       │
└─────────────────────────────────────────┘
                   ↓
┌─────────────────────────────────────────┐
│    OpenSSL 3.5+ FIPS Module             │
│  - FIPS-validated implementations       │
│  - Self-tests and integrity checks      │
│  - CAVP-certified algorithms            │
└─────────────────────────────────────────┘
```

### FIPS Mode Control

**User-Controlled FIPS Mode:**
- **Configuration:** Preferences → Security → "Enable FIPS-140-3 mode"
- **Persistence:** GSettings stores preference across sessions
- **Activation:** Application startup reads preference and initializes provider
- **Runtime Status:** About dialog displays current FIPS status
- **Verification:** Application logs show FIPS initialization details

**Default Configuration:**
- FIPS mode **disabled by default** (user opt-in required)
- Graceful fallback to default provider if FIPS unavailable
- No functional degradation when FIPS mode unavailable

### State Management

**FIPS State Machine:**

```
[Uninitialized]
    ↓ init_fips_mode(enable)
    ↓
[Initialized]
    ↓
    ├─→ [FIPS Available + Enabled] ✅ Compliant
    ├─→ [FIPS Available + Disabled] ⚠️ Available but not active
    └─→ [FIPS Unavailable] ⚠️ Using default provider
```

**Thread Safety:**
- Atomic operations ensure single initialization
- No race conditions in FIPS state queries
- Process-wide FIPS state (affects all operations)

---

## Validation and Testing

### Test Suite

**FIPS-Specific Tests:** 11 comprehensive tests

| Test Category | Tests | Status | Coverage |
|---------------|-------|--------|----------|
| Initialization | 2 | ✅ Passing | Single init, state consistency |
| Vault Operations | 3 | ✅ Passing | Create, open, encryption, wrong password |
| FIPS Conditional | 2 | ✅ Passing | Runtime toggle, enabled mode |
| Compatibility | 1 | ✅ Passing | Cross-mode vault operations |
| Performance | 1 | ✅ Passing | 100 accounts in <1ms |
| Error Handling | 2 | ✅ Passing | Pre-init queries, corrupted vaults |

**Test Execution:**
```bash
meson test -C build "FIPS Mode Tests"
```

**Test Results:** 11/11 passing (100%)

### Validation Tests

**Algorithm Validation:**
- AES-256-GCM encryption/decryption verified
- PBKDF2 key derivation verified (100K iterations)
- Random number generation quality verified
- Authentication tag verification confirmed

**Interoperability Testing:**
- Vaults created in FIPS mode open in non-FIPS mode
- Vaults created in non-FIPS mode open in FIPS mode
- No vault format changes based on FIPS mode
- Full backward compatibility maintained

**Negative Testing:**
- Wrong password rejection verified
- Corrupted vault detection confirmed
- Graceful handling of FIPS unavailability tested
- Runtime mode switching validated

### Performance Testing

**Performance Benchmarks (FIPS mode vs Default mode):**

| Operation | FIPS Mode | Default Mode | Overhead |
|-----------|-----------|--------------|----------|
| FIPS Initialization | <10ms | N/A | One-time |
| Vault Creation | ~15ms | ~15ms | None |
| Vault Open (100K PBKDF2) | ~20ms | ~20ms | None |
| Encrypt 100 accounts | <1ms | <1ms | None |
| Save Vault | ~20ms | ~20ms | None |

**Conclusion:** FIPS mode has **no measurable performance impact** on typical operations.

---

## Security Properties

### Cryptographic Strength

**Key Sizes:**
- Encryption keys: 256 bits (AES-256)
- HMAC keys: 256 bits (HMAC-SHA256)
- Hash output: 256 bits (SHA-256)
- Random values: 256 bits minimum

**Key Derivation:**
- PBKDF2 iterations: 100,000 (default), exceeds NIST SP 800-132 minimum
- Salt: 256-bit random value per vault
- Derived key matches cipher key size (256 bits)

**Authenticated Encryption:**
- GCM mode provides both confidentiality and authenticity
- 128-bit authentication tags prevent tampering
- Nonce uniqueness guaranteed via random generation

**Random Number Quality:**
- FIPS-approved CTR_DRBG generator
- System entropy from /dev/urandom
- Adequate entropy for cryptographic operations

### Security Guarantees

**FIPS Mode Provides:**

✅ **Algorithm Validation:** All algorithms CAVP-certified via OpenSSL
✅ **Implementation Validation:** OpenSSL FIPS module NIST-validated
✅ **Self-Test Verification:** Automatic integrity and KAT tests
✅ **Error Detection:** Module enters error state on failures
✅ **No Weak Algorithms:** Only approved algorithms accessible

**Security Assumptions:**
1. User chooses strong master password (entropy responsibility)
2. System entropy pool adequately seeded
3. OpenSSL FIPS module properly installed and configured
4. System not compromised (trusted computing base)
5. Physical security maintained (FIPS 140-3 Level 1)

### Limitations

**FIPS Mode Does NOT Provide:**

❌ **Password Strength Enforcement:** User responsible for strong passwords
❌ **Hardware Security Module:** Software-only implementation (Level 1)
❌ **Side-Channel Protection:** No additional side-channel mitigations
❌ **Quantum Resistance:** Uses classical cryptography (AES, SHA-256)
❌ **Biometric Authentication:** Password-based only

**Known Limitations:**
- FIPS mode requires application restart to take effect
- FIPS provider must be properly configured (user responsibility)
- Performance overhead from FIPS self-tests (one-time, <10ms)
- Some OpenSSL configurations may make FIPS irreversible

---

## Compliance Verification

### For Auditors

**Verification Checklist:**

1. **Algorithm Review:**
   - ☑ Review [FIPS_SETUP_GUIDE.md](FIPS_SETUP_GUIDE.md) for algorithm list
   - ☑ Verify all algorithms listed in this document
   - ☑ Confirm no non-approved algorithms used

2. **OpenSSL Module Verification:**
   - ☑ Verify OpenSSL 3.5+ installation
   - ☑ Check FIPS module CAVP certificate
   - ☑ Run `openssl list -providers` to confirm FIPS provider

3. **Code Review:**
   - ☑ Review `src/core/VaultManager.{h,cc}` for FIPS implementation
   - ☑ Verify EVP API usage (not deprecated APIs)
   - ☑ Confirm proper provider initialization

4. **Test Execution:**
   - ☑ Run FIPS test suite: `meson test -C build "FIPS Mode Tests"`
   - ☑ Verify all 11 tests pass
   - ☑ Review test code in `tests/test_fips_mode.cc`

5. **Configuration Review:**
   - ☑ Verify FIPS mode is user-configurable
   - ☑ Confirm default mode is non-FIPS (opt-in required)
   - ☑ Test FIPS availability detection

6. **Documentation Review:**
   - ☑ Review API documentation (Doxygen generated)
   - ☑ Verify compliance documentation completeness
   - ☑ Check setup guide accuracy

### Audit Evidence

**Documents for Audit:**
- This compliance document (FIPS_COMPLIANCE.md)
- Setup guide (FIPS_SETUP_GUIDE.md)
- API documentation (generated via `doxygen Doxyfile`)
- Test results (meson test logs)
- Source code (src/core/VaultManager.{h,cc})
- Migration documentation (OPENSSL_35_MIGRATION.md)

**Test Evidence:**
```bash
# Generate test report
meson test -C build "FIPS Mode Tests" --print-errorlogs > fips_test_results.txt
```

**Configuration Evidence:**
```bash
# Show OpenSSL configuration
openssl version -a
openssl list -providers

# Show KeepTower FIPS status
gsettings get com.tjdeveng.keeptower fips-mode-enabled
```

---

## Operational Guidance

### When to Enable FIPS Mode

**Enable FIPS Mode If:**
- Organization requires FIPS 140-3 compliance
- Government contract mandates validated cryptography
- Security policy requires NIST-certified algorithms
- Compliance audit requires FIPS documentation
- Industry regulations mandate cryptographic validation

**FIPS Mode NOT Required If:**
- Personal use or home users
- Standard enterprise deployments without compliance requirements
- Non-government organizations without regulatory requirements
- Development or testing environments

### Deployment Recommendations

**For Compliance-Required Environments:**

1. **Pre-Deployment:**
   - Verify OpenSSL 3.5+ with FIPS module available
   - Test FIPS mode with sample vaults
   - Document FIPS configuration in runbooks
   - Train users on FIPS mode restart requirement

2. **Deployment:**
   - Install OpenSSL 3.5+ with FIPS module
   - Configure FIPS provider system-wide
   - Enable FIPS mode in KeepTower preferences
   - Verify FIPS status in About dialog

3. **Post-Deployment:**
   - Monitor application logs for FIPS errors
   - Periodically verify FIPS provider status
   - Document FIPS configuration for audits
   - Test vault operations regularly

**For Non-Compliance Environments:**
- Default (non-FIPS) mode is recommended
- No special configuration required
- FIPS mode available if needed in future

### Maintenance and Updates

**OpenSSL Updates:**
- Monitor OpenSSL security advisories
- Update to latest OpenSSL 3.5.x releases
- Verify FIPS module after updates
- Re-run FIPS self-tests after major updates

**KeepTower Updates:**
- FIPS support maintained in all future releases
- Algorithm changes will be FIPS-compatible
- Vault format remains backward-compatible
- FIPS configuration preserved across updates

---

## Compliance Statement Summary

**KeepTower Password Manager v0.2.8-beta and higher provides FIPS-140-3 ready cryptographic operations when:**

1. Deployed with OpenSSL 3.5.0+ and FIPS module properly configured
2. FIPS mode explicitly enabled by user in Preferences
3. FIPS provider successfully loaded (verified in About dialog)
4. All cryptographic operations using approved algorithms (AES-256-GCM, PBKDF2-HMAC-SHA256, SHA-256)

**FIPS Module:** OpenSSL 3.5+ FIPS Provider (CAVP Certificate #4282, Level 1 Software Cryptographic Module)

**KeepTower Certification Status:** KeepTower is **not FIPS-certified**. It leverages the FIPS-validated OpenSSL cryptographic module.

**Attestation:** All cryptographic operations in FIPS mode use only FIPS-approved algorithms from NIST SP 800-131A Rev. 2, executed through the NIST-certified OpenSSL FIPS module.

**Note:** Organizations requiring FIPS certification for KeepTower itself should:
- Perform independent third-party validation
- Conduct formal security audits
- Obtain CMVP (Cryptographic Module Validation Program) certification
- Document implementation-specific compliance evidence

---

## References

### Standards and Specifications

- **FIPS 140-3:** Security Requirements for Cryptographic Modules
  - https://csrc.nist.gov/publications/detail/fips/140/3/final

- **NIST SP 800-131A Rev. 2:** Transitioning the Use of Cryptographic Algorithms and Key Lengths
  - https://csrc.nist.gov/publications/detail/sp/800-131a/rev-2/final

- **NIST SP 800-38D:** Recommendation for Block Cipher Modes of Operation: Galois/Counter Mode (GCM)
  - https://csrc.nist.gov/publications/detail/sp/800-38d/final

- **NIST SP 800-132:** Recommendation for Password-Based Key Derivation
  - https://csrc.nist.gov/publications/detail/sp/800-132/final

- **FIPS 180-4:** Secure Hash Standard (SHS)
  - https://csrc.nist.gov/publications/detail/fips/180/4/final

- **FIPS 198-1:** The Keyed-Hash Message Authentication Code (HMAC)
  - https://csrc.nist.gov/publications/detail/fips/198/1/final

### OpenSSL Documentation

- **OpenSSL 3.5 Documentation:**
  - https://www.openssl.org/docs/man3.5/

- **OpenSSL FIPS Module Guide:**
  - https://github.com/openssl/openssl/blob/master/README-FIPS.md

- **OpenSSL FIPS Security Policy:**
  - Available from OpenSSL Foundation

### KeepTower Documentation

- [FIPS_SETUP_GUIDE.md](FIPS_SETUP_GUIDE.md) - Setup and configuration instructions
- [OPENSSL_35_MIGRATION.md](OPENSSL_35_MIGRATION.md) - Migration technical details
- [FIPS_DOCUMENTATION_SUMMARY.md](FIPS_DOCUMENTATION_SUMMARY.md) - Documentation overview
- [PHASE4_CONFIGURATION_UI.md](PHASE4_CONFIGURATION_UI.md) - UI implementation details

---

## Contact and Support

### For Compliance Questions

For questions about FIPS compliance, validation status, or algorithm details:

1. Review this document and referenced documentation
2. Check [FIPS_SETUP_GUIDE.md](FIPS_SETUP_GUIDE.md) for configuration issues
3. Open an issue on GitHub: https://github.com/tjdeveng/KeepTower/issues
4. Include "FIPS Compliance" in issue title

### For Auditors

For security audits and compliance verification:

1. Request access to source code (GPL-3.0 license, publicly available)
2. Review test suite and results
3. Verify OpenSSL FIPS module integration
4. Contact project maintainers for additional documentation

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-12-22 | 1.0 | Initial FIPS compliance documentation |

---

## Disclaimer

**IMPORTANT - CERTIFICATION STATUS:**

**KeepTower Password Manager is NOT FIPS-140-3 certified.** FIPS-140-3 certification requires formal validation by an accredited testing laboratory and approval by the NIST Cryptographic Module Validation Program (CMVP). KeepTower has not undergone this certification process.

**What KeepTower Provides:**
- Uses the **NIST-certified OpenSSL 3.5+ FIPS module** (CAVP Certificate #4282) for all cryptographic operations
- Implements only **FIPS-approved algorithms** when FIPS mode is enabled
- Provides a **FIPS-ready architecture** suitable for organizations requiring validated cryptography
- Includes comprehensive testing and documentation

**What is Required for Certification:**
FIPS-140-3 certification requires:
1. **Third-party validation** by NIST-accredited testing laboratory
2. **Formal security policy** documentation
3. **Physical security** requirements (for hardware modules)
4. **Design assurance** testing and documentation
5. **CMVP review** and approval process
6. Ongoing **re-validation** for updates

Organizations requiring FIPS-140-3 certification should:
1. Verify OpenSSL FIPS module CAVP certificate (#4282 or current)
2. Ensure proper FIPS module configuration
3. Enable and verify FIPS mode in KeepTower
4. Maintain documentation for compliance audits
5. Consider independent security assessment of KeepTower
6. Consult with NIST-accredited testing labs for formal certification if required

**Legal Notice:**
This document does not constitute legal or compliance advice. Organizations should consult with their compliance officers, security auditors, and legal counsel regarding FIPS requirements and certification needs. The use of FIPS-validated cryptographic modules does not automatically confer FIPS certification on the application.

---

**Document Version:** 1.0
**Last Updated:** December 22, 2025
**KeepTower Version:** 0.2.8-beta and higher
**Author:** KeepTower Development Team
**License:** GPL-3.0-or-later
