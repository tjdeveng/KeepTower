# KeepTower Development Roadmap

This document outlines the planned features and improvements for KeepTower, organized by release milestones.

## Current Version: v0.2.0-beta (In Development)

✅ Core vault management with AES-256-GCM encryption
✅ Reed-Solomon forward error correction (5-50% redundancy)
✅ Automatic backup system
✅ Password strength validation
✅ FEC preferences with per-vault settings
✅ GTK4/libadwaita UI with dark mode support
✅ **NEW: Configurable clipboard auto-clear (5-300 seconds)**
✅ **NEW: Auto-lock after inactivity (60-3600 seconds)**
✅ **NEW: Password history tracking (UI ready, backend pending)**
✅ **NEW: Modern sidebar preferences dialog (GNOME HIG compliant)**
✅ **NEW: Activity monitoring for security features**
✅ **NEW: Session timeout with re-authentication**

---

## Near-term (v0.2.x - Security Quick Wins) - IN PROGRESS

### Security Enhancements
- [x] Clipboard auto-clear after paste (configurable 5-300s delay)
- [x] Auto-lock after configurable inactivity timeout (60-3600s)
- [x] Session timeout with re-authentication (integrated with auto-lock)
- [x] Password history tracking per account (prevents reuse) - **Completed**
- [ ] Two-factor authentication support (TOTP/HOTP code generation) - *requires hardware*
- [ ] Biometric unlock support (fingerprint via polkit) - *requires hardware access*

### Usability Improvements
- [x] Import functionality (KeePass XML, 1Password 1PIF, CSV formats) - **v0.2.5-beta**
- [x] Export functionality with security warnings - **v0.2.5-beta**
- [x] Enhanced password generator with customizable rules - **Completed**
- [x] Tags system for organizing accounts - **v0.2.6-beta**
- [ ] Browser extension for auto-fill (Firefox, Chrome)
- [ ] Favorites/starred accounts for quick access
- [ ] Tag-based filtering in account list
- [ ] Advanced search (fuzzy search, filters by field)
- [ ] Undo/redo support for vault operations
- [ ] Drag-and-drop account reordering

### Data Features
- [ ] Custom fields per account (key-value pairs)
- [ ] File attachments (encrypted documents, notes, images)
- [ ] Multi-vault support (open/switch between multiple vaults)
- [ ] Vault merge capabilities
- [ ] Account groups/folders for organization
- [ ] Bulk operations (edit, delete, move)

---

## Mid-term (v0.3.x - Polish & Integration)

### User Experience
- [ ] Refined dark/light theme with accent colors
- [ ] Comprehensive keyboard shortcuts
- [ ] Keyboard shortcuts help dialog (Ctrl+?)
- [ ] First-run wizard with security best practices
- [ ] Vault templates (personal, business, family)
- [ ] Trash/recycle bin for deleted accounts
- [ ] Account duplication feature
- [ ] Recently accessed accounts list
- [ ] Account usage statistics

### Distribution & Packaging
- [ ] Flatpak packaging for Flathub
- [ ] AppImage distribution
- [ ] Snap package
- [ ] AUR package for Arch Linux
- [ ] RPM packaging improvements
- [ ] Debian/Ubuntu PPA

### Cross-platform Considerations
- [ ] Evaluate Windows port feasibility
- [ ] Evaluate macOS port feasibility
- [ ] Abstract platform-specific code

---

## Long-term (v0.4.x+ - Advanced Features)

### Cloud & Synchronization
- [ ] Optional cloud sync support
- [ ] Self-hosted sync server implementation
- [ ] Conflict resolution UI with merge strategies
- [ ] Offline-first architecture with sync reconciliation
- [ ] End-to-end encrypted sync protocol
- [ ] Delta sync for bandwidth efficiency
- [ ] Sync history and rollback

### Collaboration Features
- [ ] Shared vaults (family/team use cases)
- [ ] Permission levels (read-only, edit, admin)
- [ ] Audit logs for shared vaults
- [ ] Activity notifications
- [ ] Access revocation system

### Advanced Security
- [ ] Hardware security key support (YubiKey, FIDO2)
- [ ] Secure password sharing (time-limited, one-time access)
- [ ] Breach monitoring integration (HaveIBeenPwned API)
- [ ] Security reports (weak passwords, reused passwords, old passwords)
- [ ] Password expiration policies
- [ ] Emergency access system
- [ ] Vault integrity monitoring

### Enterprise Features
- [ ] LDAP/Active Directory integration
- [ ] SSO support
- [ ] Centralized policy management
- [ ] Compliance reporting (audit trails)
- [ ] Vault access monitoring dashboard

---

## Technical Debt & Quality (Ongoing)

### Throughout All Versions
- [ ] Maintain >80% test coverage for all new features
- [ ] Performance optimization for vaults with 1000+ accounts
- [ ] Memory usage profiling and optimization
- [ ] Accessibility improvements (WCAG 2.1 Level AA)
- [ ] Full screen reader support
- [ ] Comprehensive keyboard navigation
- [ ] Internationalization (i18n) framework
- [ ] Translations (Spanish, French, German, Japanese, Chinese)
- [ ] Complete API documentation (Doxygen)
- [ ] User manual and help system
- [ ] Video tutorials and screencasts
- [ ] Developer documentation for contributors

### Code Quality
- [ ] Static analysis integration (clang-tidy, cppcheck)
- [ ] Continuous fuzzing setup (OSS-Fuzz)
- [ ] Code coverage tracking (Codecov)
- [ ] Performance benchmarking suite
- [ ] Automated security scanning (CodeQL)

---

## Quick Wins (Potential Next Sprint)

These features provide high value with relatively low implementation effort:

1. **Password strength indicator** - Visual feedback during password creation
2. **Recently used accounts** - Quick access list on main window
3. **Keyboard shortcuts** - Ctrl+C for copy password, Ctrl+F for search, etc.
4. **Account notes field** - Store security questions, recovery info
5. **Auto-backup verification** - Test restore integrity on backup creation
6. **Show/hide password toggle** - Eye icon in password fields
7. **Password generation in account dialog** - One-click generation
8. **Account creation timestamp** - Display "Created" date
9. **Vault statistics** - Account count, last modified, file size
10. **Export single account** - Share one account securely

---

## Community & Project Management

### Short-term
- [ ] Contributing guidelines (CONTRIBUTING.md)
- [ ] Code of conduct
- [ ] Issue templates (bug report, feature request)
- [ ] Pull request template
- [ ] Roadmap voting/feedback system

### Medium-term
- [ ] Project website
- [ ] Community forum or Discord server
- [ ] Regular release schedule (monthly/quarterly)
- [ ] Release notes automation
- [ ] Beta testing program

### Long-term
- [ ] Foundation or sponsorship structure
- [ ] Commercial support options
- [ ] Plugin/extension API for third-party developers
- [ ] Public roadmap with community input

---

## Version Numbering Scheme

- **v0.x.x** - Pre-1.0 development releases (current phase)
- **v1.0.0** - First stable release (feature complete for core use cases)
- **v1.x.x** - Stable releases with incremental features
- **v2.0.0+** - Major architectural changes or paradigm shifts

---

## Contributing to the Roadmap

This roadmap is a living document. If you have suggestions, please:
1. Open an issue on GitHub with the `roadmap` label
2. Discuss in community channels
3. Submit a pull request with proposed changes to this document

**Last Updated:** December 8, 2025
**Current Status:** Active development, beta phase
