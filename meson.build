# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2025 tjdeveng

project('keeptower', 'cpp',
  version: '0.3.1',
  license: 'GPL-3.0-or-later',
  default_options: [
    'cpp_std=c++23',
    'warning_level=2',
  ],
  meson_version: '>= 0.59.0'
)

# Import modules
fs = import('fs')

# Project information
project_name = 'keeptower'
application_id = 'com.tjdeveng.keeptower'

# Dependencies
gtkmm_dep = dependency('gtkmm-4.0', version: '>= 4.10')
sigcxx_dep = dependency('sigc++-3.0')
protobuf_dep = dependency('protobuf', version: '>= 3.0')

# Get compiler for library checks
cc = meson.get_compiler('cpp')

# OpenSSL 3.5+ required for FIPS-140-3 support
# First, try to find OpenSSL 3.5+ on the system
openssl_dep = dependency('openssl', version: '>= 3.5.0', required: false)

if not openssl_dep.found()
  message('OpenSSL 3.5.0+ not found on system')
else
  # OpenSSL 3.5 found via pkg-config
  # Check if it's from a non-standard location and set rpath if needed
  pkgconfig = find_program('pkg-config')
  result = run_command(pkgconfig, '--variable=libdir', 'openssl', check: false)
  if result.returncode() == 0
    openssl_libdir = result.stdout().strip()
    message('OpenSSL libdir from pkg-config: ' + openssl_libdir)
    # If OpenSSL is not in standard system paths, add rpath
    if not openssl_libdir.startswith('/usr/lib') and not openssl_libdir.startswith('/lib')
      message('→ Adding rpath for non-system OpenSSL: ' + openssl_libdir)
      add_project_link_arguments('-Wl,-rpath,' + openssl_libdir, language: 'cpp')
    else
      message('→ Using system OpenSSL, no rpath needed')
    endif
  endif
  message('Using system OpenSSL @0@ (FIPS-140-3 capable)'.format(openssl_dep.version()))
endif

if not openssl_dep.found()
  message('Attempting to build OpenSSL 3.5 locally...')

  # Check if we have a local build already
  local_openssl_root = meson.project_source_root() / 'build' / 'openssl-install'
  local_openssl_pc = local_openssl_root / 'lib' / 'pkgconfig' / 'openssl.pc'

  if not fs.exists(local_openssl_pc)
    message('========================================')
    message('Building OpenSSL 3.5 with FIPS support')
    message('This will take 5-10 minutes on first build')
    message('Subsequent builds will use the cached version')
    message('========================================')

    # Run the OpenSSL build script
    build_script = find_program('bash')
    script_path = meson.project_source_root() / 'scripts' / 'build-openssl-3.5.sh'
    result = run_command(build_script, script_path, check: false, capture: true)

    if result.returncode() != 0
      error('''
Failed to build OpenSSL 3.5. Build script output:

@0@

@1@

Please check the error messages above or run manually:
  bash scripts/build-openssl-3.5.sh
      '''.format(result.stdout(), result.stderr()))
    endif

    message('OpenSSL 3.5 build completed successfully!')
  else
    message('Found existing local OpenSSL build')
  endif

  # Use pkg-config to verify the local build
  pkgconfig = find_program('pkg-config')
  result = run_command(pkgconfig, '--modversion', 'openssl',
                      env: {'PKG_CONFIG_PATH': local_openssl_root / 'lib' / 'pkgconfig'},
                      check: false)

  if result.returncode() == 0
    openssl_version = result.stdout().strip()
    message('Detected locally built OpenSSL @0@'.format(openssl_version))

    # Determine which lib directory exists (lib or lib64)
    local_openssl_libdir = local_openssl_root / 'lib64'
    if not fs.is_dir(local_openssl_libdir)
      local_openssl_libdir = local_openssl_root / 'lib'
    endif
    message('Using OpenSSL library directory: @0@'.format(local_openssl_libdir))

    # Manually create dependency with local paths
    # Use relative path from source root
    openssl_dep = declare_dependency(
      include_directories: include_directories('build/openssl-install/include'),
      dependencies: [
        cc.find_library('ssl', dirs: local_openssl_libdir),
        cc.find_library('crypto', dirs: local_openssl_libdir)
      ],
      link_args: ['-L' + local_openssl_libdir],
    )

    # Set runtime path for the locally built OpenSSL
    add_project_link_arguments('-Wl,-rpath,' + local_openssl_libdir, language: 'cpp')
  else
    error('''
Failed to locate OpenSSL 3.5 after build attempt.

Please try building manually:
  bash scripts/build-openssl-3.5.sh

Then configure with:
  PKG_CONFIG_PATH="$(pwd)/build/openssl-install/lib/pkgconfig:$PKG_CONFIG_PATH" meson setup build
    ''')
  endif

  message('Using locally built OpenSSL 3.5+ (FIPS-140-3 capable)')
endif

# Find libcorrect using compiler find_library
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
  sigcxx_dep,
  protobuf_dep,
  openssl_dep,
  libcorrect_dep,
]

# Add YubiKey dependencies if available
if yubikey_available
  common_deps += [libyubikey_dep, ykpers_dep]
endif

# Code coverage support
if get_option('coverage')
  add_project_arguments('-fprofile-arcs', '-ftest-coverage', language: 'cpp')
  add_project_link_arguments('-lgcov', '--coverage', language: 'cpp')
  message('Code coverage: ENABLED')
else
  message('Code coverage: DISABLED (use -Dcoverage=true to enable)')
endif

# Configuration data
conf_data = configuration_data()
conf_data.set_quoted('PROJECT_NAME', project_name)
conf_data.set_quoted('APPLICATION_ID', application_id)
conf_data.set_quoted('VERSION', meson.project_version())
conf_data.set_quoted('PACKAGE_VERSION', meson.project_version())
conf_data.set_quoted('GETTEXT_PACKAGE', project_name)
conf_data.set_quoted('LOCALEDIR', get_option('prefix') / get_option('localedir'))
conf_data.set_quoted('DATADIR', get_option('prefix') / get_option('datadir'))
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

# Install help documentation files (if generated)
# These are installed to datadir for filesystem access
# GResources provides embedded fallback
help_files = [
  'resources/help/00-home.html',
  'resources/help/01-getting-started.html',
  'resources/help/02-installation.html',
  'resources/help/03-user-guide.html',
  'resources/help/04-faq.html',
  'resources/help/05-security.html',
  'resources/help/SECURITY_BEST_PRACTICES.html',
]

# Only install if files exist (pandoc was available during build)
foreach help_file : help_files
  if fs.exists(help_file)
    install_data(
      help_file,
      install_dir: get_option('datadir') / 'keeptower' / 'help',
    )
  endif
endforeach

# Install CSS for help files
if fs.exists('resources/help/css/help-style.css')
  install_data(
    'resources/help/css/help-style.css',
    install_dir: get_option('datadir') / 'keeptower' / 'help' / 'css',
  )
endif

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
