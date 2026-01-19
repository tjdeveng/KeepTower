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
  proto_dir = fs.parent(proto)

  proto_gen = custom_target(proto_base + '_proto',
    input: proto,
    output: [proto_base + '.pb.cc', proto_base + '.pb.h'],
    command: [protoc, '--proto_path=@SOURCE_ROOT@/src', '--cpp_out=@BUILD_ROOT@/src', '@INPUT@'],
  )

  proto_sources += proto_gen[0]
  proto_headers += proto_gen[1]
endforeach

# Application sources
sources = files(
  'main.cc',
  'application/Application.cc',
  'ui/windows/MainWindow.cc',
  'ui/dialogs/PasswordDialog.cc',
  'ui/dialogs/CreatePasswordDialog.cc',
  'ui/dialogs/PreferencesDialog.cc',
  'ui/dialogs/GroupCreateDialog.cc',
  'ui/dialogs/GroupRenameDialog.cc',
  'ui/dialogs/V2UserLoginDialog.cc',
  'ui/dialogs/ChangePasswordDialog.cc',
  'ui/dialogs/UserManagementDialog.cc',
  'ui/dialogs/VaultMigrationDialog.cc',
  'ui/widgets/AccountTreeWidget.cc',
  'ui/widgets/AccountRowWidget.cc',
  'ui/widgets/GroupRowWidget.cc',
  'ui/widgets/AccountDetailWidget.cc',
  'ui/controllers/AccountViewController.cc',
  'ui/controllers/SearchController.cc',
  'ui/controllers/AutoLockManager.cc',
  'ui/controllers/ClipboardManager.cc',
  'ui/managers/DialogManager.cc',
  'ui/managers/MenuManager.cc',
  'ui/managers/UIStateManager.cc',
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
  'core/services/VaultCryptoService.cc',
  'core/services/VaultYubiKeyService.cc',
  'core/services/VaultFileService.cc',
  'core/services/UsernameHashService.cc',
  'core/controllers/VaultCreationOrchestrator.cc',
  'core/ReedSolomon.cc',
  'core/MultiUserTypes.cc',
  'core/KeyWrapping.cc',
  'core/VaultFormatV2.cc',
  'core/PasswordHistory.cc',
  'core/crypto/VaultCrypto.cc',
  'core/io/VaultIO.cc',
  'core/serialization/VaultSerialization.cc',
  'core/format/VaultFormat.cc',
  'core/managers/AccountManager.cc',
  'core/managers/GroupManager.cc',
  'utils/ImportExport.cc',
  'utils/helpers/HelpManager.cc',
)

# Add YubiKey support if available
if yubikey_available
  sources += files('core/managers/YubiKeyManager.cc')
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

# Build executable
executable(
  project_name,
  all_sources,
  dependencies: common_deps,
  include_directories: [root_inc, src_inc],
  install: true,
)
