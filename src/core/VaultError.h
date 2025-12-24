// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng
//
// VaultError.h - Error types for VaultManager operations
// C++23 std::expected-based error handling

#ifndef KEEPTOWER_VAULT_ERROR_H
#define KEEPTOWER_VAULT_ERROR_H

#include <expected>
#include <string>
#include <string_view>

namespace KeepTower {

// Comprehensive error types for vault operations
enum class VaultError {
    // File operations
    FileNotFound,
    FileOpenFailed,
    FileReadFailed,
    FileWriteFailed,
    FilePermissionDenied,

    // Vault operations
    VaultAlreadyOpen,
    VaultNotOpen,
    VaultCorrupted,

    // Cryptography
    InvalidPassword,
    EncryptionFailed,
    DecryptionFailed,
    KeyDerivationFailed,

    // Data operations
    SerializationFailed,
    DeserializationFailed,
    InvalidData,
    CorruptedFile,
    InvalidProtobuf,
    DecodingFailed,
    UnsupportedVersion,
    FECEncodingFailed,
    FECDecodingFailed,

    // YubiKey operations
    YubiKeyMetadataMissing,
    YubiKeyNotConnected,
    YubiKeyDeviceInfoFailed,
    YubiKeyUnauthorized,
    YubiKeyChallengeResponseFailed,
    YubiKeyError,           // General YubiKey operation failed
    YubiKeyNotPresent,      // YubiKey required but not connected

    // Account operations
    AccountNotFound,
    InvalidIndex,
    DuplicateAccount,

    // V2 Multi-User operations
    InvalidUsername,
    UserAlreadyExists,
    UserNotFound,
    AuthenticationFailed,
    PermissionDenied,
    WeakPassword,
    SelfRemovalNotAllowed,
    LastAdministrator,
    MaxUsersReached,
    CryptoError,
    FileReadError,
    FileWriteError,

    // Generic
    UnknownError
};

// Convert error enum to human-readable string
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
        case VaultError::UnknownError:
            return "Unknown error occurred";
    }
    return "Unknown error";
}

// Helper type aliases
template<typename T = void>
using VaultResult = std::expected<T, VaultError>;

} // namespace KeepTower

#endif // KEEPTOWER_VAULT_ERROR_H
