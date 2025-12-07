# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2025 tjdeveng

project('keeptower', 'cpp',
  version: '0.1.0',
  license: 'GPL-3.0-or-later',
  default_options: [
    'cpp_std=c++23',
    'warning_level=2',
  ],
  meson_version: '>= 0.59.0'
)

# Project information
project_name = 'keeptower'
application_id = 'com.example.keeptower'

# Dependencies
gtkmm_dep = dependency('gtkmm-4.0', version: '>= 4.10')
protobuf_dep = dependency('protobuf', version: '>= 3.0')
openssl_dep = dependency('openssl', version: '>= 1.1.0')

# Common dependencies
common_deps = [
  gtkmm_dep,
  protobuf_dep,
  openssl_dep,
]

# Configuration data
conf_data = configuration_data()
conf_data.set_quoted('PROJECT_NAME', project_name)
conf_data.set_quoted('APPLICATION_ID', application_id)
conf_data.set_quoted('VERSION', meson.project_version())
conf_data.set_quoted('PACKAGE_VERSION', meson.project_version())
conf_data.set_quoted('GETTEXT_PACKAGE', project_name)
conf_data.set_quoted('LOCALEDIR', get_option('prefix') / get_option('localedir'))

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
endif# Summary
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
