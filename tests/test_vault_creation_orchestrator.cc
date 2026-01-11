/**
 * @file test_vault_creation_orchestrator.cc
 * @brief Unit tests for VaultCreationOrchestrator
 *
 * Tests the orchestrator with mocked services to verify:
 * - Each step method independently
 * - Error handling at each step
 * - Progress callbacks
 * - Full sync creation flow
 */

#include <gtest/gtest.h>
#include "../src/core/controllers/VaultCreationOrchestrator.h"
#include "../src/core/services/VaultCryptoService.h"
#include "../src/core/services/VaultYubiKeyService.h"
#include "../src/core/services/VaultFileService.h"

using namespace KeepTower;

// ============================================================================
// Manual Mock Service Implementations
// ============================================================================

class MockVaultCryptoService : public VaultCryptoService {
public:
    // Control behaviors
    bool fail_generate_dek = false;
    bool fail_derive_kek = false;
    bool fail_wrap_key = false;
    bool fail_encrypt = false;

    std::expected<VaultCryptoService::DEKResult, VaultError>
    generate_dek(uint8_t key_size) override {
        if (fail_generate_dek) {
            return std::unexpected(VaultError::CryptoError);
        }
        DEKResult result;
        result.dek.resize(key_size);
        std::fill(result.dek.begin(), result.dek.end(), 0xAA);
        result.memory_locked = true;
        return result;
    }

    std::expected<VaultCryptoService::KEKResult, VaultError>
    derive_kek(const Glib::ustring& password,
               std::optional<std::span<const uint8_t>> salt,
               uint32_t iterations) override {
        (void)password; (void)salt; (void)iterations;
        if (fail_derive_kek) {
            return std::unexpected(VaultError::CryptoError);
        }
        KEKResult result;
        result.kek.resize(32);
        std::fill(result.kek.begin(), result.kek.end(), 0xBB);
        result.salt.resize(32);
        std::fill(result.salt.begin(), result.salt.end(), 0xCC);
        return result;
    }

    std::expected<VaultCryptoService::WrapResult, VaultError>
    wrap_key(std::span<const uint8_t> dek,
             std::span<const uint8_t> kek) override {
        (void)dek; (void)kek;
        if (fail_wrap_key) {
            return std::unexpected(VaultError::CryptoError);
        }
        WrapResult result;
        result.wrapped_key.resize(40);
        std::fill(result.wrapped_key.begin(), result.wrapped_key.end(), 0xEE);
        return result;
    }

    std::expected<VaultCryptoService::EncryptionResult, VaultError>
    encrypt_vault_data(std::span<const uint8_t> plaintext,
                      std::span<const uint8_t> dek) override {
        (void)plaintext; (void)dek;
        if (fail_encrypt) {
            return std::unexpected(VaultError::CryptoError);
        }
        EncryptionResult result;
        result.ciphertext.resize(100);
        result.iv.resize(12);
        return result;
    }

    std::expected<std::vector<uint8_t>, VaultError>
    unwrap_key(std::span<const uint8_t> wrapped_dek,
               std::span<const uint8_t> kek) override {
        (void)wrapped_dek; (void)kek;
        return std::unexpected(VaultError::NotImplemented);
    }

    std::expected<std::vector<uint8_t>, VaultError>
    decrypt_vault_data(std::span<const uint8_t> ciphertext,
                      std::span<const uint8_t> dek,
                      std::span<const uint8_t> iv) override {
        (void)ciphertext; (void)dek; (void)iv;
        return std::unexpected(VaultError::NotImplemented);
    }
};

class MockVaultYubiKeyService : public VaultYubiKeyService {
public:
    bool fail_enroll = false;

    std::expected<VaultYubiKeyService::EnrollmentResult, VaultError>
    enroll_yubikey(std::span<const uint8_t> policy_challenge,
                  std::span<const uint8_t> user_challenge,
                  const std::string& pin,
                  uint8_t slot) override {
        (void)policy_challenge; (void)user_challenge; (void)pin; (void)slot;
        if (fail_enroll) {
            return std::unexpected(VaultError::YubiKeyError);
        }
        EnrollmentResult result;
        result.device_info.serial = "12345678";
        result.device_info.is_fips_mode = true;
        result.policy_response.resize(32);
        result.user_response.resize(32);
        std::fill(result.user_response.begin(), result.user_response.end(), 0xDD);
        return result;
    }

    std::expected<VaultYubiKeyService::ChallengeResult, VaultError>
    challenge_response(std::span<const uint8_t> challenge,
                      const std::string& pin,
                      uint8_t slot) override {
        (void)challenge; (void)pin; (void)slot;
        return std::unexpected(VaultError::NotImplemented);
    }

    std::expected<VaultYubiKeyService::DeviceInfo, VaultError>
    get_device_info() override {
        return std::unexpected(VaultError::NotImplemented);
    }

    bool is_device_present() override {
        return true;
    }
};

class MockVaultFileService : public VaultFileService {
public:
    bool fail_write = false;

    std::expected<VaultFileService::ReadResult, VaultError>
    read_vault_file(const std::string& path) override {
        (void)path;
        return std::unexpected(VaultError::NotImplemented);
    }

    std::expected<void, VaultError>
    write_vault_file(const std::string& path,
                    std::span<const uint8_t> data,
                    bool create_backup) override {
        (void)path; (void)data; (void)create_backup;
        if (fail_write) {
            return std::unexpected(VaultError::FileWriteError);
        }
        return {};
    }

    std::expected<void, VaultError>
    backup_vault_file(const std::string& path) override {
        (void)path;
        return std::unexpected(VaultError::NotImplemented);
    }

    std::expected<VaultFormatVersion, VaultError>
    detect_format(const std::string& path) override {
        (void)path;
        return std::unexpected(VaultError::NotImplemented);
    }

        orchestrator = std::make_unique<VaultCreationOrchestrator>(
            mock_crypto, mock_yubikey, mock_file);

        // Default valid parameters
        params.vault_path = "/tmp/test.vault";
        params.username = "admin";
        params.password = "SecurePassword123!";
        params.policy = create_default_policy();
        params.progress_callback = [this](float progress, const std::string& msg) {
            progress_updates.push_back({progress, msg});
        };
    }

    VaultSecurityPolicy create_default_policy() {
        VaultSecurityPolicy policy;
        policy.min_password_length = 8;
        policy.require_uppercase = true;
        policy.require_lowercase = true;
        policy.require_digit = true;
        policy.require_special_char = true;
        policy.password_history_depth = 5;
        policy.require_yubikey = false;
        policy.yubikey_algorithm = 0x02;  // HMAC-SHA256
        policy.pbkdf2_iterations = 100000;
        return policy;
    }

    std::shared_ptr<MockVaultCryptoService> mock_crypto;
    std::shared_ptr<MockVaultYubiKeyService> mock_yubikey;
    std::shared_ptr<MockVaultFileService> mock_file;
    std::unique_ptr<VaultCreationOrchestrator> orchestrator;
    VaultCreationOrchestrator::CreationParams params;
    std::vector<std::pair<float, std::string>> progress_updates;
};

// ============================================================================
// Step 1: validate_params() Tests
// ============================================================================

TEST_F(VaultCreationOrchestratorTest, ValidateParams_ValidInput) {
    auto result = orchestrator->create_vault_v2_sync(params);

    // Will fail at later steps, but validation should pass
    ASSERT_TRUE(result.has_value() || result.error() != VaultError::InvalidParameter);
}

TEST_F(VaultCreationOrchestratorTest, ValidateParams_EmptyPath) {
    params.vault_path = "";

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::InvalidParameter);
}

TEST_F(VaultCreationOrchestratorTest, ValidateParams_EmptyUsername) {
    params.username = "";

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::InvalidParameter);
}

TEST_F(VaultCreationOrchestratorTest, ValidateParams_ShortPassword) {
    params.password = "short";

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::PasswordTooWeak);
}

TEST_F(VaultCreationOrchestratorTest, ValidateParams_NoUppercase) {
    params.password = "weakpassword123!";

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::PasswordTooWeak);
}

TEST_F(VaultCreationOrchestratorTest, ValidateParams_NoDigit) {
    params.password = "WeakPassword!";

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::PasswordTooWeak);
}

TEST_F(VaultCreationOrchestratorTest, ValidateParams_NoSpecialChar) {
    params.password = "WeakPassword123";

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::PasswordTooWeak);
}

TEST_F(VaultCreationOrchestratorTest, ValidateParams_YubiKeyRequiredButNoPIN) {
    params.policy.require_yubikey = true;
    params.yubikey_pin = std::nullopt;

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::InvalidParameter);
}

// ============================================================================
// Step 2: generate_dek() Tests
// ============================================================================

TEST_F(VaultCreationOrchestratorTest, GenerateDEK_Success) {
    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->memory_locked);
}

TEST_F(VaultCreationOrchestratorTest, GenerateDEK_CryptoError) {
    mock_crypto->fail_generate_dek = true;

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::CryptoError);
}

// ============================================================================
// Step 3: derive_admin_kek() Tests
// ============================================================================

TEST_F(VaultCreationOrchestratorTest, DeriveKEK_Success) {
    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value());
}

TEST_F(VaultCreationOrchestratorTest, DeriveKEK_CryptoError) {
    mock_crypto->fail_derive_kek = true;

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::CryptoError);
}

// ============================================================================
// Step 4: enroll_yubikey() Tests
// ============================================================================

TEST_F(VaultCreationOrchestratorTest, EnrollYubiKey_Success) {
    params.policy.require_yubikey = true;
    params.yubikey_pin = "123456";

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value());
}

TEST_F(VaultCreationOrchestratorTest, EnrollYubiKey_EnrollmentError) {
    params.policy.require_yubikey = true;
    params.yubikey_pin = "123456";
    mock_yubikey->fail_enroll = true;

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::YubiKeyError);
}

TEST_F(VaultCreationOrchestratorTest, EnrollYubiKey_NotRequired) {
    params.policy.require_yubikey = false;

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value());
}

// ============================================================================
// Step 5: create_admin_key_slot() Tests
// ============================================================================

TEST_F(VaultCreationOrchestratorTest, CreateKeySlot_Success) {
    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value());
}

TEST_F(VaultCreationOrchestratorTest, CreateKeySlot_WrapError) {
    mock_crypto->fail_wrap_key = true;

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::CryptoError);
}

// ============================================================================
// Step 8: write_vault_file() Tests
// ============================================================================

TEST_F(VaultCreationOrchestratorTest, WriteVaultFile_Success) {
    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->file_path, params.vault_path);
}

TEST_F(VaultCreationOrchestratorTest, WriteVaultFile_FileError) {
    mock_file->fail_write = true;
    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::FileWriteError);
}

// ============================================================================
// Progress Callback Tests
// ============================================================================

TEST_F(VaultCreationOrchestratorTest, ProgressCallbacks_AllStepsReported) {
    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value());

    // Verify progress updates (should have at least 8 steps)
    EXPECT_GE(progress_updates.size(), 8);

    // Verify progress is increasing
    for (size_t i = 1; i < progress_updates.size(); ++i) {
        EXPECT_GE(progress_updates[i].first, progress_updates[i-1].first)
            << "Progress should be monotonically increasing";
    }

    // Verify final progress is 100%
    EXPECT_EQ(progress_updates.back().first, 100.0f);
}

TEST_F(VaultCreationOrchestratorTest, ProgressCallbacks_NoCallback) {
    params.progress_callback = nullptr;

    // Should not crash with nullptr callback
    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value());
}

// ============================================================================
// Full Integration Tests
// ============================================================================

TEST_F(VaultCreationOrchestratorTest, FullSync_SuccessWithoutYubiKey) {
    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->file_path, params.vault_path);
    EXPECT_TRUE(result->memory_locked);
    EXPECT_FALSE(result->header.key_slots.empty());
    EXPECT_EQ(result->header.key_slots[0].username, params.username);
}

TEST_F(VaultCreationOrchestratorTest, FullSync_SuccessWithYubiKey) {
    params.policy.require_yubikey = true;
    params.yubikey_pin = "123456";

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->header.key_slots[0].yubikey_enrolled);
    EXPECT_EQ(result->header.key_slots[0].yubikey_serial, "12345678");

TEST_F(VaultCreationOrchestratorTest, Constructor_NullCryptoService) {
    EXPECT_THROW(
        VaultCreationOrchestrator(nullptr, mock_yubikey, mock_file),
        std::invalid_argument
    );
}

TEST_F(VaultCreationOrchestratorTest, Constructor_NullYubiKeyService) {
    EXPECT_THROW(
        VaultCreationOrchestrator(mock_crypto, nullptr, mock_file),
        std::invalid_argument
    );
}

TEST_F(VaultCreationOrchestratorTest, Constructor_NullFileService) {
    EXPECT_THROW(
        VaultCreationOrchestrator(mock_crypto, mock_yubikey, nullptr),
        std::invalid_argument
    );
}

// ============================================================================
// Async Interface Tests
// ============================================================================

TEST_F(VaultCreationOrchestratorTest, Async_NotYetImplemented) {
    auto future = orchestrator->create_vault_v2_async(params);

    ASSERT_TRUE(future.valid());

    auto result = future.get();

    // Should return NotImplemented error until Phase 3
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::NotImplemented);
}
