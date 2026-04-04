# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2025 tjdeveng

# Import filesystem module
fs = import('fs')

# Import gnome module for resources
gnome = import('gnome')

# Compile GResources
gresources = gnome.compile_resources(
  'keeptower-resources',
  '../resources/gresource.xml',
  source_dir: '../resources',
  c_name: 'keeptower',
)

# Compile help resources (if pandoc is available)
# First check if pandoc exists
pandoc = find_program('pandoc', required: false)
if pandoc.found()
  # Generate help HTML files in build directory during build
  # Note: generate-help.sh automatically syncs to resources/help/ to ensure
  # development runs always see latest help (HelpManager checks there first)
  help_output_dir = meson.current_build_dir() / 'help-generated'
  generate_help = custom_target(
    'generate-help-src',
    output: 'help-src-generated.stamp',
    command: [files('../scripts/generate-help.sh'), help_output_dir],
    build_by_default: true,
    build_always_stale: true,
  )

  # Create a temporary gresource XML that points to build directory
  help_gresource_xml = configure_file(
    input: '../resources/help.gresource.xml',
    output: 'help.gresource.xml',
    copy: true,
  )

  # Compile help GResources from build directory with dependency on generation
  help_gresources = gnome.compile_resources(
    'keeptower-help-resources',
    help_gresource_xml,
    source_dir: [help_output_dir, meson.project_source_root() / 'resources'],
    c_name: 'keeptower_help',
    dependencies: [generate_help],
  )
else
  warning('pandoc not found - help documentation will not be embedded')
  help_gresources = []
endif

# Generate protocol buffer sources
protoc = find_program('protoc', required: true)

# Proto files
proto_files = files(
  'record.proto',
)

# Generate C++ sources from proto files
proto_sources = []
proto_headers = []

foreach proto : proto_files
  proto_base = fs.stem(proto)

  proto_gen = custom_target(proto_base + '_proto',
    input: proto,
    output: [proto_base + '.pb.cc', proto_base + '.pb.h'],
    command: [protoc, '--proto_path=@SOURCE_ROOT@/src', '--cpp_out=@BUILD_ROOT@/src', '@INPUT@'],
  )

  proto_sources += proto_gen[0]
  proto_headers += proto_gen[1]
endforeach

# Phase D.1: Extract backup policy into dedicated library target.
backup_library_sources = files(
  'lib/backup/VaultBackupPolicy.cc',
)

backup_library = static_library(
  'keeptower-backup',
  backup_library_sources,
  dependencies: [protobuf_dep],
  include_directories: [root_inc, include_directories('.'), include_directories('core')],
)

# Phase H: Extract VaultIO into dedicated storage library target.
storage_library_sources = files(
  'lib/storage/VaultIO.cc',
)

storage_library = static_library(
  'keeptower-storage',
  storage_library_sources,
  include_directories: [root_inc, include_directories('.'), include_directories('core')],
)

storage_dep = declare_dependency(
  link_with: storage_library,
  include_directories: [root_inc, include_directories('core')],
)

meson.override_dependency('keeptower-storage', storage_dep)

backup_dep = declare_dependency(
  link_with: backup_library,
  dependencies: [storage_dep],
  include_directories: [root_inc],
)

meson.override_dependency('keeptower-backup', backup_dep)

# Phase D.2: Extract FEC/Reed-Solomon into dedicated library target.
fec_library_sources = files(
  'lib/fec/ReedSolomon.cc',
)

fec_library = static_library(
  'keeptower-fec',
  fec_library_sources,
  dependencies: [libcorrect_dep],
  include_directories: [root_inc, include_directories('.'), include_directories('core')],
)

fec_dep = declare_dependency(
  link_with: fec_library,
  include_directories: [root_inc, include_directories('core')],
)

meson.override_dependency('keeptower-fec', fec_dep)

# Phase I: Extract vault format and serialization into dedicated library target.
vaultformat_library_sources = files(
  'lib/vaultformat/VaultFormatV2.cc',
  'lib/vaultformat/VaultSerialization.cc',
)

vaultformat_library = static_library(
  'keeptower-vaultformat',
  vaultformat_library_sources,
  dependencies: [protobuf_dep, libcorrect_dep],
  include_directories: [root_inc, include_directories('.'), include_directories('core')],
)

vaultformat_dep = declare_dependency(
  link_with: vaultformat_library,
  dependencies: [protobuf_dep, fec_dep],
  include_directories: [root_inc, include_directories('core')],
)

meson.override_dependency('keeptower-vaultformat', vaultformat_dep)

# Phase D.4: Extract FIPS provider management into dedicated library target.
fips_library_sources = files(
  'lib/fips/FipsProviderManager.cc',
)

fips_library = static_library(
  'keeptower-fips',
  fips_library_sources,
  dependencies: [openssl_dep],
  include_directories: [root_inc, include_directories('.'), include_directories('core')],
)

fips_dep = declare_dependency(
  link_with: fips_library,
  include_directories: [root_inc, include_directories('core')],
)

meson.override_dependency('keeptower-fips', fips_dep)

# Phase D.3: Extract crypto primitives/services into dedicated library target.
crypto_library_sources = files(
  'lib/crypto/KeyWrapping.cc',
  'lib/crypto/VaultCrypto.cc',
  'lib/crypto/VaultCryptoService.cc',
)

crypto_library = static_library(
  'keeptower-crypto',
  crypto_library_sources,
  dependencies: [openssl_dep, gtkmm_dep],
  include_directories: [root_inc, include_directories('.'), include_directories('core')],
)

crypto_dep = declare_dependency(
  link_with: crypto_library,
  dependencies: [fips_dep],
  include_directories: [root_inc, include_directories('core')],
)

meson.override_dependency('keeptower-crypto', crypto_dep)

# Phase D.5: Extract YubiKey hardware manager into dedicated library target.
if yubikey_available
  yubikey_library_sources = files(
    'lib/yubikey/YubiKeyManager.cc',
  )

  yubikey_library = static_library(
    'keeptower-yubikey',
    yubikey_library_sources,
    dependencies: [fido2_dep, openssl_dep, gtkmm_dep],
    include_directories: [root_inc, include_directories('.')],
  )

  yubikey_dep = declare_dependency(
    link_with: yubikey_library,
    dependencies: [fido2_dep, openssl_dep, gtkmm_dep],
    include_directories: [root_inc, include_directories('.')],
  )

  meson.override_dependency('keeptower-yubikey', yubikey_dep)
endif

# Application sources
sources = files(
  'main.cc',
  'application/Application.cc',
  'ui/windows/MainWindow.cc',
  'ui/dialogs/PasswordDialog.cc',
  'ui/dialogs/CreatePasswordDialog.cc',
  'ui/dialogs/PreferencesDialog.cc',
  'ui/dialogs/preferences/PreferencesPresenter.cc',
  'ui/dialogs/preferences/AppearancePreferencesPage.cc',
  'ui/dialogs/preferences/AccountSecurityPreferencesPage.cc',
  'ui/dialogs/preferences/StoragePreferencesPage.cc',
  'ui/dialogs/preferences/VaultSecurityPreferencesPage.cc',
  'ui/dialogs/GroupCreateDialog.cc',
  'ui/dialogs/GroupRenameDialog.cc',
  'ui/dialogs/V2UserLoginDialog.cc',
  'ui/dialogs/ChangePasswordDialog.cc',
  'ui/dialogs/UserManagementDialog.cc',
  'ui/widgets/AccountTreeWidget.cc',
  'ui/widgets/AccountRowWidget.cc',
  'ui/widgets/GroupRowWidget.cc',
  'ui/widgets/AccountDetailWidget.cc',
  'ui/controllers/AccountViewController.cc',
  'ui/controllers/SearchController.cc',
  'ui/controllers/ThemeController.cc',
  'ui/controllers/AutoLockManager.cc',
  'ui/controllers/ClipboardManager.cc',
  'ui/controllers/flows/ExportFlowController.cc',
  'ui/controllers/flows/ImportFlowController.cc',
  'ui/coordinators/VaultUiCoordinator.cc',
  'ui/managers/DialogManager.cc',
  'ui/managers/MenuManager.cc',
  'ui/managers/VaultUiStateApplier.cc',
  'ui/managers/V2AuthenticationHandler.cc',
  'ui/managers/VaultIOHandler.cc',
  'ui/managers/YubiKeyHandler.cc',
  'ui/managers/GroupHandler.cc',
  'ui/managers/AccountEditHandler.cc',
  'ui/managers/AutoLockHandler.cc',
  'ui/managers/UserAccountHandler.cc',
  'ui/managers/VaultOpenHandler.cc',
  'core/VaultManager.cc',
  'core/VaultManagerV2.cc',
  'core/repositories/AccountRepository.cc',
  'core/repositories/GroupRepository.cc',
  'core/services/AccountService.cc',
  'core/services/GroupService.cc',
  'core/services/KeySlotManager.cc',
  'core/services/VaultYubiKeyService.cc',
  'core/services/VaultFileService.cc',
  'core/services/VaultDataService.cc',
  'core/services/V2AuthService.cc',
  'core/services/UsernameHashService.cc',
  'core/services/KekDerivationService.cc',
  'core/controllers/VaultCreationOrchestrator.cc',
  'core/MultiUserTypes.cc',
  'core/MultiUserTypesSerDe.cc',
  'core/PasswordHistory.cc',
  'core/managers/AccountManager.cc',
  'core/managers/GroupManager.cc',
  'utils/ImportExport.cc',
  'utils/import_export/ImportExportDetail.cc',
  'utils/import_export/ImportExportCsv.cc',
  'utils/import_export/ImportExportKeePassXml.cc',
  'utils/import_export/ImportExport1Password1pif.cc',
  'utils/PasswordGenerator.cc',
  'utils/helpers/HelpManager.cc',
)

# Add YubiKey support if available
if yubikey_available
  sources += files('ui/dialogs/YubiKeyPromptDialog.cc')
  sources += files('ui/dialogs/YubiKeyManagerDialog.cc')
endif

# Combine regular sources with generated proto sources and resources
all_sources = sources + proto_sources + gresources + help_gresources

# Include directories
src_inc = include_directories(
  '.',
  'application',
  'ui',
  'ui/windows',
  'ui/dialogs',
  'ui/controllers',
  'core',
  'utils',
)

app_deps = common_deps + [backup_dep, storage_dep, fec_dep, vaultformat_dep, crypto_dep, fips_dep]
if yubikey_available
  app_deps += [dependency('keeptower-yubikey')]
endif

# Build executable
executable(
  project_name,
  all_sources,
  dependencies: app_deps,
  include_directories: [root_inc, src_inc],
  install: true,
)
