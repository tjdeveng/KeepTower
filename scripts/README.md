# KeepTower Scripts

This directory contains utility scripts for KeepTower development and maintenance.

## Available Scripts

### `cleanup.sh`

Removes KeepTower configuration and cache files. Safe to run - **never touches vault files or backups**.

**Usage:**
```bash
./scripts/cleanup.sh
```

**What it removes:**
- GSettings configuration (`com.tjdeveng.keeptower`)
- Compiled GSchema cache (`~/.local/share/glib-2.0/schemas/gschemas.compiled`)
- Application config directory (`~/.config/keeptower/`)
- dconf entries (if dconf is available)

**What it preserves:**
- ✓ Vault files (`*.vault`)
- ✓ Vault backups (`*.vault.backup`)
- ✓ All user data

**When to use:**
- After uninstalling for complete removal
- Before fresh reinstall
- Fixing corrupted settings or schema conflicts
- Resolving issues after version upgrades

**Safety:**
- Requires confirmation before executing
- Color-coded output for clarity
- Detailed summary of actions taken
- Never requires root privileges

---

### `configure-yubikey-fips.sh`

Interactive script to configure YubiKey devices for FIPS-140-3 compliant use with KeepTower.

**Usage:**
```bash
# Interactive mode (recommended)
./scripts/configure-yubikey-fips.sh

# Check only (no configuration changes)
./scripts/configure-yubikey-fips.sh --check-only

# Specify slot and options
./scripts/configure-yubikey-fips.sh --slot 2 --no-touch

# Show help
./scripts/configure-yubikey-fips.sh --help
```

**What it does:**
- Checks YubiKey firmware compatibility (requires 5.0+)
- Verifies prerequisite tools (ykman or ykpersonalize)
- Inspects current YubiKey configuration
- Configures HMAC-SHA256 challenge-response (FIPS-approved)
- Tests the configured slot with a test challenge
- Provides detailed verification output

**Options:**
- `--slot <1|2>`: Configure specific slot (default: 2)
- `--no-touch`: Disable touch requirement (not recommended)
- `--check-only`: Check configuration without making changes
- `--help`: Display detailed usage information

**When to use:**
- Initial YubiKey setup for KeepTower
- Migrating from legacy HMAC-SHA1 to FIPS-compliant HMAC-SHA256
- Verifying FIPS compliance of existing configuration
- Troubleshooting YubiKey authentication issues

**Safety:**
- Shows current configuration before making changes
- Requires confirmation before overwriting existing slots
- Supports both ykman (modern) and ykpersonalize (legacy) tools
- Generates cryptographically secure random secrets

**Documentation:**
- See [docs/user/YUBIKEY_FIPS_SETUP.md](../docs/user/YUBIKEY_FIPS_SETUP.md) for complete setup guide
- See [docs/user/05-security.md](../docs/user/05-security.md#yubikey-fips-configuration) for FIPS compliance overview

---

### `generate-help.sh`

Generates HTML help documentation from Markdown sources using pandoc.

**Usage:**
```bash
./scripts/generate-help.sh
```

**What it does:**
- Converts `docs/user/*.md` to `resources/help/*.html`
- Applies custom template and CSS styling
- Automatically called during build process

**Requirements:**
- pandoc ≥ 3.0

---

## Development Notes

All scripts:
- Use `#!/bin/bash` shebang
- Include SPDX license headers
- Are marked executable (`chmod +x`)
- Follow shell best practices (`set -e`, quoted variables)
- Provide user-friendly output with color coding
