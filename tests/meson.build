# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2025 tjdeveng

# Test configuration
gtest_dep = dependency('gtest', main: true, required: true)

# Get dependencies from parent
giomm_dep = dependency('giomm-2.68')
backup_dep = dependency('keeptower-backup')
storage_dep = dependency('keeptower-storage')
fec_dep = dependency('keeptower-fec')
vaultformat_dep = dependency('keeptower-vaultformat')
crypto_dep = dependency('keeptower-crypto')
fips_dep = dependency('keeptower-fips')
yubikey_dep = dependency('keeptower-yubikey', required: false)

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
proto_gen = custom_target('record_proto_test',
  input: files('../src/record.proto'),
  output: ['record.pb.cc', 'record.pb.h'],
    command: [find_program('protoc', required: true), '--proto_path=' + meson.project_source_root() / 'src', '--cpp_out=@OUTDIR@', '@PLAINNAME@'],
  depend_files: files('../src/record.proto')
)

# Direct source inclusion policy for tests:
# - Prefer extracted Meson dependencies for backup/storage/fec/vaultformat/
#   crypto/fips/yubikey code.
# - Keep direct .cc inclusion only for app-layer code that has no dedicated
#   library target yet, or when a test intentionally white-boxes implementation
#   details of a controller/service/widget.
# - Do not re-add extracted library implementation files to tests for
#   convenience; add the correct dependency instead.

# Common app-layer sources for VaultManager-oriented tests.
# These remain direct because they model application integration behavior rather
# than isolated library linkage.
vault_manager_common_app_sources = [
    '../src/core/VaultManager.cc',
    '../src/core/VaultManagerV2.cc',
    '../src/core/MultiUserTypes.cc',
    '../src/core/MultiUserTypesSerDe.cc',
    '../src/core/PasswordHistory.cc',
    '../src/core/managers/AccountManager.cc',
    '../src/core/managers/GroupManager.cc',
    '../src/core/controllers/VaultCreationOrchestrator.cc',
    '../src/core/services/KeySlotManager.cc',
    '../src/core/services/VaultYubiKeyService.cc',
    '../src/core/services/VaultFileService.cc',
    '../src/core/services/VaultDataService.cc',
    '../src/core/services/V2AuthService.cc',
    proto_gen
]

# Common dependencies for tests using vault_manager_common_app_sources.
# These tests exercise app-layer code that sits on top of the extracted
# storage/format/crypto/fips/backup libraries.
vault_manager_arch_deps = [
    backup_dep,
    storage_dep,
    fec_dep,
    vaultformat_dep,
    crypto_dep,
    fips_dep,
]

vault_manager_common_deps = [
    gtest_dep,
    protobuf_dep,
    openssl_dep,
    giomm_dep,
    libcorrect_dep,
    argon2_dep,
]

vault_manager_common_deps += vault_manager_arch_deps

vault_manager_common_test_deps = vault_manager_common_deps
if yubikey_available
    vault_manager_common_test_deps += [yubikey_dep]
endif

# VaultManager tests
vault_manager_sources = [
    'test_vault_manager.cc',
 ] + vault_manager_common_app_sources

vault_manager_deps = vault_manager_common_test_deps

vault_manager_test = executable(
    'vault_manager_test',
    vault_manager_sources,
    dependencies: vault_manager_deps,
    include_directories: test_inc
)

# FIPS mode tests
fips_mode_sources = [
    'test_fips_mode.cc',
] + vault_manager_common_app_sources

fips_mode_deps = vault_manager_common_test_deps

fips_mode_test = executable(
    'fips_mode_test',
    fips_mode_sources,
    dependencies: fips_mode_deps,
    include_directories: test_inc
)

# Memory locking tests (FIPS-140-3 Section 7.9 compliance)
memory_locking_sources = [
    'test_memory_locking.cc',
] + vault_manager_common_app_sources

memory_locking_deps = vault_manager_common_test_deps

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
    ] + vault_manager_common_app_sources

security_deps = vault_manager_common_test_deps

security_features_test = executable(
    'test_security_features',
    security_sources,
    dependencies: security_deps,
    include_directories: test_inc
)

test('Security Features Tests', security_features_test, timeout: 60)

# Input validation tests
input_validation_test = executable(
    'input_validation_test',
    'test_input_validation.cc',
    dependencies: [
        gtest_dep
    ],
    include_directories: test_inc
)

# VaultRuntimePreferences unit tests
vault_runtime_preferences_test = executable(
    'vault_runtime_preferences_test',
    'test_vault_runtime_preferences.cc',
    dependencies: [
        gtest_dep,
    ],
    include_directories: test_inc
)

# Vault boundary model tests (pure public model/view structs with no app-layer
# implementation linkage)
vault_boundary_models_test = executable(
    'vault_boundary_models_test',
    'test_vault_boundary_models.cc',
    dependencies: [
        gtest_dep,
    ],
    include_directories: test_inc
)

# Vault boundary accessor tests. These exercise the public VaultManager view
# accessors against a closed vault, so they still link the common app-layer
# implementation bundle.
vault_boundary_types_test = executable(
    'vault_boundary_types_test',
    ['test_vault_boundary_types.cc'] + vault_manager_common_app_sources,
    dependencies: vault_manager_common_test_deps,
    include_directories: test_inc
)

# Reed-Solomon tests
reed_solomon_test = executable(
    'reed_solomon_test',
    ['test_reed_solomon.cc'],

    dependencies: [
        gtest_dep,
        fec_dep
    ],
    include_directories: test_inc
)

# Vault Reed-Solomon integration tests
vault_rs_sources = [
    'test_vault_reed_solomon.cc',
    ] + vault_manager_common_app_sources

vault_rs_deps = vault_manager_common_test_deps

vault_reed_solomon_test = executable(
    'vault_reed_solomon_test',
    vault_rs_sources,
    dependencies: vault_rs_deps,
    include_directories: test_inc
)

# FEC preferences tests
fec_pref_sources = [
    'test_fec_preferences.cc',
    ] + vault_manager_common_app_sources

fec_pref_deps = vault_manager_common_test_deps

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

# File dialog extension normalization tests (regression coverage for export save dialog)
file_dialog_extension_test = executable(
    'file_dialog_extension_test',
    'test_file_dialog_extension.cc',
    dependencies: [
        gtest_dep,
    ],
    include_directories: test_inc
)

# White-box controller tests: compile controller implementation directly because
# there is no dedicated controller library target and the tests intentionally
# exercise implementation-level flow behavior.

# VaultIO flow controller tests (SRP/DIP refactor validation)
vault_io_flow_controllers_sources = [
    'test_vault_io_flow_controllers.cc',
    '../src/ui/controllers/flows/ExportFlowController.cc',
    '../src/ui/controllers/flows/ImportFlowController.cc',
]

vault_io_flow_controllers_test = executable(
    'vault_io_flow_controllers_test',
    vault_io_flow_controllers_sources,
    dependencies: [
        gtest_dep,
    ],
    include_directories: test_inc
)

# White-box utility tests for non-library helpers.

# PasswordGenerator utility tests (temporary password generation)
password_generator_test = executable(
    'password_generator_test',
    ['test_password_generator.cc', '../src/utils/PasswordGenerator.cc'],
    dependencies: [
        gtest_dep,
        openssl_dep
    ],
    include_directories: test_inc
)

# Import/Export shared helper tests
import_export_common_test = executable(
    'import_export_common_test',
    ['test_import_export_common.cc', '../src/utils/ImportExport.cc', proto_gen],
    dependencies: [
        gtest_dep,
        protobuf_dep,
    ],
    include_directories: test_inc
)

test('ImportExport Common Tests', import_export_common_test)

# Import/Export CSV edge-case tests
import_export_csv_sources = [
    'test_import_export_csv.cc',
    '../src/utils/ImportExport.cc',
    '../src/utils/import_export/ImportExportDetail.cc',
    '../src/utils/import_export/ImportExportCsv.cc',
    proto_gen,
]

import_export_csv_test = executable(
    'import_export_csv_test',
    import_export_csv_sources,
    dependencies: [
        gtest_dep,
        protobuf_dep,
    ],
    include_directories: test_inc
)

test('ImportExport CSV Tests', import_export_csv_test)

# Import/Export KeePass XML tests
import_export_keepass_xml_sources = [
    'test_import_export_keepass_xml.cc',
    '../src/utils/ImportExport.cc',
    '../src/utils/import_export/ImportExportDetail.cc',
    '../src/utils/import_export/ImportExportKeePassXml.cc',
    proto_gen,
]

import_export_keepass_xml_test = executable(
    'import_export_keepass_xml_test',
    import_export_keepass_xml_sources,
    dependencies: [
        gtest_dep,
        protobuf_dep,
    ],
    include_directories: test_inc
)

test('ImportExport KeePass XML Tests', import_export_keepass_xml_test)

# Import/Export 1Password 1PIF tests
import_export_1pif_sources = [
    'test_import_export_1pif.cc',
    '../src/utils/ImportExport.cc',
    '../src/utils/import_export/ImportExportDetail.cc',
    '../src/utils/import_export/ImportExport1Password1pif.cc',
    proto_gen,
]

import_export_1pif_test = executable(
    'import_export_1pif_test',
    import_export_1pif_sources,
    dependencies: [
        gtest_dep,
        protobuf_dep,
    ],
    include_directories: test_inc
)

test('ImportExport 1PIF Tests', import_export_1pif_test)

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

# White-box UI/service-adjacent tests. These intentionally compile the
# implementation under test directly because the app-layer presenters and
# helpers do not have standalone library targets.

# Preferences presenter tests (vault-open backup persistence semantics)
preferences_presenter_sources = [
    'test_preferences_presenter.cc',
    '../src/ui/dialogs/preferences/PreferencesPresenter.cc',
    ] + vault_manager_common_app_sources


preferences_presenter_deps = vault_manager_common_test_deps

preferences_presenter_test = executable(
    'preferences_presenter_test',
    preferences_presenter_sources,
    dependencies: preferences_presenter_deps,
    include_directories: test_inc
)

# Vault helper function tests (unit tests for refactored helpers)
vault_helpers_sources = [
    'test_vault_helpers.cc',
    ] + vault_manager_common_app_sources

vault_helpers_deps = vault_manager_common_test_deps

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
    ] + vault_manager_common_app_sources

undo_redo_deps = vault_manager_common_test_deps

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

# YubiKeyManager characterization tests (Issue #7 refactor safety net)
yubikey_manager_characterization_sources = [
    'test_yubikey_manager_characterization.cc',
]

yubikey_manager_characterization_deps = [gtest_dep, openssl_dep, giomm_dep]
if yubikey_available
    yubikey_manager_characterization_deps += [yubikey_dep]
endif

yubikey_manager_characterization_test = executable(
    'yubikey_manager_characterization_test',
    yubikey_manager_characterization_sources,
    dependencies: yubikey_manager_characterization_deps,
    include_directories: test_inc
)

# Username Hashing tests (Phase 1: Core Hashing Infrastructure)
username_hashing_sources = [
    'test_username_hashing.cc',
]

username_hashing_deps = [
    gtest_dep,
    openssl_dep,
    crypto_dep
]

# Add argon2 if available
if argon2_available
    username_hashing_deps += [argon2_dep]
endif

username_hashing_test = executable(
    'username_hashing_test',
    username_hashing_sources,
    dependencies: username_hashing_deps,
    include_directories: test_inc
)

# Username Hash Migration tests (Phase 1: Two-phase authentication and migration)
username_hash_migration_sources = [
    'test_username_hash_migration.cc',
    vault_manager_common_app_sources,
    proto_gen,
]

username_hash_migration_deps = vault_manager_common_test_deps

username_hash_migration_test = executable(
    'username_hash_migration_test',
    username_hash_migration_sources,
    dependencies: username_hash_migration_deps,
    include_directories: test_inc
)

# Username Hash Migration Priority 2 tests (advanced scenarios)
username_hash_migration_priority2_sources = [
    'test_username_hash_migration_priority2.cc',
    ] + vault_manager_common_app_sources

username_hash_migration_priority2_deps = vault_manager_common_test_deps

username_hash_migration_priority2_test = executable(
    'username_hash_migration_priority2_test',
    username_hash_migration_priority2_sources,
    dependencies: username_hash_migration_priority2_deps,
    include_directories: test_inc
)

# Username Hash Migration Priority 3 tests (performance, security, edge cases)
username_hash_migration_priority3_sources = [
    'test_username_hash_migration_priority3.cc',
    ] + vault_manager_common_app_sources

username_hash_migration_priority3_deps = vault_manager_common_test_deps

username_hash_migration_priority3_test = executable(
    'username_hash_migration_priority3_test',
    username_hash_migration_priority3_sources,
    dependencies: username_hash_migration_priority3_deps,
    include_directories: test_inc
)

# Username Hash Migration Concurrency tests (Phase 3)
username_hash_migration_concurrency_sources = [
    'test_username_hash_migration_concurrency.cc',
    ] + vault_manager_common_app_sources

username_hash_migration_concurrency_deps = vault_manager_common_test_deps

username_hash_migration_concurrency_test = executable(
    'username_hash_migration_concurrency_test',
    username_hash_migration_concurrency_sources,
    dependencies: username_hash_migration_concurrency_deps,
    include_directories: test_inc
)

# KEK Derivation tests (Phase 1: Core KEK Derivation Service)
kek_derivation_sources = [
    'test_kek_derivation.cc',
    '../src/core/services/KekDerivationSettingsAdapter.cc',
]

kek_derivation_deps = [
    gtest_dep,
    gtkmm_dep,
    openssl_dep,
    crypto_dep
]

# Add argon2 if available
if argon2_available
    kek_derivation_deps += [argon2_dep]
endif

kek_derivation_test = executable(
    'kek_derivation_test',
    kek_derivation_sources,
    dependencies: kek_derivation_deps,
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
test('VaultRuntimePreferences Unit Tests', vault_runtime_preferences_test)
test('Vault Boundary Model Tests', vault_boundary_models_test)
test('Vault Boundary Types Tests', vault_boundary_types_test)
test('Reed-Solomon Tests', reed_solomon_test)
test('Vault Reed-Solomon Integration', vault_reed_solomon_test)
test('FEC Preferences Tests', fec_preferences_test)
test('UI Features Tests', ui_features_test)
test('File Dialog Extension Tests', file_dialog_extension_test)
test('VaultIO Flow Controller Tests', vault_io_flow_controllers_test)
test('Password Generator Tests', password_generator_test)
test('UI Security Tests', ui_security_test, env: ['GSETTINGS_SCHEMA_DIR=' + meson.project_build_root() / 'data'])
test('Settings Validator Tests', settings_validator_test, env: ['GSETTINGS_SCHEMA_DIR=' + meson.project_build_root() / 'data'])
test('Preferences Presenter Tests', preferences_presenter_test,
    env: {
        'GSETTINGS_SCHEMA_DIR': meson.project_build_root() / 'data',
        'DISABLE_YUBIKEY_DETECT': '1',
    }
)
test('Vault Helper Functions', vault_helpers_test)
test('Fuzzy Match Tests', fuzzy_match_test)
test('Undo/Redo Tests', undo_redo_test)

test('FIPS Mode Tests', fips_mode_test)
test('Memory Locking Security Tests', memory_locking_test)
test('YubiKey Algorithm Tests', yubikey_algorithms_test)
test('YubiKeyManager Characterization Tests', yubikey_manager_characterization_test,
    env: {'DISABLE_YUBIKEY_DETECT': '1'}
)
test('Username Hashing Tests', username_hashing_test)
test('Username Hash Migration Tests', username_hash_migration_test, suite: 'migration')
test('Username Hash Migration Priority 2 Tests', username_hash_migration_priority2_test, suite: 'migration')
test('Username Hash Migration Priority 3 Tests', username_hash_migration_priority3_test, timeout: 600, suite: 'migration', is_parallel: false)
test('Username Hash Migration Concurrency Tests', username_hash_migration_concurrency_test, timeout: 600, suite: 'migration', is_parallel: false)
test('KEK Derivation Tests', kek_derivation_test, env: ['GSETTINGS_SCHEMA_DIR=' + meson.project_build_root() / 'data'])

# Secure memory tests
secure_memory_sources = [
    'test_secure_memory.cc',
    ] + vault_manager_common_app_sources

secure_memory_deps = vault_manager_common_test_deps

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
    ] + vault_manager_common_app_sources

undo_redo_prefs_deps = vault_manager_common_test_deps

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
    ] + vault_manager_common_app_sources

account_groups_deps = vault_manager_common_test_deps

account_groups_test = executable(
    'account_groups_test',
    account_groups_sources,
    dependencies: account_groups_deps,
    include_directories: test_inc
)

test('Account Groups Tests', account_groups_test)

# Direct manager/service/unit tests below are intentional white-box targets.
# They validate app-layer implementations that still live outside extracted
# libraries, so keeping their .cc files direct is expected in Phase L.

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
    '../src/core/MultiUserTypes.cc',
    '../src/core/MultiUserTypesSerDe.cc',
    proto_gen
]
vault_format_v2_deps = [gtest_dep, protobuf_dep, openssl_dep, giomm_dep, libcorrect_dep, argon2_dep, vaultformat_dep]

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
]
key_wrapping_deps = [gtest_dep, openssl_dep, giomm_dep, crypto_dep]

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
]
vault_crypto_service_deps = [gtest_dep, openssl_dep, giomm_dep, crypto_dep]

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
]
vault_yubikey_service_deps = [gtest_dep, openssl_dep, giomm_dep, yubikey_dep, crypto_dep]

vault_yubikey_service_test = executable(
    'vault_yubikey_service_test',
    vault_yubikey_service_sources,
    dependencies: vault_yubikey_service_deps,
    include_directories: test_inc
)

test('VaultYubiKeyService Unit Tests', vault_yubikey_service_test)

# V2AuthService unit tests
v2_auth_service_sources = [
    'test_v2_auth_service.cc',
    '../src/core/services/V2AuthService.cc',
    '../src/core/services/KeySlotManager.cc',
    '../src/core/MultiUserTypes.cc',
    '../src/core/PasswordHistory.cc',
]
v2_auth_service_deps = [gtest_dep, openssl_dep, giomm_dep, argon2_dep, yubikey_dep, crypto_dep]

v2_auth_service_test = executable(
    'v2_auth_service_test',
    v2_auth_service_sources,
    dependencies: v2_auth_service_deps,
    include_directories: test_inc
)

test('V2AuthService Unit Tests', v2_auth_service_test)

# KeySlotManager unit tests
key_slot_manager_sources = [
    'test_key_slot_manager.cc',
    '../src/core/services/KeySlotManager.cc',
    '../src/core/MultiUserTypes.cc',
    '../src/core/PasswordHistory.cc',
]
key_slot_manager_deps = [gtest_dep, openssl_dep, giomm_dep, argon2_dep, crypto_dep]

key_slot_manager_test = executable(
    'key_slot_manager_test',
    key_slot_manager_sources,
    dependencies: key_slot_manager_deps,
    include_directories: test_inc
)

test('KeySlotManager Unit Tests', key_slot_manager_test)

# VaultFileService unit tests
vault_file_service_sources = [
    'test_vault_file_service.cc',
    '../src/core/services/VaultFileService.cc',
    '../src/core/MultiUserTypes.cc',
    '../src/core/MultiUserTypesSerDe.cc',
    '../src/core/PasswordHistory.cc'
]
vault_file_service_deps = [gtest_dep, protobuf_dep, openssl_dep, giomm_dep, libcorrect_dep, argon2_dep, storage_dep, vaultformat_dep]

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
    '../src/core/services/VaultYubiKeyService.cc',
    '../src/core/services/VaultFileService.cc',
    '../src/core/services/VaultDataService.cc',
    '../src/core/MultiUserTypes.cc',
    '../src/core/MultiUserTypesSerDe.cc',
    '../src/core/PasswordHistory.cc',
    proto_gen
]
vault_creation_orchestrator_deps = [gtest_dep, protobuf_dep, openssl_dep, giomm_dep, yubikey_dep, argon2_dep, storage_dep, vaultformat_dep, fec_dep, crypto_dep]

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
    '../src/core/MultiUserTypes.cc',
    '../src/core/MultiUserTypesSerDe.cc',
]
multiuser_deps = [gtest_dep, openssl_dep, giomm_dep, argon2_dep, vaultformat_dep, crypto_dep]

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
] + vault_manager_common_app_sources
v2_auth_deps = vault_manager_common_test_deps

v2_auth_test = executable(
    'v2_auth_test',
    v2_auth_sources,
    dependencies: v2_auth_deps,
    include_directories: test_inc
)

test('V2 Authentication Integration Tests', v2_auth_test, timeout: 600)

# Password history tests (Phase 9)
password_history_test_sources = [
    'test_password_history.cc',
] + vault_manager_common_app_sources
password_history_test_deps = vault_manager_common_test_deps

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
]
vault_crypto_test_deps = [gtest_dep, openssl_dep, giomm_dep, crypto_dep]

vault_crypto_test = executable(
    'vault_crypto_test',
    vault_crypto_test_sources,
    dependencies: vault_crypto_test_deps,
    include_directories: test_inc
)

test('vault_crypto', vault_crypto_test)

# VaultIO unit tests (File I/O operations)
vault_io_test_sources = [
    'test_vault_io.cc'
]
vault_io_test_deps = [gtest_dep, giomm_dep, storage_dep]

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
#     '../src/lib/fec/ReedSolomon.cc'
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
    proto_gen
]
vault_serialization_test_deps = [gtest_dep, protobuf_dep, vaultformat_dep]

vault_serialization_test = executable(
    'vault_serialization_test',
    vault_serialization_test_sources,
    dependencies: vault_serialization_test_deps,
    include_directories: test_inc
)

test('vault_serialization', vault_serialization_test)

vault_data_service_test_sources = [
    'test_vault_data_service.cc',
    proto_gen,
    '../src/core/services/VaultDataService.cc',
]

vault_data_service_test_deps = [gtest_dep, protobuf_dep, vaultformat_dep]

vault_data_service_test = executable(
    'vault_data_service_test',
    vault_data_service_test_sources,
    dependencies: vault_data_service_test_deps,
    include_directories: test_inc
)

test('vault_data_service', vault_data_service_test)

# Controller/widget/repository/service tests below intentionally white-box the
# concrete implementation under test. These are not candidates for dependency-
# only linkage unless those app-layer components are extracted into dedicated
# libraries in a later phase.

# AccountViewController tests (Phase 1 refactoring)
account_view_controller_test_sources = [
    'test_account_view_controller.cc',
    '../src/ui/controllers/AccountViewController.cc',
    '../src/core/repositories/AccountRepository.cc',
    '../src/core/repositories/GroupRepository.cc',
 ] + vault_manager_common_app_sources

account_view_controller_test_deps = vault_manager_common_test_deps

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

# AccountTreeWidget tests (selection/rebuild regression coverage)
account_tree_widget_test_sources = [
    'test_account_tree_widget.cc',
    '../src/ui/widgets/AccountTreeWidget.cc',
    '../src/ui/widgets/AccountRowWidget.cc',
    '../src/ui/widgets/GroupRowWidget.cc',
    proto_gen
]

account_tree_widget_test_deps = [
    gtest_dep,
    gtkmm_dep,
    protobuf_dep
]

account_tree_widget_test = executable(
    'account_tree_widget_test',
    account_tree_widget_test_sources,
    dependencies: account_tree_widget_test_deps,
    include_directories: test_inc
)

test('account_tree_widget', account_tree_widget_test)

# AccountDetailWidget tests (dirty-state regression coverage)
account_detail_widget_test_sources = [
    'test_account_detail_widget.cc',
    '../src/ui/widgets/AccountDetailWidget.cc'
]

account_detail_widget_test_deps = [
    gtest_dep,
    gtkmm_dep
]

account_detail_widget_test = executable(
    'account_detail_widget_test',
    account_detail_widget_test_sources,
    dependencies: account_detail_widget_test_deps,
    include_directories: test_inc
)

test('account_detail_widget', account_detail_widget_test)

# ThemeController tests (Issue #5 refactor)
theme_controller_test_sources = [
    'test_theme_controller.cc',
    '../src/ui/controllers/ThemeController.cc'
]

theme_controller_test_deps = [
    gtest_dep,
    gtkmm_dep,
    giomm_dep
]

theme_controller_test = executable(
    'theme_controller_test',
    theme_controller_test_sources,
    dependencies: theme_controller_test_deps,
    include_directories: test_inc
)

test('theme_controller', theme_controller_test, env: ['GSETTINGS_SCHEMA_DIR=' + meson.project_build_root() / 'data'])

# AccountManager unit tests
account_manager_test_sources = [
    'test_account_manager.cc',
    '../src/core/managers/AccountManager.cc',
    proto_gen,
]

account_manager_test_deps = [
    gtest_dep,
    protobuf_dep,
]

account_manager_test = executable(
    'account_manager_test',
    account_manager_test_sources,
    dependencies: account_manager_test_deps,
    include_directories: test_inc
)

test('AccountManager Unit Tests', account_manager_test)

# AccountRepository tests (Phase 2 refactoring)
account_repository_test_sources = [
    'test_account_repository.cc',
    '../src/core/repositories/AccountRepository.cc',
 ] + vault_manager_common_app_sources

account_repository_test_deps = vault_manager_common_test_deps

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
 ] + vault_manager_common_app_sources

group_repository_test_deps = vault_manager_common_test_deps

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
 ] + vault_manager_common_app_sources

account_service_test_deps = vault_manager_common_test_deps

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
 ] + vault_manager_common_app_sources

group_service_test_deps = vault_manager_common_test_deps

group_service_test = executable(
    'group_service_test',
    group_service_test_sources,
    dependencies: group_service_test_deps,
    include_directories: test_inc
)

test('group_service', group_service_test)
