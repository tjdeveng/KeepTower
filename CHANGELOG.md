# Changelog

All notable changes to KeepTower will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0-beta] - 2025-12-07

### Added
- Initial beta release of KeepTower Password Manager
- **Encryption & Security:**
  - AES-256-GCM authenticated encryption
  - PBKDF2-SHA256 key derivation with 100,000 iterations (configurable)
  - Secure memory clearing using OPENSSL_cleanse()
  - Memory locking (mlock) to prevent swap file exposure
  - Atomic file writes with automatic backup creation
  - File permissions restricted to owner only (0600)

- **Password Management:**
  - NIST SP 800-63B compliant password validation
  - Common password dictionary checking (10,000+ entries)
  - Password strength calculation
  - Clipboard auto-clear after 30 seconds

- **Input Validation:**
  - Field length limits enforced on all inputs
  - Account name: 256 characters max
  - Username: 256 characters max
  - Password: 512 characters max
  - Email: 256 characters max
  - Website: 512 characters max
  - Notes: 1000 characters max

- **Vault Management:**
  - Create and manage multiple encrypted vaults
  - Store passwords with associated metadata (username, email, website, notes)
  - Backward-compatible vault format with versioning
  - Magic header for file type identification

- **User Interface:**
  - Modern GTK4-based interface
  - Native GNOME desktop integration
  - Password creation dialog with strength indicators
  - Password visibility toggle
  - Account list management

- **Build System:**
  - Meson build system configuration
  - Desktop file integration
  - AppStream metadata
  - GSettings schema
  - Internationalization support (framework)

- **Testing:**
  - Comprehensive unit test suite (77+ tests)
  - VaultManager tests (encryption, file I/O, error handling)
  - Password validation tests (NIST compliance, strength calculation)
  - Input validation tests (boundary conditions, field limits)
  - Security feature tests (magic header, backups, iterations)

- **Documentation:**
  - README.md with building and usage instructions
  - GPL-3.0-or-later licensing
  - SPDX-compliant file headers throughout
  - CODE_REVIEW.md with security audit findings

### Security
- Implemented all 21 recommendations from CODE_REVIEW.md
- Memory protection for sensitive data
- Secure password requirements enforcement
- Protection against common password attacks

### Known Limitations
- No internationalization translations (framework only)
- No preferences dialog yet
- No save-on-close prompts
- No multi-vault switching in same session

[Unreleased]: https://github.com/yourusername/keeptower/compare/v0.1.0-beta...HEAD
[0.1.0-beta]: https://github.com/yourusername/keeptower/releases/tag/v0.1.0-beta
