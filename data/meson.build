# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2025 tjdeveng

# Import i18n module
i18n = import('i18n')

# Desktop file
desktop_file = application_id + '.desktop'

desktop_conf = configuration_data()
desktop_conf.set('APPLICATION_ID', application_id)

desktop_file_in = configure_file(
  input: desktop_file + '.in',
  output: desktop_file + '.in.tmp',
  configuration: desktop_conf,
)

i18n.merge_file(
  type: 'desktop',
  input: desktop_file_in,
  output: desktop_file,
  po_dir: '../po',
  install: true,
  install_dir: get_option('datadir') / 'applications',
)

# Validate desktop file
desktop_file_validate = find_program('desktop-file-validate', required: false)
if desktop_file_validate.found()
  test('Validate desktop file',
    desktop_file_validate,
    args: [meson.current_build_dir() / desktop_file],
  )
endif

# AppData/AppStream metadata
appdata_file = application_id + '.appdata.xml'

appdata_conf = configuration_data()
appdata_conf.set('APPLICATION_ID', application_id)

appdata_file_in = configure_file(
  input: appdata_file + '.in',
  output: appdata_file + '.in.tmp',
  configuration: appdata_conf,
)

# Create the final appdata file for testing
appdata_file_final = configure_file(
  input: appdata_file_in,
  output: appdata_file,
  copy: true,
)

# Skip translations for appdata for now - causes issues with msgfmt
install_data(
  appdata_file_in,
  install_dir: get_option('datadir') / 'metainfo',
  rename: appdata_file,
)

# Commented out for now - translation issues
# i18n.merge_file(
#   input: appdata_file_in,
#   output: appdata_file,
#   po_dir: '../po',
#   install: true,
#   install_dir: get_option('datadir') / 'metainfo',
# )

# Validate AppData file
appstreamcli = find_program('appstreamcli', required: false)
if appstreamcli.found()
  test('Validate appdata file',
    appstreamcli,
    args: ['validate', '--no-net', '--pedantic', meson.current_build_dir() / appdata_file],
  )
endif

# GSettings schema
gschema_file = application_id + '.gschema.xml'

install_data(
  gschema_file,
  install_dir: get_option('datadir') / 'glib-2.0' / 'schemas',
)

# Compile GSettings schemas for local testing
compile_schemas = find_program('glib-compile-schemas', required: false)
if compile_schemas.found()
  test('Validate schema file',
    compile_schemas,
    args: ['--strict', '--dry-run', meson.current_source_dir()],
  )
endif

# Icons
subdir('icons')
