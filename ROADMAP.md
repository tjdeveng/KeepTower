# KeepTower Development Roadmap

This document outlines the planned features and improvements for KeepTower, organized by release milestones.

## Current Version: v0.2.8-beta (In Development)

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
- [x] Hardware-based two-factor authentication (YubiKey challenge-response) - **v0.2.0+**
- [ ] Authenticator app support (TOTP/HOTP code generation for services)
- [ ] Biometric unlock support (fingerprint via polkit) - *requires hardware access*

### Usability Improvements
- [x] Import functionality (KeePass XML, 1Password 1PIF, CSV formats) - **v0.2.5-beta**
- [x] Export functionality with security warnings - **v0.2.5-beta**
- [x] Enhanced password generator with customizable rules - **Completed**
- [x] Tags system for organizing accounts - **v0.2.6-beta**
- [x] Tag-based filtering in account list - **v0.2.6-beta**
- [x] Favorites/starred accounts for quick access - **v0.2.6-beta**
- [x] Advanced search (fuzzy search, filters by field) - **v0.2.7-beta**
- [x] Undo/redo support for vault operations - **v0.2.7-beta**
- [x] Drag-and-drop to move accounts between groups - **v0.2.8-beta**
- [x] Sort accounts alphabetically (A-Z/Z-A toggle with persistent preference) - **v0.2.9-beta**

### Data Features
- [ ] Custom fields per account (key-value pairs)
- [ ] File attachments (encrypted documents, notes, images)
- [ ] Multi-vault support (open/switch between multiple vaults)
- [ ] Vault merge capabilities
- [x] **Account groups/folders for organization - v0.2.8-beta**
  - [x] Multi-group membership support
  - [x] System "Favorites" group (auto-created)
  - [x] Create, delete, add/remove accounts from groups
  - [x] UUID-based group identification
  - [x] Comprehensive test suite (18 tests)
  - [x] UI integration (tree-based groups, context menu, dialogs)
  - [x] GNOME HIG compliant design
- [ ] Bulk operations (edit, delete, move)

---

## Mid-term (v0.3.x - Polish & Integration)

### Multi-User Vault (v0.3.0 - IN PROGRESS)
- [x] **Phase 1:** Key slot infrastructure - **COMPLETED**
- [x] **Phase 2:** Authentication & user management - **COMPLETED**
- [x] **Phase 3:** UI integration (login, password change dialogs) - **COMPLETED**
- [x] **Phase 4:** Permissions & role-based UI restrictions - **COMPLETED**
  - [x] Implement role-based menu item visibility (admin vs standard user)
  - [x] Add user management dialog (admin-only: add/remove/reset users)
  - [x] Lock UI features based on UserRole (Administrator vs Standard User)
  - [x] Add "Change My Password" menu item (all users)
  - [x] Add "Logout" functionality for V2 vaults
  - [x] Follow Phase 3 patterns: RAII, [[nodiscard]], proper documentation
- [x] **Phase 5:** User management operations - **COMPLETED**
  - [x] Admin: Add user with temporary password generation (completed in Phase 4)
  - [x] Admin: Remove user (with safety checks - prevent self-removal, last admin) (completed in Phase 4)
  - [x] Admin: Reset user password (completed in Phase 5)
  - [x] Admin: List all users with roles (completed in Phase 4)
  - [x] Safety checks: prevent orphaning vault (must have ≥1 admin) (completed in Phase 4)
- [x] **Phase 6:** Polish & testing - **COMPLETED**
  - [x] Add `VaultManager::get_vault_security_policy()` getter method
  - [x] Replace hardcoded `min_length = 12` in ChangePasswordDialog handler
  - [x] Add CSS classes for color-coded messages (GNOME HIG compliant, theme-aware)
  - [x] Optimize: Use `std::span` for buffer operations (already implemented in prior phases)
  - [ ] Add automated security tests (password clearing verification) **(Deferred)**
  - [ ] Consider rate limiting for authentication failures **(Deferred)**
  - [x] UI polish: Theme-aware colors instead of hardcoded values
  - [ ] Add V1 → V2 vault migration UI workflow **(Deferred to Phase 8)**
  - [ ] Integration tests for complete authentication flows **(Deferred)**
- [x] **Phase 7:** Account privacy controls - **COMPLETED**
  - [x] Add privacy fields to protobuf schema (`is_admin_only_viewable`, `is_admin_only_deletable`)
  - [x] Implement `VaultManager::can_view_account()` and `can_delete_account()` permission checks
  - [x] Add privacy checkboxes to AccountDetailWidget UI
  - [x] Filter account list based on user permissions (hide admin-only accounts from standard users)
  - [x] Check delete permissions in MainWindow::on_delete_account() with friendly error dialog
  - [x] Update save_current_account() to persist privacy flags
  - [x] Create comprehensive PHASE7_IMPLEMENTATION.md documentation
- [x] **Phase 8:** Migration from V1 vaults (V1 → V2 migration UI)
- [ ] **Phase 9:** User password history tracking (prevent reuse of previous vault user passwords)
  - Track N previous passwords per user (configurable, default 5)
  - Hash previous passwords for comparison
  - Prevent user from reusing any password in history
  - Admin tool to clear password history if needed
  - Note: This is separate from account password history (already implemented)

### User Experience
- [x] Refined dark/light theme with accent colors
- [x] Basic keyboard shortcuts (Ctrl+Q, Ctrl+Z, Ctrl+Shift+Z, Ctrl+comma) - **v0.2.7-beta**
- [x] Keyboard shortcuts help dialog (Ctrl+?) - **v0.2.7-beta**
- [ ] First-run wizard with security best practices
- [ ] Vault templates (personal, business, family)
- [ ] Trash/recycle bin for deleted accounts
- [ ] Account duplication feature
- [ ] Recently accessed accounts list
- [ ] Account usage statistics

### Distribution & Packaging
- [ ] Flatpak packaging for Flathub
- [x] AppImage distribution
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
- [x] Hardware security key support (YubiKey challenge-response) - **v0.2.0+**
- [ ] FIDO2/WebAuthn support for vault unlock
- [ ] Secure password sharing (time-limited, one-time access)
- [ ] Breach monitoring integration (HaveIBeenPwned API)
- [ ] Security reports (weak passwords, reused passwords, old passwords)
- [ ] Password expiration policies
- [ ] Emergency access system
- [ ] Vault integrity monitoring

### Browser Integration (Post-v1.0.0)
- [ ] Browser extension for auto-fill (Firefox, Chrome)
- [ ] Native messaging host implementation
- [ ] Cross-browser compatibility layer
- [ ] Browser store distribution

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
- [ ] Translations (Spanish, French, German - volunteer contributors welcome)
- [x] Complete API documentation (Doxygen)
- [ ] User manual and help system
- [ ] Video tutorials and screencasts
- [ ] Developer documentation for contributors

### Code Quality
- [ ] Static analysis integration (clang-tidy, cppcheck)
- [ ] Continuous fuzzing setup (OSS-Fuzz)
- [ ] Code coverage tracking (Codecov)
- [ ] Performance benchmarking suite
- [ ] Automated security scanning (CodeQL)
- [ ] **Consolidate secure memory handling** - Migrate VaultManager.cc's EVPCipherContext to SecureMemory.h's EVPCipherContextPtr, standardize all OPENSSL_cleanse usage
- [ ] **Phase 2 Deferred Items:**
  - [ ] Issue #2: Consolidate secure memory utilities (SecureMemory.h vs VaultManager methods)
  - [ ] Issue #3: Add GCM_IV_SIZE constant to replace magic number "12"
  - [ ] Issue #4: Inconsistent KEK cleanup - add explicit secure_clear() for all KEKs
  - [ ] Issue #5: Document or fix timing attack in username enumeration
  - [ ] Review all uses of std::vector for sensitive data (consider SecureBuffer)
- [ ] **Phase 3 Deferred Items:**
  - [ ] Consider `std::span` for buffer operations (optimization)
  - [ ] Use CSS classes for color-coded messages (theme support)
  - [ ] Add security tests for password clearing verification
  - [ ] Link TODOs to tracking documents (maintenance)

---

## Quick Wins (Potential Next Sprint)

These features provide high value with relatively low implementation effort:

1. ✅ **Password strength indicator** - Visual feedback during password creation (Completed)
2. **Recently used accounts** - Quick access list on main window
3. ✅ **Basic keyboard shortcuts** - Ctrl+Q, Ctrl+Z, Ctrl+Shift+Z implemented (v0.2.7-beta)
4. **Account notes field** - Store security questions, recovery info
5. **Auto-backup verification** - Test restore integrity on backup creation
6. ✅ **Show/hide password toggle** - Eye icon in password fields (Completed)
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

**Last Updated:** December 15, 2025
**Current Status:** Active development, beta phase
