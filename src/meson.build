# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2025 tjdeveng

# Import filesystem module
fs = import('fs')

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
  'core/VaultManager.cc',
  'core/ReedSolomon.cc',
  # Add more source files as you create them
)

# Combine regular sources with generated proto sources
all_sources = sources + proto_sources

# Include directories
src_inc = include_directories(
  '.',
  'application',
  'ui',
  'ui/windows',
  'ui/dialogs',
  'ui/widgets',
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
