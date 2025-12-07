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

# Make resources available to src/
# The generated C file will be compiled into the executable
