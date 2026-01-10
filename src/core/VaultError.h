// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file VaultError.h
 * @brief Error types and result handling for VaultManager operations
 *
 * Provides comprehensive error types for all vault operations using C++23
 * std::expected-based error handling. This enables type-safe error propagation
 * without exceptions, with clear error semantics.
 *
 * @section usage Usage Example
 * @code
 * VaultResult<std::string> result = vault_manager->get_account_name(idx);
 * if (result.has_value()) {
 *     std::string name = result.value();
 * } else {
 *     VaultError error = result.error();
 *     std::cerr << "Error: " << to_string(error) << "\n";
 * }
 * @endcode
 *
 * @section categories Error Categories
 * - **File Operations:** File I/O and permission errors
 * - **Vault Operations:** Vault lifecycle and corruption errors
 * - **Cryptography:** Encryption, decryption, and key derivation errors
 * - **Data Operations:** Serialization and format errors
 * - **YubiKey:** Hardware token operations
 * - **Account Operations:** Account management errors
 * - **Multi-User:** V2 authentication and permissions
 */

#ifndef KEEPTOWER_VAULT_ERROR_H
#define KEEPTOWER_VAULT_ERROR_H

#include <expected>
#include <string>
#include <string_view>

namespace KeepTower {

/**
 * @brief Comprehensive error types for vault operations
 *
 * Categorized error enumeration covering all possible vault operation failures.
 * Used with std::expected for type-safe error handling without exceptions.
 *
 * @note All error codes are designed to be user-friendly when converted via to_string()
 */
enum class VaultError {
    // File operations
    FileNotFound,              ///< Vault file does not exist
    FileOpenFailed,            ///< Unable to open vault file
    FileReadFailed,            ///< Error reading from vault file
    FileWriteFailed,           ///< Error writing to vault file
    FilePermissionDenied,      ///< Insufficient permissions for file operation

    // Vault operations
    VaultAlreadyOpen,          ///< Attempted to open vault when one is already open
    VaultNotOpen,              ///< Operation requires open vault
    VaultCorrupted,            ///< Vault data integrity check failed

    // Cryptography
    InvalidPassword,           ///< Incorrect vault password provided
    EncryptionFailed,          ///< AES-256-GCM encryption operation failed
    DecryptionFailed,          ///< AES-256-GCM decryption operation failed
    KeyDerivationFailed,       ///< PBKDF2 key derivation failed

    // Data operations
    SerializationFailed,       ///< Failed to serialize vault data to protobuf
    DeserializationFailed,     ///< Failed to deserialize protobuf data
    InvalidData,               ///< Data format validation failed
    CorruptedFile,             ///< File structure corrupted
    InvalidProtobuf,           ///< Protobuf parsing error
    DecodingFailed,            ///< Reed-Solomon decoding error
    UnsupportedVersion,        ///< Vault version not supported by this build
    FECEncodingFailed,         ///< Forward error correction encoding failed
    FECDecodingFailed,         ///< Forward error correction decoding failed

    // YubiKey operations
    YubiKeyMetadataMissing,    ///< Vault requires YubiKey but metadata absent
    YubiKeyNotConnected,       ///< YubiKey device not detected
    YubiKeyDeviceInfoFailed,   ///< Unable to read YubiKey device information
    YubiKeyUnauthorized,       ///< YubiKey serial does not match vault
    YubiKeyChallengeResponseFailed, ///< Challenge-response protocol failed
    YubiKeyError,              ///< General YubiKey operation failed
    YubiKeyNotPresent,         ///< YubiKey required but not connected

    // Account operations
    AccountNotFound,           ///< Account does not exist at specified index
    InvalidIndex,              ///< Account index out of bounds
    DuplicateAccount,          ///< Account with same name already exists

    // V2 Multi-User operations
    InvalidUsername,           ///< Username validation failed
    UserAlreadyExists,         ///< User with this username already registered
    UserNotFound,              ///< User account does not exist
    AuthenticationFailed,      ///< Username or password incorrect
    PermissionDenied,          ///< User lacks required permissions (role)
    WeakPassword,              ///< Password does not meet security requirements
    PasswordReused,            ///< Password appears in user's password history
    SelfRemovalNotAllowed,     ///< Users cannot remove their own account
    LastAdministrator,         ///< Cannot remove last admin user
    MaxUsersReached,           ///< Maximum user limit reached
    CryptoError,               ///< Generic cryptographic operation error
    FileReadError,             ///< Generic file read error
    FileWriteError,            ///< Generic file write error

    // Threading
    Busy,                      ///< Operation already in progress

    // Generic
    UnknownError               ///< Unspecified error occurred
};

/**
 * @brief Convert error enum to human-readable string
 * @param error The VaultError to convert
 * @return User-friendly error message
 *
 * Provides human-readable error messages suitable for display in UI dialogs.
 * All messages are concise and actionable where possible.
 *
 * @note This function is constexpr and noexcept for performance
 */
inline constexpr std::string_view to_string(VaultError error) noexcept {
    switch (error) {
        case VaultError::FileNotFound:
            return "File not found";
        case VaultError::FileOpenFailed:
            return "Failed to open file";
        case VaultError::FileReadFailed:
            return "Failed to read file";
        case VaultError::FileWriteFailed:
            return "Failed to write file";
        case VaultError::FilePermissionDenied:
            return "Permission denied";
        case VaultError::VaultAlreadyOpen:
            return "A vault is already open";
        case VaultError::VaultNotOpen:
            return "No vault is open";
        case VaultError::VaultCorrupted:
            return "Vault data is corrupted";
        case VaultError::InvalidPassword:
            return "Invalid password";
        case VaultError::EncryptionFailed:
            return "Encryption failed";
        case VaultError::DecryptionFailed:
            return "Decryption failed";
        case VaultError::KeyDerivationFailed:
            return "Key derivation failed";
        case VaultError::SerializationFailed:
            return "Failed to serialize data";
        case VaultError::DeserializationFailed:
            return "Failed to deserialize data";
        case VaultError::InvalidData:
            return "Invalid data format";
        case VaultError::CorruptedFile:
            return "Vault file is corrupted";
        case VaultError::InvalidProtobuf:
            return "Invalid protobuf format";
        case VaultError::DecodingFailed:
            return "Reed-Solomon decoding failed";
        case VaultError::UnsupportedVersion:
            return "Unsupported vault version";
        case VaultError::FECEncodingFailed:
            return "Forward error correction encoding failed";
        case VaultError::FECDecodingFailed:
            return "Forward error correction decoding failed";
        case VaultError::YubiKeyMetadataMissing:
            return "YubiKey metadata missing from vault";
        case VaultError::YubiKeyNotConnected:
            return "YubiKey not connected";
        case VaultError::YubiKeyDeviceInfoFailed:
            return "Failed to get YubiKey device info";
        case VaultError::YubiKeyUnauthorized:
            return "YubiKey not authorized for this vault";
        case VaultError::YubiKeyChallengeResponseFailed:
            return "YubiKey challenge-response failed";
        case VaultError::YubiKeyError:
            return "YubiKey operation failed";
        case VaultError::YubiKeyNotPresent:
            return "YubiKey required but not present";
        case VaultError::AccountNotFound:
            return "Account not found";
        case VaultError::InvalidIndex:
            return "Invalid account index";
        case VaultError::DuplicateAccount:
            return "Account already exists";
        case VaultError::InvalidUsername:
            return "Invalid username";
        case VaultError::UserAlreadyExists:
            return "User already exists";
        case VaultError::UserNotFound:
            return "User not found";
        case VaultError::AuthenticationFailed:
            return "Authentication failed";
        case VaultError::PermissionDenied:
            return "Permission denied";
        case VaultError::WeakPassword:
            return "Password does not meet security requirements";
        case VaultError::PasswordReused:
            return "Password was used previously";
        case VaultError::SelfRemovalNotAllowed:
            return "Cannot remove yourself";
        case VaultError::LastAdministrator:
            return "Cannot remove the last administrator";
        case VaultError::MaxUsersReached:
            return "Maximum number of users reached";
        case VaultError::CryptoError:
            return "Cryptographic operation failed";
        case VaultError::FileReadError:
            return "File read error";
        case VaultError::FileWriteError:
            return "File write error";
        case VaultError::Busy:
            return "Operation already in progress";
        case VaultError::UnknownError:
            return "Unknown error occurred";
    }
    return "Unknown error";
}

/**
 * @brief Type alias for std::expected<T, VaultError>
 * @tparam T The success value type (defaults to void for operations with no return value)
 *
 * Convenient type alias for functions that return either a value or a VaultError.
 * Enables clear, type-safe error handling without exceptions.
 *
 * @section usage_examples Usage Examples
 * @code
 * // Function that returns a value on success
 * VaultResult<std::string> get_account_name(int idx) {
 *     if (idx < 0) return std::unexpected(VaultError::InvalidIndex);
 *     return accounts[idx].name;
 * }
 *
 * // Function that returns void on success (just error checking)
 * VaultResult<> save_vault() {
 *     if (!is_open) return std::unexpected(VaultError::VaultNotOpen);
 *     // ... save logic ...
 *     return {};  // Success
 * }
 * @endcode
 */
template<typename T = void>
using VaultResult = std::expected<T, VaultError>;

} // namespace KeepTower

#endif // KEEPTOWER_VAULT_ERROR_H
