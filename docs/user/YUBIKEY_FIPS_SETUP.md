# YubiKey FIPS-140-3 Configuration Guide

**Version:** 1.0
**Date:** 2026-01-01
**Applies to:** KeepTower v0.3.0-beta and later

---

## Overview

This guide explains how to configure your YubiKey for FIPS-140-3 compliant use with KeepTower. Following these steps ensures your vault encryption uses only FIPS-approved cryptographic algorithms.

### What You'll Learn
- ✅ Check if your YubiKey supports FIPS-compliant algorithms
- ✅ Configure YubiKey Challenge-Response with HMAC-SHA256
- ✅ Verify your configuration is FIPS-compliant
- ✅ Troubleshoot common configuration issues

---

## Prerequisites

### Hardware Requirements
- **YubiKey 5 Series** (firmware 5.0 or later)
  - YubiKey 5 NFC
  - YubiKey 5 Nano
  - YubiKey 5C
  - YubiKey 5C Nano
  - YubiKey 5 FIPS ⭐ (recommended for FIPS-140-3 certified hardware)

**Note:** YubiKey 4 and earlier do NOT support HMAC-SHA256 and cannot be used in FIPS mode.

### Software Requirements
- `yubikey-manager` or `ykman` (recommended)
- OR `yubikey-personalization` tools (`ykpersonalize`, `ykinfo`)
- Linux users: Install via your package manager
- Windows users: Download from [Yubico website](https://www.yubico.com/support/download/)
- macOS users: Install via Homebrew (`brew install ykman`)

---

## Step 1: Check YubiKey Compatibility

### Using ykman (Recommended)
```bash
# Check YubiKey info
ykman info

# Expected output should show:
# Device type: YubiKey 5 ...
# Firmware version: 5.x.x (must be 5.0 or higher)
```

### Using ykinfo (Alternative)
```bash
# Check firmware version
ykinfo -v

# Expected output: version: 5.x.x
```

### Verify HMAC-SHA256 Support
```bash
# List available algorithms (ykman)
ykman otp info

# Should show: Challenge-response with both SHA-1 and SHA-256 available
```

**✅ Compatibility Check:**
- Firmware 5.0-5.3: ✅ HMAC-SHA256 supported
- Firmware 5.4+: ✅ HMAC-SHA256 supported + FIPS Edition available
- Firmware 4.x or earlier: ❌ Not compatible (SHA-1 only)

---

## Step 2: Configure YubiKey for FIPS Mode

### Option A: Using ykman (Recommended)

**Configure Slot 2 with HMAC-SHA256:**
```bash
# Program slot 2 with HMAC-SHA256 challenge-response
ykman otp chalresp --touch --generate 2

# Flags explanation:
# --touch: Require touch for each operation (security best practice)
# --generate: Generate a random secret (recommended)
# 2: Use slot 2 (slot 1 often reserved for OTP)
```

**Alternative: Use a specific secret (advanced users):**
```bash
# Use your own 20-byte hex secret
ykman otp chalresp --touch 2 <YOUR_SECRET_HEX>

# WARNING: Store this secret securely - you'll need it for backup
```

### Option B: Using ykpersonalize (Alternative)

**Configure Slot 2 with HMAC-SHA256:**
```bash
# Generate random secret and program slot 2
ykpersonalize -2 -ochal-resp -ochal-hmac -ohmac-sha256 -oserial-api-visible

# Flags explanation:
# -2: Program slot 2
# -ochal-resp: Enable challenge-response mode
# -ochal-hmac: Use HMAC algorithm
# -ohmac-sha256: Use SHA-256 (FIPS-approved)
# -oserial-api-visible: Allow serial number reading
```

**Important Notes:**
- ⚠️ This will **overwrite** any existing configuration in slot 2
- ✅ Slot 2 is recommended (slot 1 typically used for OTP)
- 🔒 The secret is generated randomly and stored on the YubiKey
- 💾 Consider backing up the secret for disaster recovery

---

## Step 3: Verify Configuration

### Quick Verification
```bash
# Check slot 2 configuration
ykman otp info

# Expected output:
# Slot 2: programmed
#   - Challenge-response (HMAC-SHA256)
```

### Detailed Verification
```bash
# Get all YubiKey information
ykinfo -a

# Should show:
# serial: <your serial number>
# version: 5.x.x
# touch_level: <configured touch policy>
# slot2: CHAL_RESP HMAC_SHA256
```

### Test Challenge-Response
```bash
# Test with a sample challenge (ykman)
echo "test_challenge_12345678901234567890123456789012" | ykman otp calculate 2 -H

# Should return: 64-character hex string (32-byte HMAC-SHA256 response)
```

---

## Step 4: Configure KeepTower

### Creating a New FIPS-Compliant Vault

1. **Launch KeepTower**
2. **Create New Vault** → Choose location
3. **Enable 2FA:** Check "Use YubiKey for 2-factor authentication"
4. **Select Algorithm:** Ensure "HMAC-SHA256" is selected (default)
5. **Touch YubiKey** when prompted
6. **Set Master Password** and complete setup

### Verifying FIPS Mode in KeepTower

Check the vault properties:
- Algorithm: `HMAC-SHA256` ✅ (FIPS-approved)
- NOT: `HMAC-SHA1` ❌ (deprecated, not FIPS-approved)

---

## FIPS-140-3 Compliance Details

### What Makes This FIPS-Compliant?

1. **Algorithm:** HMAC-SHA256 is FIPS-approved (FIPS 180-4, FIPS 198-1)
2. **Key Size:** 256-bit keys meet FIPS minimum requirements
3. **Implementation:** OpenSSL 3.5+ FIPS module for all cryptographic operations
4. **Zeroization:** Secure memory cleanup per FIPS-140-3 Section 7.9

### YubiKey FIPS Edition (Optional)

For the highest assurance:
- **YubiKey 5 FIPS** (firmware 5.4.3+)
- FIPS 140-2 Level 2 certified hardware
- Can be locked into FIPS mode (disables non-FIPS algorithms)
- Available from Yubico's enterprise store

---

## Troubleshooting

### Issue: "YubiKey not detected"

**Solution:**
```bash
# Check if YubiKey is connected
lsusb | grep Yubico

# Check permissions (Linux)
sudo usermod -aG plugdev $USER  # Log out and back in

# Test basic connectivity
ykinfo -v
```

### Issue: "Slot 2 not configured"

**Solution:**
```bash
# Check current configuration
ykman otp info

# If slot 2 shows "not configured", follow Step 2 above
```

### Issue: "HMAC-SHA256 not available"

**Cause:** Your YubiKey firmware is too old (< 5.0)

**Solution:**
- Firmware cannot be upgraded on YubiKey
- You need to purchase a YubiKey 5 Series device
- YubiKey 4 and earlier only support SHA-1 (not FIPS-compliant)

### Issue: "Touch not working"

**Solution:**
```bash
# Reconfigure with touch enabled
ykman otp chalresp --touch --generate 2

# Test touch requirement
echo "test" | ykman otp calculate 2 -H
# You should see: "Touch your YubiKey..."
```

### Issue: "Wrong response size"

**Cause:** Slot is configured with SHA-1 instead of SHA-256

**Solution:**
```bash
# Check algorithm
ykman otp info

# If showing SHA-1, reconfigure with SHA-256
ykman otp chalresp --touch --generate 2
```

---

## Security Best Practices

### 1. Use Touch Requirement ✅
Always enable touch requirement (`--touch` flag):
- Prevents malware from silently using YubiKey
- Provides physical confirmation of each operation
- Recommended by NIST and security experts

### 2. Backup Your Configuration 💾
```bash
# Export slot 2 secret (if you used ykpersonalize)
# STORE THIS SECURELY - it's your backup key
ykpersonalize -2 -ooath-hotp -oappend-cr -ofixed=<backup_secret>

# Consider:
# - Print and store in a safe
# - Split into multiple parts (Shamir's Secret Sharing)
# - Store in password manager (encrypted)
```

### 3. Physical Security 🔒
- Keep YubiKey on your person (keychain, lanyard)
- Never leave unattended with computer unlocked
- Consider multiple YubiKeys (primary + backup)

### 4. Firmware Updates
- YubiKey firmware **cannot** be updated
- Keep track of security advisories from Yubico
- Plan hardware refresh cycle (replace every 3-5 years)

---

## Migration from SHA-1 (Legacy Vaults)

If you have existing vaults using SHA-1:

### Approach 1: Create New FIPS-Compliant Vault
1. Create new vault with HMAC-SHA256 (follow Step 4)
2. Export data from old vault
3. Import into new FIPS-compliant vault
4. Securely delete old vault

### Approach 2: Continue Using SHA-1 (Not Recommended)
- ⚠️ SHA-1 is cryptographically deprecated (collision attacks)
- ❌ Cannot open in FIPS mode
- ✅ Still supported for backward compatibility
- 🔄 Plan migration to SHA-256

---

## Reference: Command Quick Reference

```bash
# Check YubiKey info
ykman info                          # Comprehensive info
ykinfo -a                          # All info (alternative)

# Configure FIPS-compliant Challenge-Response
ykman otp chalresp --touch --generate 2   # Recommended
ykpersonalize -2 -ochal-resp -ochal-hmac -ohmac-sha256  # Alternative

# Verify configuration
ykman otp info                     # Check all slots
ykinfo -a                          # Detailed info

# Test Challenge-Response
echo "test" | ykman otp calculate 2 -H

# Reset slot (WARNING: erases configuration)
ykman otp delete 2                 # ykman method
ykpersonalize -2 -z                # ykpersonalize method
```

---

## Additional Resources

- **Yubico Documentation:** https://docs.yubico.com/
- **YubiKey Manager:** https://www.yubico.com/support/download/yubikey-manager/
- **FIPS-140-3 Standard:** https://csrc.nist.gov/publications/detail/fips/140/3/final
- **KeepTower Security:** [SECURITY_BEST_PRACTICES.md](SECURITY_BEST_PRACTICES.md)
- **FIPS Compliance Audit:** [FIPS_YUBIKEY_COMPLIANCE_ISSUE.md](../audits/FIPS_YUBIKEY_COMPLIANCE_ISSUE.md)

---

## Getting Help

- **Issues:** https://github.com/your-org/keeptower/issues
- **Discussions:** https://github.com/your-org/keeptower/discussions
- **Security:** security@keeptower.example.com

---

**Last Updated:** 2026-01-01
**Document Version:** 1.0
**Maintainer:** KeepTower Security Team
