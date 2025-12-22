#!/bin/bash
# Convenience script to run KeepTower with local GSettings schema

cd "$(dirname "$0")"
#GSETTINGS_SCHEMA_DIR=$PWD/build/data ./build/src/keeptower "$@"
GSETTINGS_SCHEMA_DIR=$PWD/build/data G_MESSAGES_DEBUG=all ./build/src/keeptower "$@"

