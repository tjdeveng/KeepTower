# Welcome to KeepTower 🗝️

**A secure, modern password manager for Linux with military-grade encryption and built-in error correction.**

## What is KeepTower?

KeepTower is a native GTK4 password manager designed for Linux users who value security, privacy, and reliability. Your passwords are encrypted locally with AES-256-GCM and protected by optional Reed-Solomon error correction—ensuring your data survives bit rot and storage corruption.

## Key Features

✨ **Military-Grade Encryption** - AES-256-GCM with PBKDF2 key derivation
🏛️ **FIPS 140-3 Ready** - Uses FIPS-approved algorithms, OpenSSL FIPS provider support
👥 **Multi-User Support** - V2 vaults with role-based access (Admin/Standard)
🔐 **YubiKey Hardware 2FA** - Optional hardware-based second factor authentication
📤 **Import/Export** - CSV, KeePass XML, and 1Password 1PIF formats
🛡️ **Error Correction** - Reed-Solomon FEC (5-50% redundancy) protects against data corruption
💾 **Automatic Backups** - Configurable rolling backups keep your data safe
�� **Modern UI** - Clean GTK4/libadwaita interface with dark mode support
🔒 **Memory Protection** - Sensitive data locked in RAM, never swapped to disk
🔐 **Strong Password Validation** - Prevents weak passwords and common patterns
🔍 **Password History** - Prevents password reuse with configurable depth
📦 **Zero Dependencies on Cloud** - Your data stays on your machine

## Why KeepTower?

| Feature | KeepTower | Others |
|---------|-----------|--------|
| **Local-first** | ✅ Always | ⚠️ Often cloud-dependent |
| **FIPS 140-3 Ready** | ✅ FIPS-approved algorithms | ❌ Not typically |
| **Multi-User Vaults** | ✅ Role-based access | ❌ Single-user only |
| **Forward Error Correction** | ✅ Built-in | ❌ Not available |
| **Hardware 2FA** | ✅ YubiKey support | ⚠️ Limited or none |
| **Multi-format Import/Export** | ✅ CSV, KeePass, 1Password | ⚠️ Varies |
| **Native Performance** | ✅ C++/GTK4 | ⚠️ Electron/web-based |
| **Open Source** | ✅ GPL-3.0 | ⚠️ Varies |
| **Linux Integration** | ✅ Deep (libadwaita) | ⚠️ Generic |

## Quick Start

```bash
# Install dependencies (Fedora/RHEL)
sudo dnf install gtkmm4.0-devel protobuf-devel openssl-devel libcorrect-devel

# Build
meson setup build
meson compile -C build

# Run
./build/src/keeptower
```

See **[[Installation]]** for detailed instructions for your distribution.

## Documentation

- **[[Installation]]** - Installation instructions for different distributions
- **[[Getting Started]]** - Create your first vault
- **[[User Guide]]** - Complete feature walkthrough
- **[[Security]]** - Encryption details and threat model
- **[[FAQ]]** - Frequently asked questions
- **[[Contributing]]** - How to contribute to KeepTower

## Version

**Current Version:** v0.3.4
**Release Date:** January 21, 2026

### Recent Updates (v0.3.4)

- ✅ Per-keyslot KEK derivation algorithm selection (PBKDF2-SHA256/SHA512, Argon2id)
- ✅ UI integration for KEK algorithm selection with FIPS-140 compliance
- ✅ Username hashing with configurable algorithms (SHA3-256, PBKDF2, Argon2id)
- ✅ Display current vault's security algorithms in Preferences
- ✅ Comprehensive vault security algorithms documentation
- ✅ GCC 13 compatibility fixes for broader platform support

## Support

- **Issues:** [GitHub Issues](https://github.com/tjdeveng/KeepTower/issues)
- **Security:** See [[Security]] for reporting security vulnerabilities
- **Contributing:** See [[Contributing]] for development guidelines

## License

KeepTower is free and open-source software licensed under the GNU General Public License v3.0 or later (GPL-3.0-or-later).
