// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024 KeepTower Contributors
// File: src/core/controllers/VaultCreationOrchestrator.h

#ifndef KEEPTOWER_VAULT_CREATION_ORCHESTRATOR_H
#define KEEPTOWER_VAULT_CREATION_ORCHESTRATOR_H

#include "../VaultError.h"
#include "../MultiUserTypes.h"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <array>
#include <vector>
#include <cstdint>
#include <ctime>
#include <glibmm/ustring.h>

namespace KeepTower {

// Forward declarations
class VaultCryptoService;
class VaultYubiKeyService;
class VaultFileService;

/**
 * @brief Orchestrates multi-step vault creation process
 *
 * VaultCreationOrchestrator coordinates the complex process of creating a V2 vault
 * by delegating to specialized service classes. This follows the Orchestrator/Coordinator
 * pattern and maintains Single Responsibility Principle.
 *
 * **Design Philosophy:**
 * - **Orchestration Only**: Coordinates services, doesn't implement logic
 * - **Dependency Injection**: Services injected via constructor
 * - **Testability**: Services can be mocked for unit testing
 * - **Progress Reporting**: Built-in callback support
 * - **Step-by-Step**: Each step is a small, focused method
 *
 * **Responsibilities:**
 * 1. Coordinate vault creation steps in correct order
 * 2. Report progress to UI layer
 * 3. Handle errors and rollback if needed
 * 4. Provide both sync and async interfaces
 *
 * **NOT Responsible For:**
 * - Cryptographic operations (VaultCryptoService)
 * - YubiKey operations (VaultYubiKeyService)
 * - File I/O (VaultFileService)
 * - State management (VaultManager)
 *
 * @section creation_steps Creation Steps
 *
 * 1. **Validation** - Verify all parameters
 * 2. **Generate DEK** - Create Data Encryption Key
 * 3. **Derive Admin KEK** - PBKDF2 from password
 * 4. **YubiKey Enrollment** (if enabled) - Two-touch process
 * 5. **Create Admin Key Slot** - Wrap DEK with KEK
 * 6. **Create Vault Header** - Initialize security policy
 * 7. **Serialize & Encrypt** - Protect vault data
 * 8. **Write to File** - Atomic write with FEC
 *
 * @section progress_reporting Progress Reporting
 *
 * Progress callbacks provide:
 * - Step number (1-8)
 * - Total steps (8)
 * - Step description
 * - Progress percentage (0-100)
 *
 * @section usage Usage Example
 *
 * @code
 * // Create services
 * auto crypto = std::make_shared<VaultCryptoService>();
 * auto yubikey = std::make_shared<VaultYubiKeyService>();
 * auto file = std::make_shared<VaultFileService>();
 *
 * // Create orchestrator
 * VaultCreationOrchestrator orchestrator(crypto, yubikey, file);
 *
 * // Setup parameters
 * VaultCreationOrchestrator::CreationParams params;
 * params.path = "/path/to/vault.vault";
 * params.admin_username = "admin";
 * params.admin_password = "strong_password";
 * params.policy = default_policy;
 * params.progress_callback = [](int step, int total, const std::string& msg) {
 *     std::cout << "Step " << step << "/" << total << ": " << msg << std::endl;
 * };
 *
 * // Create vault (synchronous)
 * auto result = orchestrator.create_vault_v2_sync(params);
 * if (!result) {
 *     // Handle error
 * }
 * @endcode
 *
 * @since Version 0.3.2 (Phase 2 Refactoring)
 * @author KeepTower Development Team
 */
class VaultCreationOrchestrator {
public:
    // ========================================================================
    // Types and Structures
    // ========================================================================

    /**
     * @brief Progress reporting callback type
     *
     * @param current_step Current step number (1-based)
     * @param total_steps Total number of steps
     * @param step_description Human-readable description
     */
    using ProgressCallback = std::function<void(
        int current_step,
        int total_steps,
        const std::string& step_description)>;

    /**
     * @brief Result of successful vault creation
     */
    struct CreationResult {
        std::array<uint8_t, 32> dek;             ///< Data Encryption Key (for VaultManager)
        VaultHeaderV2 header;                    ///< Vault header structure
        std::string file_path;                   ///< Actual file path written
        bool memory_locked;                      ///< Whether DEK was successfully locked in memory

        CreationResult() : dek{}, memory_locked(false) {}
    };

    /**
     * @brief Completion callback for async operations
     *
     * @param result CreationResult on success or VaultError on failure
     */
    using CompletionCallback = std::function<void(VaultResult<CreationResult>)>;

    /**
     * @brief Enumeration of creation steps for progress tracking
     */
    enum class CreationStep {
        Validation,        ///< Step 1: Validate input parameters
        GenerateDEK,       ///< Step 2: Generate Data Encryption Key
        DeriveAdminKEK,    ///< Step 3: Derive admin Key Encryption Key
        EnrollYubiKey,     ///< Step 4: YubiKey enrollment (if enabled)
        CreateKeySlot,     ///< Step 5: Create admin key slot
        CreateHeader,      ///< Step 6: Initialize vault header
        EncryptData,       ///< Step 7: Serialize and encrypt vault data
        WriteFile          ///< Step 8: Write to file with FEC
    };

    /**
     * @brief All parameters needed for vault creation
     */
    struct CreationParams {
        std::string path;                        ///< Filesystem path for vault file
        Glib::ustring admin_username;            ///< Initial admin username
        Glib::ustring admin_password;            ///< Admin password
        VaultSecurityPolicy policy;              ///< Security settings
        std::optional<std::string> yubikey_pin;  ///< Optional YubiKey PIN
        bool enforce_fips = false;               ///< Enforce FIPS-140-3 mode
        ProgressCallback progress_callback;      ///< Optional progress reporting

        CreationParams() = default;
    };

    // ========================================================================
    // Constructor and Initialization
    // ========================================================================

    /**
     * @brief Construct orchestrator with injected services
     *
     * Services are injected via constructor for:
     * - Testability (can inject mocks)
     * - Flexibility (can swap implementations)
     * - Explicit dependencies
     *
     * @param crypto Cryptographic operations service
     * @param yubikey YubiKey operations service
     * @param file File I/O operations service
     *
     * @note Services are shared_ptr to allow sharing with VaultManager
     * @note All services must be non-null
     */
    VaultCreationOrchestrator(
        std::shared_ptr<VaultCryptoService> crypto,
        std::shared_ptr<VaultYubiKeyService> yubikey,
        std::shared_ptr<VaultFileService> file);

    // ========================================================================
    // Public Interface
    // ========================================================================

    /**
     * @brief Create V2 vault synchronously
     *
     * Executes all creation steps in sequence on the calling thread.
     * Suitable for:
     * - Command-line tools
     * - Background workers
     * - Unit tests
     *
     * @param params All creation parameters
     * @return CreationResult on success, VaultError on failure
     *
     * @note This method blocks until completion (may take 10-15 seconds)
     * @note Progress callbacks are called synchronously
     * @note Not suitable for UI thread if YubiKey is enabled
     */
    [[nodiscard]] VaultResult<CreationResult> create_vault_v2_sync(
        const CreationParams& params);

    /**
     * @brief Create V2 vault asynchronously
     *
     * Executes creation in background thread, reports progress on GTK thread.
     * Suitable for:
     * - GTK UI applications
     * - Long-running operations
     * - Operations requiring user interaction (YubiKey touches)
     *
     * @param params All creation parameters
     * @param completion_callback Called on GTK thread when complete
     *
     * @note Progress callbacks are called on GTK thread via Glib::signal_idle()
     * @note Completion callback is called on GTK thread
     * @note Thread-safe for concurrent calls (each gets own thread)
     */
    void create_vault_v2_async(
        const CreationParams& params,
        CompletionCallback completion_callback);

private:
    // ========================================================================
    // Step Implementation Methods
    // ========================================================================

    /**
     * @brief Step 1: Validate all input parameters
     *
     * Validates:
     * - Path is not empty and writable
     * - Username is valid (3-64 chars)
     * - Password meets policy requirements
     * - Policy is valid (iterations, min length, etc.)
     * - YubiKey PIN format (if provided)
     *
     * @return Success or VaultError::InvalidParameter
     */
    [[nodiscard]] VaultResult<> validate_params(const CreationParams& params);

    /**
     * @brief Step 2: Generate Data Encryption Key
     *
     * Creates a cryptographically secure 256-bit DEK using system RNG.
     * DEK will be used to encrypt all vault data.
     *
     * @return DEKResult with key and memory lock status, or VaultError::CryptoError
     */
    struct DEKData {
        std::array<uint8_t, 32> dek;
        bool memory_locked;
    };
    [[nodiscard]] VaultResult<DEKData> generate_dek();

    /**
     * @brief Step 3: Derive admin Key Encryption Key from password
     *
     * Uses PBKDF2-HMAC-SHA256 to derive KEK from admin password.
     * Iteration count from security policy (default 100,000).
     *
     * @param params Creation parameters (password, policy)
     * @return KEK (32 bytes) and salt (32 bytes) or error
     */
    struct KEKResult {
        std::array<uint8_t, 32> kek;
        std::array<uint8_t, 32> salt;
    };
    [[nodiscard]] VaultResult<KEKResult> derive_admin_kek(
        const CreationParams& params);

    /**
     * @brief Step 4: Enroll YubiKey (if enabled in policy)
     *
     * Two-touch YubiKey enrollment:
     * 1. Create FIDO2 credential (touch 1)
     * 2. Challenge-response (touch 2)
     *
     * Combines YubiKey response with KEK via XOR for hybrid authentication.
     *
     * @param params Creation parameters (path, PIN)
     * @param kek KEK to combine with YubiKey (modified in-place)
     * @return YubiKey device info or error (if enabled), nullopt if disabled
     */
    struct EnrollmentData {
        std::string serial;
        std::array<uint8_t, 32> user_challenge;     ///< User-specific challenge (input)
        std::vector<uint8_t> policy_response;       ///< Policy challenge response
        std::vector<uint8_t> user_response;         ///< User challenge response
        std::vector<uint8_t> encrypted_pin;         ///< PIN encrypted with password-derived KEK
        std::vector<uint8_t> credential_id;         ///< FIDO2 credential ID
        uint8_t slot;
    };
    [[nodiscard]] VaultResult<std::optional<EnrollmentData>> enroll_yubikey(
        const CreationParams& params,
        std::array<uint8_t, 32>& kek);

    /**
     * @brief Step 5: Create admin key slot
     *
     * Creates first key slot for admin user:
     * - Wraps DEK with KEK using AES-256-KW
     * - Stores username, role, salt
     * - Initializes password history
     * - Sets timestamp
     *
     * @return KeySlot structure or error
     */
    [[nodiscard]] VaultResult<KeySlot> create_admin_key_slot(
        const CreationParams& params,
        const KEKResult& kek,
        const std::array<uint8_t, 32>& dek,
        const std::optional<EnrollmentData>& yubikey_data);

    /**
     * @brief Step 6: Create vault header
     *
     * Initializes VaultHeaderV2 with:
     * - Security policy
     * - Admin key slot
     * - Metadata (created_at, version)
     *
     * @return VaultHeaderV2 structure
     */
    [[nodiscard]] VaultResult<VaultHeaderV2> create_header(
        const CreationParams& params,
        const KeySlot& admin_slot);

    /**
     * @brief Step 7: Encrypt vault data
     *
     * - Serializes initial (empty) VaultData protobuf
     * - Encrypts with DEK using AES-256-GCM
     * - Returns ciphertext and IV
     *
     * @return Encrypted data and IV or error
     */
    struct EncryptionResult {
        std::vector<uint8_t> ciphertext;
        std::vector<uint8_t> iv;
    };
    [[nodiscard]] VaultResult<EncryptionResult> encrypt_vault_data(
        const std::array<uint8_t, 32>& dek);

    /**
     * @brief Step 8: Write vault file
     *
     * - Builds V2FileHeader (header + encryption metadata)
     * - Applies FEC if enabled in policy
     * - Writes atomically with secure permissions
     * - Syncs to disk
     *
     * @return Success or file I/O error
     */
    [[nodiscard]] VaultResult<> write_vault_file(
        const CreationParams& params,
        const VaultHeaderV2& header,
        const EncryptionResult& encrypted);

    // ========================================================================
    // Helper Methods
    // ========================================================================

    /**
     * @brief Report progress if callback is set
     */
    void report_progress(
        const CreationParams& params,
        CreationStep step,
        const std::string& description);

    /**
     * @brief Get step number for progress reporting
     */
    static int step_number(CreationStep step);

    /**
     * @brief Get total number of steps
     */
    static constexpr int total_steps() { return 8; }

    // ========================================================================
    // Member Variables
    // ========================================================================

    std::shared_ptr<VaultCryptoService> m_crypto;    ///< Crypto operations service
    std::shared_ptr<VaultYubiKeyService> m_yubikey;  ///< YubiKey operations service
    std::shared_ptr<VaultFileService> m_file;        ///< File I/O service
};

} // namespace KeepTower

#endif // KEEPTOWER_VAULT_CREATION_ORCHESTRATOR_H
