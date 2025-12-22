# KeepTower Password Manager

[![CI](https://github.com/tjdeveng/KeepTower/workflows/CI/badge.svg)](https://github.com/tjdeveng/KeepTower/actions/workflows/ci.yml)
[![Build](https://github.com/tjdeveng/KeepTower/workflows/Build/badge.svg)](https://github.com/tjdeveng/KeepTower/actions/workflows/build.yml)
[![License](https://img.shields.io/badge/license-GPL--3.0--or--later-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-0.2.8--beta-orange.svg)](CHANGELOG.md)

A secure, modern password manager built with C++23 and GTK4.

## Platform Support

**Linux only** - Currently supports Ubuntu 24.04+ and Fedora 39+. Windows and macOS are not supported at this time.

## Features

- **Strong Encryption**: AES-256-GCM with authenticated encryption
- **FIPS-140-3 Ready Cryptography**: Optional FIPS-validated cryptographic operations via OpenSSL 3.5+ FIPS module (KeepTower itself is not FIPS-certified)
- **Secure Key Derivation**: PBKDF2-SHA256 with 100,000 iterations (configurable)
- **Import/Export**: Multi-format data portability
  - CSV format (tested and verified)
  - KeePass 2.x XML format (round-trip tested)
  - 1Password 1PIF format (round-trip tested)
  - Password re-authentication required for security
  - Exported files protected with 0600 permissions
- **YubiKey Hardware 2FA**: Optional hardware-based second factor authentication
  - Challenge-response HMAC-SHA1 using YubiKey slot 2
  - Two-factor encryption: password key ⊕ YubiKey response
  - Automatic key detection with user-friendly prompts
  - Serial number tracking for key identification
- **Error Correction**: Reed-Solomon forward error correction (FEC) for vault files
  - Configurable redundancy levels (5-50%)
  - Automatic corruption detection and recovery
  - Based on CCSDS RS(255,223) standard
- **Timestamped Backups**: Automatic backup creation with configurable retention
  - Keep 1-50 timestamped backups
  - Automatic cleanup of oldest backups
  - Protection against accidental data loss
- **Memory Protection**: Sensitive data secured with mlock() and OPENSSL_cleanse()
- **Atomic Operations**: Atomic file writes with automatic backups
- **Appearance Preferences**: Light, dark, or system default color schemes
- **Account Groups**: Organize accounts into expandable folders with multi-group membership
  - System "Favorites" group for quick access
  - Tree-based hierarchical display
  - Context menu for group management
- **Modern C++23**: Uses std::span, std::expected, RAII throughout
- **GTK4 Interface**: Native GNOME desktop integration
- **Input Validation**: Field length limits enforced on all inputs

## Security Features

- **FIPS-140-3 Ready**: Optional FIPS-validated cryptographic operations
  - Uses NIST-certified OpenSSL 3.5+ FIPS module
  - All algorithms FIPS-approved (AES-256-GCM, PBKDF2-HMAC-SHA256, SHA-256)
  - User-configurable in Preferences → Security
  - **Note:** KeepTower uses FIPS-validated modules but is not itself FIPS-certified
  - See [FIPS_COMPLIANCE.md](FIPS_COMPLIANCE.md) for details
- Secure memory clearing prevents data remnants
- Memory locking prevents swap file exposure
- Clipboard auto-clear (30 seconds)
- File permissions restricted to owner only
- Optional YubiKey hardware 2FA for vault encryption
- Backward-compatible vault format with versioning
- Reed-Solomon error correction protects against bit rot and corruption
- Comprehensive unit test suite (103 tests)

## Building

### Dependencies

- C++23 compatible compiler (GCC 13+ or Clang 16+)
- GTKmm 4.0 (>= 4.10) - Available in Ubuntu 24.04+, Fedora 39+
- **OpenSSL 3.5.0 or higher** (required for FIPS-140-3 support)
  - OpenSSL 1.1.0+ supported but FIPS mode unavailable
  - See [FIPS_SETUP_GUIDE.md](FIPS_SETUP_GUIDE.md) for FIPS configuration
- Protocol Buffers (>= 3.0)
- libcorrect (for Reed-Solomon error correction)
  - Fedora: `dnf install libcorrect-devel`
  yubikey-personalization (optional, for YubiKey 2FA support)
  - Fedora: `dnf install ykpers-devel`
  - Ubuntu: `apt install libykpers-1-dev`
- - Ubuntu: Build from source (see CI workflows for instructions)
- Meson build system
- GTest (for tests)

### Supported Platforms

- Ubuntu 24.04 LTS or newer
- Fedora 39 or newer
- Other Linux distributions with GTKmm 4.0+ available

### Compile

**Note:** If OpenSSL 3.5+ is not found on your system, the build system will automatically download and compile it (takes 5-10 minutes on first build, then cached).

```bash
meson setup build
meson compile -C build
```

You can also manually pre-build OpenSSL 3.5:
```bash
bash scripts/build-openssl-3.5.sh
```

### Run Tests

```bash
meson test -C build
```

### Generate API Documentation

Doxygen API documentation can be generated with:

```bash
doxygen Doxyfile
```

The HTML documentation will be created in `docs/api/html/`. Open `docs/api/html/index.html` in your browser to view it.

## Installation

```bash
meson install -C build
```

## Usage

```bash
### Creating YubiKey-Protected Vaults

To create a vault with YubiKey hardware 2FA:

1. Ensure your YubiKey is connected and configured with HMAC-SHA1 challenge-response in slot 2
   ```bash
   # Configure slot 2 for HMAC-SHA1 (one-time setup)
   ykpersonalize -2 -ochal-resp -ochal-hmac -ohmac-lt64 -oserial-api-visible
   ```
2. Click **New Vault** and create a strong password
3. Check **"Require YubiKey for vault access"** (appears when YubiKey is detected)
4. Touch your YubiKey when prompted during vault creation
5. The vault will now require both your password AND the YubiKey to open

**Important**: Keep your YubiKey safe! If lost, you will need both a backup YubiKey (programmed with the same secret) or the vault cannot be opened.

### Managing Backup YubiKeys

KeepTower supports multiple YubiKeys for redundancy. This protects against losing access if your primary key is lost or damaged.

To add a backup YubiKey:

1. Open a YubiKey-protected vault with your primary key
2. Select **Menu → Manage YubiKeys**
3. Insert your backup YubiKey (must be programmed with the **same HMAC secret** as the primary)
4. Click **Add Current YubiKey** and give it a name (e.g., "Backup", "Office Key")
5. Touch the YubiKey when prompted to verify it works
6. Your backup key can now open the vault

**Programming Multiple Keys with Same Secret:**
```bash
# Save the secret from your primary key (64 hex characters)
# Then program the backup key with the SAME secret:
ykpersonalize -2 -ochal-resp -ochal-hmac -ohmac-lt64 -oserial-api-visible -a<YOUR_SECRET_HERE>
```

**Note**: All backup keys **must** be programmed with the same HMAC-SHA1 secret. If a key has a different secret, it will be rejected when you try to add it.

### Importing and Exporting Accounts

KeepTower supports importing and exporting password data in multiple formats:

#### Exporting Accounts

1. Open a vault with your accounts
2. Select **Menu → Export Accounts**
3. Review the security warning (all exports are unencrypted plaintext)
4. Re-authenticate with your password (and YubiKey if configured)
5. Choose the export format:
   - **CSV** - Simple comma-separated format (fully tested)
   - **KeePass XML** - KeePass 2.x compatible XML (round-trip tested)
   - **1Password 1PIF** - 1Password Interchange Format (round-trip tested)
6. Select destination and save

**Security Warning**: All export formats save passwords in UNENCRYPTED plaintext. The file is NOT encrypted. Anyone with access can read all your passwords. Delete exported files immediately after use.

**File Protection**: Exported files are created with 0600 permissions (owner read/write only) for additional security.

#### Importing Accounts

1. Open a vault
2. Select **Menu → Import Accounts**
3. Choose the file format (CSV, KeePass XML, or 1Password 1PIF)
4. Select the file to import
5. Review the import results
   - Success count and any failures are shown
   - Failed imports are listed with account names
   - Duplicate accounts are added as separate entries

**Supported Import Formats**:
- CSV with header: `Account Name,Username,Password,Email,Website,Notes`
- KeePass 2.x XML (unencrypted export)
- 1Password 1PIF format

**Round-Trip Testing**: All formats have been tested for round-trip integrity (export → import → verify). Compatibility with actual KeePass and 1Password applications is pending real-world testing.

For detailed format specifications, see `docs/EXPORT_FORMATS.md`.

### Power User Tips
```

### Configuring Preferences

Open **Preferences** from the toolbar to configure:

#### Appearance
- **Color Scheme**: Choose between System Default, Light, or Dark mode
- Changes apply immediately with live preview

#### Reed-Solomon Error Correction
1. Enable **"Enable error correction for new vaults"**
2. Adjust the **Redundancy Level** slider (5-50%)
   - Higher values provide more protection but increase file size
   - 10% is recommended for most users
   - 20-30% for critical data
3. New vaults created after enabling will include error correction

#### Automatic Backups
1. Enable **"Enable automatic backups"** (enabled by default)
2. Set **"Keep up to"** number of backups (1-50, default 5)
   - Older backups are automatically deleted when limit is exceeded
   - Backup files are named with timestamps: `vault.vault.backup.20251207_183045_123`
   - Provides protection against accidental data loss
3. Each save operation creates a new timestamped backup

Click **Apply** to save all preference changes.

## Documentation

- **README.md** - This file
- **docs/EXPORT_FORMATS.md** - Import/export format specifications and security notes
- **docs/api/** - Doxygen API documentation (generated)
- **CODE_REVIEW.md** - Security audit and code review
- **CONTRIBUTING.md** - Contribution guidelines
- **SECURITY.md** - Security policy and vulnerability reporting

## License

Copyright (C) 2025 tjdeveng

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>.

## Third-Party Licenses

KeepTower uses the following open source libraries:

- **GTKmm**: LGPL 2.1 or later
- **OpenSSL**: Apache License 2.0
- **Protocol Buffers**: BSD 3-Clause License
- **Google Test**: BSD 3-Clause License
- **libcorrect**: BSD 3-Clause License

## Contributing

Contributions are welcome! Please ensure:

1. Code follows C++23 best practices
2. All tests pass
3. New features include tests
4. Security-related changes are documented

## Security

For security issues, please contact the maintainer directly rather than using the public issue tracker.

## Architecture

- **Core**: VaultManager handles encryption, key derivation, and vault operations
- **UI**: GTK4-based interface with password dialogs
- **Storage**: Protobuf-based vault format with versioning
- **Tests**: Comprehensive unit and security test suite

## Version

v0.2.6-beta
>= 0.59.0
>= 4.10
>= 3.0
>= 1.1.0 - Tags system with filtering, password history limit control
