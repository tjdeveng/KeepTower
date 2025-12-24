# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2025 tjdeveng

# Test configuration
gtest_dep = dependency('gtest', main: true, required: true)

# Get dependencies from parent
giomm_dep = dependency('giomm-2.68')

# Include directories
test_inc = include_directories(
    '../src',
    '../src/core',
    '../src/ui',
    '../src/ui/windows',
    '../src/ui/dialogs',
    '../build/src',  # For generated protobuf headers
    '../build'       # For config.h
)

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
    '../src/core/ReedSolomon.cc',
    '../src/core/MultiUserTypes.cc',
    '../src/core/KeyWrapping.cc',
    '../src/core/VaultFormatV2.cc',
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
    vault_manager_sources += files('../src/core/YubiKeyManager.cc')
    vault_manager_deps += [ykpers_dep, libyubikey_dep]
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
    fips_mode_sources += files('../src/core/YubiKeyManager.cc')
    fips_mode_deps += [ykpers_dep, libyubikey_dep]
endif

fips_mode_test = executable(
    'fips_mode_test',
    fips_mode_sources,
    dependencies: fips_mode_deps,
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
    security_sources += files('../src/core/YubiKeyManager.cc')
    security_deps += [ykpers_dep, libyubikey_dep]
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
    vault_rs_sources += files('../src/core/YubiKeyManager.cc')
    vault_rs_deps += [ykpers_dep, libyubikey_dep]
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
    fec_pref_sources += files('../src/core/YubiKeyManager.cc')
    fec_pref_deps += [ykpers_dep, libyubikey_dep]
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
    vault_helpers_sources += files('../src/core/YubiKeyManager.cc')
    vault_helpers_deps += [ykpers_dep, libyubikey_dep]
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
    undo_redo_sources += files('../src/core/YubiKeyManager.cc')
    undo_redo_deps += [ykpers_dep, libyubikey_dep]
endif

undo_redo_test = executable(
    'undo_redo_test',
    undo_redo_sources,
    dependencies: undo_redo_deps,
    include_directories: test_inc
)

# Register all tests
test('VaultManager Tests', vault_manager_test)
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

# Secure memory tests
secure_memory_sources = [
    'test_secure_memory.cc',
    ] + vault_manager_common_sources


secure_memory_deps = [gtest_dep, protobuf_dep, openssl_dep, giomm_dep, libcorrect_dep]
if yubikey_available
    secure_memory_sources += files('../src/core/YubiKeyManager.cc')
    secure_memory_deps += [ykpers_dep, libyubikey_dep]
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
    undo_redo_prefs_sources += files('../src/core/YubiKeyManager.cc')
    undo_redo_prefs_deps += [ykpers_dep, libyubikey_dep]
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
    account_groups_sources += files('../src/core/YubiKeyManager.cc')
    account_groups_deps += [ykpers_dep, libyubikey_dep]
endif

account_groups_test = executable(
    'account_groups_test',
    account_groups_sources,
    dependencies: account_groups_deps,
    include_directories: test_inc
)

test('Account Groups Tests', account_groups_test)
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
    v2_auth_sources += files('../src/core/YubiKeyManager.cc')
    v2_auth_deps += [ykpers_dep, libyubikey_dep]
endif

v2_auth_test = executable(
    'v2_auth_test',
    v2_auth_sources,
    dependencies: v2_auth_deps,
    include_directories: test_inc
)

test('V2 Authentication Integration Tests', v2_auth_test)

# Migration test (manual execution)
migration_test_sources = [
    'test_migration_manual.cc',
    '../src/core/VaultManagerV2.cc',
] + vault_manager_common_sources
migration_test_deps = [protobuf_dep, openssl_dep, giomm_dep, libcorrect_dep]
if yubikey_available
    migration_test_sources += files('../src/core/YubiKeyManager.cc')
    migration_test_deps += [ykpers_dep, libyubikey_dep]
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
    v2_creation_test_sources += files('../src/core/YubiKeyManager.cc')
    v2_creation_test_deps += [ykpers_dep, libyubikey_dep]
endif

v2_creation_test = executable(
    'v2_creation_test',
    v2_creation_test_sources,
    dependencies: v2_creation_test_deps,
    include_directories: test_inc
)

