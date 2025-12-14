# VaultManager::open_vault() Refactoring Plan

## Current State
- **Cognitive Complexity**: 75 (threshold: 25)
- **Lines**: ~276 lines
- **Responsibilities**: 7+ distinct operations

## Problems
1. Too many nested conditionals
2. Multiple responsibilities in one function
3. Hard to test individual components
4. Difficult to understand flow

## Refactoring Strategy

### Helper Structures
```cpp
struct VaultFileMetadata {
    std::vector<uint8_t> salt;
    std::vector<uint8_t> iv;
    bool has_fec = false;
    uint8_t fec_redundancy = 0;
    bool requires_yubikey = false;
    std::string yubikey_serial;
    std::vector<uint8_t> yubikey_challenge;
};

struct ParsedVaultData {
    VaultFileMetadata metadata;
    std::vector<uint8_t> ciphertext;
};
```

### Extracted Functions

#### 1. `parse_vault_format()` - Complexity ~20
**Purpose**: Parse vault file format and extract metadata
**Input**: `std::vector<uint8_t>& file_data`
**Output**: `std::expected<ParsedVaultData, VaultError>`
**Responsibilities**:
- Validate file size
- Extract salt and IV
- Parse flags byte
- Handle Reed-Solomon metadata
- Extract YubiKey metadata
- Return structured data

#### 2. `decode_with_reed_solomon()` - Complexity ~10
**Purpose**: Decode RS-encoded data
**Input**: `encoded_data, original_size, redundancy`
**Output**: `std::expected<std::vector<uint8_t>, VaultError>`
**Responsibilities**:
- Create/reuse ReedSolomon instance
- Decode data
- Log results
- Handle errors

#### 3. `authenticate_yubikey()` - Complexity ~15
**Purpose**: Perform YubiKey challenge-response
**Input**: `VaultFileMetadata`, `encryption_key&`
**Output**: `std::expected<void, VaultError>`
**Responsibilities**:
- Initialize YubiKey manager
- Verify device present
- Check serial authorization
- Perform challenge-response
- XOR with encryption key

#### 4. `decrypt_and_parse_vault()` - Complexity ~10
**Purpose**: Decrypt and parse protobuf
**Input**: `ciphertext, key, iv`
**Output**: `std::expected<keeptower::VaultData, VaultError>`
**Responsibilities**:
- Decrypt data
- Parse protobuf
- Migrate schema if needed
- Return vault data

### Refactored `open_vault()` - Complexity ~15
```cpp
bool VaultManager::open_vault(const std::string& path, const Glib::ustring& password) {
    // 1. Close existing vault if needed
    if (m_vault_open && !close_vault()) {
        return false;
    }

    // 2. Read vault file
    std::vector<uint8_t> file_data;
    if (!read_vault_file(path, file_data)) {
        return false;
    }

    // 3. Parse vault format and metadata
    auto parsed_result = parse_vault_format(file_data);
    if (!parsed_result) {
        log_error(parsed_result.error());
        return false;
    }
    auto [metadata, ciphertext] = std::move(parsed_result.value());

    // 4. Derive encryption key from password
    m_encryption_key.resize(KEY_LENGTH);
    if (!derive_key(password, metadata.salt, m_encryption_key)) {
        return false;
    }

    // 5. Authenticate with YubiKey if required
    if (metadata.requires_yubikey) {
        auto yk_result = authenticate_yubikey(metadata, m_encryption_key);
        if (!yk_result) {
            log_error(yk_result.error());
            secure_clear(m_encryption_key);
            return false;
        }
    }

    // 6. Lock sensitive memory
    lock_memory(m_encryption_key);
    lock_memory(metadata.salt);

    // 7. Decrypt and parse vault data
    auto vault_result = decrypt_and_parse_vault(ciphertext, m_encryption_key, metadata.iv);
    if (!vault_result) {
        log_error(vault_result.error());
        return false;
    }

    // 8. Store vault state
    m_vault_data = std::move(vault_result.value());
    m_salt = std::move(metadata.salt);
    m_use_reed_solomon = metadata.has_fec;
    m_rs_redundancy_percent = metadata.fec_redundancy;
    m_fec_loaded_from_file = true;
    m_current_vault_path = path;
    m_vault_open = true;
    m_modified = false;

    return true;
}
```

## Benefits
1. **Reduced Complexity**: From 75 to ~15 per function
2. **Testability**: Each function can be unit tested
3. **Readability**: Clear separation of concerns
4. **Maintainability**: Easier to modify individual parts
5. **Error Handling**: Structured with std::expected
6. **Reusability**: Helper functions can be used elsewhere

## Implementation Order
1. Add helper structures to VaultManager.h
2. Implement `parse_vault_format()` with tests
3. Implement `decode_with_reed_solomon()` with tests
4. Implement `authenticate_yubikey()` with tests
5. Implement `decrypt_and_parse_vault()` with tests
6. Refactor `open_vault()` to use new functions
7. Run full test suite
8. Commit

## Testing Strategy
- Unit test each new helper function
- Integration test with existing vaults
- Test error conditions
- Verify backward compatibility
