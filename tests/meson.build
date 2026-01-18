# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2025 tjdeveng

# Test configuration
gtest_dep = dependency('gtest', main: true, required: true)

# Get dependencies from parent
giomm_dep = dependency('giomm-2.68')

# Include directories - include project root for config.h and generated files
test_inc = [
    include_directories(
        '../src',
        '../src/core',
        '../src/ui',
        '../src/ui/windows',
        '../src/ui/dialogs'
    ),
    root_inc  # From parent meson.build, provides access to config.h and build dir
]

# Generate protobuf sources for tests (same as in src/meson.build)
fs = import('fs')
protoc = find_program('protoc', required: true)

proto_file = meson.project_source_root() / 'src' / 'record.proto'
proto_gen = custom_target('record_proto_test',
  input: files('../src/record.proto'),
  output: ['record.pb.cc', 'record.pb.h'],
  command: [protoc, '--proto_path=' + meson.project_source_root() / 'src', '--cpp_out=@OUTDIR@', '@PLAINNAME@'],
  depend_files: files('../src/record.proto')
)

# Common VaultManager dependencies (for V2 vault support)
vault_manager_common_sources = [
    '../src/core/VaultManager.cc',
    '../src/core/VaultManagerV2.cc',
    '../src/core/ReedSolomon.cc',
    '../src/core/MultiUserTypes.cc',
    '../src/core/KeyWrapping.cc',
    '../src/core/VaultFormatV2.cc',
    '../src/core/PasswordHistory.cc',
    '../src/core/crypto/VaultCrypto.cc',
    '../src/core/io/VaultIO.cc',
    '../src/core/serialization/VaultSerialization.cc',
    '../src/core/format/VaultFormat.cc',
    '../src/core/managers/AccountManager.cc',
    '../src/core/managers/GroupManager.cc',
    # Phase 2 Day 5: Add orchestrator and services
    '../src/core/controllers/VaultCreationOrchestrator.cc',
    '../src/core/services/VaultCryptoService.cc',
    '../src/core/services/VaultYubiKeyService.cc',
    '../src/core/services/VaultFileService.cc',
    proto_gen
]

# VaultManager tests
vault_manager_sources = [
    'test_vault_manager.cc',
] + vault_manager_common_sources

vault_manager_deps = [
    gtest_dep,
    protobuf_dep,
    openssl_dep,
    giomm_dep,
    libcorrect_dep
]

# Add YubiKey support if available
if yubikey_available
    vault_manager_sources += files('../src/core/managers/YubiKeyManager.cc')
    vault_manager_deps += [fido2_dep]
endif

vault_manager_test = executable(
    'vault_manager_test',
    vault_manager_sources,
    dependencies: vault_manager_deps,
    include_directories: test_inc
)

# FIPS mode tests
fips_mode_sources = [
    'test_fips_mode.cc',
] + vault_manager_common_sources

fips_mode_deps = [
    gtest_dep,
    protobuf_dep,
    openssl_dep,
    giomm_dep,
    libcorrect_dep
]

if yubikey_available
    fips_mode_sources += files('../src/core/managers/YubiKeyManager.cc')
    fips_mode_deps += [fido2_dep]
endif

fips_mode_test = executable(
    'fips_mode_test',
    fips_mode_sources,
    dependencies: fips_mode_deps,
    include_directories: test_inc
)

# Memory locking tests (FIPS-140-3 Section 7.9 compliance)
memory_locking_sources = [
    'test_memory_locking.cc',
    '../src/core/VaultManagerV2.cc',
] + vault_manager_common_sources

memory_locking_deps = [
    gtest_dep,
    protobuf_dep,
    openssl_dep,
    giomm_dep,
    libcorrect_dep
]

if yubikey_available
    memory_locking_sources += files('../src/core/managers/YubiKeyManager.cc')
    memory_locking_deps += [fido2_dep]
endif

memory_locking_test = executable(
    'memory_locking_test',
    memory_locking_sources,
    dependencies: memory_locking_deps,
    include_directories: test_inc
)

# Password validation tests
password_validation_test = executable(
    'password_validation_test',
    'test_password_validation.cc',
    dependencies: [
        gtest_dep,
        gtkmm_dep
    ],
    include_directories: test_inc
)

# Security features test (standalone verification)
security_sources = [
    'test_security_features.cc',
    ] + vault_manager_common_sources


security_deps = [protobuf_dep, openssl_dep, giomm_dep, libcorrect_dep]
if yubikey_available
    security_sources += files('../src/core/managers/YubiKeyManager.cc')
    security_deps += [fido2_dep]
endif

security_features_test = executable(
    'test_security_features',
    security_sources,
    dependencies: security_deps,
    include_directories: test_inc
)

# Input validation tests
input_validation_test = executable(
    'input_validation_test',
    'test_input_validation.cc',
    dependencies: [
        gtest_dep
    ],
    include_directories: test_inc
)

# Reed-Solomon tests
reed_solomon_test = executable(
    'reed_solomon_test',
    ['test_reed_solomon.cc', '../src/core/ReedSolomon.cc'],

    dependencies: [
        gtest_dep,
        libcorrect_dep
    ],
    include_directories: test_inc
)

# Vault Reed-Solomon integration tests
vault_rs_sources = [
    'test_vault_reed_solomon.cc',
    ] + vault_manager_common_sources


vault_rs_deps = [gtest_dep, protobuf_dep, openssl_dep, giomm_dep, libcorrect_dep]
if yubikey_available
    vault_rs_sources += files('../src/core/managers/YubiKeyManager.cc')
    vault_rs_deps += [fido2_dep]
endif

vault_reed_solomon_test = executable(
    'vault_reed_solomon_test',
    vault_rs_sources,
    dependencies: vault_rs_deps,
    include_directories: test_inc
)

# FEC preferences tests
fec_pref_sources = [
    'test_fec_preferences.cc',
    ] + vault_manager_common_sources


fec_pref_deps = [gtest_dep, protobuf_dep, openssl_dep, giomm_dep, libcorrect_dep]
if yubikey_available
    fec_pref_sources += files('../src/core/managers/YubiKeyManager.cc')
    fec_pref_deps += [fido2_dep]
endif

fec_preferences_test = executable(
    'fec_preferences_test',
    fec_pref_sources,
    dependencies: fec_pref_deps,
    include_directories: test_inc
)

# UI features tests (password generator, delete functionality)
ui_features_test = executable(
    'ui_features_test',
    'test_ui_features.cc',
    dependencies: [
        gtest_dep
    ],
    include_directories: test_inc
)

# UI security tests (GSettings security configuration)
ui_security_test = executable(
    'ui_security_test',
    'test_ui_security.cc',
    dependencies: [
        gtest_dep,
        giomm_dep
    ],
    include_directories: test_inc
)

# Settings validator tests (runtime validation against schema tampering)
settings_validator_test = executable(
    'settings_validator_test',
    'test_settings_validator.cc',
    dependencies: [
        gtest_dep,
        giomm_dep
    ],
    include_directories: test_inc
)

# Vault helper function tests (unit tests for refactored helpers)
vault_helpers_sources = [
    'test_vault_helpers.cc',
    ] + vault_manager_common_sources


vault_helpers_deps = [gtest_dep, protobuf_dep, openssl_dep, giomm_dep, libcorrect_dep]
if yubikey_available
    vault_helpers_sources += files('../src/core/managers/YubiKeyManager.cc')
    vault_helpers_deps += [fido2_dep]
endif

vault_helpers_test = executable(
    'vault_helpers_test',
    vault_helpers_sources,
    dependencies: vault_helpers_deps,
    include_directories: test_inc
)

# Fuzzy match tests (utility function tests)
fuzzy_match_test = executable(
    'fuzzy_match_test',
    'test_fuzzy_match.cc',
    dependencies: [],  # No external dependencies needed
    include_directories: test_inc
)

# Undo/Redo tests
undo_redo_sources = [
    'test_undo_redo.cc',
    ] + vault_manager_common_sources


undo_redo_deps = [gtest_dep, protobuf_dep, openssl_dep, giomm_dep, libcorrect_dep]
if yubikey_available
    undo_redo_sources += files('../src/core/managers/YubiKeyManager.cc')
    undo_redo_deps += [fido2_dep]
endif

undo_redo_test = executable(
    'undo_redo_test',
    undo_redo_sources,
    dependencies: undo_redo_deps,
    include_directories: test_inc
)

# YubiKey Algorithm tests (FIPS-140-3 compliance framework)
yubikey_algorithms_test = executable(
    'yubikey_algorithms_test',
    'test_yubikey_algorithms.cc',
    dependencies: [gtest_dep],
    include_directories: test_inc
)

# Username Hashing tests (Phase 1: Core Hashing Infrastructure)
username_hashing_sources = [
    'test_username_hashing.cc',
    '../src/core/services/UsernameHashService.cc',
]

username_hashing_deps = [
    gtest_dep,
    openssl_dep
]

username_hashing_test = executable(
    'username_hashing_test',
    username_hashing_sources,
    dependencies: username_hashing_deps,
    include_directories: test_inc
)

# Register all tests
# VaultManager concurrency tests can be flaky under heavy CI load - allow retry
test('VaultManager Tests', vault_manager_test,
     timeout: 60,
     should_fail: false,
     priority: -10,
     env: {'DISABLE_YUBIKEY_DETECT': '1'}  # Disable YubiKey enumeration due to libfido2 thread-safety issues
)
test('Password Validation Tests', password_validation_test, timeout: 30)
test('Input Validation Tests', input_validation_test)
test('Reed-Solomon Tests', reed_solomon_test)
test('Vault Reed-Solomon Integration', vault_reed_solomon_test)
test('FEC Preferences Tests', fec_preferences_test)
test('UI Features Tests', ui_features_test)
test('UI Security Tests', ui_security_test, env: ['GSETTINGS_SCHEMA_DIR=' + meson.project_build_root() / 'data'])
test('Settings Validator Tests', settings_validator_test, env: ['GSETTINGS_SCHEMA_DIR=' + meson.project_build_root() / 'data'])
test('Vault Helper Functions', vault_helpers_test)
test('Fuzzy Match Tests', fuzzy_match_test)
test('Undo/Redo Tests', undo_redo_test)
test('FIPS Mode Tests', fips_mode_test)
test('Memory Locking Security Tests', memory_locking_test)
test('YubiKey Algorithm Tests', yubikey_algorithms_test)
test('Username Hashing Tests', username_hashing_test)

# Secure memory tests
secure_memory_sources = [
    'test_secure_memory.cc',
    ] + vault_manager_common_sources


secure_memory_deps = [gtest_dep, protobuf_dep, openssl_dep, giomm_dep, libcorrect_dep]
if yubikey_available
    secure_memory_sources += files('../src/core/managers/YubiKeyManager.cc')
    secure_memory_deps += [fido2_dep]
endif

secure_memory_test = executable(
    'secure_memory_test',
    secure_memory_sources,
    dependencies: secure_memory_deps,
    include_directories: test_inc
)

test('Secure Memory Tests', secure_memory_test)

# Undo/Redo preferences integration tests
undo_redo_prefs_sources = [
    'test_undo_redo_preferences.cc',
    ] + vault_manager_common_sources


undo_redo_prefs_deps = [gtest_dep, protobuf_dep, openssl_dep, giomm_dep, libcorrect_dep]
if yubikey_available
    undo_redo_prefs_sources += files('../src/core/managers/YubiKeyManager.cc')
    undo_redo_prefs_deps += [fido2_dep]
endif

undo_redo_prefs_test = executable(
    'undo_redo_prefs_test',
    undo_redo_prefs_sources,
    dependencies: undo_redo_prefs_deps,
    include_directories: test_inc
)

test('Undo/Redo Preferences Tests', undo_redo_prefs_test, env: ['GSETTINGS_SCHEMA_DIR=' + meson.project_build_root() / 'data'])

# Account Groups tests
account_groups_sources = [
    'test_account_groups.cc',
    ] + vault_manager_common_sources


account_groups_deps = [gtest_dep, protobuf_dep, openssl_dep, giomm_dep, libcorrect_dep]
if yubikey_available
    account_groups_sources += files('../src/core/managers/YubiKeyManager.cc')
    account_groups_deps += [fido2_dep]
endif

account_groups_test = executable(
    'account_groups_test',
    account_groups_sources,
    dependencies: account_groups_deps,
    include_directories: test_inc
)

test('Account Groups Tests', account_groups_test)

# Group Manager unit tests (direct testing for coverage)
group_manager_sources = [
    'test_group_manager.cc',
    '../src/core/managers/GroupManager.cc',
    proto_gen
]
group_manager_deps = [gtest_dep, protobuf_dep]

group_manager_test = executable(
    'group_manager_test',
    group_manager_sources,
    dependencies: group_manager_deps,
    include_directories: test_inc
)

test('Group Manager Unit Tests', group_manager_test)
# VaultFormatV2 unit tests
vault_format_v2_sources = [
    'test_vault_format_v2.cc',
    '../src/core/VaultFormatV2.cc',
    '../src/core/ReedSolomon.cc',
    '../src/core/MultiUserTypes.cc',
    proto_gen
]
vault_format_v2_deps = [gtest_dep, protobuf_dep, openssl_dep, giomm_dep, libcorrect_dep]

vault_format_v2_test = executable(
    'vault_format_v2_test',
    vault_format_v2_sources,
    dependencies: vault_format_v2_deps,
    include_directories: test_inc
)

test('VaultFormatV2 Unit Tests', vault_format_v2_test)

# KeyWrapping unit tests
key_wrapping_sources = [
    'test_key_wrapping.cc',
    '../src/core/KeyWrapping.cc'
]
key_wrapping_deps = [gtest_dep, openssl_dep, giomm_dep]

key_wrapping_test = executable(
    'key_wrapping_test',
    key_wrapping_sources,
    dependencies: key_wrapping_deps,
    include_directories: test_inc
)

test('KeyWrapping Unit Tests', key_wrapping_test)

# VaultCryptoService unit tests
vault_crypto_service_sources = [
    'test_vault_crypto_service.cc',
    '../src/core/services/VaultCryptoService.cc',
    '../src/core/KeyWrapping.cc',
    '../src/core/crypto/VaultCrypto.cc'
]
vault_crypto_service_deps = [gtest_dep, openssl_dep, giomm_dep]

vault_crypto_service_test = executable(
    'vault_crypto_service_test',
    vault_crypto_service_sources,
    dependencies: vault_crypto_service_deps,
    include_directories: test_inc
)

test('VaultCryptoService Unit Tests', vault_crypto_service_test)

# VaultYubiKeyService unit tests
vault_yubikey_service_sources = [
    'test_vault_yubikey_service.cc',
    '../src/core/services/VaultYubiKeyService.cc',
    '../src/core/crypto/VaultCrypto.cc',
    '../src/core/managers/YubiKeyManager.cc'
]
vault_yubikey_service_deps = [gtest_dep, openssl_dep, giomm_dep, fido2_dep]

vault_yubikey_service_test = executable(
    'vault_yubikey_service_test',
    vault_yubikey_service_sources,
    dependencies: vault_yubikey_service_deps,
    include_directories: test_inc
)

test('VaultYubiKeyService Unit Tests', vault_yubikey_service_test)

# VaultFileService unit tests
vault_file_service_sources = [
    'test_vault_file_service.cc',
    '../src/core/services/VaultFileService.cc',
    '../src/core/format/VaultFormat.cc',
    '../src/core/VaultFormatV2.cc',
    '../src/core/ReedSolomon.cc',
    '../src/core/MultiUserTypes.cc',
    '../src/core/PasswordHistory.cc'
]
vault_file_service_deps = [gtest_dep, protobuf_dep, openssl_dep, giomm_dep, libcorrect_dep]

vault_file_service_test = executable(
    'vault_file_service_test',
    vault_file_service_sources,
    dependencies: vault_file_service_deps,
    include_directories: test_inc
)

test('VaultFileService Unit Tests', vault_file_service_test)

# VaultCreationOrchestrator unit tests (Phase 2 Day 2)
vault_creation_orchestrator_sources = [
    '../src/core/controllers/VaultCreationOrchestrator.cc',
    '../src/core/services/VaultCryptoService.cc',
    '../src/core/services/VaultYubiKeyService.cc',
    '../src/core/services/VaultFileService.cc',
    '../src/core/KeyWrapping.cc',
    '../src/core/crypto/VaultCrypto.cc',
    '../src/core/managers/YubiKeyManager.cc',
    '../src/core/format/VaultFormat.cc',
    '../src/core/VaultFormatV2.cc',
    '../src/core/ReedSolomon.cc',
    '../src/core/MultiUserTypes.cc',
    '../src/core/PasswordHistory.cc',
    proto_gen
]
vault_creation_orchestrator_deps = [gtest_dep, protobuf_dep, openssl_dep, giomm_dep, libcorrect_dep, fido2_dep]

# TODO: Fix corrupted test file
# vault_creation_orchestrator_test = executable(
#     'vault_creation_orchestrator_test',
#     ['test_vault_creation_orchestrator.cc'] + vault_creation_orchestrator_sources,
#     dependencies: vault_creation_orchestrator_deps,
#     include_directories: test_inc
# )
#
# test('VaultCreationOrchestrator Unit Tests', vault_creation_orchestrator_test)

# VaultCreationOrchestrator Integration Tests (Phase 2 Day 3)
vault_orchestrator_integration_test = executable(
    'vault_orchestrator_integration_test',
    ['test_vault_creation_orchestrator_integration.cc'] + vault_creation_orchestrator_sources,
    dependencies: vault_creation_orchestrator_deps,
    include_directories: test_inc
)

test('VaultCreationOrchestrator Integration Tests', vault_orchestrator_integration_test, timeout: 60)

# Multi-User Infrastructure tests
multiuser_sources = [
    'test_multiuser.cc',
    '../src/core/ReedSolomon.cc',
    '../src/core/MultiUserTypes.cc',
    '../src/core/KeyWrapping.cc',
    '../src/core/VaultFormatV2.cc',
]
multiuser_deps = [gtest_dep, openssl_dep, giomm_dep, libcorrect_dep]

multiuser_test = executable(
    'multiuser_test',
    multiuser_sources,
    dependencies: multiuser_deps,
    include_directories: test_inc
)

test('Multi-User Infrastructure Tests', multiuser_test)

# V2 Authentication integration tests (Phase 2)
v2_auth_sources = [
    'test_vault_manager_v2.cc',
    '../src/core/VaultManagerV2.cc',
] + vault_manager_common_sources
v2_auth_deps = [gtest_dep, protobuf_dep, openssl_dep, giomm_dep, libcorrect_dep]
if yubikey_available
    v2_auth_sources += files('../src/core/managers/YubiKeyManager.cc')
    v2_auth_deps += [fido2_dep]
endif

v2_auth_test = executable(
    'v2_auth_test',
    v2_auth_sources,
    dependencies: v2_auth_deps,
    include_directories: test_inc
)

test('V2 Authentication Integration Tests', v2_auth_test, timeout: 60)

# Migration test (manual execution)
migration_test_sources = [
    'test_migration_manual.cc',
    '../src/core/VaultManagerV2.cc',
] + vault_manager_common_sources
migration_test_deps = [protobuf_dep, openssl_dep, giomm_dep, libcorrect_dep]
if yubikey_available
    migration_test_sources += files('../src/core/managers/YubiKeyManager.cc')
    migration_test_deps += [fido2_dep]
endif

migration_test = executable(
    'migration_test',
    migration_test_sources,
    dependencies: migration_test_deps,
    include_directories: test_inc
)

# V2 creation test (manual execution)
v2_creation_test_sources = [
    'test_v2_creation.cc',
    '../src/core/VaultManagerV2.cc',
] + vault_manager_common_sources
v2_creation_test_deps = [protobuf_dep, openssl_dep, giomm_dep, libcorrect_dep]
if yubikey_available
    v2_creation_test_sources += files('../src/core/managers/YubiKeyManager.cc')
    v2_creation_test_deps += [fido2_dep]
endif

v2_creation_test = executable(
    'v2_creation_test',
    v2_creation_test_sources,
    dependencies: v2_creation_test_deps,
    include_directories: test_inc
)

# Password history tests (Phase 9)
password_history_test_sources = [
    'test_password_history.cc',
    '../src/core/VaultManagerV2.cc',
] + vault_manager_common_sources
password_history_test_deps = [
    gtest_dep,
    protobuf_dep,
    openssl_dep,
    giomm_dep,
    libcorrect_dep
]
if yubikey_available
    password_history_test_sources += files('../src/core/managers/YubiKeyManager.cc')
    password_history_test_deps += [fido2_dep]
endif

password_history_test = executable(
    'password_history_test',
    password_history_test_sources,
    dependencies: password_history_test_deps,
    include_directories: test_inc
)

test('password_history', password_history_test)

# VaultCrypto unit tests (AES-256-GCM encryption and PBKDF2)
vault_crypto_test_sources = [
    'test_vault_crypto.cc',
    '../src/core/crypto/VaultCrypto.cc'
]
vault_crypto_test_deps = [gtest_dep, openssl_dep, giomm_dep]

vault_crypto_test = executable(
    'vault_crypto_test',
    vault_crypto_test_sources,
    dependencies: vault_crypto_test_deps,
    include_directories: test_inc
)

test('vault_crypto', vault_crypto_test)

# VaultIO unit tests (File I/O operations)
vault_io_test_sources = [
    'test_vault_io.cc',
    '../src/core/io/VaultIO.cc'
]
vault_io_test_deps = [gtest_dep, giomm_dep]

vault_io_test = executable(
    'vault_io_test',
    vault_io_test_sources,
    dependencies: vault_io_test_deps,
    include_directories: test_inc
)

test('vault_io', vault_io_test)

# VaultFormat unit tests (Format parsing and encoding) - Temporarily disabled
# vault_format_test_sources = [
#     'test_vault_format.cc',
#     '../src/core/format/VaultFormat.cc',
#     '../src/core/ReedSolomon.cc'
# ]
# vault_format_test_deps = [gtest_dep, giomm_dep, libcorrect_dep]

# vault_format_test = executable(
#     'vault_format_test',
#     vault_format_test_sources,
#     dependencies: vault_format_test_deps,
#     include_directories: test_inc
# )

# test('vault_format', vault_format_test)

# VaultSerialization unit tests (Protobuf serialization)
# Fixed protobuf API compatibility (string IDs)
vault_serialization_test_sources = [
    'test_vault_serialization.cc',
    '../src/core/serialization/VaultSerialization.cc',
    proto_gen
]
vault_serialization_test_deps = [gtest_dep, protobuf_dep]

vault_serialization_test = executable(
    'vault_serialization_test',
    vault_serialization_test_sources,
    dependencies: vault_serialization_test_deps,
    include_directories: test_inc
)

test('vault_serialization', vault_serialization_test)

# AccountViewController tests (Phase 1 refactoring)
account_view_controller_test_sources = [
    'test_account_view_controller.cc',
    '../src/ui/controllers/AccountViewController.cc',
    '../src/core/repositories/AccountRepository.cc',
    '../src/core/repositories/GroupRepository.cc',
    '../src/core/VaultManagerV2.cc',
] + vault_manager_common_sources

account_view_controller_test_deps = [
    gtest_dep,
    protobuf_dep,
    openssl_dep,
    giomm_dep,
    libcorrect_dep
]

if yubikey_available
    account_view_controller_test_sources += files('../src/core/managers/YubiKeyManager.cc')
    account_view_controller_test_deps += [fido2_dep]
endif

account_view_controller_test = executable(
    'account_view_controller_test',
    account_view_controller_test_sources,
    dependencies: account_view_controller_test_deps,
    include_directories: test_inc
)

test('account_view_controller', account_view_controller_test)

# SearchController tests (Phase 1 refactoring)
search_controller_test_sources = [
    'test_search_controller.cc',
    '../src/ui/controllers/SearchController.cc',
    proto_gen
]

search_controller_test_deps = [
    gtest_dep,
    protobuf_dep
]

search_controller_test = executable(
    'search_controller_test',
    search_controller_test_sources,
    dependencies: search_controller_test_deps,
    include_directories: test_inc
)

test('search_controller', search_controller_test)

# AutoLockManager tests (Phase 1.3 refactoring)
auto_lock_manager_test_sources = [
    'test_auto_lock_manager.cc',
    '../src/ui/controllers/AutoLockManager.cc'
]

auto_lock_manager_test_deps = [
    gtest_dep,
    giomm_dep
]

auto_lock_manager_test = executable(
    'auto_lock_manager_test',
    auto_lock_manager_test_sources,
    dependencies: auto_lock_manager_test_deps,
    include_directories: test_inc
)

test('auto_lock_manager', auto_lock_manager_test)

# ClipboardManager tests (Phase 1.3 refactoring)
clipboard_manager_test_sources = [
    'test_clipboard_manager.cc',
    '../src/ui/controllers/ClipboardManager.cc'
]

clipboard_manager_test_deps = [
    gtest_dep,
    gtkmm_dep,
    giomm_dep
]

clipboard_manager_test = executable(
    'clipboard_manager_test',
    clipboard_manager_test_sources,
    dependencies: clipboard_manager_test_deps,
    include_directories: test_inc
)

test('clipboard_manager', clipboard_manager_test)

# AccountRepository tests (Phase 2 refactoring)
account_repository_test_sources = [
    'test_account_repository.cc',
    '../src/core/repositories/AccountRepository.cc',
    '../src/core/VaultManagerV2.cc',
] + vault_manager_common_sources

account_repository_test_deps = [
    gtest_dep,
    protobuf_dep,
    openssl_dep,
    giomm_dep,
    libcorrect_dep
]

if yubikey_available
    account_repository_test_sources += files('../src/core/managers/YubiKeyManager.cc')
    account_repository_test_deps += [fido2_dep]
endif

account_repository_test = executable(
    'account_repository_test',
    account_repository_test_sources,
    dependencies: account_repository_test_deps,
    include_directories: test_inc
)

test('account_repository', account_repository_test)

# GroupRepository tests (Phase 2 refactoring)
group_repository_test_sources = [
    'test_group_repository.cc',
    '../src/core/repositories/GroupRepository.cc',
    '../src/core/VaultManagerV2.cc',
] + vault_manager_common_sources

group_repository_test_deps = [
    gtest_dep,
    protobuf_dep,
    openssl_dep,
    giomm_dep,
    libcorrect_dep
]

if yubikey_available
    group_repository_test_sources += files('../src/core/managers/YubiKeyManager.cc')
    group_repository_test_deps += [fido2_dep]
endif

group_repository_test = executable(
    'group_repository_test',
    group_repository_test_sources,
    dependencies: group_repository_test_deps,
    include_directories: test_inc
)

test('group_repository', group_repository_test)

# AccountService tests (Phase 3 refactoring - Service Layer)
account_service_test_sources = [
    'test_account_service.cc',
    '../src/core/services/AccountService.cc',
    '../src/core/repositories/AccountRepository.cc',
    '../src/core/VaultManagerV2.cc',
] + vault_manager_common_sources

account_service_test_deps = [
    gtest_dep,
    protobuf_dep,
    openssl_dep,
    giomm_dep,
    libcorrect_dep
]

if yubikey_available
    account_service_test_sources += files('../src/core/managers/YubiKeyManager.cc')
    account_service_test_deps += [fido2_dep]
endif

account_service_test = executable(
    'account_service_test',
    account_service_test_sources,
    dependencies: account_service_test_deps,
    include_directories: test_inc
)

test('account_service', account_service_test)

# GroupService tests (Phase 3 refactoring - Service Layer)
group_service_test_sources = [
    'test_group_service.cc',
    '../src/core/services/GroupService.cc',
    '../src/core/repositories/GroupRepository.cc',
    '../src/core/VaultManagerV2.cc',
] + vault_manager_common_sources

group_service_test_deps = [
    gtest_dep,
    protobuf_dep,
    openssl_dep,
    giomm_dep,
    libcorrect_dep
]

if yubikey_available
    group_service_test_sources += files('../src/core/managers/YubiKeyManager.cc')
    group_service_test_deps += [fido2_dep]
endif

group_service_test = executable(
    'group_service_test',
    group_service_test_sources,
    dependencies: group_service_test_deps,
    include_directories: test_inc
)

test('group_service', group_service_test)
