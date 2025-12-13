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

    // YubiKey operations
    YubiKeyMetadataMissing,
    YubiKeyNotConnected,
    YubiKeyDeviceInfoFailed,
    YubiKeyUnauthorized,
    YubiKeyChallengeResponseFailed,

    // Account operations
    AccountNotFound,
    InvalidIndex,
    DuplicateAccount,

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
        case VaultError::AccountNotFound:
            return "Account not found";
        case VaultError::InvalidIndex:
            return "Invalid account index";
        case VaultError::DuplicateAccount:
            return "Account already exists";
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
