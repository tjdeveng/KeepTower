# FIPS-140-3 Setup Guide

## Overview

This guide explains how to configure KeepTower to use FIPS-140-3 validated cryptographic operations via OpenSSL 3.5+. FIPS mode is optional and designed for environments requiring cryptographic compliance.

## Requirements

### Minimum Requirements

- **OpenSSL 3.5.0 or higher** with FIPS module
- Linux operating system (Ubuntu 24.04+, Fedora 39+, or equivalent)
- KeepTower v0.2.8-beta or higher

### When to Use FIPS Mode

FIPS mode is recommended for:
- Government and military organizations
- Financial institutions
- Healthcare providers (HIPAA compliance)
- Organizations with strict security policies
- Environments requiring NIST validation

FIPS mode is **not required** for personal use or standard enterprise deployments.

## Installation Steps

### Option 1: Build OpenSSL 3.5 from Source (Recommended)

KeepTower provides a build script that compiles OpenSSL 3.5 with FIPS support:

```bash
cd /path/to/KeepTower
./scripts/build-openssl-3.5.sh
```

**This script will:**
1. Download OpenSSL 3.5.0 source
2. Compile with FIPS module enabled
3. Run FIPS self-tests (35 Known Answer Tests)
4. Install to `/usr/local/ssl` by default
5. Generate `fipsmodule.cnf` configuration

**Build time:** 5-10 minutes on modern hardware

**Installation:** After building, set environment variables:

```bash
export PKG_CONFIG_PATH=/usr/local/ssl/lib64/pkgconfig:$PKG_CONFIG_PATH
export LD_LIBRARY_PATH=/usr/local/ssl/lib64:$LD_LIBRARY_PATH
```

Add these to your `~/.bashrc` or `~/.profile` for persistence.

### Option 2: Use Distribution OpenSSL 3.5+ (If Available)

Check your distribution's OpenSSL version:

```bash
openssl version
```

If OpenSSL 3.5.0+ is available:

```bash
# Fedora (when available)
sudo dnf install openssl openssl-devel

# Ubuntu (when available in repos)
sudo apt install openssl libssl-dev
```

**Note:** As of December 2025, most distributions ship OpenSSL 3.0.x or 3.1.x. FIPS-140-3 requires OpenSSL 3.5.0+.

### Option 3: Pre-built OpenSSL 3.5 Packages

Some third-party repositories may provide OpenSSL 3.5 packages. Check your distribution's documentation.

## FIPS Module Configuration

### 1. Verify FIPS Module Installation

After installing OpenSSL 3.5, verify the FIPS module exists:

```bash
# Check for FIPS module (location varies)
ls -la /usr/local/ssl/lib64/ossl-modules/fips.so
# or
ls -la /usr/lib64/ossl-modules/fips.so
```

### 2. Generate FIPS Configuration (If Not Present)

If `fipsmodule.cnf` doesn't exist, generate it:

```bash
openssl fipsinstall -out /usr/local/ssl/fipsmodule.cnf \
    -module /usr/local/ssl/lib64/ossl-modules/fips.so
```

This runs FIPS self-tests and creates the configuration file.

### 3. Configure OpenSSL to Use FIPS Module

Create or edit `/usr/local/ssl/openssl.cnf`:

```ini
config_diagnostics = 1

openssl_conf = openssl_init

[openssl_init]
providers = provider_sect

[provider_sect]
fips = fips_sect
base = base_sect

[base_sect]
activate = 1

[fips_sect]
activate = 1
module = /usr/local/ssl/lib64/ossl-modules/fips.so
```

### 4. Set OpenSSL Configuration Environment Variable

```bash
export OPENSSL_CONF=/usr/local/ssl/openssl.cnf
```

Add to `~/.bashrc` or `~/.profile` for persistence.

### 5. Verify FIPS Provider

Test that OpenSSL recognizes the FIPS provider:

```bash
openssl list -providers
```

Expected output should include:
```
Providers:
  base
    name: OpenSSL Base Provider
    ...
  fips
    name: OpenSSL FIPS Provider
    ...
```

## Building KeepTower with FIPS Support

### Configure Build

```bash
cd /path/to/KeepTower

# Set up build with PKG_CONFIG_PATH pointing to OpenSSL 3.5
export PKG_CONFIG_PATH=/usr/local/ssl/lib64/pkgconfig:$PKG_CONFIG_PATH
meson setup build
```

Meson will detect OpenSSL 3.5 and configure accordingly. Check build output:

```
Found OpenSSL 3.5.0: YES
```

### Compile

```bash
meson compile -C build
```

### Verify FIPS Support

Run FIPS-specific tests:

```bash
meson test -C build "FIPS Mode Tests"
```

All 11 tests should pass:
```
Ok:                 11
```

## Enabling FIPS Mode in KeepTower

### Method 1: GUI (Recommended for Users)

1. Launch KeepTower
2. Open **Preferences** (hamburger menu → Preferences)
3. Navigate to **Security** tab
4. Scroll to **FIPS-140-3 Compliance** section
5. Check **"Enable FIPS-140-3 mode (requires restart)"**
6. Click **Apply**
7. **Restart KeepTower**

**Status Indicators:**
- ✓ **"FIPS module available and ready"** - FIPS can be enabled
- ⚠️ **"FIPS module not available"** - Check OpenSSL configuration

### Method 2: GSettings (Command Line)

```bash
gsettings set com.tjdeveng.keeptower fips-mode-enabled true
```

Then restart KeepTower.

### Method 3: Edit Settings File Directly

Edit `~/.config/glib-2.0/settings/keyfile`:

```ini
[com/tjdeveng/keeptower]
fips-mode-enabled=true
```

Then restart KeepTower.

## Verification

### Check FIPS Status in Application

1. Open KeepTower
2. Go to **Help → About KeepTower**
3. Check the bottom of the About dialog

Expected status messages:
- **"FIPS-140-3: Enabled ✓"** - FIPS mode active (compliant)
- **"FIPS-140-3: Available (not enabled)"** - FIPS available but not active
- **"FIPS-140-3: Not available"** - FIPS provider not configured

### Check Application Logs

KeepTower logs FIPS status at startup:

```bash
keeptower 2>&1 | grep -i fips
```

Expected output when FIPS is enabled:
```
[INFO] FIPS mode preference: enabled
[INFO] Initializing OpenSSL FIPS mode (enable=true)
[INFO] FIPS provider loaded successfully
[INFO] FIPS-140-3 provider available (enabled=true)
```

Expected output when FIPS is unavailable:
```
[INFO] FIPS mode preference: enabled
[WARN] FIPS provider not available - using default provider
[INFO] FIPS-140-3 provider not available - using default provider
```

### Test Vault Operations

Create a test vault to verify FIPS cryptography works:

1. Open KeepTower
2. Create a new vault
3. Add a test account
4. Save and close vault
5. Reopen vault with password

If all operations succeed, FIPS cryptography is working correctly.

## Troubleshooting

### Problem: "FIPS module not available" in Preferences

**Possible Causes:**
1. OpenSSL 3.5 not installed
2. FIPS module not built or installed
3. `fipsmodule.cnf` missing or invalid
4. `openssl.cnf` not configured correctly
5. `OPENSSL_CONF` environment variable not set

**Solutions:**

**Check OpenSSL version:**
```bash
openssl version
# Should show: OpenSSL 3.5.0 or higher
```

**Check FIPS module exists:**
```bash
ls -la /usr/local/ssl/lib64/ossl-modules/fips.so
```

**Verify FIPS configuration:**
```bash
openssl list -providers
# Should list both 'base' and 'fips' providers
```

**Check environment:**
```bash
echo $OPENSSL_CONF
# Should show path to openssl.cnf
echo $LD_LIBRARY_PATH
# Should include /usr/local/ssl/lib64
```

**Regenerate FIPS configuration:**
```bash
sudo openssl fipsinstall -out /usr/local/ssl/fipsmodule.cnf \
    -module /usr/local/ssl/lib64/ossl-modules/fips.so
```

### Problem: KeepTower crashes or segfaults

**Possible Causes:**
1. LD_LIBRARY_PATH not set correctly
2. Multiple OpenSSL versions conflicting
3. FIPS self-tests failing

**Solutions:**

**Verify library path:**
```bash
ldd /path/to/keeptower | grep libssl
# Should show /usr/local/ssl/lib64/libssl.so.3
```

**Check for conflicts:**
```bash
ldconfig -p | grep libssl
# Look for multiple OpenSSL versions
```

**Run with debug logging:**
```bash
export OPENSSL_CONF=/usr/local/ssl/openssl.cnf
LD_LIBRARY_PATH=/usr/local/ssl/lib64:$LD_LIBRARY_PATH keeptower
```

### Problem: FIPS mode enabled but vaults won't open

**Cause:** FIPS mode change requires application restart

**Solution:** Always restart KeepTower after changing FIPS mode setting

### Problem: Build fails with "OpenSSL >= 3.5.0 not found"

**Cause:** PKG_CONFIG_PATH not set or OpenSSL 3.5 not in pkg-config path

**Solution:**
```bash
export PKG_CONFIG_PATH=/usr/local/ssl/lib64/pkgconfig:$PKG_CONFIG_PATH
meson setup build --wipe
```

### Problem: FIPS self-tests fail during openssl fipsinstall

**Possible Causes:**
1. Corrupted build
2. Hardware incompatibility
3. Insufficient entropy

**Solutions:**

**Rebuild OpenSSL from scratch:**
```bash
cd /path/to/openssl-3.5.0
make clean
./Configure linux-x86_64 enable-fips
make -j$(nproc)
sudo make install
```

**Check system entropy:**
```bash
cat /proc/sys/kernel/random/entropy_avail
# Should be >200
```

### Problem: "OpenSSL error" in logs

**Check OpenSSL error details:**
```bash
keeptower 2>&1 | grep "OpenSSL error"
```

Common errors:
- **"error:1C800066"** - FIPS self-test failure
- **"error:25066067"** - FIPS provider not available
- **"error:0308010C"** - Unsupported algorithm in FIPS mode

**Solution:** Review OpenSSL configuration and ensure FIPS module is properly installed.

## Performance Considerations

### FIPS Mode Performance Impact

**Minimal overhead observed:**
- FIPS initialization: <10ms
- Vault encryption (100 accounts): <1ms
- Vault decryption: <20ms (PBKDF2 dominates)

**FIPS mode does NOT significantly impact:**
- Vault open/close performance
- Encryption/decryption speed
- UI responsiveness

### Benchmarking

Run performance tests:

```bash
meson test -C build "FIPS Mode Tests"
```

Check performance test output:
```
Default mode: 100 accounts saved in 0ms
```

## Security Considerations

### FIPS Mode Benefits

1. **Validated Cryptography**: All algorithms NIST FIPS 140-3 validated
2. **Compliance**: Meets government and industry requirements
3. **Auditable**: Clear cryptographic provenance

### FIPS Mode Limitations

1. **Stricter Requirements**: Some algorithms rejected (e.g., MD5, RC4)
2. **Configuration Complexity**: Requires proper OpenSSL setup
3. **Restart Required**: Mode changes need application restart

### Cryptographic Algorithms

All KeepTower algorithms are FIPS-approved:

| Algorithm | Purpose | FIPS Status |
|-----------|---------|-------------|
| AES-256-GCM | Vault encryption | ✅ Approved |
| PBKDF2-HMAC-SHA256 | Key derivation | ✅ Approved |
| SHA-256 | Hashing | ✅ Approved |
| HMAC-SHA256 | Authentication | ✅ Approved |
| DRBG (CTR) | Random generation | ✅ Approved |

No algorithm changes are needed for FIPS compliance.

## Backup and Migration

### Vault Compatibility

**FIPS and non-FIPS modes are fully compatible:**
- Vaults created in FIPS mode open in default mode
- Vaults created in default mode open in FIPS mode
- No vault format changes based on FIPS mode

### Data Migration

**No migration needed:**
- Existing vaults work with FIPS mode
- Switching FIPS mode doesn't affect vault data
- Backup/restore works across FIPS modes

### Best Practices

1. **Test FIPS mode with existing vaults before enforcing**
2. **Keep backups before enabling FIPS mode**
3. **Document FIPS configuration for disaster recovery**
4. **Test vault operations after enabling FIPS**

## Environment-Specific Setup

### Ubuntu 24.04

```bash
# Build OpenSSL 3.5 from source (no package available)
./scripts/build-openssl-3.5.sh

# Set environment
echo 'export PKG_CONFIG_PATH=/usr/local/ssl/lib64/pkgconfig:$PKG_CONFIG_PATH' >> ~/.bashrc
echo 'export LD_LIBRARY_PATH=/usr/local/ssl/lib64:$LD_LIBRARY_PATH' >> ~/.bashrc
echo 'export OPENSSL_CONF=/usr/local/ssl/openssl.cnf' >> ~/.bashrc
source ~/.bashrc

# Build KeepTower
meson setup build
meson compile -C build
```

### Fedora 39+

```bash
# Same as Ubuntu - build from source
./scripts/build-openssl-3.5.sh

# Set environment (Fedora uses lib64)
echo 'export PKG_CONFIG_PATH=/usr/local/ssl/lib64/pkgconfig:$PKG_CONFIG_PATH' >> ~/.bashrc
echo 'export LD_LIBRARY_PATH=/usr/local/ssl/lib64:$LD_LIBRARY_PATH' >> ~/.bashrc
echo 'export OPENSSL_CONF=/usr/local/ssl/openssl.cnf' >> ~/.bashrc
source ~/.bashrc

# Build KeepTower
meson setup build
meson compile -C build
```

### Docker/Container Environments

```dockerfile
FROM fedora:39

# Install dependencies
RUN dnf install -y gcc-c++ meson pkgconfig gtk4-devel \
    gtkmm40-devel protobuf-devel libcorrect-devel

# Build OpenSSL 3.5 with FIPS
COPY scripts/build-openssl-3.5.sh /tmp/
RUN /tmp/build-openssl-3.5.sh

# Set environment
ENV PKG_CONFIG_PATH=/usr/local/ssl/lib64/pkgconfig:$PKG_CONFIG_PATH
ENV LD_LIBRARY_PATH=/usr/local/ssl/lib64:$LD_LIBRARY_PATH
ENV OPENSSL_CONF=/usr/local/ssl/openssl.cnf

# Build KeepTower
COPY . /app
WORKDIR /app
RUN meson setup build && meson compile -C build
```

## Compliance Documentation

For compliance audits and certifications, see:
- [FIPS_COMPLIANCE.md](FIPS_COMPLIANCE.md) - Compliance statement
- [OPENSSL_35_MIGRATION.md](OPENSSL_35_MIGRATION.md) - Migration details
- [FIPS_DOCUMENTATION_SUMMARY.md](FIPS_DOCUMENTATION_SUMMARY.md) - Documentation overview

## Support

### Getting Help

1. Check this guide's troubleshooting section
2. Review [FIPS_COMPLIANCE.md](FIPS_COMPLIANCE.md)
3. Check application logs for error details
4. Open an issue on GitHub with:
   - OpenSSL version (`openssl version`)
   - Operating system and version
   - KeepTower version
   - Error messages from logs

### Reporting Issues

Include the following information:
```bash
# System info
uname -a
cat /etc/os-release

# OpenSSL info
openssl version
openssl list -providers

# Environment
echo $OPENSSL_CONF
echo $LD_LIBRARY_PATH
echo $PKG_CONFIG_PATH

# FIPS status
gsettings get com.tjdeveng.keeptower fips-mode-enabled

# KeepTower logs (if available)
keeptower 2>&1 | grep -i fips
```

## References

- [NIST FIPS 140-3 Standard](https://csrc.nist.gov/publications/detail/fips/140/3/final)
- [OpenSSL 3.5 Documentation](https://www.openssl.org/docs/man3.5/)
- [OpenSSL FIPS Module Guide](https://github.com/openssl/openssl/blob/master/README-FIPS.md)
- [KeepTower FIPS Compliance](FIPS_COMPLIANCE.md)

## Quick Reference

### Enable FIPS Mode
```bash
# Via gsettings
gsettings set com.tjdeveng.keeptower fips-mode-enabled true

# Restart application
keeptower
```

### Disable FIPS Mode
```bash
# Via gsettings
gsettings set com.tjdeveng.keeptower fips-mode-enabled false

# Restart application
keeptower
```

### Check FIPS Status
```bash
# Check setting
gsettings get com.tjdeveng.keeptower fips-mode-enabled

# Check runtime status (in About dialog)
# Or check logs
keeptower 2>&1 | grep "FIPS-140-3"
```

### Verify OpenSSL FIPS
```bash
# Check version
openssl version

# List providers
openssl list -providers

# Test FIPS module
openssl fipsinstall -verify -in /usr/local/ssl/fipsmodule.cnf \
    -module /usr/local/ssl/lib64/ossl-modules/fips.so
```

---

**Last Updated:** 2025-12-22
**KeepTower Version:** 0.2.8-beta
**OpenSSL Version:** 3.5.0+
