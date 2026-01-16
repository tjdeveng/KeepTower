# KeepTower v0.3.3 Post-Refactoring Audit Plan

**Status**: Pending platform testing (Fedora 43, Ubuntu 24.04)
**Version**: 0.3.3.2
**Date Created**: 2026-01-15
**Last Updated**: 2026-01-15

## Overview

After extensive refactoring (Phase 5 UI extraction, YubiKey async operations, service layer), multiple bug fixes, and patch releases (v0.3.3.1, v0.3.3.2), a comprehensive code audit is required to ensure:

- A+ code quality maintained
- FIPS-140-3 compliance preserved
- Test coverage adequate
- Security standards met
- Documentation current
- Maintainability high

---

## Phase 1: Build & Platform Compatibility ✅ (Next Step)

**Objective**: Verify builds and functions correctly on target platforms

### Tasks

- [ ] **Fedora 43 Testing**
  - Clean build from source
  - All dependencies resolve (GCC 15.2.1 compatible)
  - Runtime functionality verified
  - YubiKey operations functional
  - AppImage creation successful
  - AppImage execution (with/without GSettings)

- [ ] **Ubuntu 24.04 Testing**
  - Clean build from source
  - All dependencies resolve
  - Runtime functionality verified
  - YubiKey operations functional
  - Package creation (if applicable)

- [ ] **Cross-Platform Issues**
  - Document any platform-specific warnings
  - Check for endianness issues
  - Verify filesystem operations (path separators, permissions)
  - Test with different locale settings

### Success Criteria
- Clean builds on both platforms with no errors
- All tests pass (42/42)
- No ASAN/UBSAN violations
- YubiKey FIPS detection works correctly
- AppImage runs without crashes

---

## Phase 2: Code Quality & Architecture Review

**Objective**: Ensure refactoring maintained A+ standards and clean architecture

### 2.1: Service Layer Validation

**Files to Review**:
- `src/core/services/VaultCryptoService.{h,cc}`
- `src/core/services/VaultYubiKeyService.{h,cc}`
- `src/core/services/VaultFileService.{h,cc}`

**Checklist**:
- [ ] Interfaces are clean and focused (single responsibility)
- [ ] Dependencies are minimal and well-defined
- [ ] Error handling uses `VaultResult<T>` consistently
- [ ] All methods have Doxygen documentation
- [ ] No business logic leaks into services
- [ ] Thread safety considerations documented

### 2.2: Controller Layer Review

**Files to Review**:
- `src/core/controllers/VaultCreationOrchestrator.{h,cc}`
- `src/ui/controllers/AccountViewController.{h,cc}`

**Checklist**:
- [ ] Separation of concerns maintained (orchestration vs business logic)
- [ ] Clear ownership of dependencies
- [ ] Proper use of callbacks for async operations
- [ ] Error propagation is clean
- [ ] No direct UI manipulation in controllers

### 2.3: UI Manager Extraction (Phase 5)

**Files to Review**:
- `src/ui/managers/GroupHandler.{h,cc}`
- `src/ui/managers/AccountEditHandler.{h,cc}`
- `src/ui/managers/MenuManager.{h,cc}`
- `src/ui/managers/DialogManager.{h,cc}`

**Checklist**:
- [ ] Delegation from MainWindow is clean
- [ ] Each manager has clear, focused responsibility
- [ ] Callbacks properly connected and disconnected
- [ ] No circular dependencies
- [ ] Memory management correct (shared_ptr usage)
- [ ] Update callbacks trigger UI refreshes correctly

### 2.4: Repository Pattern

**Files to Review**:
- `src/core/repositories/AccountRepository.{h,cc}`
- `src/core/repositories/GroupRepository.{h,cc}`

**Checklist**:
- [ ] Abstraction properly maintained
- [ ] No VaultManager direct access bypassing repos
- [ ] Permission filtering implemented correctly
- [ ] Error handling consistent with service layer
- [ ] Thread safety appropriate for use case

### 2.5: Recent Bug Fixes

**Review Recent Changes**:
- [ ] Empty groups now display (AccountTreeWidget.cc line 190-192)
- [ ] Drag-and-drop uses `update_account_list()` (MainWindow.cc)
- [ ] GSettings graceful fallback (Application.cc, PreferencesDialog.cc)
- [ ] Test TearDown cleanup (test_vault_creation_orchestrator_integration.cc)

---

## Phase 3: Security & FIPS-140-3 Compliance 🔴 CRITICAL

**Objective**: Verify all security requirements and FIPS compliance

### 3.1: Cryptographic Operations

**Files to Audit**:
- `src/core/services/VaultCryptoService.cc`
- `src/core/Crypto.cc`
- `src/core/KeyWrapping.cc`

**FIPS Requirements**:
- [ ] PBKDF2-HMAC-SHA256 used (no other KDFs)
  - [ ] Minimum 100,000 iterations enforced
  - [ ] 32-byte salt (256 bits)
  - [ ] NIST SP 800-132 compliant
- [ ] AES-256-GCM used exclusively (no other ciphers)
  - [ ] 256-bit keys
  - [ ] 96-bit nonces (never reused)
  - [ ] 128-bit authentication tags
- [ ] Key wrapping follows NIST SP 800-38F
- [ ] Random number generation uses approved sources
  - [ ] `/dev/urandom` or `getrandom()`
  - [ ] No predictable seeds
- [ ] Memory cleared after sensitive operations
  - [ ] `sodium_memzero()` or equivalent used
  - [ ] Keys not left in memory longer than needed

**Code Review Points**:
- [ ] No hardcoded keys or IVs
- [ ] Salt generation is truly random
- [ ] Nonce/IV never reused with same key
- [ ] Authentication tags verified before decryption
- [ ] Timing attack resistance (constant-time operations)

### 3.2: YubiKey Integration

**Files to Audit**:
- `src/core/managers/YubiKeyManager.{h,cc}`
- `src/core/services/VaultYubiKeyService.cc`
- `src/ui/handlers/YubiKeyHandler.cc`

**FIPS Requirements**:
- [ ] FIPS capability detection accurate
  - [ ] Uses `fido_dev_get_cbor_info()` correctly
  - [ ] Checks for `hmac-secret` extension
  - [ ] No false positives/negatives
- [ ] FIPS mode detection functional
  - [ ] `fido_dev_is_fips_certified()` called
  - [ ] Mode properly reported to user
- [ ] Challenge size correct (32 bytes)
  - [ ] No 64-byte challenges used
  - [ ] HMAC-SHA256 operation validated
- [ ] PIN handling secure
  - [ ] PIN not logged or stored
  - [ ] Cleared from memory after use
  - [ ] Timeout handling correct
- [ ] Credential management secure
  - [ ] Enrollment requires policy compliance
  - [ ] Unenrollment requires authentication
  - [ ] No orphaned credentials

**Test on Real Hardware**:
- [ ] YubiKey 5 FIPS Series detection
- [ ] Standard YubiKey 5 detection
- [ ] FIPS mode enforcement when policy requires
- [ ] Challenge-response operations work
- [ ] Async operations handle device removal

### 3.3: Multi-User V2 Vaults

**Files to Audit**:
- `src/core/VaultManagerV2.cc`
- `src/core/MultiUserTypes.h`
- `src/core/format/VaultFormat.cc`

**Security Requirements**:
- [ ] Permission system integrity
  - [ ] Admins can perform all operations
  - [ ] Standard users cannot delete admin-protected accounts
  - [ ] Standard users cannot modify security policy
  - [ ] Role enforcement at every operation
- [ ] Password history privacy
  - [ ] Each user has isolated history
  - [ ] Admin cannot view user passwords (hashed only)
  - [ ] History limited to configured size
  - [ ] Old passwords properly cleared
- [ ] Key derivation per-user
  - [ ] Unique salt per key slot
  - [ ] Independent KEKs (no key reuse)
  - [ ] Wrapped DEK same for all users (shared vault data)
- [ ] Admin operations auditable
  - [ ] Password resets logged
  - [ ] Policy changes tracked
  - [ ] User additions/removals recorded

### 3.4: GSettings Schema Security

**Files to Audit**:
- `src/application/Application.cc` (lines 23-35)
- `src/ui/dialogs/PreferencesDialog.cc` (lines 76-94, 955-972, 1152-1158)
- `data/com.example.keeptower.gschema.xml`

**Security Checks**:
- [ ] No sensitive data stored in GSettings
  - [ ] No passwords, keys, or vault data
  - [ ] Only preferences and non-sensitive config
- [ ] Graceful degradation secure
  - [ ] Defaults are secure (FIPS disabled, conservative settings)
  - [ ] Missing schema doesn't compromise security
  - [ ] AppImage mode functions safely
- [ ] FIPS setting persistence
  - [ ] Setting honored when schema available
  - [ ] Default (disabled) safe when schema missing
  - [ ] No security regression in degraded mode

---

## Phase 4: Test Coverage & FIPS Compliance

**Objective**: Ensure comprehensive test coverage, especially for security-critical paths

### 4.1: Code Coverage Analysis

**Generate Coverage Report**:
```bash
meson configure build -Db_coverage=true
ninja -C build test
ninja -C build coverage-html
# Review: build/meson-logs/coveragereport/index.html
```

**Coverage Targets**:
- [ ] Overall coverage: >80%
- [ ] Service layer: >90%
- [ ] Controller layer: >85%
- [ ] Cryptographic operations: >95%
- [ ] YubiKey operations: >90%
- [ ] Repository layer: >85%

**Specific Areas to Check**:
- [ ] VaultCryptoService: All methods covered, error paths tested
- [ ] VaultYubiKeyService: Enrollment/unenrollment, FIPS detection
- [ ] VaultFileService: Write operations, error conditions
- [ ] VaultCreationOrchestrator: All error conditions, validation failures
- [ ] GroupHandler, AccountEditHandler, MenuManager, DialogManager (Phase 5 additions)
- [ ] YubiKeyManager: Async operations, error propagation, callbacks
- [ ] Error handling paths: File I/O, crypto failures, YubiKey errors

### 4.2: FIPS Compliance Testing

**Create New Test Suite**: `tests/test_fips_compliance.cc`

**Required Tests**:

```cpp
// FIPS Mode Detection
TEST(FIPSCompliance, DetectFIPSCapableDevice)
TEST(FIPSCompliance, DetectFIPSMode)
TEST(FIPSCompliance, RejectNonFIPSDevice_WhenRequired)

// Cryptographic Algorithms
TEST(FIPSCompliance, PBKDF2_HMAC_SHA256_MinIterations)
TEST(FIPSCompliance, PBKDF2_Iterations_Min100k_Enforced)
TEST(FIPSCompliance, AES256_GCM_OnlyAlgorithm)
TEST(FIPSCompliance, RandomNumberGeneration_FIPS186)
TEST(FIPSCompliance, KeyDerivation_NIST_SP800_132)
TEST(FIPSCompliance, SaltGeneration_256Bits_Random)

// YubiKey FIPS
TEST(FIPSCompliance, YubiKey_32ByteChallenge_Only)
TEST(FIPSCompliance, YubiKey_HMACSecret_Required)
TEST(FIPSCompliance, YubiKey_FIPS_Capability_Accurate)
TEST(FIPSCompliance, RejectEnrollment_NonFIPSKey_WhenRequired)

// Policy Enforcement
TEST(FIPSCompliance, EnforceMinPasswordLength)
TEST(FIPSCompliance, EnforcePasswordComplexity)
TEST(FIPSCompliance, EnforcePBKDF2Iterations_Min100k)
TEST(FIPSCompliance, RequireYubiKey_WhenPolicySet)

// Memory Security
TEST(FIPSCompliance, ClearSensitiveMemory_AfterUse)
TEST(FIPSCompliance, NoPlaintextKeys_InMemoryDumps)
TEST(FIPSCompliance, SecureMemory_ForKeys)

// V2 Multi-User FIPS
TEST(FIPSCompliance, PerUserKeyDerivation_Unique)
TEST(FIPSCompliance, AdminOnly_SecurityPolicy_Changes)
TEST(FIPSCompliance, PasswordHistory_Secure_Storage)
```

### 4.3: Missing Unit Tests

**Tests to Add**:

1. **GroupHandler** (`tests/test_group_handler.cc`)
   - [ ] HandleCreate_Success
   - [ ] HandleCreate_EmptyName
   - [ ] HandleCreate_DuplicateName
   - [ ] HandleRename_Success
   - [ ] HandleDelete_Confirmation
   - [ ] HandleDelete_SystemGroup_Rejected

2. **AccountEditHandler** (`tests/test_account_edit_handler.cc`)
   - [ ] SaveAccount_ValidationSuccess
   - [ ] SaveAccount_EmptyName_Fails
   - [ ] SaveAccount_FieldTooLong_Fails
   - [ ] SaveAccount_InvalidEmail_Fails
   - [ ] UpdatePasswordHistory_Correctly

3. **MenuManager** (`tests/test_menu_manager.cc`)
   - [ ] CreateAccountContextMenu_AllOptions
   - [ ] CreateGroupContextMenu_SystemVsUser
   - [ ] AddToGroup_Submenu_Populated
   - [ ] RemoveFromGroup_OnlyIfInGroup

4. **DialogManager** (`tests/test_dialog_manager.cc`)
   - [ ] ShowErrorDialog_DisplaysMessage
   - [ ] ShowPasswordDialog_ReturnsInput
   - [ ] ShowConfirmDialog_YesNo

5. **YubiKey Async Operations** (`tests/test_yubikey_async.cc`)
   - [ ] ChangePasswordAsync_Success
   - [ ] ChangePasswordAsync_DeviceTimeout
   - [ ] UnenrollAsync_Success
   - [ ] UnenrollAsync_Cancelled
   - [ ] AsyncCallback_ThreadSafety

6. **GSettings Graceful Degradation** (`tests/test_settings_fallback.cc`)
   - [ ] Application_NoSchema_UsesDefaults
   - [ ] PreferencesDialog_NoSchema_DisablesSave
   - [ ] FIPS_Setting_Persists_WhenSchemaAvailable

7. **AccountTreeWidget UI Fixes** (`tests/test_account_tree_widget.cc`)
   - [ ] DisplayEmptyGroups
   - [ ] HideSystemGroups_OnlyWhenEmpty
   - [ ] AddAccountToGroup_RefreshesDisplay
   - [ ] DragAndDrop_RefreshConsistent

### 4.4: Integration Test Gaps

**Tests to Add**:

1. **End-to-End FIPS Vault Workflow** (`tests/integration/test_fips_vault_workflow.cc`)
   - [ ] CreateVault_FIPSRequired_WithYubiKey
   - [ ] OpenVault_FIPSMode_Enforced
   - [ ] ChangePassword_FIPSCompliant
   - [ ] MultipleUsers_FIPS_Enforced

2. **Multi-User Permission Integration** (`tests/integration/test_multiuser_permissions.cc`)
   - [ ] StandardUser_CannotDeleteAdminProtected
   - [ ] StandardUser_CannotChangePolicy
   - [ ] StandardUser_PasswordHistory_Isolated
   - [ ] AdminReset_ClearsUserHistory

3. **Drag-and-Drop Consistency** (`tests/integration/test_drag_drop_updates.cc`)
   - [ ] DragAccountToGroup_UpdatesView
   - [ ] ReorderGroups_UpdatesView
   - [ ] DragAndDrop_Uses_UpdateAccountList

### 4.5: Test Quality Audit

**Memory Leak Check**:
- [ ] All test TearDown methods reset shared_ptrs
  - ✅ VaultCreationOrchestratorIntegrationTest (fixed 2026-01-15)
  - [ ] AccountViewControllerTest
  - [ ] Other controller tests
- [ ] Run valgrind on full test suite: `meson test -C build --wrap=valgrind`
- [ ] No leaks reported (except GTK/GNOME library false positives)

**Test Independence**:
- [ ] Each test creates its own fixtures
- [ ] No shared state between tests
- [ ] Tests pass when run in isolation
- [ ] Tests pass when run in random order

**ASAN/UBSAN Compliance**:
- [ ] All tests pass with ASAN enabled
- [ ] All tests pass with UBSAN enabled
- [ ] Review `asan.supp` - only library false positives
- [ ] No project-specific suppressions

**Timeout Appropriateness**:
- [ ] Long-running tests have timeouts
  - ✅ VaultCreationOrchestrator Integration (60s, added 2026-01-15)
  - [ ] Other PBKDF2-heavy tests
- [ ] No tests hang indefinitely
- [ ] Timeouts generous enough for CI runners

---

## Phase 5: Documentation & Contributions

**Objective**: Ensure all guidelines met and documentation current

### 5.1: CONTRIBUTIONS.md Compliance

**Code Style Check**:
- [ ] Consistent indentation (spaces, not tabs)
- [ ] Naming conventions followed
  - [ ] Classes: PascalCase
  - [ ] Methods: snake_case
  - [ ] Members: m_prefix for member variables
- [ ] Line length reasonable (<120 chars)
- [ ] Include guards correct (`#ifndef`, `#define`, `#endif`)
- [ ] No trailing whitespace

**Documentation Standards**:
- [ ] All public methods have Doxygen comments
- [ ] @brief, @param, @return tags present
- [ ] Complex logic has inline comments
- [ ] Phase markers accurate (Phase 1, Phase 2, etc.)
- [ ] File headers complete (SPDX, copyright, description)

**Commit Message Quality**:
- [ ] Messages follow format: `type: brief description`
- [ ] Body explains "why" not just "what"
- [ ] References issues when applicable
- [ ] Breaking changes noted in footer

### 5.2: Code Documentation Audit

**Doxygen Completeness**:
- [ ] Generate doxygen: `doxygen Doxyfile`
- [ ] Review `docs/api/html/index.html`
- [ ] Check for undocumented public APIs
- [ ] Verify @deprecated tags for old interfaces

**Recent Changes Documentation**:
- [ ] Service layer classes documented
- [ ] Phase 5 UI managers documented
- [ ] Async operation callbacks documented
- [ ] Repository pattern documented
- [ ] Multi-user V2 features documented

### 5.3: User Documentation

**Help HTML Generation**:
- [ ] Verify `scripts/generate-help.sh` works
- [ ] Check output in `build/src/help-generated/`
- [ ] Test embedded help in running application
- [ ] Verify all links work

**Security Documentation**:
- [ ] `docs/user/SECURITY_BEST_PRACTICES.md` current
- [ ] `docs/user/YUBIKEY_FIPS_SETUP.md` accurate
- [ ] Reflects current FIPS requirements
- [ ] Installation instructions correct

**README.md**:
- [ ] Version badge auto-updates (uses GitHub API)
- [ ] Dependencies list current
- [ ] Build instructions accurate for Fedora/Ubuntu
- [ ] Screenshots current (if included)

---

## Phase 6: Maintainability & Technical Debt

**Objective**: Ensure long-term health and maintainability

### 6.1: TODOs and FIXMEs

**Catalog All Markers**:
```bash
grep -rn "TODO\|FIXME\|XXX\|HACK" src/ tests/ --exclude-dir=build
```

**Categorize**:
- [ ] Critical (security, correctness)
- [ ] Important (performance, refactoring)
- [ ] Nice-to-have (features, optimizations)

**Plan Remediation**:
- [ ] File GitHub issues for critical items
- [ ] Estimate effort for important items
- [ ] Defer or remove stale nice-to-haves

### 6.2: Deprecation Handling

**Review Deprecated Items**:
- [ ] `keeptower::YubiKeyConfig::serial()` (deprecated in protobuf)
  - [ ] Document migration to `serial_number()`
  - [ ] Add deprecation warnings
  - [ ] Plan removal timeline
- [ ] Any other deprecated interfaces

**Migration Paths**:
- [ ] Document breaking changes
- [ ] Provide upgrade scripts if needed
- [ ] Version compatibility matrix

### 6.3: Build System

**meson.build Organization**:
- [ ] Dependencies clearly listed
- [ ] Version requirements documented
- [ ] Optional features properly conditional
- [ ] Test configuration clean

**Dependency Versions**:
- [ ] Minimum versions appropriate
  - GTK4 >= 4.10
  - libsodium >= 3.0
  - protobuf >= 3.5.0
  - libfido2 >= 1.13.0
- [ ] No unnecessary upper bounds
- [ ] Compatible with target platforms

**Test Configurations**:
- [ ] All tests have proper dependencies
- [ ] Timeouts set where needed
- [ ] Test organization logical

### 6.4: CI/CD Pipeline

**GitHub Actions Workflows**:
- [ ] `.github/workflows/ci.yml`
  - [ ] All jobs have correct dependencies (libfido2-dev, etc.)
  - [ ] Matrix builds for multiple platforms
  - [ ] ASAN/UBSAN enabled in test job
- [ ] `.github/workflows/coverage.yml`
  - [ ] Coverage report generated correctly
  - [ ] Thresholds set appropriately
- [ ] `.github/workflows/build.yml`
  - [ ] Release builds work
  - [ ] AppImage creation successful
  - [ ] Artifacts uploaded correctly

**Flaky Tests**:
- [ ] Identify any intermittent failures
- [ ] Fix root causes (timing, race conditions)
- [ ] Add retries only as last resort

**Release Process**:
- [ ] Tag creation triggers release
- [ ] Changelog auto-generated from commits
- [ ] Assets uploaded correctly
- [ ] Documentation deployed

---

## Phase 7: Performance & Resource Usage

**Objective**: Ensure efficiency and good user experience

### 7.1: Memory Usage Profiling

**Tools**:
- [ ] Run heaptrack: `heaptrack build/src/keeptower`
- [ ] Run massif: `valgrind --tool=massif build/src/keeptower`
- [ ] Analyze results

**Profile**:
- [ ] Peak memory usage reasonable
- [ ] No unexpected allocations
- [ ] Large objects properly lifetime-managed
- [ ] Shared resources (fonts, icons) not duplicated

### 7.2: PBKDF2 Performance

**Measurements**:
- [ ] Time 100k iterations on target hardware
- [ ] Ensure <3 seconds on typical systems
- [ ] Check UI responsiveness during operation
- [ ] Verify async operations don't block UI

**Consider**:
- [ ] Progress feedback for slow operations
- [ ] Async vault opening if needed
- [ ] Cancellation support for long operations

### 7.3: File I/O Efficiency

**Vault File Operations**:
- [ ] No unnecessary reads on vault open
- [ ] Writes are atomic (use temp file + rename)
- [ ] Backups created efficiently
- [ ] No data corruption on crash/power loss

**Optimization Opportunities**:
- [ ] Lazy loading of accounts if vault is large
- [ ] Incremental saves for minor changes
- [ ] Compression for large vaults (future)

---

## Execution Timeline

**Week 1**: Phases 1-2
- Platform testing (Fedora 43, Ubuntu 24.04)
- Code quality and architecture review
- Initial findings documented

**Week 2**: Phase 3
- Security audit (MOST CRITICAL)
- FIPS compliance verification
- YubiKey testing on real hardware
- Security findings reported immediately

**Week 3**: Phases 4-5
- Test coverage analysis
- Write missing tests
- FIPS compliance test suite
- Documentation review and updates

**Week 4**: Phases 6-7
- Technical debt assessment
- Performance profiling
- Final recommendations
- Audit report completed

---

## Priority Levels

Issues found during audit should be prioritized:

- 🔴 **Critical**: Security issues, data loss, crashes, FIPS non-compliance
  - Fix immediately, release hotfix if needed
  - Block any major release

- 🟡 **Important**: Code quality issues, test gaps, significant bugs
  - Fix before next minor release
  - May require refactoring

- 🟢 **Nice-to-have**: Optimizations, minor technical debt, documentation improvements
  - Address in future releases
  - Good first issues for contributors

---

## Audit Completion Checklist

- [ ] All phases completed
- [ ] Findings documented with severity levels
- [ ] Critical issues resolved
- [ ] Important issues have GitHub issues filed
- [ ] Test coverage meets targets (>80% overall)
- [ ] FIPS compliance verified
- [ ] Documentation updated
- [ ] Audit report written
- [ ] Stakeholders informed

---

## Notes

- This audit was triggered after v0.3.3 and subsequent patches (v0.3.3.1, v0.3.3.2)
- Major refactorings included:
  - Phase 5: UI manager extraction
  - YubiKey async operations
  - Service layer introduction
  - Repository pattern implementation
- Recent bug fixes:
  - Empty groups display (v0.3.3.1)
  - GSettings graceful degradation (v0.3.3.2)
  - Help HTML build artifacts (v0.3.3.2)
  - Test memory leaks (v0.3.3.2)

**Status Updates**: Track progress by checking boxes and updating this document.
