# OpenSSL 3.5 + FIPS-140-3 Migration Plan

## Overview

This document outlines the migration strategy for upgrading KeepTower to OpenSSL 3.5 with FIPS-140-3 compliance support.

## Current State (v0.2.8-beta)

- **OpenSSL Version**: 3.2.6 (local), varies by CI environment
- **FIPS Status**: Not FIPS-compliant
- **Cryptographic Operations**:
  - AES-256-GCM encryption/decryption
  - PBKDF2-HMAC-SHA256 key derivation (100,000 iterations default)
  - Secure random generation (RAND_bytes)
  - EVP API (OpenSSL 3.x compatible)

## Target State

- **OpenSSL Version**: 3.5.0+ (REQUIRED)
- **FIPS Status**: FIPS-140-3 ready (optional runtime activation)
- **Provider Architecture**: OpenSSL 3.5 provider model
- **Backward Compatibility**: OpenSSL 3.5+ only - no legacy support

## FIPS-140-3 Requirements

### Approved Algorithms (Currently Used)
✅ **AES-256-GCM**: FIPS-approved authenticated encryption
✅ **SHA-256**: FIPS-approved hash function (via PBKDF2)
✅ **PBKDF2-HMAC-SHA256**: FIPS-approved key derivation
✅ **Random Generation**: Must use FIPS-approved DRBG

### Additional Requirements
- ⚠️ **Self-tests**: OpenSSL FIPS module includes automatic self-tests
- ⚠️ **Integrity Checks**: FIPS module verifies its own integrity
- ⚠️ **Approved Mode**: Must be explicitly enabled at runtime
- ⚠️ **Key Size Minimums**: Already compliant (256-bit keys)
- ⚠️ **IV Generation**: Must use approved random generator

## Build Strategy

### System OpenSSL Detection

```meson
# OpenSSL 3.5+ required for FIPS-140-3 support
# Build from source if not available: scripts/build-openssl-3.5.sh
openssl_dep = dependency('openssl', version: '>= 3.5.0', required: true)
```

**No fallback support** - OpenSSL 3.5+ is mandatory. Systems without OpenSSL 3.5+
must build from source using the provided build script.

### Custom Build Option

For CI environments and users requiring FIPS support:

1. **Detect system OpenSSL version**
2. **If < 3.5.0**: Build OpenSSL 3.5 from source using `scripts/build-openssl-3.5.sh`
3. **Cache build**: Store in CI cache or `/opt/openssl-3.5` locally
4. **Set PKG_CONFIG_PATH**: Point to custom build

### Distribution Strategy

- **Fedora 43+**: Use system OpenSSL 3.5
- **Fedora 42, Ubuntu 24.04**: Build from source
- **Future releases**: System packages as they become available

## Migration Tasks

### Phase 1: Build Infrastructure ✅
- [x] Create `scripts/build-openssl-3.5.sh` build script
- [x] Update `meson.build` for version detection
- [x] Add OpenSSL version check at compile time
- [x] Update CI workflows for conditional building
- [x] Add caching for custom OpenSSL builds

### Phase 2: Code Migration 🔄
- [x] Audit all OpenSSL API usage
- [x] Add FIPS provider initialization (optional)
- [x] Add static FIPS mode state tracking
- [x] Implement `VaultManager::init_fips_mode()`
- [x] Implement `VaultManager::is_fips_available()`
- [x] Implement `VaultManager::is_fips_enabled()`
- [x] Implement `VaultManager::set_fips_mode()`
- [x] Initialize FIPS mode in Application::on_startup()
- [x] Update EVP context creation for provider model (already compatible)
- [x] Verify RAND_bytes uses FIPS-approved DRBG in FIPS mode
- [x] Add runtime FIPS mode detection/logging
- [x] Update error handling for provider errors

### Phase 3: Testing ✅
- [x] Test with OpenSSL 3.5 in non-FIPS mode
- [x] Test with FIPS mode enabled (when available)
- [x] Verify all crypto operations work in FIPS mode
- [x] Test vault open/save with FIPS provider
- [x] Verify backward compatibility with existing vaults
- [x] Performance testing (FIPS vs non-FIPS)
- [x] Created comprehensive test suite (test_fips_mode.cc)
- [x] 11 FIPS-specific tests, all passing
- [x] Existing tests still pass (18/19 - unrelated failure)

### Phase 4: Configuration
- [ ] Add FIPS mode preference to settings
- [ ] Document FIPS mode activation
- [ ] Add FIPS status indicator in UI
- [ ] Create FIPS configuration guide
- [ ] Add runtime FIPS verification

### Phase 5: Documentation
- [ ] Update README with FIPS requirements
- [ ] Create FIPS compliance documentation
- [ ] Update build instructions for OpenSSL 3.5
- [ ] Document FIPS mode limitations (if any)
- [ ] Security audit documentation

## API Changes Required

### Provider Initialization (New)

```cpp
// In VaultManager constructor - optional FIPS activation
void VaultManager::init_openssl_fips() {
    #ifdef OPENSSL_VERSION_MAJOR >= 3 && OPENSSL_VERSION_MINOR >= 5
    if (m_fips_mode_enabled) {
        if (OSSL_PROVIDER_load(nullptr, "fips") == nullptr) {
            Log::error("Failed to load FIPS provider");
            // Fall back to default provider
            OSSL_PROVIDER_load(nullptr, "default");
        } else {
            Log::info("FIPS provider loaded successfully");
            // Verify FIPS mode is active
            if (EVP_default_properties_is_fips_enabled(nullptr)) {
                Log::info("FIPS mode activated");
            }
        }
    }
    #endif
}
```

### No Changes Required (Already Compatible)
- `EVP_CIPHER_CTX_new()` / `EVP_CIPHER_CTX_free()`
- `EVP_aes_256_gcm()`
- `EVP_sha256()`
- `PKCS5_PBKDF2_HMAC()`
- `RAND_bytes()`
- `EVP_EncryptInit_ex()` / `EVP_DecryptInit_ex()`
- `EVP_EncryptUpdate()` / `EVP_DecryptUpdate()`
- `EVP_EncryptFinal_ex()` / `EVP_DecryptFinal_ex()`

## Configuration File Changes

### New GSettings Schema

```xml
<key name="fips-mode-enabled" type="b">
  <default>false</default>
  <summary>Enable FIPS 140-3 Mode</summary>
  <description>
    Enable FIPS 140-3 compliant cryptographic operations.
    Requires OpenSSL 3.5+ with FIPS module installed.
    Changes take effect after restarting the application.
  </description>
</key>
```

### Preferences Dialog

Add new "Security" → "FIPS Compliance" section:
- Checkbox: "Enable FIPS 140-3 Mode (requires restart)"
- Label: "Status: FIPS Available / Not Available"
- Warning: "FIPS mode may impact performance"

## Compatibility Matrix

| OpenSSL Version | FIPS Support | Status |
|-----------------|--------------|--------|
| 3.5.0+          | ✅ Yes       | **REQUIRED** |
| 3.0.x - 3.4.x   | ❌ No        | ❌ Not Supported |
| 1.1.1           | ❌ No        | ❌ Not Supported |

**Note**: KeepTower requires OpenSSL 3.5+ for FIPS-140-3 compliance.
Older versions are not supported.

## Testing Strategy

### Unit Tests
- Test with FIPS mode enabled
- Test with FIPS mode disabled
- Test provider switching
- Verify error handling

### Integration Tests
- Open vault with FIPS mode
- Save vault with FIPS mode
- Encrypt/decrypt with FIPS provider
- Key derivation with FIPS provider

### CI/CD Tests
- Build with system OpenSSL 3.5 (Fedora 43)
- Build with custom OpenSSL 3.5 (Ubuntu 24.04)
- Run all tests in FIPS mode
- Run all tests in non-FIPS mode

## Performance Considerations

FIPS mode typically adds 5-15% overhead due to:
- Additional self-tests
- Integrity verification
- Stricter compliance checks

This is acceptable for a password manager prioritizing security.

## Security Considerations

### Benefits
- FIPS-140-3 certification ready
- Government/enterprise compliance
- Audited cryptographic module
- Stronger security guarantees

### Limitations
- FIPS mode restricts algorithm choices
- Cannot use non-approved algorithms
- Performance overhead
- Complexity in configuration

## Timeline

- **Week 1**: Build infrastructure and CI integration
- **Week 2**: Code migration and provider support
- **Week 3**: Testing and validation
- **Week 4**: Documentation and release

## References

- [OpenSSL 3.5 Release Notes](https://www.openssl.org/news/openssl-3.5-notes.html)
- [FIPS 140-3 Implementation Guidance](https://csrc.nist.gov/projects/fips-140-3)
- [OpenSSL FIPS Provider Documentation](https://github.com/openssl/openssl/blob/master/README-FIPS.md)
- [OpenSSL 3.0 Migration Guide](https://www.openssl.org/docs/man3.0/man7/migration_guide.html)

## Status

**Current Phase**: Phase 1 - Build Infrastructure
**Last Updated**: December 22, 2025
**Target Completion**: January 2026
