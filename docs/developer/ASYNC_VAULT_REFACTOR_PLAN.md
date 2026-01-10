# Async Vault Creation Refactor Plan

**Date:** 2026-01-10
**Status:** Planning Phase
**Goal:** Refactor vault creation to support async YubiKey operations with proper SRP compliance

## Table of Contents
- [Problem Statement](#problem-statement)
- [Previous Attempt Analysis](#previous-attempt-analysis)
- [SOLID Principles Applied](#solid-principles-applied)
- [Architecture Design](#architecture-design)
- [Implementation Phases](#implementation-phases)
- [Testing Strategy](#testing-strategy)
- [Success Criteria](#success-criteria)

---

## Problem Statement

### Current Issues

**1. Blocking YubiKey Operations (UX Problem)**
- YubiKey operations in `create_vault_v2()` block the GTK main thread
- Two YubiKey touches required (credential creation + challenge-response)
- UI appears frozen during ~8-12 second operations
- No progress feedback between touches
- Poor user experience

**2. SRP Violations (Code Quality)**
```cpp
// Current create_vault_v2() does TOO MUCH:
KeepTower::VaultResult<> VaultManager::create_vault_v2(...) {
    // 1. Input validation
    // 2. Close existing vault
    // 3. Generate DEK
    // 4. Derive KEK from password (PBKDF2 - expensive)
    // 5. YubiKey operations:
    //    a. Initialize YubiKey
    //    b. Create FIDO2 credential (touch 1)
    //    c. Challenge-response (touch 2)
    //    d. Encrypt PIN
    //    e. Combine KEK with YubiKey
    // 6. Key wrapping
    // 7. Create key slots
    // 8. Password history
    // 9. Serialize vault data
    // 10. Encrypt data
    // 11. Write file with FEC
    // 12. Set permissions
    // 13. Initialize state
    // 14. Create managers
}
```

**Current Method Complexity:**
- **Lines of code:** ~315 lines in single method
- **Cyclomatic complexity:** High (multiple nested conditions)
- **Responsibilities:** At least 10 distinct operations
- **Testability:** Poor (can't test individual steps in isolation)

**3. God Object Pattern**
- `VaultManager` handles everything: crypto, file I/O, YubiKey, state management
- Violates SRP at class level, not just method level
- Hard to extend or modify without breaking existing functionality

### Requirements

**Functional:**
- ✅ Two-touch YubiKey workflow with progress feedback
- ✅ Non-blocking UI during long operations
- ✅ Maintain current security guarantees
- ✅ Backward compatible with existing vault format

**Non-Functional:**
- ✅ Clean separation of concerns (SRP)
- ✅ Testable components in isolation
- ✅ Easy to maintain and extend
- ✅ Follow CONTRIBUTING.md guidelines
- ✅ Maximum 100 lines per method
- ✅ Single responsibility per class

---

## Previous Attempt Analysis

### What Was Attempted (stash@{0})

**Changes Made:**
- Added 178 lines to `VaultManager.h`
- Added 793 lines to `VaultManagerV2.cc`
- Total: 971 lines changed

**Approach Taken:**
1. Added async wrapper methods to `VaultManager`:
   - `create_vault_v2_async()`
   - `begin_yubikey_enrollment_async()`
   - `complete_yubikey_enrollment_async()`

2. Created `YubiKeyEnrollmentContext` struct to hold state

3. Used callbacks and `Glib::signal_idle()` for async operations

### Why It Failed (Root Cause Analysis)

**❌ Didn't Address Core SRP Problem**
- Added async wrappers AROUND monolithic methods
- Methods still did too much, just in background threads
- No separation of concerns, just threading layer

**❌ Wrong Abstraction Level**
- Put async logic in `VaultManager` (already too large)
- Should have been in separate orchestrator/coordinator

**❌ Complexity Explosion**
- 971 lines added without removing any complexity
- Made codebase harder to understand, not easier
- Violated CONTRIBUTING.md: "If a class name contains 'And' or 'Manager', consider splitting it"

**❌ Poor Testability**
- Can't test crypto separately from YubiKey operations
- Can't test file I/O separately from encryption
- Async timing makes tests flaky

### Lessons Learned

✅ **What to Keep:**
- Progress callback concept (`VaultCreationProgressCallback`)
- Two-step YubiKey enrollment (begin/complete)
- Using `Glib::signal_idle()` for GTK thread safety

❌ **What to Avoid:**
- Adding complexity without simplifying existing code
- Keeping all logic in `VaultManager`
- Async wrappers around monolithic methods

✅ **What to Do Instead:**
- **First refactor for SRP, then add async**
- Break apart monolithic methods into focused components
- Create specialized service classes
- Use dependency injection for testability

---

## SOLID Principles Applied

### Single Responsibility Principle (SRP)

**Goal:** Each class has ONE reason to change

**Current Violations:**
```cpp
class VaultManager {
    // Reason 1: Vault file operations
    void save_vault();
    void close_vault();

    // Reason 2: Cryptographic operations
    bool encrypt_data();
    bool decrypt_data();

    // Reason 3: YubiKey hardware operations
    bool enroll_yubikey();

    // Reason 4: User management
    bool add_user();
    bool remove_user();

    // Reason 5: Account/group data management
    AccountManager* m_account_manager;
    GroupManager* m_group_manager;

    // Reason 6: State management
    bool m_vault_open;
    bool m_modified;

    // TOO MANY REASONS TO CHANGE!
};
```

**Solution:** Split into focused classes:
```cpp
// 1. Crypto operations
class VaultCryptoService {
    VaultResult<DEK> generate_dek();
    VaultResult<KEK> derive_kek(password, salt, iterations);
    VaultResult<Wrapped> wrap_key(kek, dek);
};

// 2. YubiKey operations
class YubiKeyService {
    VaultResult<Credential> create_credential(user_id, pin);
    VaultResult<Response> challenge_response(challenge, algo, pin);
    VaultResult<Combined> combine_kek(kek, yk_response);
};

// 3. File operations
class VaultFileService {
    VaultResult<Data> read_vault(path);
    VaultResult<void> write_vault(path, data, fec_settings);
};

// 4. Orchestrates the process
class VaultCreationOrchestrator {
    VaultResult<void> create_vault_v2(params, progress_callback);
    // Coordinates crypto, yubikey, and file services
};

// 5. State management (existing)
class VaultManager {
    bool m_vault_open;
    VaultResult<void> open_vault_v2();
    VaultResult<void> close_vault();
    // Reduced responsibilities!
};
```

### Open/Closed Principle (OCP)

**Goal:** Open for extension, closed for modification

**Strategy:**
- Define interfaces for services
- Allow new YubiKey algorithms without changing core code
- Support future async patterns (std::future, coroutines)

### Liskov Substitution Principle (LSP)

**Goal:** Derived classes substitutable for base classes

**Strategy:**
- Service interfaces define contracts
- Implementations honor those contracts
- Mock services for testing

### Interface Segregation Principle (ISP)

**Goal:** Many small interfaces, not one large interface

**Strategy:**
```cpp
// Instead of one large interface:
class IVaultService {
    virtual void create() = 0;
    virtual void open() = 0;
    virtual void save() = 0;
    virtual void encrypt() = 0;
    virtual void yubikey_ops() = 0;
    // Too much!
};

// Use focused interfaces:
class ICryptoService { ... };
class IYubiKeyService { ... };
class IFileService { ... };
```

### Dependency Inversion Principle (DIP)

**Goal:** Depend on abstractions, not concretions

**Strategy:**
```cpp
// Orchestrator depends on abstractions
class VaultCreationOrchestrator {
    std::unique_ptr<ICryptoService> m_crypto;
    std::unique_ptr<IYubiKeyService> m_yubikey;
    std::unique_ptr<IFileService> m_file_io;

    // Inject dependencies (constructor injection)
    VaultCreationOrchestrator(
        std::unique_ptr<ICryptoService> crypto,
        std::unique_ptr<IYubiKeyService> yubikey,
        std::unique_ptr<IFileService> file_io);
};
```

---

## Architecture Design

### Component Hierarchy

```
┌─────────────────────────────────────────────────────────┐
│                    UI Layer (GTK)                        │
│  - VaultOpenHandler                                      │
│  - YubiKeyPromptDialog (already uses progress bar)      │
└────────────────────┬────────────────────────────────────┘
                     │ Callbacks
                     ▼
┌─────────────────────────────────────────────────────────┐
│          VaultCreationOrchestrator (NEW)                 │
│  - Coordinates the creation process                      │
│  - Manages progress callbacks                            │
│  - Handles async/threading                               │
│  - Dependency injection                                  │
└───┬─────────────────┬──────────────────┬────────────────┘
    │                 │                  │
    ▼                 ▼                  ▼
┌─────────┐   ┌──────────────┐   ┌──────────────┐
│ Crypto  │   │   YubiKey    │   │  File I/O    │
│ Service │   │   Service    │   │  Service     │
│ (NEW)   │   │   (NEW)      │   │  (NEW)       │
└─────────┘   └──────────────┘   └──────────────┘
    │                 │                  │
    ▼                 ▼                  ▼
┌─────────┐   ┌──────────────┐   ┌──────────────┐
│KeyWrap  │   │YubiKeyMgr    │   │VaultFormatV2 │
│(exists) │   │(exists)      │   │(exists)      │
└─────────┘   └──────────────┘   └──────────────┘
```

### Service Classes (New)

#### 1. VaultCryptoService

**Location:** `src/core/services/VaultCryptoService.{h,cc}`

**Responsibility:** ALL cryptographic operations for vault creation

**Public Interface:**
```cpp
class VaultCryptoService {
public:
    struct DEKResult {
        std::array<uint8_t, 32> dek;
        bool memory_locked;
    };

    struct KEKResult {
        std::array<uint8_t, 32> kek;
        std::array<uint8_t, 32> salt;
    };

    struct EncryptionResult {
        std::vector<uint8_t> ciphertext;
        std::vector<uint8_t> iv;
    };

    // Pure crypto operations - no side effects
    [[nodiscard]] VaultResult<DEKResult> generate_dek();
    [[nodiscard]] VaultResult<KEKResult> derive_kek_from_password(
        const Glib::ustring& password,
        uint32_t iterations);
    [[nodiscard]] VaultResult<WrappedKey> wrap_dek(
        const std::array<uint8_t, 32>& kek,
        const std::array<uint8_t, 32>& dek);
    [[nodiscard]] VaultResult<EncryptionResult> encrypt_vault_data(
        const std::vector<uint8_t>& plaintext,
        const std::array<uint8_t, 32>& dek);
    [[nodiscard]] VaultResult<std::vector<uint8_t>> encrypt_pin(
        const std::string& pin,
        const std::array<uint8_t, 32>& kek);
};
```

**Why This Helps:**
- ✅ Testable in isolation (no hardware, no file I/O)
- ✅ Clear input/output contracts
- ✅ Reusable across create/open/save operations
- ✅ Can mock for orchestrator tests

#### 2. VaultYubiKeyService

**Location:** `src/core/services/VaultYubiKeyService.{h,cc}`

**Responsibility:** ALL YubiKey hardware operations

**Public Interface:**
```cpp
class VaultYubiKeyService {
public:
    struct EnrollmentStep1Result {
        std::array<uint8_t, 20> challenge;
        std::vector<uint8_t> credential_id;
        std::string device_serial;
    };

    struct EnrollmentStep2Result {
        std::vector<uint8_t> response;
        YubiKeyAlgorithm algorithm_used;
    };

    // Two-step enrollment for UI feedback
    [[nodiscard]] VaultResult<EnrollmentStep1Result> begin_enrollment(
        const std::string& user_id,
        const std::string& pin,
        YubiKeyAlgorithm algorithm,
        bool enforce_fips);

    [[nodiscard]] VaultResult<EnrollmentStep2Result> complete_enrollment(
        const std::array<uint8_t, 20>& challenge,
        const std::vector<uint8_t>& credential_id,
        const std::string& pin,
        YubiKeyAlgorithm algorithm);

    // Utility methods
    [[nodiscard]] bool is_present();
    [[nodiscard]] VaultResult<DeviceInfo> get_device_info();

    // KEK combination (crypto + YubiKey response)
    [[nodiscard]] std::array<uint8_t, 32> combine_kek_with_response(
        const std::array<uint8_t, 32>& password_kek,
        const std::vector<uint8_t>& yubikey_response);
};
```

**Why This Helps:**
- ✅ Encapsulates all YubiKey complexity
- ✅ Two-step API natural for progress feedback
- ✅ Can mock for testing vault creation without hardware
- ✅ Async wrapper easy to add later

#### 3. VaultFileService

**Location:** `src/core/services/VaultFileService.{h,cc}`

**Responsibility:** ALL file I/O and serialization

**Public Interface:**
```cpp
class VaultFileService {
public:
    struct WriteParams {
        std::string path;
        VaultHeaderV2 header;
        std::vector<uint8_t> encrypted_data;
        std::vector<uint8_t> data_iv;
        uint32_t pbkdf2_iterations;
        bool enable_fec;
        uint8_t fec_redundancy_percent;
    };

    [[nodiscard]] VaultResult<void> write_vault_v2(const WriteParams& params);
    [[nodiscard]] VaultResult<std::vector<uint8_t>> read_vault_v2(const std::string& path);
    [[nodiscard]] VaultResult<VaultHeaderV2> read_header_v2(const std::string& path);

    // Serialization helpers
    [[nodiscard]] VaultResult<std::vector<uint8_t>> serialize_vault_data(
        const keeptower::VaultData& vault_data);
};
```

**Why This Helps:**
- ✅ All file operations in one place
- ✅ Easy to test with temp directories
- ✅ Can mock for testing without real files
- ✅ FEC logic contained here

#### 4. VaultCreationOrchestrator (NEW)

**Location:** `src/core/controllers/VaultCreationOrchestrator.{h,cc}`

**Responsibility:** Coordinate vault creation process ONLY

**Public Interface:**
```cpp
class VaultCreationOrchestrator {
public:
    // Progress callback types
    enum class CreationStep {
        INITIALIZING = 1,
        DERIVING_KEY = 2,
        YUBIKEY_CREDENTIAL = 3,
        YUBIKEY_RESPONSE = 4,
        ENCRYPTING_DATA = 5,
        WRITING_FILE = 6
    };

    using ProgressCallback = std::function<void(
        CreationStep step,
        int current,
        int total,
        const std::string& message)>;

    struct CreationParams {
        std::string path;
        Glib::ustring admin_username;
        Glib::ustring admin_password;
        VaultSecurityPolicy policy;
        std::optional<std::string> yubikey_pin;
        ProgressCallback progress_callback;
    };

    // Constructor injection
    VaultCreationOrchestrator(
        std::shared_ptr<VaultCryptoService> crypto,
        std::shared_ptr<VaultYubiKeyService> yubikey,
        std::shared_ptr<VaultFileService> file_io);

    // Sync version - breaks down into steps
    [[nodiscard]] VaultResult<VaultCreationResult> create_vault_v2_sync(
        const CreationParams& params);

    // Async version - runs in background thread
    void create_vault_v2_async(
        const CreationParams& params,
        std::function<void(VaultResult<VaultCreationResult>)> completion_callback);

private:
    // Internal step methods (each < 50 lines)
    VaultResult<void> validate_params(const CreationParams& params);
    VaultResult<KEKResult> create_admin_kek(const CreationParams& params);
    VaultResult<EnrollmentData> enroll_yubikey(const CreationParams& params, KEKResult& kek);
    VaultResult<KeySlot> create_admin_key_slot(/* ... */);
    VaultResult<void> write_vault_file(/* ... */);

    std::shared_ptr<VaultCryptoService> m_crypto;
    std::shared_ptr<VaultYubiKeyService> m_yubikey;
    std::shared_ptr<VaultFileService> m_file_io;
};
```

**Why This is Better:**
- ✅ Clear sequence of operations
- ✅ Each step is a small, testable method
- ✅ Progress callbacks built-in
- ✅ Services injected (mockable for tests)
- ✅ Async is wrapper around sync
- ✅ No direct hardware/file/crypto calls

### Modified VaultManager

**Changes to VaultManager:**
```cpp
class VaultManager {
public:
    // NEW: Simple factory method that uses orchestrator
    [[nodiscard]] VaultResult<void> create_vault_v2(
        const std::string& path,
        const Glib::ustring& admin_username,
        const Glib::ustring& admin_password,
        const VaultSecurityPolicy& policy,
        const std::optional<std::string>& yubikey_pin = std::nullopt);

    // NEW: Async version
    void create_vault_v2_async(
        const std::string& path,
        const Glib::ustring& admin_username,
        const Glib::ustring& admin_password,
        const VaultSecurityPolicy& policy,
        const std::optional<std::string>& yubikey_pin,
        VaultCreationOrchestrator::ProgressCallback progress_callback,
        std::function<void(VaultResult<void>)> completion_callback);

private:
    // NEW: Service instances (lazy init)
    std::shared_ptr<VaultCryptoService> m_crypto_service;
    std::shared_ptr<VaultYubiKeyService> m_yubikey_service;
    std::shared_ptr<VaultFileService> m_file_service;

    // Helper to get/create orchestrator
    std::unique_ptr<VaultCreationOrchestrator> create_orchestrator();
};
```

**Implementation:**
```cpp
VaultResult<void> VaultManager::create_vault_v2(...) {
    // Lightweight wrapper - delegates to orchestrator
    auto orchestrator = create_orchestrator();

    VaultCreationOrchestrator::CreationParams params{
        .path = path,
        .admin_username = admin_username,
        .admin_password = admin_password,
        .policy = policy,
        .yubikey_pin = yubikey_pin,
        .progress_callback = nullptr  // No progress for sync
    };

    auto result = orchestrator->create_vault_v2_sync(params);

    if (result) {
        // Initialize VaultManager state from result
        m_vault_open = true;
        m_is_v2_vault = true;
        m_current_vault_path = path;
        m_v2_dek = result.value().dek;
        m_v2_header = result.value().header;
        // ... etc
    }

    return result.has_value() ? VaultResult<void>{} : std::unexpected(result.error());
}
```

---

## Implementation Phases

### Phase 1: Extract Service Classes (Week 1)

**Goal:** Create service classes without changing behavior

**Tasks:**

1. **Create VaultCryptoService** (Day 1-2)
   - [ ] Create `src/core/services/VaultCryptoService.h`
   - [ ] Create `src/core/services/VaultCryptoService.cc`
   - [ ] Implement `generate_dek()`
   - [ ] Implement `derive_kek_from_password()`
   - [ ] Implement `wrap_dek()`
   - [ ] Implement `encrypt_vault_data()`
   - [ ] Implement `encrypt_pin()`
   - [ ] Add unit tests in `tests/unit/core/test_vault_crypto_service.cc`
   - [ ] Run existing vault creation tests - should still pass

2. **Create VaultYubiKeyService** (Day 3-4)
   - [ ] Create `src/core/services/VaultYubiKeyService.h`
   - [ ] Create `src/core/services/VaultYubiKeyService.cc`
   - [ ] Implement `begin_enrollment()`
   - [ ] Implement `complete_enrollment()`
   - [ ] Implement `combine_kek_with_response()`
   - [ ] Implement `is_present()`, `get_device_info()`
   - [ ] Add unit tests (with mock YubiKeyManager)
   - [ ] Run existing YubiKey tests - should still pass

3. **Create VaultFileService** (Day 5)
   - [ ] Create `src/core/services/VaultFileService.h`
   - [ ] Create `src/core/services/VaultFileService.cc`
   - [ ] Implement `write_vault_v2()`
   - [ ] Implement `read_vault_v2()`
   - [ ] Implement `read_header_v2()`
   - [ ] Implement `serialize_vault_data()`
   - [ ] Add unit tests with temp directories
   - [ ] Run existing file I/O tests - should still pass

**Success Criteria Phase 1:**
- ✅ All new service classes have >90% code coverage
- ✅ All existing tests still pass
- ✅ No behavior changes - services extract existing logic
- ✅ Each service class < 300 lines
- ✅ Each method < 50 lines

### Phase 2: Create Orchestrator (Week 2)

**Goal:** Build VaultCreationOrchestrator using services

**Tasks:**

1. **Create Orchestrator Structure** (Day 1-2)
   - [ ] Create `src/core/controllers/VaultCreationOrchestrator.h`
   - [ ] Create `src/core/controllers/VaultCreationOrchestrator.cc`
   - [ ] Implement constructor with dependency injection
   - [ ] Define `CreationParams` struct
   - [ ] Define `CreationStep` enum
   - [ ] Define callback types

2. **Implement Sync Creation** (Day 3-4)
   - [ ] Implement `validate_params()`
   - [ ] Implement `create_admin_kek()` using `VaultCryptoService`
   - [ ] Implement `enroll_yubikey()` using `VaultYubiKeyService`
   - [ ] Implement `create_admin_key_slot()`
   - [ ] Implement `write_vault_file()` using `VaultFileService`
   - [ ] Implement `create_vault_v2_sync()` - orchestrates all steps
   - [ ] Add unit tests with mocked services
   - [ ] Test each step method independently

3. **Refactor VaultManager** (Day 5)
   - [ ] Add service member variables to `VaultManager`
   - [ ] Implement `create_orchestrator()` helper
   - [ ] Refactor `create_vault_v2()` to use orchestrator
   - [ ] Verify all existing tests pass
   - [ ] Compare behavior with old implementation

**Success Criteria Phase 2:**
- ✅ Orchestrator has >90% code coverage
- ✅ All steps independently tested
- ✅ Mock services work correctly
- ✅ Old `create_vault_v2()` behavior preserved
- ✅ All existing integration tests pass
- ✅ Orchestrator class < 400 lines
- ✅ Each method < 50 lines

### Phase 3: Add Async Support (Week 3)

**Goal:** Add non-blocking async operations

**Tasks:**

1. **Implement Async Wrapper** (Day 1-2)
   - [ ] Implement `create_vault_v2_async()` in orchestrator
   - [ ] Use `std::thread` + `Glib::signal_idle()` for GTK safety
   - [ ] Call progress callbacks between steps
   - [ ] Call completion callback on GTK thread
   - [ ] Handle errors gracefully
   - [ ] Add cancellation support (future enhancement)

2. **Update VaultManager API** (Day 3)
   - [ ] Add `create_vault_v2_async()` to `VaultManager`
   - [ ] Delegate to orchestrator's async method
   - [ ] Update state on completion callback
   - [ ] Add documentation with examples

3. **Update UI Layer** (Day 4-5)
   - [ ] Update `VaultOpenHandler` to use async API
   - [ ] Update progress messages in `YubiKeyPromptDialog`
   - [ ] Add step indicators (Step 1/4, 2/4, etc.)
   - [ ] Test with real YubiKey hardware
   - [ ] Verify no UI freezing

**Success Criteria Phase 3:**
- ✅ UI remains responsive during creation
- ✅ Progress feedback works correctly
- ✅ Two YubiKey touches clearly separated
- ✅ No GTK thread violations
- ✅ Async tests pass (may need `Glib::MainLoop` in tests)
- ✅ Manual testing with YubiKey confirms good UX

### Phase 4: Cleanup and Documentation (Week 4)

**Goal:** Remove old code, document new architecture

**Tasks:**

1. **Remove Deprecated Code** (Day 1)
   - [ ] Mark old monolithic methods as deprecated (if any remain)
   - [ ] Remove stashed async attempt
   - [ ] Clean up any unused includes
   - [ ] Run static analysis (cppcheck, clang-tidy)

2. **Documentation** (Day 2-3)
   - [ ] Update `docs/developer/ARCHITECTURE.md` with new services
   - [ ] Add sequence diagrams for vault creation
   - [ ] Document service interfaces
   - [ ] Add examples to `docs/developer/EXAMPLES.md`
   - [ ] Update `CONTRIBUTING.md` if needed

3. **Performance Testing** (Day 4)
   - [ ] Benchmark vault creation time (async vs sync)
   - [ ] Profile memory usage
   - [ ] Test with various PBKDF2 iteration counts
   - [ ] Ensure no performance regressions

4. **Final Review** (Day 5)
   - [ ] Code review of all changes
   - [ ] Run full test suite
   - [ ] Manual testing checklist
   - [ ] Update CHANGELOG.md
   - [ ] Create demo video showing progress feedback

**Success Criteria Phase 4:**
- ✅ All tests pass
- ✅ Code coverage ≥ 90% for new code
- ✅ Documentation complete
- ✅ No performance regressions
- ✅ Static analysis clean
- ✅ Ready for PR

---

## Testing Strategy

### Unit Tests (Per Service)

**VaultCryptoService:**
```cpp
TEST(VaultCryptoServiceTest, GenerateDEK_Success) {
    VaultCryptoService crypto;
    auto result = crypto.generate_dek();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().dek.size(), 32);
    EXPECT_TRUE(result.value().memory_locked);  // or false if mlock failed
}

TEST(VaultCryptoServiceTest, DeriveKEK_CorrectIterations) {
    VaultCryptoService crypto;
    auto result = crypto.derive_kek_from_password("TestPass123!", 100000);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().kek.size(), 32);
    EXPECT_EQ(result.value().salt.size(), 32);
}

TEST(VaultCryptoServiceTest, WrapDEK_RoundTrip) {
    VaultCryptoService crypto;
    std::array<uint8_t, 32> kek = {/* ... */};
    std::array<uint8_t, 32> dek = {/* ... */};

    auto wrapped = crypto.wrap_dek(kek, dek);
    ASSERT_TRUE(wrapped.has_value());

    // Verify can unwrap (use KeyWrapping::unwrap_key)
    auto unwrapped = KeyWrapping::unwrap_key(kek, wrapped.value());
    ASSERT_TRUE(unwrapped.has_value());
    EXPECT_EQ(unwrapped.value(), dek);
}
```

**VaultYubiKeyService (Mocked):**
```cpp
class MockYubiKeyManager : public YubiKeyManager {
    // Override virtual methods for testing
};

TEST(VaultYubiKeyServiceTest, BeginEnrollment_Success) {
    auto service = VaultYubiKeyService(std::make_unique<MockYubiKeyManager>());
    auto result = service.begin_enrollment("alice", "123456", YubiKeyAlgorithm::HMAC_SHA256, true);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().challenge.size(), 20);
    EXPECT_FALSE(result.value().credential_id.empty());
}
```

**VaultFileService:**
```cpp
TEST(VaultFileServiceTest, WriteVault_CreatesFile) {
    VaultFileService file_svc;
    TempDirectory temp;

    VaultFileService::WriteParams params{
        .path = temp.path() / "test.vault",
        // ... set other params
    };

    auto result = file_svc.write_vault_v2(params);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(params.path));
}
```

### Integration Tests (Orchestrator)

**With Real Services:**
```cpp
TEST(VaultCreationOrchestratorTest, CreateVault_NoYubiKey_Success) {
    auto crypto = std::make_shared<VaultCryptoService>();
    auto yubikey = std::make_shared<VaultYubiKeyService>();
    auto file_io = std::make_shared<VaultFileService>();

    VaultCreationOrchestrator orchestrator(crypto, yubikey, file_io);

    TempDirectory temp;
    VaultCreationOrchestrator::CreationParams params{
        .path = temp.path() / "test.vault",
        .admin_username = "admin",
        .admin_password = "TestPass123!",
        .policy = {.require_yubikey = false, /* ... */},
        .yubikey_pin = std::nullopt,
        .progress_callback = nullptr
    };

    auto result = orchestrator.create_vault_v2_sync(params);
    ASSERT_TRUE(result.has_value());

    // Verify vault file exists and can be opened
    VaultManager vm;
    auto open_result = vm.open_vault_v2(params.path, "admin", "TestPass123!");
    ASSERT_TRUE(open_result.has_value());
}
```

**With Mock Services (Fast):**
```cpp
TEST(VaultCreationOrchestratorTest, CreateVault_CryptoFails_ReturnsError) {
    auto mock_crypto = std::make_shared<MockVaultCryptoService>();
    EXPECT_CALL(*mock_crypto, generate_dek())
        .WillOnce(Return(std::unexpected(VaultError::CryptoError)));

    auto yubikey = std::make_shared<VaultYubiKeyService>();
    auto file_io = std::make_shared<VaultFileService>();

    VaultCreationOrchestrator orchestrator(mock_crypto, yubikey, file_io);

    auto result = orchestrator.create_vault_v2_sync(/* params */);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::CryptoError);
}
```

### End-to-End Tests

**Full Workflow:**
```cpp
TEST(E2E_VaultCreation, CreateOpenSaveClose_WithYubiKey) {
    // Skip if no YubiKey present
    if (!YubiKeyManager().is_yubikey_present()) {
        GTEST_SKIP();
    }

    VaultManager vm;
    TempDirectory temp;
    std::string vault_path = temp.path() / "test.vault";

    // Create vault with YubiKey
    VaultSecurityPolicy policy{
        .require_yubikey = true,
        .yubikey_algorithm = 0x02,  // HMAC-SHA256
        // ...
    };

    auto create_result = vm.create_vault_v2(
        vault_path,
        "admin",
        "TestPass123!",
        policy,
        "654321"  // YubiKey PIN
    );
    ASSERT_TRUE(create_result.has_value());

    // Close and reopen
    ASSERT_TRUE(vm.close_vault().has_value());

    auto open_result = vm.open_vault_v2(vault_path, "admin", "TestPass123!");
    ASSERT_TRUE(open_result.has_value());
}
```

---

## Success Criteria

### Code Quality Metrics

- ✅ **Code Coverage:** ≥90% for new service classes
- ✅ **Method Complexity:** All methods < 50 lines
- ✅ **Class Size:** Service classes < 300 lines, Orchestrator < 400 lines
- ✅ **Cyclomatic Complexity:** < 10 per method
- ✅ **SRP Compliance:** Each class has one clear responsibility

### Functional Requirements

- ✅ **Backward Compatibility:** All existing vault files open correctly
- ✅ **Security:** No regression in security guarantees
- ✅ **Performance:** Vault creation time within 5% of current implementation
- ✅ **YubiKey Support:** Two-touch workflow with progress feedback
- ✅ **Error Handling:** All error paths tested and handled gracefully

### User Experience

- ✅ **UI Responsiveness:** No freezing during vault creation
- ✅ **Progress Feedback:** Clear messages between YubiKey touches
- ✅ **Error Messages:** User-friendly error messages
- ✅ **Manual Testing:** Passes checklist with real YubiKey hardware

### Developer Experience

- ✅ **Testability:** Can test components in isolation
- ✅ **Documentation:** Architecture clearly documented
- ✅ **Examples:** Working code examples provided
- ✅ **Extensibility:** Easy to add new features (e.g., new algorithms)

---

## Risk Mitigation

### Risk 1: Breaking Existing Functionality

**Mitigation:**
- Run full test suite after each phase
- Keep old implementation during refactor
- Extensive integration testing
- Manual QA checklist

### Risk 2: Threading Issues

**Mitigation:**
- Use `Glib::signal_idle()` for GTK thread safety
- Careful mutex usage if needed
- Test with ThreadSanitizer
- Document thread ownership clearly

### Risk 3: Complexity Creep

**Mitigation:**
- Strict adherence to line limits
- Regular code reviews
- Refactor if any method exceeds limits
- Follow CONTRIBUTING.md guidelines strictly

### Risk 4: YubiKey Hardware Dependencies

**Mitigation:**
- Mock interfaces for all services
- Unit tests don't require hardware
- Integration tests skip if no YubiKey
- CI/CD tests without hardware

---

## Conclusion

This refactor plan addresses the root causes of the previous async attempt's failure by:

1. **SRP First:** Breaking apart monolithic methods before adding async
2. **Service Extraction:** Creating focused, testable components
3. **Dependency Injection:** Enabling mocking and testing
4. **Incremental Approach:** Phased implementation with validation at each step
5. **SOLID Compliance:** Following all SOLID principles
6. **Contributing Guidelines:** Adhering to code quality standards

**Estimated Timeline:** 3-4 weeks for complete implementation

**Next Steps:**
1. Review and approve this plan
2. Create GitHub issues for each phase
3. Begin Phase 1: Service extraction
4. Regular check-ins after each phase

---

**Document Version:** 1.0
**Last Updated:** 2026-01-10
**Author:** GitHub Copilot + Developer
**Status:** ✅ Ready for Implementation
