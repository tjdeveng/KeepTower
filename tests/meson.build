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
    '../build/src'  # For generated protobuf headers
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
vault_manager_test = executable(
    'vault_manager_test',
    'test_vault_manager.cc',
    '../src/core/VaultManager.cc',
    proto_gen,
    dependencies: [
        gtest_dep,
        protobuf_dep,
        openssl_dep,
        giomm_dep
    ],
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
security_features_test = executable(
    'test_security_features',
    'test_security_features.cc',
    '../src/core/VaultManager.cc',
    proto_gen,
    dependencies: [
        protobuf_dep,
        openssl_dep,
        giomm_dep
    ],
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

# Register all tests
test('VaultManager Tests', vault_manager_test)
test('Password Validation Tests', password_validation_test, timeout: 30)
test('Input Validation Tests', input_validation_test)