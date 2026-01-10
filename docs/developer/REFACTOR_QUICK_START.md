# Async Vault Refactor - Quick Start Guide

**Full Plan:** [ASYNC_VAULT_REFACTOR_PLAN.md](ASYNC_VAULT_REFACTOR_PLAN.md)

## TL;DR

**Problem:** `create_vault_v2()` is 315 lines doing 10+ different things, blocks UI during YubiKey ops

**Solution:** Extract services (Crypto, YubiKey, File I/O) → Create Orchestrator → Add Async

**Timeline:** 3-4 weeks, 4 phases

---

## Phase Overview

### Phase 1: Extract Services (Week 1)
```
create_vault_v2() (315 lines, 10 responsibilities)
           ↓ EXTRACT
    ┌──────┴──────┬────────────┐
    ▼             ▼            ▼
CryptoService  YubiKeyService  FileService
(~200 lines)   (~250 lines)    (~150 lines)
```

**Deliverables:**
- `src/core/services/VaultCryptoService.{h,cc}`
- `src/core/services/VaultYubiKeyService.{h,cc}`
- `src/core/services/VaultFileService.{h,cc}`
- Unit tests for each (>90% coverage)

### Phase 2: Create Orchestrator (Week 2)
```
VaultCreationOrchestrator
  ├─ validate_params()      (~20 lines)
  ├─ create_admin_kek()     (~30 lines)
  ├─ enroll_yubikey()       (~40 lines)
  ├─ create_key_slot()      (~35 lines)
  ├─ write_vault_file()     (~25 lines)
  └─ create_vault_v2_sync() (~50 lines) ← orchestrates all
```

**Deliverables:**
- `src/core/controllers/VaultCreationOrchestrator.{h,cc}`
- Refactored `VaultManager::create_vault_v2()` (now ~30 lines)
- Integration tests with mocked services

### Phase 3: Add Async (Week 3)
```cpp
// Old: Blocks UI for 8-12 seconds
vm.create_vault_v2(path, admin, pass, policy, pin);

// New: Non-blocking with progress
vm.create_vault_v2_async(path, admin, pass, policy, pin,
    // Progress callback (runs on GTK thread)
    [](step, current, total, msg) {
        dialog.update_progress(msg, current, total);
    },
    // Completion callback (runs on GTK thread)
    [](result) {
        if (result) handle_success();
        else handle_error(result.error());
    }
);
```

**Deliverables:**
- `VaultCreationOrchestrator::create_vault_v2_async()`
- Updated UI in `VaultOpenHandler`
- Progress feedback in `YubiKeyPromptDialog`

### Phase 4: Cleanup (Week 4)
- Documentation
- Performance testing
- Code review
- CHANGELOG update

---

## Key Architecture Decisions

### 1. Services Use Dependency Injection
```cpp
// Good: Testable, mockable
class VaultCreationOrchestrator {
    VaultCreationOrchestrator(
        std::shared_ptr<VaultCryptoService> crypto,     // Inject
        std::shared_ptr<VaultYubiKeyService> yubikey,   // Inject
        std::shared_ptr<VaultFileService> file_io);     // Inject
};

// Bad: Hard-coded dependencies
class VaultCreationOrchestrator {
    VaultCryptoService m_crypto;  // Can't mock!
};
```

### 2. Each Service Has ONE Responsibility
```cpp
VaultCryptoService    → ONLY crypto ops (no file I/O, no YubiKey)
VaultYubiKeyService   → ONLY YubiKey ops (no crypto, no files)
VaultFileService      → ONLY file I/O (no crypto, no YubiKey)
VaultCreationOrchestrator → ONLY coordinates services
```

### 3. Async is Wrapper Around Sync
```cpp
void create_vault_v2_async(...) {
    std::thread([this, params, callback]() {
        // Run sync version in background
        auto result = create_vault_v2_sync(params);
        
        // Return to GTK thread for callback
        Glib::signal_idle().connect_once([result, callback]() {
            callback(result);
        });
    }).detach();
}
```

### 4. Progress Callbacks Between Steps
```cpp
enum class CreationStep {
    INITIALIZING = 1,        // "Initializing vault..."
    DERIVING_KEY = 2,        // "Deriving encryption key..."
    YUBIKEY_CREDENTIAL = 3,  // "Creating credential (Touch 1/2)"
    YUBIKEY_RESPONSE = 4,    // "Verifying credential (Touch 2/2)"
    ENCRYPTING_DATA = 5,     // "Encrypting vault data..."
    WRITING_FILE = 6         // "Writing vault file..."
};
```

---

## Implementation Checklist

### Phase 1 Tasks (Week 1)

**VaultCryptoService:**
- [ ] Create header file with interface
- [ ] Implement `generate_dek()`
- [ ] Implement `derive_kek_from_password()`
- [ ] Implement `wrap_dek()`
- [ ] Implement `encrypt_vault_data()`
- [ ] Implement `encrypt_pin()`
- [ ] Write unit tests (>90% coverage)
- [ ] Verify existing tests still pass

**VaultYubiKeyService:**
- [ ] Create header file with interface
- [ ] Implement `begin_enrollment()` (credential creation)
- [ ] Implement `complete_enrollment()` (challenge-response)
- [ ] Implement `combine_kek_with_response()`
- [ ] Implement utility methods (`is_present()`, `get_device_info()`)
- [ ] Write unit tests with mocked YubiKeyManager
- [ ] Verify existing YubiKey tests still pass

**VaultFileService:**
- [ ] Create header file with interface
- [ ] Implement `write_vault_v2()`
- [ ] Implement `read_vault_v2()`
- [ ] Implement `read_header_v2()`
- [ ] Implement `serialize_vault_data()`
- [ ] Write unit tests with temp directories
- [ ] Verify existing file I/O tests still pass

### Phase 2 Tasks (Week 2)

**VaultCreationOrchestrator:**
- [ ] Create header with interface
- [ ] Implement constructor with DI
- [ ] Implement `validate_params()`
- [ ] Implement `create_admin_kek()`
- [ ] Implement `enroll_yubikey()`
- [ ] Implement `create_admin_key_slot()`
- [ ] Implement `write_vault_file()`
- [ ] Implement `create_vault_v2_sync()` (orchestrates)
- [ ] Write unit tests with mocked services
- [ ] Write integration tests with real services

**VaultManager Refactor:**
- [ ] Add service member variables
- [ ] Implement `create_orchestrator()` factory
- [ ] Refactor `create_vault_v2()` to use orchestrator
- [ ] Verify all existing tests pass

### Phase 3 Tasks (Week 3)

**Async Implementation:**
- [ ] Implement `create_vault_v2_async()` in orchestrator
- [ ] Use `std::thread` + `Glib::signal_idle()`
- [ ] Call progress callbacks between steps
- [ ] Call completion callback on GTK thread
- [ ] Handle errors gracefully

**VaultManager Async API:**
- [ ] Add `create_vault_v2_async()` to VaultManager
- [ ] Delegate to orchestrator
- [ ] Update state in completion callback

**UI Updates:**
- [ ] Update `VaultOpenHandler` to use async API
- [ ] Update `YubiKeyPromptDialog` progress messages
- [ ] Add step indicators (1/6, 2/6, etc.)
- [ ] Manual testing with YubiKey

### Phase 4 Tasks (Week 4)

**Cleanup:**
- [ ] Remove deprecated code
- [ ] Clean stashed async attempt
- [ ] Run static analysis (cppcheck, clang-tidy)

**Documentation:**
- [ ] Update `ARCHITECTURE.md`
- [ ] Add sequence diagrams
- [ ] Document service interfaces
- [ ] Update `EXAMPLES.md`

**Testing:**
- [ ] Benchmark performance
- [ ] Profile memory usage
- [ ] Full manual QA checklist
- [ ] Update CHANGELOG.md

---

## Code Quality Gates

Every commit must meet:
- ✅ All existing tests pass
- ✅ New tests have >90% coverage
- ✅ No method >50 lines
- ✅ No class >400 lines
- ✅ Cyclomatic complexity <10
- ✅ Static analysis clean

---

## Testing Strategy Summary

```
Unit Tests (Service Layer)
  ├─ VaultCryptoService (no hardware/files)
  ├─ VaultYubiKeyService (mocked hardware)
  └─ VaultFileService (temp directories)

Integration Tests (Orchestrator)
  ├─ With mocked services (fast)
  └─ With real services (comprehensive)

End-to-End Tests
  └─ Full workflow with real YubiKey (manual)
```

---

## Example: Service Usage

```cpp
// Create services
auto crypto = std::make_shared<VaultCryptoService>();
auto yubikey = std::make_shared<VaultYubiKeyService>();
auto file_io = std::make_shared<VaultFileService>();

// Create orchestrator with injected dependencies
VaultCreationOrchestrator orchestrator(crypto, yubikey, file_io);

// Create vault asynchronously
VaultCreationOrchestrator::CreationParams params{
    .path = "/home/user/my.vault",
    .admin_username = "admin",
    .admin_password = "SecurePass123!",
    .policy = policy,
    .yubikey_pin = "123456",
    .progress_callback = [](step, current, total, msg) {
        std::cout << "[" << current << "/" << total << "] " << msg << std::endl;
    }
};

orchestrator.create_vault_v2_async(params,
    [](VaultResult<VaultCreationResult> result) {
        if (result) {
            std::cout << "Vault created successfully!" << std::endl;
        } else {
            std::cerr << "Error: " << static_cast<int>(result.error()) << std::endl;
        }
    }
);
```

---

## Success Metrics

**Code Quality:**
- Service classes: <300 lines each ✅
- Orchestrator: <400 lines ✅
- Methods: <50 lines ✅
- Coverage: >90% ✅

**Functional:**
- UI responsive during creation ✅
- Progress feedback works ✅
- No security regressions ✅
- All existing vaults open ✅

**Performance:**
- Creation time within 5% of current ✅
- Memory usage comparable ✅

---

## Next Steps

1. **Review this plan** with team
2. **Create GitHub issues** for each phase
3. **Begin Phase 1** (Week 1): Extract VaultCryptoService
4. **Daily standups** to track progress
5. **Phase gates** - review before moving to next phase

---

## Questions?

See full plan: [ASYNC_VAULT_REFACTOR_PLAN.md](ASYNC_VAULT_REFACTOR_PLAN.md)

Contact: Developer team
