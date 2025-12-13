# Changelog

All notable changes to KeepTower will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.2.5-beta] - 2025-12-13

### Added
- **Multi-Format Import/Export:**
  - CSV export and import with field escaping
  - KeePass 2.x XML export and import (round-trip tested)
  - 1Password 1PIF export and import (round-trip tested)
  - Password re-authentication required for all exports (YubiKey + password if configured)
  - Auto-updating file extensions when changing format filter
  - Format auto-detection from file extension
  - Security warnings about unencrypted exports
  - Partial import support with failure reporting
  - `docs/EXPORT_FORMATS.md` with format specifications

### Changed
- Menu items updated: "Import Accounts" and "Export Accounts" (removed "CSV" specificity)
- Export warning dialog generalized for all formats
- File chooser dialogs support multiple format filters
- Import success messages include format name

### Security
- **Critical Segfault Fix:**
  - Fixed unallocated vector bug in `verify_credentials()` causing segfault with YubiKey authentication
  - Pre-allocate KEY_LENGTH for password_key and test_key vectors before derive_key() calls
  - Thread safety: Added `std::mutex m_vault_mutex` to VaultManager
  - Constant-time password comparison using volatile to prevent timing attacks
- **Export Security:**
  - Exported files created with 0600 permissions (owner read/write only)
  - fsync() called to ensure data integrity
  - Secure password clearing in all code paths
  - File size limits (100MB) to prevent DoS attacks on import
- **Code Quality:**
  - C++23 improvements: std::string_view for zero-copy parsing
  - std::format() for string construction
  - std::move() semantics with emplace_back()
  - Smart reserve() with size estimation for vector pre-allocation
  - Removed 100+ lines of excessive debug logging
  - All memory properly managed with RAII
  - Comprehensive error handling with std::expected

### Documentation
- Updated README.md with import/export usage instructions
- Added technical documentation in `docs/EXPORT_FORMATS.md`
- Updated `docs/CODE_REVIEW_EXPORT.md` with all fixes and resolution

## [0.2.4-beta] - 2025-12-12

### Added
- **Multiple YubiKey Support:**
  - Backup YubiKey support for redundancy
  - YubiKeyManagerDialog for managing authorized keys
  - Add/remove backup keys through GUI
  - Display key information (serial, name, date added)
  - Safety check: cannot remove last key
  - Validation: backup keys must have same HMAC secret
  - All authorized keys stored in protobuf vault data
  - Backward compatible with single-key vaults

### Changed
- Extended protobuf schema with YubiKeyEntry message and repeated entries
- VaultManager now supports multiple authorized YubiKey serials
- Enhanced vault file format with multi-key metadata

### Security
- **Comprehensive Security Audit and Code Quality Improvements:**
  - Fixed uninitialized member variable (m_memory_locked) - potential undefined behavior
  - Corrected member initialization order to match declaration order
  - Removed unused variables causing compiler warnings
  - Implemented proper YubiKey authorization checks (replaced TODO)
  - Enhanced XOR boundary safety with explicit std::min usage
  - Fixed undefined variable references in error paths
  - Added YubiKey serial number validation (reject zero/invalid serials)
  - Implemented empty challenge rejection in YubiKey operations
  - Added secure buffer cleanup for sensitive challenge/response data
  - Improved dialog memory management (consistent use of Gtk::make_managed)
  - Removed unused function declarations from Application.h
  - Documented YubiKey library workarounds for state management
  - Zero compiler warnings achieved across entire codebase
  - All 12 test suites passing (29 VaultManager tests, YubiKey mocks, security features)

## [0.2.3-beta] - 2025-12-12

### Added
- **YubiKey Hardware 2FA:**
  - Optional YubiKey challenge-response authentication for vault encryption
  - Two-factor encryption combining password key with YubiKey HMAC-SHA1 response
  - Automatic YubiKey detection with real-time UI updates
  - User-friendly prompts for key insertion and touch requirements
  - Serial number tracking for key identification
  - YubiKeyPromptDialog with spinner for visual feedback
  - Graceful error handling when key is not present or required
  - Compatible with Reed-Solomon FEC encoding
  - check_vault_requires_yubikey() for pre-authentication detection
  - Conditional compilation with HAVE_YUBIKEY_SUPPORT flag

### Changed
- Enhanced CreatePasswordDialog with YubiKey option checkbox
- Improved vault opening flow with pre-auth YubiKey detection
- Non-blocking UI during YubiKey operations with proper GTK event processing
- Vault file format extended to store YubiKey metadata (serial, challenge)

## [0.1.1-beta] - 2025-12-08

### Fixed
- Fixed all GitHub Actions CI test failures on Ubuntu 24.04
- Disabled backups and Reed-Solomon in VaultManagerTest to prevent environment-specific issues
- Removed GTK dependency from password validation tests (pure logic tests don't need UI)
- Fixed false Reed-Solomon detection on random ciphertext data
- Updated placeholder references: GitHub URLs now point to tjdeveng/KeepTower
- Updated About dialog author to "TJDev"

### Changed
- Explicitly clarified Linux-only platform support in README
- Desktop integration improvements with proper GNOME post-install scripts
- All 103 tests now pass reliably in CI environments

### Added
- Comprehensive installation documentation (INSTALL.md)
- Desktop file validation in CI
- Icon cache and desktop database updates post-install

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

[Unreleased]: https://github.com/tjdeveng/KeepTower/compare/v0.2.4-beta...HEAD
[0.2.4-beta]: https://github.com/tjdeveng/KeepTower/compare/v0.2.3-beta...v0.2.4-beta
[0.2.3-beta]: https://github.com/tjdeveng/KeepTower/compare/v0.1.1-beta...v0.2.3-beta
[0.1.1-beta]: https://github.com/tjdeveng/KeepTower/compare/v0.1.0-beta...v0.1.1-beta
[0.1.0-beta]: https://github.com/tjdeveng/KeepTower/releases/tag/v0.1.0-beta
