# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2025 tjdeveng

project('keeptower', 'cpp',
  version: '0.2.4',
  license: 'GPL-3.0-or-later',
  default_options: [
    'cpp_std=c++23',
    'warning_level=2',
  ],
  meson_version: '>= 0.59.0'
)

# Project information
project_name = 'keeptower'
application_id = 'com.tjdeveng.keeptower'

# Dependencies
gtkmm_dep = dependency('gtkmm-4.0', version: '>= 4.10')
protobuf_dep = dependency('protobuf', version: '>= 3.0')
openssl_dep = dependency('openssl', version: '>= 1.1.0')

# Find libcorrect using compiler find_library
cc = meson.get_compiler('cpp')
libcorrect_lib = cc.find_library('correct', required: true)
libcorrect_dep = declare_dependency(
  dependencies: libcorrect_lib
)

# YubiKey dependencies (optional for hardware 2FA support)
ykpers_dep = dependency('ykpers-1', required: false)
libyubikey_lib = cc.find_library('yubikey', required: false)
libyubikey_dep = declare_dependency(
  dependencies: libyubikey_lib
)

# Check if YubiKey support is available
yubikey_available = ykpers_dep.found() and libyubikey_lib.found()
if yubikey_available
  message('YubiKey support: ENABLED')
else
  warning('YubiKey support: DISABLED (install libyubikey-devel and ykpers-devel to enable)')
endif

# Common dependencies
common_deps = [
  gtkmm_dep,
  protobuf_dep,
  openssl_dep,
  libcorrect_dep,
]

# Add YubiKey dependencies if available
if yubikey_available
  common_deps += [libyubikey_dep, ykpers_dep]
endif

# Configuration data
conf_data = configuration_data()
conf_data.set_quoted('PROJECT_NAME', project_name)
conf_data.set_quoted('APPLICATION_ID', application_id)
conf_data.set_quoted('VERSION', meson.project_version())
conf_data.set_quoted('PACKAGE_VERSION', meson.project_version())
conf_data.set_quoted('GETTEXT_PACKAGE', project_name)
conf_data.set_quoted('LOCALEDIR', get_option('prefix') / get_option('localedir'))
conf_data.set_quoted('PROJECT_SOURCE_DIR', meson.project_source_root())
conf_data.set('HAVE_YUBIKEY_SUPPORT', yubikey_available)

# Generate config header
configure_file(
  output: 'config.h',
  configuration: conf_data
)

# Add project root to include directories
root_inc = include_directories('.')

# Subdirectories
subdir('src')
subdir('data')
subdir('resources')
subdir('po')

# Tests (optional, only if gtest is available)
gtest_dep = dependency('gtest', main: true, required: false)
if gtest_dep.found()
  subdir('tests')
endif

# Documentation (optional, only if doxygen is available)
doxygen = find_program('doxygen', required: false)
if doxygen.found()
  message('Doxygen found, API documentation target enabled')

  # Run doxygen target
  run_target('doxygen',
    command: [doxygen, meson.project_source_root() / 'Doxyfile'],
  )

  message('Run "ninja doxygen" or "meson compile -C build doxygen" to generate API documentation')
else
  message('Doxygen not found, API documentation disabled')
endif

# Post-install scripts for desktop integration
gnome = import('gnome')

gnome.post_install(
  glib_compile_schemas: true,
  gtk_update_icon_cache: true,
  update_desktop_database: true,
)

# Summary
summary({
  'prefix': get_option('prefix'),
  'bindir': get_option('bindir'),
  'datadir': get_option('datadir'),
}, section: 'Directories')

summary({
  'GTKmm4': gtkmm_dep.version(),
  'Protobuf': protobuf_dep.version(),
  'OpenSSL': openssl_dep.version(),
}, section: 'Dependencies')
