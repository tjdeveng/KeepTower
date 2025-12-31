#!/bin/bash
# Convenience script to run KeepTower with local GSettings schema

cd "$(dirname "$0")"

# Ensure schema is compiled (in case it was updated)
glib-compile-schemas build/data 2>/dev/null || true

GSETTINGS_SCHEMA_DIR=$PWD/build/data ./build/src/keeptower "$@"
#GSETTINGS_SCHEMA_DIR=$PWD/build/data G_MESSAGES_DEBUG=all ./build/src/keeptower "$@"

