# KeepTower Development Roadmap

This document outlines the planned features and improvements for KeepTower, organized by release milestones.

## Current Version: v0.3.5

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

### Internal Architecture Modernization (v0.3.x)
- [x] **Phase I:** Vault format/workflow boundary reduction - **COMPLETED**
  - [x] Extract `VaultFormatV2` and `VaultSerialization` into `keeptower-vaultformat`
  - [x] Route workflow-facing V2 file/header operations through `VaultFileService`
  - [x] Add `VaultDataService` for workflow-facing protobuf payload serialization and schema migration
  - [x] Remove direct format-layer reach-through from manager, UI, and orchestrator workflow code
- [x] **Phase K:** MainWindow decomposition stabilization - **COMPLETED**
  - [x] Consolidate action precondition helpers for MainWindow event handlers
  - [x] Extract selection/detail synchronization into focused coordinator glue
  - [x] Extract account/group context-menu and tree interaction coordination
  - [x] Reduce constructor wiring into focused setup/initialization steps
  - [x] Stabilize re-entrant GTK selection/update behavior with regression coverage
- [x] **Phase L:** Test build boundary cleanup and audit hardening - **COMPLETED**
  - [x] Remove remaining extracted-library test source bypasses in favor of Meson deps
  - [x] Document intentional white-box and app-layer direct source inclusions in `tests/meson.build`
  - [x] Split pure boundary-model coverage from manager/app-layer accessor coverage
  - [x] Enforce strict zero-warning Doxygen coverage for the checked-in public API surface

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

## Long-term (v2.5.x+ - Advanced Features)

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

### Enterprise Features
- [ ] LDAP/Active Directory integration
- [ ] SSO support
- [ ] Centralized policy management
- [ ] Compliance reporting (audit trails)
- [ ] Vault access monitoring dashboard

---

## Technical Debt & Quality (Ongoing)

### A+ Gap Closure Tracking
- Primary tracking now lives in the GitHub milestone `A+ Gap Closure` rather than new standalone status markdown files.
- Use milestone issues plus PRs/commits as the audit trail for quality work:
  - `#25` A+ scorecard: align quality claims, roadmap, and measurable thresholds
  - `#26` Close VaultManager/VaultManagerV2 static-analysis backlog with explicit invariants
  - `#27` Make high-signal static analysis enforceable in CI
  - `#28` Resolve or formalize the FIPS sanitizer leak policy
  - `#29` Raise coverage on remaining low-coverage core paths to A+ threshold
  - `#30` Reduce hotspot size and responsibility in VaultManager and MainWindow
  - `#32` *(planned)* Encrypted recovery snapshot to prevent in-memory data loss when backup path is unavailable during explicit save; see `SECURITY.md` Known Operational Limitations for the full description of the failure mode
- Local documentation should stay focused on durable reference material: architecture, user docs, API docs, and design notes that cannot be expressed cleanly in an issue or PR.

### Current Quality Scorecard
- This section is the canonical current repository quality scorecard.
- Current assessed state: strong recent progress, but not A+ yet; the working target remains A+ through milestone `A+ Gap Closure`.
- A draft future-facing coverage policy is tracked in `docs/testing/COVERAGE_POLICY_DRAFT.md` and remains non-canonical until explicitly adopted at a later development stage.
- Canonical A+ definition:
  - `Correctness and release health`
    - Primary CI, release workflow, and documentation workflow are green on `master` for the current repository state.
    - Full primary Meson test suite passes with no unexpected failures.
  - `Coverage`
    - Canonical coverage workflow reports at least `75%` line coverage and at least `80%` function coverage for the repository.
    - New security-critical or core workflow changes are expected to land with focused tests rather than relying on aggregate coverage drift.
  - `Static analysis`
    - No open High or Medium safety/correctness findings remain in the tracked static-analysis backlog.
    - High-signal static-analysis regressions fail CI rather than being report-only artifacts.
  - `Sanitizer evidence`
    - ASan and related sanitizer evidence is runnable for the supported test surface.
    - Any remaining external-tool noise, such as FIPS/OpenSSL provider leak reports, must be covered by a reviewed suppression or policy note rather than an unexplained workaround.
  - `Documentation policy`
    - Public API documentation remains under the checked-in zero-warning Doxygen policy.
    - `ROADMAP.md` is the canonical current scorecard; older quality/audit markdown files are historical evidence unless explicitly stated otherwise.
  - `Maintainability hotspots`
    - The largest responsibility-dense files are on a shrinking trend and no current hotspot exceeds `2000` lines without an explicit rationale tracked in the A+ closure issues.
    - Boundary improvements are tracked through milestone issues and PRs rather than implied by status prose alone.
- Non-blocking supporting goals:
  - Continuous fuzzing, benchmarking, accessibility, i18n, and broader packaging work remain important project goals, but they are not required to declare the current A+ repository-quality bar closed.
- Current known misses against the A+ definition:
  - Coverage is still below the proposed canonical threshold (`70.9%` lines, `78.8%` functions in the latest recorded snapshot).
  - Current hotspot files remain above the proposed `2000`-line ceiling without the closure rationale yet completed in `#30`.
- Verified current signals:
  - Full local Meson suite passes: `65/65` tests green.
  - High-signal static analysis is enforced in CI for the tracked audited subset.
  - The FIPS mode test suite passes in `build-asan` with leak detection enabled; the old Meson leak-detection override was removed after explicit provider cleanup landed.
  - Public API documentation is enforced under a strict zero-warning Doxygen policy.
  - Architecture-audit closeout phases I, K, and L are complete.
- Latest recorded coverage snapshot:
  - Line coverage: `70.9%`
  - Function coverage: `78.8%`
  - Source: `docs/testing/COVERAGE_IMPROVEMENT_SUMMARY.md`
  - Status: useful baseline, but not the canonical live execution tracker for A+ closure.
- `#29` handoff checkpoint (`2026-04-07`):
  - The latest full-repository snapshot above is stale relative to the most recent focused `#29` slices; do not treat it as the live post-session total without rerunning the canonical coverage workflow.
  - Coverage infrastructure hardening and focused slices landed in:
    - `55b81c8` `test(coverage): harden reports and cover key slots`
    - `707ae79` `test(yubikey): cover service validation paths`
    - `708ad72` `test(import-export): cover common error helpers`
    - `b3d3aa9` `test(auth): cover v2 auth service helpers`
    - `6a4706c` `test(theme): cover system theme follow mode`
  - Focused file results validated during this session:
    - `src/core/services/V2AuthService.cc`: `94.8%` lines, `100.0%` functions
    - `src/ui/controllers/ThemeController.cc`: `56.9%` lines, `71.4%` functions
  - Recommended next `#29` slice order at the next session start:
    - `src/core/managers/AccountManager.cc` for another bounded core-logic win
    - `src/core/VaultManager.cc` for the largest likely raw line-coverage gain
    - `src/lib/yubikey/YubiKeyManager.cc` only after the lower-friction core paths, because it remains hardware-adjacent and more complex to test deterministically
  - First action for the next session: rerun the canonical coverage workflow to refresh the aggregate repository totals, then reprioritize the remaining low-coverage files from that new report.
- A+ is considered closed only when the remaining milestone gaps are resolved:
  - `#29` coverage raised to the agreed A+ threshold
  - `#30` hotspot reduction in the largest responsibility-dense files

### Throughout All Versions
- [ ] Maintain >80% test coverage for all new features
- [ ] Performance optimization for vaults with 1000+ accounts
- [ ] Memory usage profiling and optimization
- [ ] Accessibility improvements (WCAG 2.1 Level AA)
- [ ] Full screen reader support
- [ ] Comprehensive keyboard navigation
- [ ] Internationalization (i18n) framework
- [ ] Translations (Spanish, French, German - volunteer contributors welcome)
- [x] Complete API documentation (Doxygen, strict zero-warning policy enforced)
- [ ] User manual and help system
- [ ] Video tutorials and screencasts
- [ ] Developer documentation for contributors

### Code Quality
- [ ] Static analysis integration (clang-tidy, cppcheck)
- [ ] Continuous fuzzing setup (OSS-Fuzz)
- [ ] Code coverage tracking (Codecov)
- [ ] Performance benchmarking suite
- [ ] Automated security scanning (CodeQL)
- [ ] **Logging policy (quiet-by-default):** Audit UI-layer logging (e.g., VaultOpenHandler/V2AuthenticationHandler/etc.) and standardize levels so normal runs are quiet; reserve `info` for major state transitions and use `debug` for flow tracing
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
