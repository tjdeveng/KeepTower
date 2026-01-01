# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2025 tjdeveng

# Compile GResources
gnome = import('gnome')

gresources = gnome.compile_resources(
  'keeptower-resources',
  'gresource.xml',
  source_dir: '.',
  c_name: 'keeptower',
  dependencies: [],
)

# Note: Help resources are compiled in src/meson.build
# This is because they need to be linked into the main executable
