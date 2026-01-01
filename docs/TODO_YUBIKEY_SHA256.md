# TODO: YubiKey SHA-256 Implementation

## Status: CRITICAL - BLOCKING FIPS COMPLIANCE

### Problem
- FIPS-140-3 requires SHA-256 (or stronger) algorithms
- Current `ykpers` library only supports SHA-1 (deprecated, not FIPS-approved)
- YubiKey hardware (5.x+) supports SHA-256 but software doesn't

### Impact
- **Cannot create new YubiKey-protected vaults in FIPS mode**
- Old vaults with backward compatibility should work (121-byte format detection implemented)
- Non-YubiKey vaults work fine

### Required Action (Tomorrow - 2 January 2026)

#### Option 1: Migrate to yubikey-manager (RECOMMENDED)
- **Library**: `yubikey-manager` (ykman) Python library
- **Pros**: Official Yubico library, supports SHA-256, actively maintained
- **Cons**: Python dependency, requires bindings or subprocess calls
- **Effort**: Medium (2-3 hours)

#### Option 2: Direct PCSC Access
- **Library**: `libpcsclite` + custom implementation
- **Pros**: Native C++, full control
- **Cons**: Complex, requires APDU command knowledge
- **Effort**: High (1-2 days)

#### Option 3: libfido2
- **Library**: `libfido2` 
- **Pros**: C library, modern, supports FIDO2/WebAuthn
- **Cons**: May not support HMAC challenge-response, overkill
- **Effort**: Medium-High

#### Option 4: ykman subprocess
- **Library**: Call `ykman` CLI as subprocess
- **Pros**: Quick, no bindings needed
- **Cons**: Requires ykman installed, subprocess overhead
- **Effort**: Low (1 hour)

### Recommended Approach
1. **Short-term (tomorrow morning)**: Implement Option 4 (ykman subprocess)
   - Fast to implement
   - Gets YubiKey working in FIPS mode
   - Can be replaced later

2. **Long-term (next week)**: Migrate to Option 1 (yubikey-manager)
   - Proper integration
   - Better performance
   - More maintainable

### Files to Modify
- `src/core/managers/YubiKeyManager.cc` - Replace ykpers calls
- `src/core/managers/YubiKeyManager.h` - Update interface if needed
- `meson.build` - Update dependencies
- `docs/BUILDING.md` - Update build instructions

### Testing Required
- Create new vault with YubiKey + SHA-256
- Open old vaults (backward compatibility)
- Test on FIPS YubiKey 5 device
- Verify FIPS audit still passes

### Current Workaround
Users can:
1. Create vaults WITHOUT YubiKey protection (FIPS-compliant encryption still works)
2. Wait for update tomorrow
3. Temporarily disable FIPS mode (not recommended)

### Code References
- SHA-256 error: `src/core/managers/YubiKeyManager.cc:285-295`
- Default algorithm: `src/core/MultiUserTypes.h:195`
- Initialization: `src/core/managers/YubiKeyManager.cc:54-72`

## Priority: P0 (CRITICAL)
This blocks a core feature (YubiKey-protected vaults) in FIPS mode.
