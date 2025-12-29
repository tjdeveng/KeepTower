# Welcome to KeepTower 🗝️

**A secure, modern password manager for Linux with military-grade encryption and built-in error correction.**

## What is KeepTower?

KeepTower is a native GTK4 password manager designed for Linux users who value security, privacy, and reliability. Your passwords are encrypted locally with AES-256-GCM and protected by optional Reed-Solomon error correction—ensuring your data survives bit rot and storage corruption.

## Key Features

✨ **Military-Grade Encryption** - AES-256-GCM with PBKDF2 key derivation
� **Multi-User Support** - V2 vaults with role-based access (Admin/Standard)
🛡️ **Error Correction** - Reed-Solomon FEC (5-50% redundancy) protects against data corruption
💾 **Automatic Backups** - Configurable rolling backups keep your data safe
🎨 **Modern UI** - Clean GTK4/libadwaita interface with dark mode support
🔒 **Memory Protection** - Sensitive data locked in RAM, never swapped to disk
🔐 **Strong Password Validation** - Prevents weak passwords and common patterns
🔍 **Password History** - Prevents password reuse with configurable depth
📦 **Zero Dependencies on Cloud** - Your data stays on your machine

## Why KeepTower?

| Feature | KeepTower | Others |
|---------|-----------|--------|
| **Local-first** | ✅ Always | ⚠️ Often cloud-dependent |
| **Forward Error Correction** | ✅ Built-in | ❌ Not available |
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
- **[[FAQ]]** - Common questions answered
- **[[Contributing]]** - Help improve KeepTower

## Status

🚧 **Current Version:** v0.1.1-beta
📅 **Released:** December 2025
✅ **Tests:** All passing
🎯 **Stability:** Beta (ready for testing, not production-critical data yet)

## Community

- 🐛 **Report Issues:** [GitHub Issues](https://github.com/tjdeveng/KeepTower/issues)
- 💬 **Discussions:** [GitHub Discussions](https://github.com/tjdeveng/KeepTower/discussions)
- 🤝 **Contributing:** See [[Contributing]]

## Philosophy

KeepTower is built on these principles:

1. **Privacy First** - Your data never leaves your machine unless you choose
2. **Reliability** - Error correction protects against bitrot and hardware failures
3. **Simplicity** - Clean UI without sacrificing power features
4. **Transparency** - Open source, auditable code
5. **Linux Native** - Deep integration with the Linux desktop

## License

KeepTower is free software licensed under **GPL-3.0-or-later**.

---

**Ready to get started?** Head to **[[Installation]]** →
