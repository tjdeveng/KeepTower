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

# VaultManager tests
vault_manager_sources = [
    'test_vault_manager.cc',
    '../src/core/VaultManager.cc',
    '../src/core/ReedSolomon.cc',
    proto_gen
]

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
    '../src/core/VaultManager.cc',
    '../src/core/ReedSolomon.cc',
    proto_gen
]
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
    'test_reed_solomon.cc',
    '../src/core/ReedSolomon.cc',
    dependencies: [
        gtest_dep,
        libcorrect_dep
    ],
    include_directories: test_inc
)

# Vault Reed-Solomon integration tests
vault_rs_sources = [
    'test_vault_reed_solomon.cc',
    '../src/core/VaultManager.cc',
    '../src/core/ReedSolomon.cc',
    proto_gen
]
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
    '../src/core/VaultManager.cc',
    '../src/core/ReedSolomon.cc',
    proto_gen
]
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
    '../src/core/VaultManager.cc',
    '../src/core/ReedSolomon.cc',
    proto_gen
]
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